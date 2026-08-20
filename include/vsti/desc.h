/*
 * vsti/desc.h - Descritores PSI/SI e decodificacao de texto ISDB-Tb.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_DESC_H
#define VSTI_DESC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Tags de descritor relevantes para este projeto. */
#define VSTI_DESC_VIDEO_STREAM      0x02u
#define VSTI_DESC_AUDIO_STREAM      0x03u
#define VSTI_DESC_REGISTRATION      0x05u
#define VSTI_DESC_CA                0x09u
#define VSTI_DESC_ISO639_LANG       0x0Au
#define VSTI_DESC_NETWORK_NAME      0x40u
#define VSTI_DESC_SERVICE_LIST      0x41u
#define VSTI_DESC_SERVICE           0x48u
#define VSTI_DESC_SHORT_EVENT       0x4Du
#define VSTI_DESC_EXTENDED_EVENT    0x4Eu
#define VSTI_DESC_COMPONENT         0x50u
#define VSTI_DESC_STREAM_IDENTIFIER 0x52u
#define VSTI_DESC_CONTENT           0x54u
#define VSTI_DESC_PARENTAL_RATING   0x55u
#define VSTI_DESC_LOCAL_TIME_OFFSET 0x58u
#define VSTI_DESC_TS_INFORMATION    0xCDu /* ARIB STD-B10 */
#define VSTI_DESC_AUDIO_COMPONENT   0xC4u /* ARIB STD-B10 */
#define VSTI_DESC_DATA_CONTENT      0xC7u /* ARIB STD-B10 */

/* Iterador sobre um laco de descritores. */
typedef struct {
    const uint8_t *p;
    const uint8_t *end;
} vsti_desc_iter_t;

typedef struct {
    uint8_t        tag;
    uint8_t        length;
    const uint8_t *data; /* Aponta para o corpo, apos tag e length */
} vsti_desc_t;

void vsti_desc_iter_init(vsti_desc_iter_t *it, const uint8_t *buf, size_t len);

/*
 * Avanca para o proximo descritor. Retorna false ao chegar ao fim ou ao
 * encontrar um descritor cujo comprimento declarado ultrapasse o buffer, o que
 * na pratica significa laco truncado ou corrompido.
 */
bool vsti_desc_iter_next(vsti_desc_iter_t *it, vsti_desc_t *out);

/*
 * Decodifica uma string de texto ISDB-Tb para UTF-8.
 *
 * A ARIB STD-B24 define um esquema de codificacao com troca dinamica de
 * conjuntos de caracteres via sequencias de escape. A variante brasileira
 * (ABNT NBR 15603-2) restringe isso na pratica ao Latin alfanumerico, o que
 * permite um decodificador bem mais simples: descartamos os codigos de
 * controle C0/C1 e as sequencias de escape, e convertemos o restante de
 * ISO/IEC 8859-15 para UTF-8.
 *
 * Escreve no maximo `outsz` bytes, sempre terminando com '\0'.
 * Retorna o numero de bytes escritos, sem contar o terminador.
 */
size_t vsti_arib_text_to_utf8(const uint8_t *in, size_t inlen,
                              char *out, size_t outsz);

/* Extrai o nome do servico e o nome do provedor de um service_descriptor. */
bool vsti_desc_service(const vsti_desc_t *d,
                       uint8_t *service_type,
                       char *provider, size_t provider_sz,
                       char *name, size_t name_sz);

/* Extrai titulo e sinopse curta de um short_event_descriptor. */
bool vsti_desc_short_event(const vsti_desc_t *d,
                           char lang[4],
                           char *title, size_t title_sz,
                           char *text, size_t text_sz);

/* Nome legivel de um service_type (ARIB STD-B10 / EN 300 468). */
const char *vsti_service_type_name(uint8_t t);

/* Nome legivel de um running_status. */
const char *vsti_running_status_name(uint8_t rs);

#endif /* VSTI_DESC_H */
