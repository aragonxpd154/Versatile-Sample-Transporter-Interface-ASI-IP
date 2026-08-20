/*
 * tests/test_ts.c - Camada de pacotes MPEG-2 TS.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#include "test_util.h"
#include "vsti/ts.h"

/* Monta um pacote basico com PID e CC dados, so payload. */
static void mk_payload_packet(uint8_t p[188], uint16_t pid, uint8_t cc, int pusi)
{
    memset(p, 0xFF, 188);
    p[0] = 0x47;
    p[1] = (uint8_t)(((pusi ? 0x40 : 0x00)) | ((pid >> 8) & 0x1F));
    p[2] = (uint8_t)(pid & 0xFF);
    p[3] = (uint8_t)(0x10 | (cc & 0x0F)); /* AFC = 01 (so payload) */
}

static void test_basic_parse(void)
{
    uint8_t p[188];
    vsti_ts_header_t h;

    mk_payload_packet(p, 0x1FC8, 5, 1);
    T_TRUE(vsti_ts_parse(p, &h), "pacote valido deve ser aceito");
    T_EQ_U(h.pid, 0x1FC8u, "PID");
    T_EQ_U(h.continuity_counter, 5u, "continuity_counter");
    T_TRUE(h.payload_unit_start, "PUSI");
    T_FALSE(h.transport_error, "TEI");
    T_TRUE(h.has_payload, "tem payload");
    T_FALSE(h.has_adaptation, "sem campo de adaptacao");
    T_EQ_U(h.payload_offset, 4u, "offset do payload");
    T_EQ_U(h.payload_length, 184u, "tamanho do payload");

    /* Sync errado deve ser rejeitado. */
    p[0] = 0x48;
    T_FALSE(vsti_ts_parse(p, &h), "sync invalido deve ser rejeitado");

    /* adaptation_field_control = 0 e reservado pela norma. */
    mk_payload_packet(p, 0x100, 0, 0);
    p[3] = 0x00;
    T_FALSE(vsti_ts_parse(p, &h), "AFC reservado deve ser rejeitado");

    /* Campo de adaptacao maior que o pacote e corrupcao. */
    mk_payload_packet(p, 0x100, 0, 0);
    p[3] = 0x30;  /* AFC = 11 */
    p[4] = 200;   /* 5 + 200 > 188 */
    T_FALSE(vsti_ts_parse(p, &h), "adaptation_length excessivo deve ser rejeitado");
}

static void test_pcr(void)
{
    uint8_t p[188];
    vsti_ts_header_t h;

    memset(p, 0xFF, 188);
    p[0] = 0x47;
    p[1] = 0x10;   /* PID = 0x1002 */
    p[2] = 0x02;
    p[3] = 0x30;   /* AFC = 11: adaptacao + payload */
    p[4] = 7;      /* comprimento do campo de adaptacao */
    p[5] = 0x10;   /* PCR_flag */

    /*
     * Codificamos PCR base = 0x1FFFFFFFF (valor maximo de 33 bits) e
     * extensao = 299 (maximo util). Testar os extremos e o que expoe erro de
     * deslocamento de bits, que e o defeito classico nesta funcao.
     */
    const uint64_t base = 0x1FFFFFFFFull;
    const uint16_t ext  = 299;

    p[6]  = (uint8_t)(base >> 25);
    p[7]  = (uint8_t)(base >> 17);
    p[8]  = (uint8_t)(base >> 9);
    p[9]  = (uint8_t)(base >> 1);
    p[10] = (uint8_t)(((base & 1) << 7) | 0x7E | ((ext >> 8) & 0x01));
    p[11] = (uint8_t)(ext & 0xFF);

    T_TRUE(vsti_ts_parse(p, &h), "pacote com PCR deve ser aceito");
    T_TRUE(h.has_pcr, "PCR presente");
    T_EQ_U(h.pcr.base, base, "PCR base");
    T_EQ_U(h.pcr.ext, ext, "PCR extensao");
    T_EQ_U(vsti_pcr_to_27mhz(h.pcr), base * 300ull + ext, "PCR em 27 MHz");

    /* Payload comeca logo apos o campo de adaptacao. */
    T_EQ_U(h.payload_offset, 12u, "offset apos adaptacao de 7 bytes");
    T_EQ_U(h.payload_length, 176u, "tamanho do payload com adaptacao");
}

static void test_alignment(void)
{
    /*
     * Constroi um buffer com 16 bytes de lixo antes de dez pacotes de 188.
     * O detector precisa achar o deslocamento 16 e o tamanho 188.
     */
    uint8_t buf[16 + 188 * 10];
    memset(buf, 0x00, sizeof(buf));
    for (int i = 0; i < 10; ++i) {
        buf[16 + i * 188] = 0x47;
    }

    size_t off = 0, sz = 0;
    T_TRUE(vsti_ts_find_alignment(buf, sizeof(buf), &off, &sz),
           "deve encontrar alinhamento");
    T_EQ_U(off, 16u, "deslocamento detectado");
    T_EQ_U(sz, 188u, "tamanho de pacote detectado");

    /* Variante de 204 bytes (com paridade Reed-Solomon). */
    uint8_t buf204[204 * 8];
    memset(buf204, 0x00, sizeof(buf204));
    for (int i = 0; i < 8; ++i) {
        buf204[i * 204] = 0x47;
    }
    T_TRUE(vsti_ts_find_alignment(buf204, sizeof(buf204), &off, &sz),
           "deve encontrar alinhamento de 204");
    T_EQ_U(off, 0u, "deslocamento 204");
    T_EQ_U(sz, 204u, "tamanho 204");

    /*
     * Um 0x47 isolado dentro de dados aleatorios nao pode ser confundido com
     * inicio de pacote: e para isso que exigimos varios syncs em sequencia.
     */
    uint8_t noise[188 * 6];
    memset(noise, 0x11, sizeof(noise));
    noise[37] = 0x47;
    T_FALSE(vsti_ts_find_alignment(noise, sizeof(noise), &off, &sz),
            "0x47 solitario nao deve gerar falso alinhamento");
}

static void test_continuity(void)
{
    vsti_cc_state_t st;
    vsti_ts_header_t h;
    uint8_t p[188];

    memset(&st, 0, sizeof(st));

    mk_payload_packet(p, 0x100, 0, 0);
    vsti_ts_parse(p, &h);
    T_EQ_U(vsti_cc_check(&st, &h), VSTI_CC_FIRST, "primeiro pacote");

    mk_payload_packet(p, 0x100, 1, 0);
    vsti_ts_parse(p, &h);
    T_EQ_U(vsti_cc_check(&st, &h), VSTI_CC_OK, "sequencia normal");

    /* Duplicata legal: mesmo CC repetido uma vez. */
    mk_payload_packet(p, 0x100, 1, 0);
    vsti_ts_parse(p, &h);
    T_EQ_U(vsti_cc_check(&st, &h), VSTI_CC_DUPLICATE, "duplicata permitida");

    /* Salto real. */
    mk_payload_packet(p, 0x100, 5, 0);
    vsti_ts_parse(p, &h);
    T_EQ_U(vsti_cc_check(&st, &h), VSTI_CC_ERROR, "salto deve virar erro");

    /* Wrap-around de 15 para 0 e continuidade correta, nao erro. */
    memset(&st, 0, sizeof(st));
    mk_payload_packet(p, 0x100, 15, 0);
    vsti_ts_parse(p, &h);
    vsti_cc_check(&st, &h);
    mk_payload_packet(p, 0x100, 0, 0);
    vsti_ts_parse(p, &h);
    T_EQ_U(vsti_cc_check(&st, &h), VSTI_CC_OK, "wrap 15->0");

    /*
     * Pacote sem payload nao incrementa o contador; repetir o valor e o
     * comportamento correto segundo a norma.
     */
    memset(&st, 0, sizeof(st));
    mk_payload_packet(p, 0x100, 7, 0);
    vsti_ts_parse(p, &h);
    vsti_cc_check(&st, &h);

    memset(p, 0xFF, 188);
    p[0] = 0x47; p[1] = 0x01; p[2] = 0x00;
    p[3] = 0x27; /* AFC = 10: so adaptacao, sem payload; CC = 7 */
    p[4] = 10;
    p[5] = 0x00;
    vsti_ts_parse(p, &h);
    T_FALSE(h.has_payload, "pacote so de adaptacao nao tem payload");
    T_EQ_U(vsti_cc_check(&st, &h), VSTI_CC_OK, "CC repetido sem payload e valido");

    /* O PID nulo nunca deve gerar erro de continuidade. */
    memset(&st, 0, sizeof(st));
    mk_payload_packet(p, VSTI_PID_NULL, 3, 0);
    vsti_ts_parse(p, &h);
    vsti_cc_check(&st, &h);
    mk_payload_packet(p, VSTI_PID_NULL, 9, 0);
    vsti_ts_parse(p, &h);
    T_EQ_U(vsti_cc_check(&st, &h), VSTI_CC_OK, "PID nulo isento de continuidade");
}

int main(void)
{
    test_basic_parse();
    test_pcr();
    test_alignment();
    test_continuity();
    return t_report("ts");
}
