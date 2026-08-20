/*
 * vsti/rtp.h - Encapsulamento de MPEG-2 TS em RTP (RFC 2250 / RFC 3550).
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_RTP_H
#define VSTI_RTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vsti/ts.h"

/* Cabecalho RTP fixo, sem lista de CSRC. */
#define VSTI_RTP_HEADER_SIZE 12

/*
 * Payload type 33 = MP2T, atribuido estaticamente pela RFC 3551.
 * Como e um tipo estatico, nao precisa de negociacao SDP para ser reconhecido
 * por receptores comuns (VLC, ffmpeg, analisadores de rede).
 */
#define VSTI_RTP_PT_MP2T 33u

/*
 * Sete pacotes TS por datagrama e o padrao de fato da industria.
 *
 * A conta: 7 * 188 = 1316 bytes de payload. Somando 12 de RTP, 8 de UDP e 20
 * de IPv4 chega-se a 1356 bytes, confortavelmente abaixo da MTU de 1500 de
 * Ethernet. Usar 8 pacotes (1504 + cabecalhos) forcaria fragmentacao IP, que
 * em rede de contribuicao de video multiplica o efeito de qualquer perda:
 * perder um fragmento invalida o datagrama inteiro.
 */
#define VSTI_RTP_TS_PER_PACKET 7
#define VSTI_RTP_MAX_TS_PER_PACKET 7
#define VSTI_RTP_MAX_PAYLOAD (VSTI_RTP_MAX_TS_PER_PACKET * VSTI_TS_PACKET_SIZE)
#define VSTI_RTP_MAX_DATAGRAM (VSTI_RTP_HEADER_SIZE + VSTI_RTP_MAX_PAYLOAD)

typedef struct {
    uint32_t ssrc;
    uint16_t sequence;
    uint32_t timestamp;   /* Relogio de 90 kHz */
    uint8_t  payload_type;
} vsti_rtp_session_t;

/*
 * Inicializa a sessao. Se `ssrc` for 0, um identificador e sorteado a partir
 * do relogio e do PID do processo — a RFC pede um SSRC aleatorio para que dois
 * emissores independentes nao colidam no mesmo grupo multicast.
 * Se `seq_start` for negativo, a sequencia inicial tambem e sorteada.
 */
void vsti_rtp_init(vsti_rtp_session_t *s, uint32_t ssrc, int32_t seq_start);

/*
 * Escreve o cabecalho RTP de 12 bytes em `dst` e avanca a sequencia.
 * `dst` precisa ter ao menos VSTI_RTP_HEADER_SIZE bytes.
 */
void vsti_rtp_write_header(vsti_rtp_session_t *s, uint8_t *dst, bool marker);

/* Estrutura de um cabecalho RTP recebido. */
typedef struct {
    uint8_t  version;
    bool     padding;
    bool     extension;
    uint8_t  csrc_count;
    bool     marker;
    uint8_t  payload_type;
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
    size_t   header_size;   /* 12 + 4 * csrc_count (+ extensao, se houver) */
} vsti_rtp_header_t;

/*
 * Decodifica um cabecalho RTP. Valida versao, contagem de CSRC e, se presente,
 * o cabecalho de extensao. Retorna false se o datagrama for curto demais ou
 * malformado.
 */
bool vsti_rtp_parse(const uint8_t *buf, size_t len, vsti_rtp_header_t *out);

/*
 * Detector de perda e reordenacao em uma sequencia RTP recebida.
 *
 * Trabalha em aritmetica de 16 bits com sinal para lidar com o wrap-around:
 * a diferenca entre sequencias e interpretada como o menor deslocamento
 * circular, de modo que 65535 -> 0 conta como avanco de 1, nao como salto de
 * 65535 para tras.
 */
typedef struct {
    bool     started;
    uint16_t expected;
    uint64_t received;
    uint64_t lost;
    uint64_t reordered;
    uint64_t duplicates;
} vsti_rtp_stats_t;

void vsti_rtp_stats_init(vsti_rtp_stats_t *st);
void vsti_rtp_stats_update(vsti_rtp_stats_t *st, uint16_t seq);

/*
 * Converte um contador de 27 MHz (PCR) para o relogio de 90 kHz do RTP.
 * A divisao por 300 e exata por construcao do PCR.
 */
static inline uint32_t vsti_rtp_ts_from_pcr27(uint64_t pcr27)
{
    return (uint32_t)((pcr27 / 300ull) & 0xFFFFFFFFull);
}

#endif /* VSTI_RTP_H */
