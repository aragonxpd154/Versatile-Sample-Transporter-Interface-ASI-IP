/*
 * src/psi.c - Remontagem de secoes PSI/SI e parsers de tabela.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#include "vsti/psi.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* CRC-32/MPEG-2                                                        */
/* ------------------------------------------------------------------ */

/*
 * Implementacao bit a bit, sem tabela estatica.
 *
 * A escolha e deliberada: secoes PSI raramente passam de alguns kilobytes e
 * sao processadas algumas dezenas de vezes por segundo, entao o ganho de uma
 * tabela de 1 KB nao compensa introduzir estado global mutavel (que exigiria
 * cuidado com inicializacao concorrente). Assim a funcao e reentrante e pura.
 */
uint32_t vsti_crc32_mpeg2(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint32_t)data[i] << 24;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x80000000u) {
                crc = (uint32_t)((crc << 1) ^ 0x04C11DB7u);
            } else {
                crc = (uint32_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ------------------------------------------------------------------ */
/* Utilitarios de leitura                                               */
/* ------------------------------------------------------------------ */

static inline uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* ------------------------------------------------------------------ */
/* Montador de secoes                                                   */
/* ------------------------------------------------------------------ */

void vsti_section_asm_init(vsti_section_asm_t *asmb, uint16_t pid)
{
    memset(asmb, 0, sizeof(*asmb));
    asmb->pid = pid;
}

void vsti_section_asm_reset(vsti_section_asm_t *asmb)
{
    asmb->len      = 0;
    asmb->expected = 0;
    asmb->active   = false;
}

bool vsti_section_parse_header(const uint8_t *data, size_t len,
                               vsti_section_header_t *out)
{
    if (len < 3) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->table_id                 = data[0];
    out->section_syntax_indicator = (data[1] & 0x80u) != 0;
    out->section_length           = (uint16_t)(((data[1] & 0x0Fu) << 8) | data[2]);

    /*
     * Secoes com sintaxe curta (section_syntax_indicator == 0), como a TDT,
     * terminam aqui: nao possuem version, section_number nem CRC.
     */
    if (!out->section_syntax_indicator) {
        return true;
    }

    if (len < 8) {
        return false;
    }

    out->table_id_extension  = rd16(&data[3]);
    out->version_number      = (uint8_t)((data[5] >> 1) & 0x1Fu);
    out->current_next        = (data[5] & 0x01u) != 0;
    out->section_number      = data[6];
    out->last_section_number = data[7];
    return true;
}

/*
 * Emite a secao acumulada, se ela passar na verificacao de integridade.
 *
 * Secoes com sintaxe estendida carregam CRC-32 nos ultimos 4 bytes. Rodar o
 * CRC sobre a secao inteira, incluindo o proprio CRC, deve resultar em zero.
 * Secoes de sintaxe curta nao tem CRC e sao entregues como estao.
 */
static void emit_section(vsti_section_asm_t *asmb,
                         vsti_section_cb cb, void *user)
{
    vsti_section_header_t hdr;

    if (cb == NULL) {
        return;
    }
    if (!vsti_section_parse_header(asmb->buf, asmb->len, &hdr)) {
        return;
    }

    if (hdr.section_syntax_indicator) {
        if (asmb->len < 12) {
            return; /* Curta demais para conter cabecalho estendido + CRC */
        }
        if (vsti_crc32_mpeg2(asmb->buf, asmb->len) != 0) {
            return; /* Secao corrompida: descartar silenciosamente */
        }
    }

    cb(asmb->pid, &hdr, asmb->buf, asmb->len, user);
}

/*
 * Consome bytes ate completar a secao corrente.
 * Retorna quantos bytes foram efetivamente absorvidos.
 */
static size_t asm_consume(vsti_section_asm_t *asmb,
                          const uint8_t *data, size_t len,
                          vsti_section_cb cb, void *user)
{
    size_t used = 0;

    while (used < len && asmb->active) {
        /*
         * Enquanto nao tivermos os 3 primeiros bytes nao sabemos o tamanho
         * total. Uma secao pode legitimamente ser fatiada no meio desse
         * cabecalho, entao acumulamos byte a byte ate poder decidir.
         */
        if (asmb->expected == 0) {
            const size_t need = 3 - asmb->len;
            const size_t take = (len - used < need) ? (len - used) : need;
            memcpy(asmb->buf + asmb->len, data + used, take);
            asmb->len += take;
            used      += take;

            if (asmb->len < 3) {
                return used; /* Pacote acabou no meio do cabecalho */
            }

            asmb->expected = 3u + (size_t)(((asmb->buf[1] & 0x0Fu) << 8) | asmb->buf[2]);

            if (asmb->expected > VSTI_SECTION_MAX || asmb->expected < 3) {
                vsti_section_asm_reset(asmb);
                return used;
            }
        }

        const size_t need = asmb->expected - asmb->len;
        const size_t take = (len - used < need) ? (len - used) : need;
        memcpy(asmb->buf + asmb->len, data + used, take);
        asmb->len += take;
        used      += take;

        if (asmb->len == asmb->expected) {
            emit_section(asmb, cb, user);
            vsti_section_asm_reset(asmb);
            return used;
        }
    }

    return used;
}

void vsti_section_asm_feed(vsti_section_asm_t *asmb,
                           const uint8_t *payload, size_t len,
                           bool payload_unit_start,
                           vsti_section_cb cb, void *user)
{
    if (asmb == NULL || payload == NULL || len == 0) {
        return;
    }

    if (!payload_unit_start) {
        /*
         * Sem PUSI o pacote so pode conter continuacao. Se nao estamos no meio
         * de nada, este e um fragmento orfao (comum logo apos dar tune) e deve
         * ser descartado ate chegar o proximo inicio de secao.
         */
        if (asmb->active) {
            (void)asm_consume(asmb, payload, len, cb, user);
        }
        return;
    }

    /*
     * Com PUSI, o primeiro byte e o pointer_field: quantos bytes de cauda da
     * secao anterior precedem o inicio da proxima. Este e o ponto que mais
     * costuma ser implementado errado, e o sintoma e sempre o mesmo — a ultima
     * secao de cada ciclo de repeticao nunca aparece.
     */
    const uint8_t pointer = payload[0];
    size_t pos = 1;

    if ((size_t)pointer > len - pos) {
        /* Ponteiro aponta para fora do pacote: fluxo malformado. */
        vsti_section_asm_reset(asmb);
        return;
    }

    if (pointer > 0) {
        if (asmb->active) {
            (void)asm_consume(asmb, payload + pos, pointer, cb, user);
        }
        pos += pointer;
    }

    /* Qualquer secao pendente foi encerrada ou abandonada neste ponto. */
    vsti_section_asm_reset(asmb);

    /*
     * Um mesmo pacote pode carregar varias secoes curtas em sequencia (tipico
     * de PAT + PMT pequenas). Iteramos ate esgotar o payload ou encontrar
     * enchimento.
     */
    while (pos < len) {
        if (payload[pos] == 0xFFu) {
            break; /* 0xFF marca inicio do enchimento ate o fim do pacote */
        }

        asmb->active   = true;
        asmb->len      = 0;
        asmb->expected = 0;

        const size_t used = asm_consume(asmb, payload + pos, len - pos, cb, user);
        pos += used;

        if (asmb->active) {
            break; /* Secao continua no proximo pacote */
        }
        if (used == 0) {
            break; /* Protecao contra laco infinito */
        }
    }
}

/* ------------------------------------------------------------------ */
/* PAT                                                                  */
/* ------------------------------------------------------------------ */

bool vsti_parse_pat(const uint8_t *sec, size_t len, vsti_pat_t *out)
{
    vsti_section_header_t hdr;

    if (!vsti_section_parse_header(sec, len, &hdr) || hdr.table_id != VSTI_TID_PAT) {
        return false;
    }
    if (len < 12) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->transport_stream_id = hdr.table_id_extension;
    out->version             = hdr.version_number;

    /*
     * Corpo da PAT: comeca no byte 8 e vai ate o inicio do CRC (4 bytes finais).
     * Cada entrada ocupa 4 bytes: program_number (16) + reserved (3) + PID (13).
     */
    const size_t body_end = len - 4;
    for (size_t p = 8; p + 4 <= body_end && out->count < VSTI_MAX_PROGRAMS; p += 4) {
        out->entries[out->count].program_number = rd16(&sec[p]);
        out->entries[out->count].pid            = (uint16_t)(rd16(&sec[p + 2]) & 0x1FFFu);
        out->count++;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* PMT                                                                  */
/* ------------------------------------------------------------------ */

bool vsti_parse_pmt(const uint8_t *sec, size_t len, vsti_pmt_t *out)
{
    vsti_section_header_t hdr;

    if (!vsti_section_parse_header(sec, len, &hdr) || hdr.table_id != VSTI_TID_PMT) {
        return false;
    }
    if (len < 16) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->program_number      = hdr.table_id_extension;
    out->version             = hdr.version_number;
    out->pcr_pid             = (uint16_t)(rd16(&sec[8]) & 0x1FFFu);
    out->program_info_length = (uint16_t)(rd16(&sec[10]) & 0x0FFFu);

    const size_t body_end = len - 4;
    size_t p = 12;

    if (p + out->program_info_length > body_end) {
        return false;
    }
    out->program_descriptors = (out->program_info_length > 0) ? &sec[p] : NULL;
    p += out->program_info_length;

    /*
     * Laco de fluxos elementares. Cada entrada: stream_type (8),
     * reserved (3) + elementary_PID (13), reserved (4) + ES_info_length (12),
     * seguido dos descritores.
     */
    while (p + 5 <= body_end && out->stream_count < VSTI_MAX_STREAMS) {
        vsti_pmt_stream_t *s = &out->streams[out->stream_count];
        s->stream_type    = sec[p];
        s->elementary_pid = (uint16_t)(rd16(&sec[p + 1]) & 0x1FFFu);
        s->es_info_length = (uint16_t)(rd16(&sec[p + 3]) & 0x0FFFu);
        p += 5;

        if (p + s->es_info_length > body_end) {
            break; /* Comprimento declarado ultrapassa a secao: parar aqui */
        }
        s->es_descriptors = (s->es_info_length > 0) ? &sec[p] : NULL;
        p += s->es_info_length;
        out->stream_count++;
    }

    return true;
}

const char *vsti_stream_type_name(uint8_t stream_type)
{
    switch (stream_type) {
    case 0x01: return "MPEG-1 Video";
    case 0x02: return "MPEG-2 Video";
    case 0x03: return "MPEG-1 Audio";
    case 0x04: return "MPEG-2 Audio";
    case 0x05: return "Private Sections";
    case 0x06: return "PES Private Data";
    case 0x07: return "MHEG";
    case 0x08: return "DSM-CC";
    case 0x0B: return "DSM-CC Type B (Carrossel)";
    case 0x0D: return "DSM-CC Type D";
    case 0x0F: return "AAC ADTS";
    case 0x10: return "MPEG-4 Video";
    case 0x11: return "AAC LATM";
    case 0x1B: return "H.264 / AVC";
    case 0x1C: return "MPEG-4 Audio (raw)";
    case 0x24: return "H.265 / HEVC";
    case 0x42: return "AVS Video";
    case 0x81: return "AC-3 (ATSC)";
    case 0x86: return "SCTE-35 Splice";
    case 0x87: return "E-AC-3 (ATSC)";
    default:   return "Desconhecido";
    }
}

/* ------------------------------------------------------------------ */
/* SDT                                                                  */
/* ------------------------------------------------------------------ */

bool vsti_parse_sdt(const uint8_t *sec, size_t len, vsti_sdt_t *out)
{
    vsti_section_header_t hdr;

    if (!vsti_section_parse_header(sec, len, &hdr)) {
        return false;
    }
    if (hdr.table_id != VSTI_TID_SDT_ACT && hdr.table_id != VSTI_TID_SDT_OTH) {
        return false;
    }
    if (len < 15) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->transport_stream_id = hdr.table_id_extension;
    out->original_network_id = rd16(&sec[8]);
    out->version             = hdr.version_number;

    const size_t body_end = len - 4;
    size_t p = 11; /* Pula o byte reservado apos original_network_id */

    while (p + 5 <= body_end && out->count < VSTI_MAX_SERVICES) {
        vsti_sdt_service_t *s = &out->services[out->count];
        s->service_id                 = rd16(&sec[p]);
        s->eit_schedule_flag          = (sec[p + 2] & 0x02u) != 0;
        s->eit_present_following_flag = (sec[p + 2] & 0x01u) != 0;
        s->running_status             = (uint8_t)((sec[p + 3] >> 5) & 0x07u);
        s->free_ca_mode               = (sec[p + 3] & 0x10u) != 0;
        s->descriptors_length         = (uint16_t)(rd16(&sec[p + 3]) & 0x0FFFu);
        p += 5;

        if (p + s->descriptors_length > body_end) {
            break;
        }
        s->descriptors = (s->descriptors_length > 0) ? &sec[p] : NULL;
        p += s->descriptors_length;
        out->count++;
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* EIT                                                                  */
/* ------------------------------------------------------------------ */

int64_t vsti_mjd_bcd_to_unix(const uint8_t *p)
{
    /* Todos os bits em 1 significa "indefinido" na norma. */
    if (p[0] == 0xFF && p[1] == 0xFF && p[2] == 0xFF &&
        p[3] == 0xFF && p[4] == 0xFF) {
        return 0;
    }

    const uint32_t mjd = (uint32_t)((p[0] << 8) | p[1]);

    /*
     * MJD 40587 corresponde a 1970-01-01. Em vez de aplicar a formula de
     * conversao para calendario do Anexo C da EN 300 468 e depois voltar para
     * epoch, basta a diferenca em dias — resultado identico, sem os casos de
     * borda de ano bissexto da formula original.
     */
    const int64_t days = (int64_t)mjd - 40587;

    const int hh = ((p[2] >> 4) & 0x0F) * 10 + (p[2] & 0x0F);
    const int mm = ((p[3] >> 4) & 0x0F) * 10 + (p[3] & 0x0F);
    const int ss = ((p[4] >> 4) & 0x0F) * 10 + (p[4] & 0x0F);

    if (hh > 23 || mm > 59 || ss > 59) {
        return 0;
    }

    return days * 86400ll + hh * 3600ll + mm * 60ll + ss;
}

uint32_t vsti_bcd_duration_to_seconds(const uint8_t *p)
{
    const uint32_t hh = (uint32_t)(((p[0] >> 4) & 0x0F) * 10 + (p[0] & 0x0F));
    const uint32_t mm = (uint32_t)(((p[1] >> 4) & 0x0F) * 10 + (p[1] & 0x0F));
    const uint32_t ss = (uint32_t)(((p[2] >> 4) & 0x0F) * 10 + (p[2] & 0x0F));
    return hh * 3600u + mm * 60u + ss;
}

bool vsti_parse_eit(const uint8_t *sec, size_t len, vsti_eit_t *out)
{
    vsti_section_header_t hdr;

    if (!vsti_section_parse_header(sec, len, &hdr) || !vsti_tid_is_eit(hdr.table_id)) {
        return false;
    }
    if (len < 18) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->service_id          = hdr.table_id_extension;
    out->transport_stream_id = rd16(&sec[8]);
    out->original_network_id = rd16(&sec[10]);
    out->table_id            = hdr.table_id;
    out->version             = hdr.version_number;

    const size_t body_end = len - 4;
    size_t p = 14; /* Pula segment_last_section_number e last_table_id */

    /*
     * Cada evento: event_id (16), start_time (40 bits), duration (24 bits),
     * running_status (3) + free_CA_mode (1) + descriptors_loop_length (12).
     * Total de 12 bytes fixos.
     */
    while (p + 12 <= body_end && out->count < VSTI_MAX_EVENTS) {
        vsti_eit_event_t *e = &out->events[out->count];
        e->event_id         = rd16(&sec[p]);
        e->start_time_utc   = vsti_mjd_bcd_to_unix(&sec[p + 2]);
        e->duration_seconds = vsti_bcd_duration_to_seconds(&sec[p + 7]);
        e->running_status   = (uint8_t)((sec[p + 10] >> 5) & 0x07u);
        e->free_ca_mode     = (sec[p + 10] & 0x10u) != 0;
        e->descriptors_length = (uint16_t)(rd16(&sec[p + 10]) & 0x0FFFu);
        p += 12;

        if (p + e->descriptors_length > body_end) {
            break;
        }
        e->descriptors = (e->descriptors_length > 0) ? &sec[p] : NULL;
        p += e->descriptors_length;
        out->count++;
    }

    return true;
}
