/*
 * tests/test_psi.c - Remontagem de secoes e parsers de tabela.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#include "test_util.h"
#include "vsti/psi.h"

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Construcao de secoes sinteticas                                      */
/* ------------------------------------------------------------------ */

static void finish_section(uint8_t *sec, size_t total)
{
    /* section_length cobre tudo depois dos 3 primeiros bytes. */
    const size_t seclen = total - 3;
    sec[1] = (uint8_t)(0xB0 | ((seclen >> 8) & 0x0F));
    sec[2] = (uint8_t)(seclen & 0xFF);

    const uint32_t crc = vsti_crc32_mpeg2(sec, total - 4);
    sec[total - 4] = (uint8_t)(crc >> 24);
    sec[total - 3] = (uint8_t)(crc >> 16);
    sec[total - 2] = (uint8_t)(crc >> 8);
    sec[total - 1] = (uint8_t)(crc);
}

/* Monta uma PAT com `n` programas. Retorna o tamanho total da secao. */
static size_t build_pat(uint8_t *sec, uint16_t tsid, int n)
{
    sec[0] = VSTI_TID_PAT;
    sec[3] = (uint8_t)(tsid >> 8);
    sec[4] = (uint8_t)(tsid & 0xFF);
    sec[5] = 0xC1;  /* version 0, current_next = 1 */
    sec[6] = 0x00;
    sec[7] = 0x00;

    size_t p = 8;
    for (int i = 0; i < n; ++i) {
        const uint16_t prog = (uint16_t)(i + 1);
        const uint16_t pid  = (uint16_t)(0x1000 + i);
        sec[p++] = (uint8_t)(prog >> 8);
        sec[p++] = (uint8_t)(prog & 0xFF);
        sec[p++] = (uint8_t)(0xE0 | ((pid >> 8) & 0x1F));
        sec[p++] = (uint8_t)(pid & 0xFF);
    }
    p += 4; /* espaco do CRC */
    finish_section(sec, p);
    return p;
}

/* ------------------------------------------------------------------ */
/* Coletor de callbacks                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    int      count;
    uint8_t  last_tid;
    size_t   last_len;
    uint8_t  buf[VSTI_SECTION_MAX];
} collector_t;

static void collect(uint16_t pid, const vsti_section_header_t *hdr,
                    const uint8_t *data, size_t len, void *user)
{
    collector_t *c = (collector_t *)user;
    (void)pid;
    c->count++;
    c->last_tid = hdr->table_id;
    c->last_len = len;
    if (len <= sizeof(c->buf)) {
        memcpy(c->buf, data, len);
    }
}

/* ------------------------------------------------------------------ */
/* Testes                                                              */
/* ------------------------------------------------------------------ */

static void test_single_packet_section(void)
{
    uint8_t sec[256];
    const size_t seclen = build_pat(sec, 1, 3);

    /* Payload de um pacote TS: pointer_field = 0 seguido da secao. */
    uint8_t payload[184];
    memset(payload, 0xFF, sizeof(payload));
    payload[0] = 0x00;
    memcpy(payload + 1, sec, seclen);

    vsti_section_asm_t asmb;
    collector_t c = { 0 };
    vsti_section_asm_init(&asmb, VSTI_PID_PAT);
    vsti_section_asm_feed(&asmb, payload, sizeof(payload), true, collect, &c);

    T_EQ_U(c.count, 1, "uma secao entregue");
    T_EQ_U(c.last_tid, VSTI_TID_PAT, "table_id");
    T_EQ_U(c.last_len, seclen, "tamanho da secao");

    vsti_pat_t pat;
    T_TRUE(vsti_parse_pat(c.buf, c.last_len, &pat), "PAT deve ser parseavel");
    T_EQ_U(pat.transport_stream_id, 1u, "transport_stream_id");
    T_EQ_U(pat.count, 3u, "numero de programas");
    T_EQ_U(pat.entries[0].program_number, 1u, "programa 0");
    T_EQ_U(pat.entries[0].pid, 0x1000u, "PMT PID 0");
    T_EQ_U(pat.entries[2].pid, 0x1002u, "PMT PID 2");
}

static void test_section_split_across_packets(void)
{
    /*
     * Uma PAT com 100 programas ocupa mais de 400 bytes e portanto nao cabe em
     * um unico pacote. Este e o caminho que exercita o acumulador.
     */
    uint8_t sec[1024];
    const size_t seclen = build_pat(sec, 42, 100);
    T_TRUE(seclen > 184, "secao de teste deve exceder um pacote");

    vsti_section_asm_t asmb;
    collector_t c = { 0 };
    vsti_section_asm_init(&asmb, VSTI_PID_PAT);

    size_t sent = 0;
    bool first = true;
    while (sent < seclen) {
        uint8_t payload[184];
        memset(payload, 0xFF, sizeof(payload));

        size_t off = 0;
        if (first) {
            payload[0] = 0x00; /* pointer_field */
            off = 1;
        }
        const size_t take = (seclen - sent < sizeof(payload) - off)
                          ? (seclen - sent) : (sizeof(payload) - off);
        memcpy(payload + off, sec + sent, take);
        sent += take;

        vsti_section_asm_feed(&asmb, payload, sizeof(payload), first, collect, &c);
        first = false;
    }

    T_EQ_U(c.count, 1, "secao fragmentada entregue uma vez");
    T_EQ_U(c.last_len, seclen, "tamanho remontado");

    vsti_pat_t pat;
    T_TRUE(vsti_parse_pat(c.buf, c.last_len, &pat), "PAT grande parseavel");
    T_EQ_U(pat.transport_stream_id, 42u, "tsid da PAT grande");
    T_EQ_U(pat.count, 100u, "100 programas");
}

static void test_pointer_field_tail(void)
{
    /*
     * Cenario que quebra implementacoes ingenuas: um pacote com PUSI cujo
     * pointer_field aponta para depois da cauda da secao anterior. Ignorar o
     * ponteiro faz a secao anterior nunca ser entregue.
     */
    uint8_t secA[256], secB[256];
    const size_t lenA = build_pat(secA, 7, 5);
    const size_t lenB = build_pat(secB, 8, 5);

    vsti_section_asm_t asmb;
    collector_t c = { 0 };
    vsti_section_asm_init(&asmb, VSTI_PID_PAT);

    /* Pacote 1: pointer 0, secA truncada (faltam 10 bytes). */
    const size_t cut = lenA - 10;
    uint8_t p1[184];
    memset(p1, 0xFF, sizeof(p1));
    p1[0] = 0x00;
    memcpy(p1 + 1, secA, cut);
    vsti_section_asm_feed(&asmb, p1, 1 + cut, true, collect, &c);
    T_EQ_U(c.count, 0, "secao ainda incompleta");

    /* Pacote 2: pointer = 10 (cauda de secA), depois secB inteira. */
    uint8_t p2[184];
    memset(p2, 0xFF, sizeof(p2));
    p2[0] = 10;
    memcpy(p2 + 1, secA + cut, 10);
    memcpy(p2 + 1 + 10, secB, lenB);
    vsti_section_asm_feed(&asmb, p2, 1 + 10 + lenB, true, collect, &c);

    T_EQ_U(c.count, 2, "cauda e nova secao entregues");

    vsti_pat_t pat;
    T_TRUE(vsti_parse_pat(c.buf, c.last_len, &pat), "ultima secao parseavel");
    T_EQ_U(pat.transport_stream_id, 8u, "ultima secao e a secB");
}

static void test_two_sections_one_packet(void)
{
    uint8_t secA[256], secB[256];
    const size_t lenA = build_pat(secA, 11, 2);
    const size_t lenB = build_pat(secB, 12, 2);

    uint8_t payload[184];
    memset(payload, 0xFF, sizeof(payload));
    payload[0] = 0x00;
    memcpy(payload + 1, secA, lenA);
    memcpy(payload + 1 + lenA, secB, lenB);

    vsti_section_asm_t asmb;
    collector_t c = { 0 };
    vsti_section_asm_init(&asmb, VSTI_PID_PAT);
    vsti_section_asm_feed(&asmb, payload, sizeof(payload), true, collect, &c);

    T_EQ_U(c.count, 2, "duas secoes no mesmo pacote");
}

static void test_bad_crc_rejected(void)
{
    uint8_t sec[256];
    const size_t seclen = build_pat(sec, 5, 2);
    sec[9] ^= 0xFF; /* corrompe um byte do corpo */

    uint8_t payload[184];
    memset(payload, 0xFF, sizeof(payload));
    payload[0] = 0x00;
    memcpy(payload + 1, sec, seclen);

    vsti_section_asm_t asmb;
    collector_t c = { 0 };
    vsti_section_asm_init(&asmb, VSTI_PID_PAT);
    vsti_section_asm_feed(&asmb, payload, sizeof(payload), true, collect, &c);

    T_EQ_U(c.count, 0, "secao com CRC invalido deve ser descartada");
}

static void test_orphan_continuation(void)
{
    /*
     * Logo apos dar tune em um canal e comum receber o meio de uma secao. Sem
     * PUSI e sem estado, esses bytes precisam ser ignorados, e nao interpretados
     * como inicio de tabela.
     */
    uint8_t payload[184];
    memset(payload, 0x5A, sizeof(payload));

    vsti_section_asm_t asmb;
    collector_t c = { 0 };
    vsti_section_asm_init(&asmb, VSTI_PID_PAT);
    vsti_section_asm_feed(&asmb, payload, sizeof(payload), false, collect, &c);

    T_EQ_U(c.count, 0, "fragmento orfao deve ser descartado");
}

static void test_pmt_parse(void)
{
    /* PMT com dois fluxos: H.264 em 0x1001 e AAC LATM em 0x1003. */
    uint8_t sec[256];
    memset(sec, 0, sizeof(sec));

    sec[0] = VSTI_TID_PMT;
    sec[3] = 0x00; sec[4] = 0x01;  /* program_number = 1 */
    sec[5] = 0xC1;
    sec[6] = 0x00; sec[7] = 0x00;
    sec[8] = 0xF0; sec[9] = 0x02;  /* PCR_PID = 0x1002 (0xE0 | 0x10) */
    sec[10] = 0xF0; sec[11] = 0x00; /* program_info_length = 0 */

    size_t p = 12;
    sec[p++] = 0x1B;               /* H.264 */
    sec[p++] = 0xF0; sec[p++] = 0x01;
    sec[p++] = 0xF0; sec[p++] = 0x00;

    sec[p++] = 0x11;               /* AAC LATM */
    sec[p++] = 0xF0; sec[p++] = 0x03;
    sec[p++] = 0xF0; sec[p++] = 0x06;
    /* Descritor de idioma: tag 0x0A, len 4, "por", audio_type 0 */
    sec[p++] = 0x0A; sec[p++] = 0x04;
    sec[p++] = 'p'; sec[p++] = 'o'; sec[p++] = 'r'; sec[p++] = 0x00;

    p += 4;
    finish_section(sec, p);

    vsti_pmt_t pmt;
    T_TRUE(vsti_parse_pmt(sec, p, &pmt), "PMT parseavel");
    T_EQ_U(pmt.program_number, 1u, "program_number");
    T_EQ_U(pmt.pcr_pid, 0x1002u, "PCR PID");
    T_EQ_U(pmt.stream_count, 2u, "dois fluxos");
    T_EQ_U(pmt.streams[0].stream_type, 0x1Bu, "tipo do fluxo 0");
    T_EQ_U(pmt.streams[0].elementary_pid, 0x1001u, "PID do fluxo 0");
    T_EQ_U(pmt.streams[1].elementary_pid, 0x1003u, "PID do fluxo 1");
    T_EQ_U(pmt.streams[1].es_info_length, 6u, "descritores do fluxo 1");
    T_EQ_STR(vsti_stream_type_name(0x1B), "H.264 / AVC", "nome do tipo 0x1B");
}

static void test_mjd_conversion(void)
{
    /*
     * MJD 59514 = 2021-10-27, mesma data das capturas do repositorio.
     * Unix = (59514 - 40587) * 86400 = 1635292800 (2021-10-27T00:00:00Z).
     */
    const uint8_t t[5] = { 0xE8, 0x7A, 0x18, 0x35, 0x07 }; /* 59514, 18:35:07 */
    const int64_t expected = (int64_t)(59514 - 40587) * 86400 + 18 * 3600 + 35 * 60 + 7;
    T_EQ_U(vsti_mjd_bcd_to_unix(t), (uint64_t)expected, "MJD + BCD para epoch");

    /* Campo indefinido (todos os bits em 1) deve virar zero. */
    const uint8_t undef[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    T_EQ_U(vsti_mjd_bcd_to_unix(undef), 0u, "start_time indefinido");

    /* Duracao BCD 01:30:00 = 5400 s. */
    const uint8_t dur[3] = { 0x01, 0x30, 0x00 };
    T_EQ_U(vsti_bcd_duration_to_seconds(dur), 5400u, "duracao BCD");
}

int main(void)
{
    test_single_packet_section();
    test_section_split_across_packets();
    test_pointer_field_tail();
    test_two_sections_one_packet();
    test_bad_crc_rejected();
    test_orphan_continuation();
    test_pmt_parse();
    test_mjd_conversion();
    return t_report("psi");
}
