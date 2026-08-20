/*
 * tests/test_desc.c - Descritores e decodificacao de texto ISDB-Tb.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#include "test_util.h"
#include "vsti/desc.h"

static void test_iterator(void)
{
    /* Tres descritores em sequencia: 0x0A(4), 0x52(1), 0x48(0). */
    const uint8_t buf[] = {
        0x0A, 0x04, 'p', 'o', 'r', 0x00,
        0x52, 0x01, 0x01,
        0x48, 0x00
    };

    vsti_desc_iter_t it;
    vsti_desc_t d;
    vsti_desc_iter_init(&it, buf, sizeof(buf));

    T_TRUE(vsti_desc_iter_next(&it, &d), "primeiro descritor");
    T_EQ_U(d.tag, 0x0Au, "tag 0x0A");
    T_EQ_U(d.length, 4u, "comprimento 4");

    T_TRUE(vsti_desc_iter_next(&it, &d), "segundo descritor");
    T_EQ_U(d.tag, 0x52u, "tag 0x52");

    T_TRUE(vsti_desc_iter_next(&it, &d), "terceiro descritor");
    T_EQ_U(d.tag, 0x48u, "tag 0x48");
    T_EQ_U(d.length, 0u, "descritor vazio e valido");

    T_FALSE(vsti_desc_iter_next(&it, &d), "fim do laco");
}

static void test_truncated_descriptor(void)
{
    /* Declara comprimento 10 mas so ha 2 bytes de corpo. */
    const uint8_t buf[] = { 0x48, 0x0A, 0x01, 0x02 };

    vsti_desc_iter_t it;
    vsti_desc_t d;
    vsti_desc_iter_init(&it, buf, sizeof(buf));
    T_FALSE(vsti_desc_iter_next(&it, &d),
            "descritor truncado deve ser rejeitado, nao lido parcialmente");
}

static void test_text_decoding(void)
{
    char out[256];

    /* ASCII puro passa intacto. */
    const uint8_t ascii[] = "Jornal da Gazeta";
    vsti_arib_text_to_utf8(ascii, sizeof(ascii) - 1, out, sizeof(out));
    T_EQ_STR(out, "Jornal da Gazeta", "ASCII intacto");

    /*
     * Acentuacao em ISO-8859-15: 0xE7 = c cedilha, 0xE3 = a til.
     * "Informacao" com cedilha e til deve virar UTF-8 correto.
     */
    const uint8_t latin[] = { 'I', 'n', 'f', 'o', 'r', 'm', 'a',
                              0xE7, 0xE3, 'o' };
    vsti_arib_text_to_utf8(latin, sizeof(latin), out, sizeof(out));
    T_EQ_STR(out, "Informa\xC3\xA7\xC3\xA3o", "acentos convertidos para UTF-8");

    /* O sinal de euro ocupa 0xA4 na 8859-15 (difere da 8859-1). */
    const uint8_t euro[] = { 0xA4 };
    vsti_arib_text_to_utf8(euro, 1, out, sizeof(out));
    T_EQ_STR(out, "\xE2\x82\xAC", "0xA4 vira sinal de euro");

    /* Codigos de controle C0 e C1 devem sumir. */
    const uint8_t ctrl[] = { 'A', 0x0C, 0x89, 'B' };
    vsti_arib_text_to_utf8(ctrl, sizeof(ctrl), out, sizeof(out));
    T_EQ_STR(out, "AB", "controles descartados");

    /* Sequencia de escape ESC 0x24 0x42 nao deve gerar texto. */
    const uint8_t esc[] = { 'X', 0x1B, 0x24, 0x42, 'Y' };
    vsti_arib_text_to_utf8(esc, sizeof(esc), out, sizeof(out));
    T_EQ_STR(out, "XY", "sequencia de escape descartada");

    /* Espaco nao separavel vira espaco comum. */
    const uint8_t nbsp[] = { 'a', 0xA0, 'b' };
    vsti_arib_text_to_utf8(nbsp, sizeof(nbsp), out, sizeof(out));
    T_EQ_STR(out, "a b", "0xA0 normalizado");

    /* Entrada vazia produz string vazia, nao lixo. */
    vsti_arib_text_to_utf8(NULL, 0, out, sizeof(out));
    T_EQ_STR(out, "", "entrada nula");

    /* Buffer pequeno nao pode transbordar. */
    char tiny[4];
    const uint8_t longstr[] = "abcdefghij";
    vsti_arib_text_to_utf8(longstr, sizeof(longstr) - 1, tiny, sizeof(tiny));
    T_TRUE(strlen(tiny) < sizeof(tiny), "saida truncada com terminador");
}

static void test_service_descriptor(void)
{
    /*
     * service_descriptor: tipo 0x01 (TV digital), provedor "TV Gazeta",
     * nome "GAZETA HD".
     */
    uint8_t body[64];
    size_t p = 0;
    body[p++] = 0x01;
    body[p++] = 9;
    memcpy(body + p, "TV Gazeta", 9); p += 9;
    body[p++] = 9;
    memcpy(body + p, "GAZETA HD", 9); p += 9;

    vsti_desc_t d = { .tag = VSTI_DESC_SERVICE, .length = (uint8_t)p, .data = body };

    uint8_t stype = 0;
    char prov[64], name[64];
    T_TRUE(vsti_desc_service(&d, &stype, prov, sizeof(prov), name, sizeof(name)),
           "service_descriptor parseavel");
    T_EQ_U(stype, 0x01u, "service_type");
    T_EQ_STR(prov, "TV Gazeta", "provedor");
    T_EQ_STR(name, "GAZETA HD", "nome do servico");
    T_EQ_STR(vsti_service_type_name(0x01), "TV digital", "nome do tipo");

    /* Comprimentos inconsistentes devem ser rejeitados. */
    body[1] = 200;
    T_FALSE(vsti_desc_service(&d, &stype, prov, sizeof(prov), name, sizeof(name)),
            "comprimento de provedor invalido");
}

static void test_short_event_descriptor(void)
{
    uint8_t body[128];
    size_t p = 0;
    body[p++] = 'p'; body[p++] = 'o'; body[p++] = 'r';
    body[p++] = 6;
    memcpy(body + p, "Jornal", 6); p += 6;
    body[p++] = 8;
    memcpy(body + p, "Noticias", 8); p += 8;

    vsti_desc_t d = { .tag = VSTI_DESC_SHORT_EVENT,
                      .length = (uint8_t)p, .data = body };

    char lang[4], title[128], text[128];
    T_TRUE(vsti_desc_short_event(&d, lang, title, sizeof(title), text, sizeof(text)),
           "short_event_descriptor parseavel");
    T_EQ_STR(lang, "por", "idioma");
    T_EQ_STR(title, "Jornal", "titulo");
    T_EQ_STR(text, "Noticias", "resumo");
}

int main(void)
{
    test_iterator();
    test_truncated_descriptor();
    test_text_decoding();
    test_service_descriptor();
    test_short_event_descriptor();
    return t_report("desc");
}
