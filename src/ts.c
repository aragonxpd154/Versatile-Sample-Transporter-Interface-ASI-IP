/*
 * src/ts.c - Implementacao da camada de pacotes MPEG-2 TS.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#include "vsti/ts.h"

#include <string.h>

/*
 * Le um PCR/OPCR de 6 bytes.
 *
 * Layout (ISO/IEC 13818-1, 2.4.3.5):
 *   program_clock_reference_base    33 bits
 *   reserved                         6 bits
 *   program_clock_reference_extension 9 bits
 *
 * Os 33 bits da base cruzam a fronteira do byte 4, por isso o deslocamento de
 * 7 posicoes no bit mais significativo do quinto byte.
 */
static vsti_pcr_t read_pcr(const uint8_t *p)
{
    vsti_pcr_t pcr;
    pcr.base = ((uint64_t)p[0] << 25) |
               ((uint64_t)p[1] << 17) |
               ((uint64_t)p[2] << 9)  |
               ((uint64_t)p[3] << 1)  |
               ((uint64_t)p[4] >> 7);
    pcr.ext  = (uint16_t)(((p[4] & 0x01u) << 8) | p[5]);
    return pcr;
}

bool vsti_ts_parse(const uint8_t *pkt, vsti_ts_header_t *out)
{
    if (pkt == NULL || out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    if (pkt[0] != VSTI_TS_SYNC_BYTE) {
        return false;
    }

    out->transport_error      = (pkt[1] & 0x80u) != 0;
    out->payload_unit_start   = (pkt[1] & 0x40u) != 0;
    out->transport_priority   = (pkt[1] & 0x20u) != 0;
    out->pid                  = (uint16_t)(((pkt[1] & 0x1Fu) << 8) | pkt[2]);
    out->transport_scrambling = (uint8_t)((pkt[3] >> 6) & 0x03u);
    out->adaptation_control   = (vsti_afc_t)((pkt[3] >> 4) & 0x03u);
    out->continuity_counter   = (uint8_t)(pkt[3] & 0x0Fu);

    /*
     * O valor 0 de adaptation_field_control e reservado pela norma. Tratamos
     * como pacote invalido em vez de tentar adivinhar, para que o contador de
     * erros do analisador reflita a realidade do fluxo.
     */
    if (out->adaptation_control == VSTI_AFC_RESERVED) {
        return false;
    }

    size_t offset = 4; /* Logo apos o cabecalho de 4 bytes */

    if (out->adaptation_control == VSTI_AFC_ADAPT_ONLY ||
        out->adaptation_control == VSTI_AFC_ADAPT_PAYLOAD) {
        out->has_adaptation    = true;
        out->adaptation_length = pkt[4];

        /*
         * O campo de adaptacao vai do byte 5 ate 5 + adaptation_length - 1.
         * Se isso ultrapassar os 188 bytes o pacote esta corrompido.
         */
        if (5u + (size_t)out->adaptation_length > VSTI_TS_PACKET_SIZE) {
            return false;
        }

        /*
         * Comprimento zero e legal: significa um unico byte de enchimento,
         * sem flags. Nesse caso nao ha byte de flags para ler.
         */
        if (out->adaptation_length > 0) {
            const uint8_t flags = pkt[5];
            out->discontinuity = (flags & 0x80u) != 0;
            out->random_access = (flags & 0x40u) != 0;
            const bool pcr_flag  = (flags & 0x10u) != 0;
            const bool opcr_flag = (flags & 0x08u) != 0;

            size_t p = 6; /* Primeiro byte apos o byte de flags */

            if (pcr_flag) {
                if (p + 6 > 5u + (size_t)out->adaptation_length ||
                    p + 6 > VSTI_TS_PACKET_SIZE) {
                    return false;
                }
                out->has_pcr = true;
                out->pcr     = read_pcr(&pkt[p]);
                p += 6;
            }
            if (opcr_flag) {
                if (p + 6 > 5u + (size_t)out->adaptation_length ||
                    p + 6 > VSTI_TS_PACKET_SIZE) {
                    return false;
                }
                out->has_opcr = true;
                out->opcr     = read_pcr(&pkt[p]);
                p += 6;
            }
        }

        offset = 5u + (size_t)out->adaptation_length;
    }

    if (out->adaptation_control == VSTI_AFC_PAYLOAD_ONLY ||
        out->adaptation_control == VSTI_AFC_ADAPT_PAYLOAD) {
        if (offset >= VSTI_TS_PACKET_SIZE) {
            /*
             * O campo de adaptacao consumiu o pacote inteiro mas o cabecalho
             * afirma que ha payload. Fluxo malformado.
             */
            return false;
        }
        out->has_payload    = true;
        out->payload_offset = (uint8_t)offset;
        out->payload_length = (uint8_t)(VSTI_TS_PACKET_SIZE - offset);
    }

    return true;
}

bool vsti_ts_find_alignment(const uint8_t *buf, size_t len,
                            size_t *out_offset, size_t *out_packet_size)
{
    static const size_t sizes[] = {
        VSTI_TS_PACKET_SIZE,
        VSTI_TS_PACKET_SIZE_192,
        VSTI_TS_PACKET_SIZE_204,
        VSTI_TS_PACKET_SIZE_208
    };
    const size_t n_sizes = sizeof(sizes) / sizeof(sizes[0]);

    if (buf == NULL || out_offset == NULL || out_packet_size == NULL) {
        return false;
    }

    /*
     * Testamos primeiro o tamanho, depois o deslocamento. A ordem importa:
     * 188 e de longe o caso comum, e queremos casar com ele antes de
     * considerar as variantes com FEC, cujo espacamento maior poderia gerar um
     * falso positivo em um fluxo de 188 muito curto.
     */
    for (size_t si = 0; si < n_sizes; ++si) {
        const size_t psize = sizes[si];
        const size_t span  = psize * (VSTI_TS_SYNC_RUN - 1) + 1;

        if (len < span) {
            continue;
        }

        for (size_t off = 0; off + span <= len && off < psize; ++off) {
            bool all_sync = true;
            for (size_t k = 0; k < VSTI_TS_SYNC_RUN; ++k) {
                /*
                 * Na variante de 192 bytes o pacote e precedido por 4 bytes de
                 * timecode, entao o sync fica no quinto byte do bloco.
                 */
                const size_t sync_at = (psize == VSTI_TS_PACKET_SIZE_192)
                                     ? off + k * psize + 4
                                     : off + k * psize;
                if (sync_at >= len || buf[sync_at] != VSTI_TS_SYNC_BYTE) {
                    all_sync = false;
                    break;
                }
            }
            if (all_sync) {
                *out_offset      = (psize == VSTI_TS_PACKET_SIZE_192) ? off + 4 : off;
                *out_packet_size = psize;
                return true;
            }
        }
    }

    return false;
}

vsti_cc_result_t vsti_cc_check(vsti_cc_state_t *st, const vsti_ts_header_t *hdr)
{
    if (st == NULL || hdr == NULL) {
        return VSTI_CC_ERROR;
    }

    /*
     * O PID nulo (0x1FFF) e enchimento puro: a norma nao define continuidade
     * para ele e transmissores costumam deixar o contador fixo. Contar erro
     * ali produziria ruido em todo fluxo com padding, que e a maioria.
     */
    if (hdr->pid == VSTI_PID_NULL) {
        return VSTI_CC_OK;
    }

    const uint8_t cc = hdr->continuity_counter;

    if (!st->seen) {
        st->seen             = true;
        st->last_cc          = cc;
        st->last_had_payload = hdr->has_payload;
        return VSTI_CC_FIRST;
    }

    const uint8_t prev = st->last_cc;
    const bool had_payload = st->last_had_payload;

    st->last_cc          = cc;
    st->last_had_payload = hdr->has_payload;

    /*
     * Descontinuidade anunciada pelo transmissor (splice, reinicio de encoder).
     * A norma manda aceitar o novo valor sem considerar erro.
     */
    if (hdr->discontinuity) {
        return VSTI_CC_DISCONTINUITY;
    }

    /*
     * Pacotes sem payload nao incrementam o contador. Repetir o valor anterior
     * e o comportamento correto, nao um erro.
     */
    if (!hdr->has_payload) {
        return (cc == prev) ? VSTI_CC_OK : VSTI_CC_ERROR;
    }

    /*
     * Com payload, o esperado e prev + 1 em modulo 16. O caso cc == prev e uma
     * duplicata, que a norma permite uma unica vez por pacote (usada para dar
     * robustez a tabelas PSI criticas). So marcamos erro fora desses dois.
     */
    const uint8_t expected = (uint8_t)((prev + 1u) & 0x0Fu);

    if (cc == expected) {
        return VSTI_CC_OK;
    }
    if (cc == prev && had_payload) {
        return VSTI_CC_DUPLICATE;
    }
    return VSTI_CC_ERROR;
}
