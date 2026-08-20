/*
 * src/cmd_analyze.c - Subcomando "analyze": inspecao de um transport stream.
 *
 * Percorre o fluxo uma unica vez coletando, ao mesmo tempo: estatisticas por
 * PID, integridade de continuidade, taxa real medida pelo PCR e o conteudo das
 * tabelas PSI/SI (PAT, PMT, SDT, EIT). O relatorio sai em texto ou em JSON.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#define _POSIX_C_SOURCE 200809L

#include "cli.h"

#include "vsti/desc.h"
#include "vsti/log.h"
#include "vsti/pacing.h"
#include "vsti/psi.h"
#include "vsti/reader.h"
#include "vsti/ts.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_TRACKED_PMT 32
#define MAX_SVC         64
#define MAX_EVT         512

typedef struct {
    uint64_t packets;
    uint64_t cc_errors;
    uint64_t tei;
    uint64_t scrambled;
    uint64_t discontinuities;
    uint64_t pcr_count;
    bool     seen;
} pid_stat_t;

typedef struct {
    uint16_t service_id;
    uint8_t  service_type;
    uint8_t  running_status;
    bool     scrambled;
    char     name[128];
    char     provider[128];
} svc_info_t;

typedef struct {
    uint16_t service_id;
    uint16_t event_id;
    int64_t  start_utc;
    uint32_t duration;
    uint8_t  running_status;
    char     lang[4];
    char     title[256];
    char     text[512];
} evt_info_t;

typedef struct {
    uint16_t program_number;
    uint16_t pmt_pid;
    uint16_t pcr_pid;
    bool     have_pmt;
    size_t   stream_count;
    struct {
        uint8_t  type;
        uint16_t pid;
        char     lang[4];
    } streams[VSTI_MAX_STREAMS];
} prog_info_t;

typedef struct {
    /* Estatisticas globais */
    uint64_t total_packets;
    uint64_t null_packets;
    uint64_t total_cc_errors;
    uint64_t total_tei;
    uint64_t invalid_packets;
    pid_stat_t pid[VSTI_PID_COUNT];

    /* PCR / taxa */
    vsti_bitrate_est_t est;
    uint64_t first_pcr27;
    uint64_t last_pcr27;
    bool     have_first_pcr;
    uint64_t bitrate_sum;
    uint64_t bitrate_n;
    uint64_t bitrate_min;
    uint64_t bitrate_max;

    /* PSI/SI */
    bool     have_pat;
    uint16_t transport_stream_id;
    uint16_t original_network_id;
    uint8_t  pat_version;

    size_t   prog_count;
    prog_info_t progs[MAX_TRACKED_PMT];

    size_t   svc_count;
    svc_info_t svcs[MAX_SVC];

    size_t   evt_count;
    evt_info_t evts[MAX_EVT];

    uint64_t sections_ok;
} analysis_t;

/* ------------------------------------------------------------------ */
/* Coleta                                                               */
/* ------------------------------------------------------------------ */

static prog_info_t *find_or_add_prog(analysis_t *a, uint16_t prog_num, uint16_t pmt_pid)
{
    for (size_t i = 0; i < a->prog_count; ++i) {
        if (a->progs[i].program_number == prog_num) {
            return &a->progs[i];
        }
    }
    if (a->prog_count >= MAX_TRACKED_PMT) {
        return NULL;
    }
    prog_info_t *p = &a->progs[a->prog_count++];
    memset(p, 0, sizeof(*p));
    p->program_number = prog_num;
    p->pmt_pid        = pmt_pid;
    return p;
}

static svc_info_t *find_or_add_svc(analysis_t *a, uint16_t sid)
{
    for (size_t i = 0; i < a->svc_count; ++i) {
        if (a->svcs[i].service_id == sid) {
            return &a->svcs[i];
        }
    }
    if (a->svc_count >= MAX_SVC) {
        return NULL;
    }
    svc_info_t *s = &a->svcs[a->svc_count++];
    memset(s, 0, sizeof(*s));
    s->service_id = sid;
    return s;
}

/* Evita duplicar o mesmo evento, que se repete a cada ciclo da EIT. */
static evt_info_t *find_or_add_evt(analysis_t *a, uint16_t sid, uint16_t eid)
{
    for (size_t i = 0; i < a->evt_count; ++i) {
        if (a->evts[i].service_id == sid && a->evts[i].event_id == eid) {
            return &a->evts[i];
        }
    }
    if (a->evt_count >= MAX_EVT) {
        return NULL;
    }
    evt_info_t *e = &a->evts[a->evt_count++];
    memset(e, 0, sizeof(*e));
    e->service_id = sid;
    e->event_id   = eid;
    return e;
}

/* Extrai o codigo de idioma de um laco de descritores de fluxo elementar. */
static void extract_lang(const uint8_t *desc, size_t len, char out[4])
{
    vsti_desc_iter_t it;
    vsti_desc_t d;

    out[0] = '\0';
    if (desc == NULL || len == 0) {
        return;
    }

    vsti_desc_iter_init(&it, desc, len);
    while (vsti_desc_iter_next(&it, &d)) {
        if (d.tag == VSTI_DESC_ISO639_LANG && d.length >= 3) {
            out[0] = (char)d.data[0];
            out[1] = (char)d.data[1];
            out[2] = (char)d.data[2];
            out[3] = '\0';
            return;
        }
    }
}

static void on_section(uint16_t pid, const vsti_section_header_t *hdr,
                       const uint8_t *data, size_t len, void *user)
{
    analysis_t *a = (analysis_t *)user;
    a->sections_ok++;

    if (pid == VSTI_PID_PAT && hdr->table_id == VSTI_TID_PAT) {
        vsti_pat_t pat;
        if (!vsti_parse_pat(data, len, &pat)) {
            return;
        }
        a->have_pat            = true;
        a->transport_stream_id = pat.transport_stream_id;
        a->pat_version         = pat.version;

        for (size_t i = 0; i < pat.count; ++i) {
            if (pat.entries[i].program_number == 0) {
                continue; /* Ponteiro para a NIT, nao e um programa */
            }
            (void)find_or_add_prog(a, pat.entries[i].program_number,
                                   pat.entries[i].pid);
        }
        return;
    }

    if (hdr->table_id == VSTI_TID_PMT) {
        vsti_pmt_t pmt;
        if (!vsti_parse_pmt(data, len, &pmt)) {
            return;
        }
        prog_info_t *p = find_or_add_prog(a, pmt.program_number, pid);
        if (p == NULL) {
            return;
        }
        p->have_pmt     = true;
        p->pcr_pid      = pmt.pcr_pid;
        p->stream_count = 0;

        for (size_t i = 0; i < pmt.stream_count && i < VSTI_MAX_STREAMS; ++i) {
            p->streams[p->stream_count].type = pmt.streams[i].stream_type;
            p->streams[p->stream_count].pid  = pmt.streams[i].elementary_pid;
            extract_lang(pmt.streams[i].es_descriptors,
                         pmt.streams[i].es_info_length,
                         p->streams[p->stream_count].lang);
            p->stream_count++;
        }
        return;
    }

    if (hdr->table_id == VSTI_TID_SDT_ACT || hdr->table_id == VSTI_TID_SDT_OTH) {
        vsti_sdt_t sdt;
        if (!vsti_parse_sdt(data, len, &sdt)) {
            return;
        }
        a->original_network_id = sdt.original_network_id;

        for (size_t i = 0; i < sdt.count; ++i) {
            svc_info_t *s = find_or_add_svc(a, sdt.services[i].service_id);
            if (s == NULL) {
                continue;
            }
            s->running_status = sdt.services[i].running_status;
            s->scrambled      = sdt.services[i].free_ca_mode;

            vsti_desc_iter_t it;
            vsti_desc_t d;
            vsti_desc_iter_init(&it, sdt.services[i].descriptors,
                                sdt.services[i].descriptors_length);
            while (vsti_desc_iter_next(&it, &d)) {
                if (d.tag == VSTI_DESC_SERVICE) {
                    vsti_desc_service(&d, &s->service_type,
                                      s->provider, sizeof(s->provider),
                                      s->name, sizeof(s->name));
                }
            }
        }
        return;
    }

    if (vsti_tid_is_eit(hdr->table_id)) {
        vsti_eit_t eit;
        if (!vsti_parse_eit(data, len, &eit)) {
            return;
        }
        for (size_t i = 0; i < eit.count; ++i) {
            evt_info_t *e = find_or_add_evt(a, eit.service_id,
                                            eit.events[i].event_id);
            if (e == NULL) {
                continue;
            }
            e->start_utc      = eit.events[i].start_time_utc;
            e->duration       = eit.events[i].duration_seconds;
            e->running_status = eit.events[i].running_status;

            vsti_desc_iter_t it;
            vsti_desc_t d;
            vsti_desc_iter_init(&it, eit.events[i].descriptors,
                                eit.events[i].descriptors_length);
            while (vsti_desc_iter_next(&it, &d)) {
                if (d.tag == VSTI_DESC_SHORT_EVENT) {
                    vsti_desc_short_event(&d, e->lang,
                                          e->title, sizeof(e->title),
                                          e->text, sizeof(e->text));
                }
            }
        }
        return;
    }
}

/* ------------------------------------------------------------------ */
/* Relatorio em texto                                                   */
/* ------------------------------------------------------------------ */

static void print_text_report(const analysis_t *a, const char *path,
                              double wall_seconds)
{
    char buf[128];

    printf("========================================================\n");
    printf(" Analise de Transport Stream — %s\n", path);
    printf("========================================================\n\n");

    printf("Transporte\n");
    printf("  pacotes TS ............ %" PRIu64 "\n", a->total_packets);
    printf("  pacotes nulos ......... %" PRIu64 " (%.2f%%)\n",
           a->null_packets,
           a->total_packets ? 100.0 * (double)a->null_packets / (double)a->total_packets : 0.0);
    printf("  pacotes invalidos ..... %" PRIu64 "\n", a->invalid_packets);
    printf("  erros de continuidade . %" PRIu64 "\n", a->total_cc_errors);
    printf("  transport_error_ind ... %" PRIu64 "\n", a->total_tei);
    printf("  secoes PSI/SI validas . %" PRIu64 "\n", a->sections_ok);

    if (a->have_pat) {
        printf("  transport_stream_id ... %u (0x%04X)\n",
               a->transport_stream_id, a->transport_stream_id);
    }
    if (a->original_network_id != 0) {
        printf("  original_network_id ... %u (0x%04X)\n",
               a->original_network_id, a->original_network_id);
    }

    if (a->bitrate_n > 0) {
        const uint64_t avg = a->bitrate_sum / a->bitrate_n;
        vsti_fmt_bitrate(avg, buf, sizeof(buf));
        printf("  taxa media (PCR) ...... %s\n", buf);
        vsti_fmt_bitrate(a->bitrate_min, buf, sizeof(buf));
        printf("  taxa minima ........... %s\n", buf);
        vsti_fmt_bitrate(a->bitrate_max, buf, sizeof(buf));
        printf("  taxa maxima ........... %s\n", buf);

        if (a->have_first_pcr && a->last_pcr27 >= a->first_pcr27) {
            const double dur = (double)(a->last_pcr27 - a->first_pcr27) /
                               (double)VSTI_SYSTEM_CLOCK_HZ;
            vsti_fmt_duration(dur, buf, sizeof(buf));
            printf("  duracao (PCR) ......... %s\n", buf);
        }
    } else {
        printf("  taxa .................. nao mensuravel (sem PCR utilizavel)\n");
    }
    printf("  tempo de analise ...... %.2f s\n\n", wall_seconds);

    /* ---- Programas ---- */

    printf("Programas (%zu)\n", a->prog_count);
    if (a->prog_count == 0) {
        printf("  nenhum programa encontrado na PAT\n");
    }
    for (size_t i = 0; i < a->prog_count; ++i) {
        const prog_info_t *p = &a->progs[i];

        const svc_info_t *svc = NULL;
        for (size_t k = 0; k < a->svc_count; ++k) {
            if (a->svcs[k].service_id == p->program_number) {
                svc = &a->svcs[k];
                break;
            }
        }

        printf("\n  Programa %u (0x%04X)", p->program_number, p->program_number);
        if (svc != NULL && svc->name[0] != '\0') {
            printf("  \"%s\"", svc->name);
        }
        printf("\n");
        printf("    PMT PID ......... 0x%04X\n", p->pmt_pid);

        if (!p->have_pmt) {
            printf("    (PMT nao recebida durante a analise)\n");
            continue;
        }

        printf("    PCR PID ......... 0x%04X\n", p->pcr_pid);
        if (svc != NULL) {
            if (svc->provider[0] != '\0') {
                printf("    provedor ........ %s\n", svc->provider);
            }
            printf("    tipo ............ %s (0x%02X)\n",
                   vsti_service_type_name(svc->service_type), svc->service_type);
            printf("    estado .......... %s%s\n",
                   vsti_running_status_name(svc->running_status),
                   svc->scrambled ? ", cifrado" : "");
        }

        printf("    fluxos elementares:\n");
        for (size_t k = 0; k < p->stream_count; ++k) {
            const pid_stat_t *ps = &a->pid[p->streams[k].pid];
            const double share = a->total_packets
                ? 100.0 * (double)ps->packets / (double)a->total_packets : 0.0;

            printf("      PID 0x%04X  %-24s %s%s%8" PRIu64 " pkts (%5.2f%%)\n",
                   p->streams[k].pid,
                   vsti_stream_type_name(p->streams[k].type),
                   p->streams[k].lang[0] ? p->streams[k].lang : "   ",
                   p->streams[k].lang[0] ? " " : " ",
                   ps->packets, share);
        }
    }

    /* ---- Servicos sem programa correspondente ---- */

    size_t orphan = 0;
    for (size_t k = 0; k < a->svc_count; ++k) {
        bool matched = false;
        for (size_t i = 0; i < a->prog_count; ++i) {
            if (a->progs[i].program_number == a->svcs[k].service_id) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            if (orphan == 0) {
                printf("\nServicos anunciados na SDT sem PMT neste TS\n");
            }
            orphan++;
            printf("  0x%04X  %-32s %s\n",
                   a->svcs[k].service_id,
                   a->svcs[k].name[0] ? a->svcs[k].name : "(sem nome)",
                   vsti_service_type_name(a->svcs[k].service_type));
        }
    }

    /* ---- Grade de programacao ---- */

    if (a->evt_count > 0) {
        printf("\nEventos EPG (%zu)\n", a->evt_count);
        for (size_t i = 0; i < a->evt_count; ++i) {
            const evt_info_t *e = &a->evts[i];

            char when[64] = "(sem horario)";
            if (e->start_utc > 0) {
                const time_t t = (time_t)e->start_utc;
                struct tm tmv;
                if (gmtime_r(&t, &tmv) != NULL) {
                    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S UTC", &tmv);
                }
            }

            printf("\n  [servico 0x%04X / evento 0x%04X] %s (+%02u:%02u:%02u)\n",
                   e->service_id, e->event_id, when,
                   e->duration / 3600, (e->duration % 3600) / 60, e->duration % 60);
            if (e->title[0]) {
                printf("    titulo: %s\n", e->title);
            }
            if (e->text[0]) {
                printf("    resumo: %s\n", e->text);
            }
        }
    }

    /* ---- Tabela de PIDs ---- */

    printf("\nPIDs ativos\n");
    printf("  %-8s %12s %8s %8s %8s %8s\n",
           "PID", "pacotes", "%", "CC err", "TEI", "PCR");
    for (unsigned p = 0; p < VSTI_PID_COUNT; ++p) {
        if (!a->pid[p].seen) {
            continue;
        }
        const double share = a->total_packets
            ? 100.0 * (double)a->pid[p].packets / (double)a->total_packets : 0.0;
        printf("  0x%04X   %12" PRIu64 " %7.3f%% %8" PRIu64 " %8" PRIu64 " %8" PRIu64 "\n",
               p, a->pid[p].packets, share,
               a->pid[p].cc_errors, a->pid[p].tei, a->pid[p].pcr_count);
    }
    printf("\n");
}

/* ------------------------------------------------------------------ */
/* Relatorio em JSON                                                    */
/* ------------------------------------------------------------------ */

static void print_json_report(const analysis_t *a, const char *path)
{
    char esc[1024];

    printf("{\n");

    vsti_json_escape(path, esc, sizeof(esc));
    printf("  \"source\": %s,\n", esc);
    printf("  \"generator\": \"vsti " VSTI_VERSION "\",\n");

    printf("  \"transport\": {\n");
    printf("    \"packets\": %" PRIu64 ",\n", a->total_packets);
    printf("    \"null_packets\": %" PRIu64 ",\n", a->null_packets);
    printf("    \"invalid_packets\": %" PRIu64 ",\n", a->invalid_packets);
    printf("    \"cc_errors\": %" PRIu64 ",\n", a->total_cc_errors);
    printf("    \"transport_errors\": %" PRIu64 ",\n", a->total_tei);
    printf("    \"sections\": %" PRIu64 ",\n", a->sections_ok);
    printf("    \"transport_stream_id\": %u,\n", a->transport_stream_id);
    printf("    \"original_network_id\": %u,\n", a->original_network_id);
    if (a->bitrate_n > 0) {
        printf("    \"bitrate_bps\": %" PRIu64 ",\n", a->bitrate_sum / a->bitrate_n);
        printf("    \"bitrate_min_bps\": %" PRIu64 ",\n", a->bitrate_min);
        printf("    \"bitrate_max_bps\": %" PRIu64 ",\n", a->bitrate_max);
    } else {
        printf("    \"bitrate_bps\": null,\n");
    }
    if (a->have_first_pcr && a->last_pcr27 >= a->first_pcr27) {
        printf("    \"duration_seconds\": %.3f\n",
               (double)(a->last_pcr27 - a->first_pcr27) / (double)VSTI_SYSTEM_CLOCK_HZ);
    } else {
        printf("    \"duration_seconds\": null\n");
    }
    printf("  },\n");

    /* Programas */
    printf("  \"programs\": [\n");
    for (size_t i = 0; i < a->prog_count; ++i) {
        const prog_info_t *p = &a->progs[i];
        printf("    {\n");
        printf("      \"program_number\": %u,\n", p->program_number);
        printf("      \"pmt_pid\": %u,\n", p->pmt_pid);
        printf("      \"pcr_pid\": %u,\n", p->pcr_pid);
        printf("      \"streams\": [\n");
        for (size_t k = 0; k < p->stream_count; ++k) {
            vsti_json_escape(vsti_stream_type_name(p->streams[k].type),
                             esc, sizeof(esc));
            printf("        { \"pid\": %u, \"stream_type\": %u, \"type_name\": %s",
                   p->streams[k].pid, p->streams[k].type, esc);
            if (p->streams[k].lang[0]) {
                vsti_json_escape(p->streams[k].lang, esc, sizeof(esc));
                printf(", \"language\": %s", esc);
            }
            printf(", \"packets\": %" PRIu64 " }%s\n",
                   a->pid[p->streams[k].pid].packets,
                   (k + 1 < p->stream_count) ? "," : "");
        }
        printf("      ]\n");
        printf("    }%s\n", (i + 1 < a->prog_count) ? "," : "");
    }
    printf("  ],\n");

    /* Servicos */
    printf("  \"services\": [\n");
    for (size_t i = 0; i < a->svc_count; ++i) {
        const svc_info_t *s = &a->svcs[i];
        printf("    {\n");
        printf("      \"service_id\": %u,\n", s->service_id);
        vsti_json_escape(s->name, esc, sizeof(esc));
        printf("      \"name\": %s,\n", esc);
        vsti_json_escape(s->provider, esc, sizeof(esc));
        printf("      \"provider\": %s,\n", esc);
        printf("      \"service_type\": %u,\n", s->service_type);
        vsti_json_escape(vsti_service_type_name(s->service_type), esc, sizeof(esc));
        printf("      \"service_type_name\": %s,\n", esc);
        vsti_json_escape(vsti_running_status_name(s->running_status), esc, sizeof(esc));
        printf("      \"running_status\": %s,\n", esc);
        printf("      \"scrambled\": %s\n", s->scrambled ? "true" : "false");
        printf("    }%s\n", (i + 1 < a->svc_count) ? "," : "");
    }
    printf("  ],\n");

    /* Eventos */
    printf("  \"events\": [\n");
    for (size_t i = 0; i < a->evt_count; ++i) {
        const evt_info_t *e = &a->evts[i];
        printf("    {\n");
        printf("      \"service_id\": %u,\n", e->service_id);
        printf("      \"event_id\": %u,\n", e->event_id);
        if (e->start_utc > 0) {
            char when[64];
            const time_t t = (time_t)e->start_utc;
            struct tm tmv;
            if (gmtime_r(&t, &tmv) != NULL) {
                strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%SZ", &tmv);
                vsti_json_escape(when, esc, sizeof(esc));
                printf("      \"start_time\": %s,\n", esc);
            }
            printf("      \"start_time_unix\": %" PRId64 ",\n", e->start_utc);
        } else {
            printf("      \"start_time\": null,\n");
            printf("      \"start_time_unix\": null,\n");
        }
        printf("      \"duration_seconds\": %u,\n", e->duration);
        vsti_json_escape(e->lang, esc, sizeof(esc));
        printf("      \"language\": %s,\n", esc);
        vsti_json_escape(e->title, esc, sizeof(esc));
        printf("      \"title\": %s,\n", esc);
        vsti_json_escape(e->text, esc, sizeof(esc));
        printf("      \"description\": %s\n", esc);
        printf("    }%s\n", (i + 1 < a->evt_count) ? "," : "");
    }
    printf("  ],\n");

    /* PIDs */
    printf("  \"pids\": [\n");
    bool first = true;
    for (unsigned p = 0; p < VSTI_PID_COUNT; ++p) {
        if (!a->pid[p].seen) {
            continue;
        }
        if (!first) {
            printf(",\n");
        }
        first = false;
        printf("    { \"pid\": %u, \"packets\": %" PRIu64
               ", \"cc_errors\": %" PRIu64
               ", \"transport_errors\": %" PRIu64
               ", \"pcr_count\": %" PRIu64 " }",
               p, a->pid[p].packets, a->pid[p].cc_errors,
               a->pid[p].tei, a->pid[p].pcr_count);
    }
    printf("\n  ]\n");
    printf("}\n");
}

/* ------------------------------------------------------------------ */
/* Comando                                                              */
/* ------------------------------------------------------------------ */

static void usage(void)
{
    fputs(
"Uso: vsti analyze -i ARQUIVO [opcoes]\n"
"\n"
"Percorre um MPEG-2 Transport Stream e relata sua estrutura.\n"
"\n"
"Obrigatorio:\n"
"  -i, --input ARQ    Arquivo .ts/.mpg de entrada ('-' para stdin)\n"
"\n"
"Opcoes:\n"
"      --json         Emitir o relatorio em JSON\n"
"      --packets N    Analisar no maximo N pacotes\n"
"      --seconds N    Analisar no maximo N segundos de fluxo (pelo PCR)\n"
"  -v, --verbose      Aumenta o nivel de log\n"
"  -q, --quiet        Somente erros\n"
"  -h, --help         Esta ajuda\n"
"\n"
"Exemplos:\n"
"  vsti analyze -i one-seg/TVGAZETA1SEG_20211027_183507.mpg\n"
"  vsti analyze -i captura.ts --json > relatorio.json\n",
        stderr);
}

int vsti_cmd_analyze(int argc, char **argv)
{
    const char *input = NULL;
    bool json = false;
    uint64_t max_packets = 0;
    double max_seconds = 0.0;
    int verbosity = VSTI_LOG_INFO;

    static const struct option longopts[] = {
        { "input",   required_argument, 0, 'i' },
        { "json",    no_argument,       0, 1001 },
        { "packets", required_argument, 0, 1002 },
        { "seconds", required_argument, 0, 1003 },
        { "verbose", no_argument,       0, 'v' },
        { "quiet",   no_argument,       0, 'q' },
        { "help",    no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };

    int c;
    optind = 1;
    while ((c = getopt_long(argc, argv, "i:vqh", longopts, NULL)) != -1) {
        switch (c) {
        case 'i':  input = optarg; break;
        case 1001: json = true; break;
        case 1002: max_packets = strtoull(optarg, NULL, 10); break;
        case 1003: max_seconds = atof(optarg); break;
        case 'v':  verbosity++; break;
        case 'q':  verbosity = VSTI_LOG_ERROR; break;
        case 'h':  usage(); return 0;
        default:   usage(); return 2;
        }
    }

    vsti_log_set_level((vsti_log_level_t)(verbosity > VSTI_LOG_DEBUG
                                          ? VSTI_LOG_DEBUG : verbosity));

    if (input == NULL) {
        VSTI_ERR("-i/--input e obrigatorio");
        usage();
        return 2;
    }

    /*
     * A analise aloca uma estrutura grande (tabela de 8192 PIDs mais os
     * acumuladores de EPG). Fica no heap para nao arriscar estourar a pilha em
     * ambientes com limite baixo, como containers.
     */
    analysis_t *a = calloc(1, sizeof(analysis_t));
    if (a == NULL) {
        VSTI_ERR("memoria insuficiente");
        return 1;
    }
    vsti_bitrate_est_init(&a->est, VSTI_PID_NULL);
    a->bitrate_min = UINT64_MAX;

    vsti_reader_t reader;
    if (vsti_reader_open(&reader, input, false) != 0) {
        VSTI_ERR("nao foi possivel abrir '%s'", input);
        free(a);
        return 1;
    }

    /*
     * Montadores de secao apenas para os PIDs que interessam. Alocar um
     * montador para cada um dos 8192 PIDs custaria 32 MB sem necessidade —
     * PSI/SI vivem em um punhado de PIDs conhecidos, mais os PMT PIDs que a
     * PAT revelar durante a analise.
     */
    typedef struct { uint16_t pid; vsti_section_asm_t asmb; bool used; } asm_slot_t;
    asm_slot_t *slots = calloc(MAX_TRACKED_PMT + 8, sizeof(asm_slot_t));
    if (slots == NULL) {
        VSTI_ERR("memoria insuficiente");
        vsti_reader_close(&reader);
        free(a);
        return 1;
    }
    size_t slot_count = 0;

    /*
     * Estado de continuidade por PID. 8192 entradas de poucos bytes cada:
     * pequeno o bastante para alocar de uma vez e evitar qualquer busca.
     */
    vsti_cc_state_t *cc = calloc(VSTI_PID_COUNT, sizeof(vsti_cc_state_t));
    if (cc == NULL) {
        VSTI_ERR("memoria insuficiente");
        free(slots);
        vsti_reader_close(&reader);
        free(a);
        return 1;
    }

    static const uint16_t base_pids[] = {
        VSTI_PID_PAT, VSTI_PID_NIT, VSTI_PID_SDT,
        VSTI_PID_EIT, VSTI_PID_TDT, VSTI_PID_BIT
    };
    for (size_t i = 0; i < sizeof(base_pids) / sizeof(base_pids[0]); ++i) {
        slots[slot_count].pid  = base_pids[i];
        slots[slot_count].used = true;
        vsti_section_asm_init(&slots[slot_count].asmb, base_pids[i]);
        slot_count++;
    }

    vsti_install_signal_handlers();

    const uint64_t t0 = vsti_now_ns();
    uint8_t pkt[VSTI_TS_PACKET_SIZE];

    while (!g_vsti_stop && vsti_reader_next(&reader, pkt) == 1) {
        if (max_packets > 0 && a->total_packets >= max_packets) {
            break;
        }

        a->total_packets++;

        vsti_ts_header_t h;
        if (!vsti_ts_parse(pkt, &h)) {
            a->invalid_packets++;
            continue;
        }

        pid_stat_t *ps = &a->pid[h.pid];
        ps->seen = true;
        ps->packets++;

        if (h.transport_error) {
            ps->tei++;
            a->total_tei++;
        }
        if (h.transport_scrambling != 0) {
            ps->scrambled++;
        }
        if (h.discontinuity) {
            ps->discontinuities++;
        }
        if (h.pid == VSTI_PID_NULL) {
            a->null_packets++;
        }

        if (vsti_cc_check(&cc[h.pid], &h) == VSTI_CC_ERROR) {
            ps->cc_errors++;
            a->total_cc_errors++;
        }

        if (h.has_pcr) {
            ps->pcr_count++;
            const uint64_t pcr27 = vsti_pcr_to_27mhz(h.pcr);
            if (!a->have_first_pcr) {
                a->have_first_pcr = true;
                a->first_pcr27    = pcr27;
            }
            a->last_pcr27 = pcr27;

            if (max_seconds > 0.0) {
                const double elapsed =
                    (double)(a->last_pcr27 - a->first_pcr27) /
                    (double)VSTI_SYSTEM_CLOCK_HZ;
                if (elapsed >= max_seconds) {
                    break;
                }
            }
        }

        if (vsti_bitrate_est_feed(&a->est, &h, VSTI_TS_PACKET_SIZE)) {
            a->bitrate_sum += a->est.bitrate_bps;
            a->bitrate_n++;
            if (a->est.bitrate_bps < a->bitrate_min) a->bitrate_min = a->est.bitrate_bps;
            if (a->est.bitrate_bps > a->bitrate_max) a->bitrate_max = a->est.bitrate_bps;
        }

        /* ---- Alimenta o montador de secoes deste PID, se houver ---- */

        if (!h.has_payload) {
            continue;
        }

        asm_slot_t *slot = NULL;
        for (size_t i = 0; i < slot_count; ++i) {
            if (slots[i].pid == h.pid) {
                slot = &slots[i];
                break;
            }
        }

        /*
         * Se este PID foi anunciado pela PAT como PMT e ainda nao tem
         * montador, criamos um agora. A PAT sempre chega antes das PMTs no
         * ciclo de repeticao, entao na pratica nenhuma PMT e perdida.
         */
        if (slot == NULL) {
            for (size_t i = 0; i < a->prog_count; ++i) {
                if (a->progs[i].pmt_pid == h.pid && slot_count < MAX_TRACKED_PMT + 8) {
                    slot = &slots[slot_count];
                    slot->pid  = h.pid;
                    slot->used = true;
                    vsti_section_asm_init(&slot->asmb, h.pid);
                    slot_count++;
                    break;
                }
            }
        }

        if (slot != NULL) {
            vsti_section_asm_feed(&slot->asmb,
                                  pkt + h.payload_offset, h.payload_length,
                                  h.payload_unit_start, on_section, a);
        }
    }

    const double wall = (double)(vsti_now_ns() - t0) / 1e9;

    if (a->bitrate_min == UINT64_MAX) {
        a->bitrate_min = 0;
    }

    if (json) {
        print_json_report(a, input);
    } else {
        print_text_report(a, input, wall);
    }

    if (reader.resyncs > 0) {
        VSTI_WARN("%" PRIu64 " perda(s) de sincronismo durante a leitura",
                  reader.resyncs);
    }

    free(cc);
    free(slots);
    vsti_reader_close(&reader);
    free(a);
    return 0;
}
