/*
 * tests/test_rtp.c - Cabecalho RTP e estatisticas de sequencia.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#include "test_util.h"
#include "vsti/rtp.h"

static void test_header_roundtrip(void)
{
    vsti_rtp_session_t s;
    uint8_t buf[VSTI_RTP_MAX_DATAGRAM];

    vsti_rtp_init(&s, 0xDEADBEEFu, 1000);
    T_EQ_U(s.ssrc, 0xDEADBEEFu, "SSRC fixado");
    T_EQ_U(s.sequence, 1000u, "sequencia inicial");
    T_EQ_U(s.payload_type, VSTI_RTP_PT_MP2T, "payload type MP2T");

    s.timestamp = 0x12345678u;
    vsti_rtp_write_header(&s, buf, false);

    /* Preenche o payload com pacotes TS falsos para o parser aceitar. */
    memset(buf + VSTI_RTP_HEADER_SIZE, 0, VSTI_RTP_MAX_PAYLOAD);
    buf[VSTI_RTP_HEADER_SIZE] = 0x47;

    vsti_rtp_header_t h;
    T_TRUE(vsti_rtp_parse(buf, VSTI_RTP_MAX_DATAGRAM, &h), "cabecalho parseavel");
    T_EQ_U(h.version, 2u, "versao 2");
    T_FALSE(h.padding, "sem padding");
    T_FALSE(h.extension, "sem extensao");
    T_EQ_U(h.csrc_count, 0u, "sem CSRC");
    T_FALSE(h.marker, "marker desligado");
    T_EQ_U(h.payload_type, 33u, "PT = 33");
    T_EQ_U(h.sequence, 1000u, "sequencia escrita");
    T_EQ_U(h.timestamp, 0x12345678u, "timestamp");
    T_EQ_U(h.ssrc, 0xDEADBEEFu, "SSRC");
    T_EQ_U(h.header_size, 12u, "tamanho do cabecalho");

    T_EQ_U(s.sequence, 1001u, "sequencia avancou");

    /* Marker deve aparecer no bit 7 do segundo byte. */
    vsti_rtp_write_header(&s, buf, true);
    T_TRUE(vsti_rtp_parse(buf, VSTI_RTP_MAX_DATAGRAM, &h), "parse com marker");
    T_TRUE(h.marker, "marker ligado");
}

static void test_sequence_wrap(void)
{
    vsti_rtp_session_t s;
    uint8_t buf[VSTI_RTP_HEADER_SIZE];

    vsti_rtp_init(&s, 1, 65535);
    vsti_rtp_write_header(&s, buf, false);
    T_EQ_U(s.sequence, 0u, "65535 deve envolver para 0");
}

static void test_reject_bad_headers(void)
{
    uint8_t buf[64];
    vsti_rtp_header_t h;

    memset(buf, 0, sizeof(buf));
    T_FALSE(vsti_rtp_parse(buf, 4, &h), "datagrama curto demais");

    buf[0] = 0x00; /* versao 0 */
    T_FALSE(vsti_rtp_parse(buf, sizeof(buf), &h), "versao invalida");

    /* CC declarado maior do que cabe no datagrama. */
    buf[0] = 0x8F; /* versao 2, CSRC count = 15 -> precisa de 72 bytes */
    T_FALSE(vsti_rtp_parse(buf, 20, &h), "CSRC alem do datagrama");

    /* Extensao com comprimento absurdo. */
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x90; /* versao 2, X = 1 */
    buf[14] = 0xFF; buf[15] = 0xFF; /* 65535 palavras de extensao */
    T_FALSE(vsti_rtp_parse(buf, sizeof(buf), &h), "extensao alem do datagrama");
}

static void test_loss_stats(void)
{
    vsti_rtp_stats_t st;

    vsti_rtp_stats_init(&st);
    for (uint16_t i = 0; i < 10; ++i) {
        vsti_rtp_stats_update(&st, (uint16_t)(100 + i));
    }
    T_EQ_U(st.received, 10u, "dez recebidos");
    T_EQ_U(st.lost, 0u, "nenhuma perda");

    /* Salto de 3: pacotes 110, 111, 112 se perderam. */
    vsti_rtp_stats_update(&st, 113);
    T_EQ_U(st.lost, 3u, "tres perdidos");

    /* Chegada atrasada de um dos perdidos abate a conta. */
    vsti_rtp_stats_update(&st, 111);
    T_EQ_U(st.reordered, 1u, "um reordenado");
    T_EQ_U(st.lost, 2u, "perda corrigida para dois");

    /*
     * Wrap-around: apos 65535 vem 0. A aritmetica de 16 bits com sinal precisa
     * enxergar isso como avanco de um, e nao como salto gigante.
     */
    vsti_rtp_stats_init(&st);
    vsti_rtp_stats_update(&st, 65534);
    vsti_rtp_stats_update(&st, 65535);
    vsti_rtp_stats_update(&st, 0);
    vsti_rtp_stats_update(&st, 1);
    T_EQ_U(st.lost, 0u, "wrap nao deve contar perda");
    T_EQ_U(st.received, 4u, "quatro recebidos no wrap");
}

static void test_pcr_to_rtp_timestamp(void)
{
    /*
     * O relogio RTP para MP2T e o de 90 kHz, que corresponde exatamente a base
     * do PCR. Um PCR com base B e extensao E deve virar timestamp B.
     */
    vsti_pcr_t pcr = { .base = 123456789ull, .ext = 271 };
    const uint64_t pcr27 = vsti_pcr_to_27mhz(pcr);
    T_EQ_U(vsti_rtp_ts_from_pcr27(pcr27), 123456789u,
           "timestamp de 90 kHz derivado do PCR");
}

int main(void)
{
    test_header_roundtrip();
    test_sequence_wrap();
    test_reject_bad_headers();
    test_loss_stats();
    test_pcr_to_rtp_timestamp();
    return t_report("rtp");
}
