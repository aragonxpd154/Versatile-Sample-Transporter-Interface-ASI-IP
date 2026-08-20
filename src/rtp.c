/*
 * src/rtp.c - Encapsulamento e decodificacao de RTP para MPEG-2 TS.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#include "vsti/rtp.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint32_t seed_from_clock(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    /*
     * Mistura simples de nanossegundos, segundos e PID. Nao pretende ser
     * criptografica: o objetivo da RFC 3550 aqui e apenas evitar colisao de
     * SSRC entre emissores que compartilhem um grupo multicast.
     */
    uint32_t x = (uint32_t)ts.tv_nsec;
    x ^= (uint32_t)ts.tv_sec * 2654435761u;
    x ^= (uint32_t)getpid() * 40503u;
    x ^= x >> 16;
    x *= 2246822519u;
    x ^= x >> 13;
    return x ? x : 0x5EEDu;
}

void vsti_rtp_init(vsti_rtp_session_t *s, uint32_t ssrc, int32_t seq_start)
{
    memset(s, 0, sizeof(*s));
    s->ssrc         = (ssrc != 0) ? ssrc : seed_from_clock();
    s->sequence     = (seq_start >= 0) ? (uint16_t)seq_start
                                       : (uint16_t)(seed_from_clock() & 0xFFFFu);
    s->payload_type = VSTI_RTP_PT_MP2T;
    s->timestamp    = 0;
}

void vsti_rtp_write_header(vsti_rtp_session_t *s, uint8_t *dst, bool marker)
{
    /*
     * Byte 0: V=2 (bits 7-6), P=0, X=0, CC=0.
     * Nao usamos padding nem extensao: ambos so atrapalhariam receptores
     * simples, e o payload TS ja tem tamanho fixo e alinhado.
     */
    dst[0] = 0x80u;
    dst[1] = (uint8_t)((marker ? 0x80u : 0x00u) | (s->payload_type & 0x7Fu));

    dst[2] = (uint8_t)(s->sequence >> 8);
    dst[3] = (uint8_t)(s->sequence & 0xFFu);

    dst[4] = (uint8_t)(s->timestamp >> 24);
    dst[5] = (uint8_t)(s->timestamp >> 16);
    dst[6] = (uint8_t)(s->timestamp >> 8);
    dst[7] = (uint8_t)(s->timestamp);

    dst[8]  = (uint8_t)(s->ssrc >> 24);
    dst[9]  = (uint8_t)(s->ssrc >> 16);
    dst[10] = (uint8_t)(s->ssrc >> 8);
    dst[11] = (uint8_t)(s->ssrc);

    /* Wrap-around de 16 bits e o comportamento correto e esperado. */
    s->sequence = (uint16_t)(s->sequence + 1u);
}

bool vsti_rtp_parse(const uint8_t *buf, size_t len, vsti_rtp_header_t *out)
{
    if (buf == NULL || out == NULL || len < VSTI_RTP_HEADER_SIZE) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->version = (uint8_t)((buf[0] >> 6) & 0x03u);
    if (out->version != 2) {
        return false;
    }

    out->padding      = (buf[0] & 0x20u) != 0;
    out->extension    = (buf[0] & 0x10u) != 0;
    out->csrc_count   = (uint8_t)(buf[0] & 0x0Fu);
    out->marker       = (buf[1] & 0x80u) != 0;
    out->payload_type = (uint8_t)(buf[1] & 0x7Fu);
    out->sequence     = (uint16_t)((buf[2] << 8) | buf[3]);
    out->timestamp    = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                        ((uint32_t)buf[6] << 8)  | (uint32_t)buf[7];
    out->ssrc         = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) |
                        ((uint32_t)buf[10] << 8) | (uint32_t)buf[11];

    size_t hs = VSTI_RTP_HEADER_SIZE + 4u * (size_t)out->csrc_count;
    if (hs > len) {
        return false;
    }

    if (out->extension) {
        /*
         * Cabecalho de extensao: 16 bits de perfil, 16 bits de comprimento em
         * palavras de 32 bits, seguidos pelo corpo.
         */
        if (hs + 4 > len) {
            return false;
        }
        const uint16_t ext_words = (uint16_t)((buf[hs + 2] << 8) | buf[hs + 3]);
        hs += 4u + 4u * (size_t)ext_words;
        if (hs > len) {
            return false;
        }
    }

    out->header_size = hs;
    return true;
}

void vsti_rtp_stats_init(vsti_rtp_stats_t *st)
{
    memset(st, 0, sizeof(*st));
}

void vsti_rtp_stats_update(vsti_rtp_stats_t *st, uint16_t seq)
{
    st->received++;

    if (!st->started) {
        st->started  = true;
        st->expected = (uint16_t)(seq + 1u);
        return;
    }

    /*
     * delta interpretado como inteiro de 16 bits com sinal resolve o
     * wrap-around sem casos especiais: 0 significa "exatamente o esperado",
     * positivo significa que pulamos `delta` pacotes, e negativo significa que
     * chegou um pacote antigo (reordenacao ou duplicata).
     */
    const int16_t delta = (int16_t)(seq - st->expected);

    if (delta == 0) {
        st->expected = (uint16_t)(seq + 1u);
    } else if (delta > 0) {
        st->lost    += (uint64_t)delta;
        st->expected = (uint16_t)(seq + 1u);
    } else if (delta == -1) {
        st->duplicates++;
    } else {
        st->reordered++;
        /*
         * Um pacote atrasado que chega depois ja foi contado como perdido.
         * Corrigimos a estatistica para nao inflar a taxa de perda.
         */
        if (st->lost > 0) {
            st->lost--;
        }
    }
}
