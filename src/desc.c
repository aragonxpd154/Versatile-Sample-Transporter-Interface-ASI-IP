/*
 * src/desc.c - Descritores PSI/SI e decodificacao de texto ISDB-Tb.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#include "vsti/desc.h"

#include <string.h>

void vsti_desc_iter_init(vsti_desc_iter_t *it, const uint8_t *buf, size_t len)
{
    it->p   = buf;
    it->end = (buf != NULL) ? buf + len : NULL;
}

bool vsti_desc_iter_next(vsti_desc_iter_t *it, vsti_desc_t *out)
{
    if (it->p == NULL || it->end == NULL) {
        return false;
    }
    /* Sao necessarios ao menos 2 bytes: tag e length. */
    if (it->p + 2 > it->end) {
        return false;
    }

    const uint8_t tag = it->p[0];
    const uint8_t len = it->p[1];

    if (it->p + 2 + len > it->end) {
        /*
         * Comprimento declarado maior que o espaco restante. Preferimos parar
         * a tentar recuperar: continuar leria bytes de outro campo e produziria
         * dados falsos, que em um analisador e pior do que dado ausente.
         */
        return false;
    }

    out->tag    = tag;
    out->length = len;
    out->data   = (len > 0) ? it->p + 2 : NULL;

    it->p += 2 + len;
    return true;
}

/*
 * Tabela de conversao das posicoes em que a ISO/IEC 8859-15 difere da
 * ISO/IEC 8859-1. Fora destas oito posicoes as duas coincidem, e o valor do
 * byte ja e o proprio code point Unicode.
 */
static uint32_t latin9_codepoint(uint8_t c)
{
    switch (c) {
    case 0xA4: return 0x20AC; /* EURO SIGN */
    case 0xA6: return 0x0160; /* S caron */
    case 0xA8: return 0x0161; /* s caron */
    case 0xB4: return 0x017D; /* Z caron */
    case 0xB8: return 0x017E; /* z caron */
    case 0xBC: return 0x0152; /* OE ligature */
    case 0xBD: return 0x0153; /* oe ligature */
    case 0xBE: return 0x0178; /* Y diaeresis */
    default:   return (uint32_t)c;
    }
}

/* Codifica um code point em UTF-8. Retorna quantos bytes foram escritos. */
static size_t utf8_encode(uint32_t cp, char *out, size_t avail)
{
    if (cp < 0x80) {
        if (avail < 1) return 0;
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        if (avail < 2) return 0;
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (avail < 3) return 0;
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}

size_t vsti_arib_text_to_utf8(const uint8_t *in, size_t inlen,
                              char *out, size_t outsz)
{
    size_t w = 0;

    if (out == NULL || outsz == 0) {
        return 0;
    }
    if (in == NULL || inlen == 0) {
        out[0] = '\0';
        return 0;
    }

    for (size_t i = 0; i < inlen; ++i) {
        const uint8_t c = in[i];

        /*
         * ESC (0x1B) inicia uma sequencia de designacao de conjunto de
         * caracteres. Na variante brasileira essas sequencias tem 1 a 3 bytes
         * e nao produzem texto visivel, entao pulamos os bytes intermediarios
         * ate o byte final, que esta na faixa 0x40-0x7E.
         */
        if (c == 0x1B) {
            ++i;
            while (i < inlen && in[i] >= 0x20 && in[i] <= 0x3F) {
                ++i;
            }
            continue; /* O laco externo consome o byte final */
        }

        /*
         * Codigos de controle C0. Preservamos apenas os que tem significado de
         * separacao visual: APR/LF (0x0A) e SP. Os demais (cores, tamanhos de
         * fonte, posicionamento) sao descartados.
         */
        if (c < 0x20) {
            if (c == 0x0A || c == 0x0D) {
                if (w + 1 >= outsz) break;
                out[w++] = ' ';
            }
            continue;
        }

        /* C1: controles estendidos, tambem sem representacao textual. */
        if (c >= 0x80 && c <= 0x9F) {
            continue;
        }

        /*
         * 0xA0 e um espaco nao separavel; normalizamos para espaco comum para
         * que o texto resultante se comporte bem em JSON e em terminais.
         */
        const uint32_t cp = (c == 0xA0) ? 0x20 : latin9_codepoint(c);

        const size_t n = utf8_encode(cp, out + w, outsz - 1 - w);
        if (n == 0) {
            break; /* Buffer cheio */
        }
        w += n;
    }

    out[w] = '\0';
    return w;
}

bool vsti_desc_service(const vsti_desc_t *d,
                       uint8_t *service_type,
                       char *provider, size_t provider_sz,
                       char *name, size_t name_sz)
{
    if (d == NULL || d->tag != VSTI_DESC_SERVICE || d->data == NULL || d->length < 3) {
        return false;
    }

    const uint8_t *p   = d->data;
    const uint8_t *end = d->data + d->length;

    const uint8_t stype    = p[0];
    const uint8_t prov_len = p[1];

    if (p + 2 + prov_len + 1 > end) {
        return false;
    }
    const uint8_t *prov = p + 2;

    const uint8_t name_len = p[2 + prov_len];
    if (p + 3 + prov_len + name_len > end) {
        return false;
    }
    const uint8_t *nm = p + 3 + prov_len;

    if (service_type) *service_type = stype;
    if (provider)     vsti_arib_text_to_utf8(prov, prov_len, provider, provider_sz);
    if (name)         vsti_arib_text_to_utf8(nm, name_len, name, name_sz);
    return true;
}

bool vsti_desc_short_event(const vsti_desc_t *d,
                           char lang[4],
                           char *title, size_t title_sz,
                           char *text, size_t text_sz)
{
    if (d == NULL || d->tag != VSTI_DESC_SHORT_EVENT || d->data == NULL || d->length < 5) {
        return false;
    }

    const uint8_t *p   = d->data;
    const uint8_t *end = d->data + d->length;

    if (lang) {
        lang[0] = (char)p[0];
        lang[1] = (char)p[1];
        lang[2] = (char)p[2];
        lang[3] = '\0';
    }

    const uint8_t title_len = p[3];
    if (p + 4 + title_len + 1 > end) {
        return false;
    }
    const uint8_t *tp = p + 4;

    const uint8_t text_len = p[4 + title_len];
    if (p + 5 + title_len + text_len > end) {
        return false;
    }
    const uint8_t *xp = p + 5 + title_len;

    if (title) vsti_arib_text_to_utf8(tp, title_len, title, title_sz);
    if (text)  vsti_arib_text_to_utf8(xp, text_len, text, text_sz);
    return true;
}

const char *vsti_service_type_name(uint8_t t)
{
    switch (t) {
    case 0x01: return "TV digital";
    case 0x02: return "Radio digital";
    case 0x03: return "Teletexto";
    case 0x0C: return "Servico de dados";
    case 0xA1: return "TV especial";
    case 0xA2: return "Audio especial";
    case 0xA3: return "Dados especiais";
    case 0xA4: return "Servico de engenharia";
    case 0xA5: return "TV promocional";
    case 0xA6: return "Audio promocional";
    case 0xC0: return "Armazenamento de dados";
    default:   return "Desconhecido";
    }
}

const char *vsti_running_status_name(uint8_t rs)
{
    switch (rs) {
    case 0: return "indefinido";
    case 1: return "nao iniciado";
    case 2: return "iniciando em segundos";
    case 3: return "pausado";
    case 4: return "em execucao";
    case 5: return "off-air";
    default: return "reservado";
    }
}
