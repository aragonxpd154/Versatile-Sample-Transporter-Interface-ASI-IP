/*
 * src/cmd_receive.c - Subcomando "receive": RTP/UDP de volta para TS.
 *
 * Contraparte de "stream". Alem de servir para gravar um fluxo da rede, e o
 * que permite validar o encapsulador de ponta a ponta: emitir um arquivo,
 * recebe-lo e comparar os bytes.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#define _POSIX_C_SOURCE 200809L

#include "cli.h"

#include "vsti/log.h"
#include "vsti/net.h"
#include "vsti/pacing.h"
#include "vsti/rtp.h"
#include "vsti/ts.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Folga generosa sobre a MTU: acomoda jumbo frames e datagramas fragmentados. */
#define RX_BUFSZ 9000

typedef struct {
    const char *output;
    const char *listen;
    const char *iface;
    int    rcvbuf;
    int    timeout_ms;
    double duration;
    double stats_interval;
    int    mode;   /* 0 = automatico, 1 = forcar RTP, 2 = forcar UDP puro */
} recv_opts_t;

static void usage(void)
{
    fputs(
"Uso: vsti receive -l [HOST:]PORTA -o ARQUIVO [opcoes]\n"
"\n"
"Recebe MPEG-2 TS sobre UDP (com ou sem RTP) e grava em arquivo.\n"
"\n"
"Obrigatorios:\n"
"  -l, --listen ALVO     Porta, ou GRUPO:PORTA para entrar em multicast\n"
"  -o, --output ARQ      Arquivo de saida ('-' para stdout)\n"
"\n"
"Opcoes:\n"
"      --rtp             Forcar interpretacao como RTP\n"
"      --raw             Forcar interpretacao como UDP puro\n"
"      --iface NOME      Interface de entrada (nome ou IP local)\n"
"      --rcvbuf BYTES    Tamanho do buffer de recepcao do socket\n"
"      --timeout SEG     Encerrar apos N segundos sem receber nada\n"
"      --duration SEG    Encerrar apos N segundos de gravacao\n"
"      --stats SEG       Intervalo do relatorio (padrao: 2)\n"
"  -v, --verbose         Aumenta o nivel de log\n"
"  -q, --quiet           Somente erros\n"
"  -h, --help            Esta ajuda\n"
"\n"
"Exemplos:\n"
"  vsti receive -l 239.1.1.1:1234 -o gravacao.ts --iface eth0\n"
"  vsti receive -l 5000 -o - | ffplay -\n",
        stderr);
}

/*
 * Decide se um datagrama e RTP ou TS puro.
 *
 * A distincao e confiavel porque os dois formatos tem tamanhos incompativeis:
 * TS puro e sempre multiplo exato de 188, e RTP acrescenta 12 bytes de
 * cabecalho, o que quebra essa divisibilidade (1316 e multiplo de 188, 1328
 * nao e). O byte de sync 0x47 na posicao esperada confirma a hipotese.
 */
static bool looks_like_rtp(const uint8_t *buf, size_t len)
{
    vsti_rtp_header_t h;

    if (len % VSTI_TS_PACKET_SIZE == 0 && buf[0] == VSTI_TS_SYNC_BYTE) {
        return false; /* TS puro, alinhado e com sync no lugar certo */
    }
    if (!vsti_rtp_parse(buf, len, &h)) {
        return false;
    }
    if (h.payload_type != VSTI_RTP_PT_MP2T) {
        return false;
    }

    const size_t payload = len - h.header_size;
    return (payload > 0) &&
           (payload % VSTI_TS_PACKET_SIZE == 0) &&
           (buf[h.header_size] == VSTI_TS_SYNC_BYTE);
}

int vsti_cmd_receive(int argc, char **argv)
{
    recv_opts_t o;
    memset(&o, 0, sizeof(o));
    o.timeout_ms     = -1;
    o.stats_interval = 2.0;

    static const struct option longopts[] = {
        { "listen",   required_argument, 0, 'l' },
        { "output",   required_argument, 0, 'o' },
        { "rtp",      no_argument,       0, 1001 },
        { "raw",      no_argument,       0, 1002 },
        { "iface",    required_argument, 0, 1003 },
        { "rcvbuf",   required_argument, 0, 1004 },
        { "timeout",  required_argument, 0, 1005 },
        { "duration", required_argument, 0, 1006 },
        { "stats",    required_argument, 0, 1007 },
        { "verbose",  no_argument,       0, 'v' },
        { "quiet",    no_argument,       0, 'q' },
        { "help",     no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };

    int c;
    int verbosity = VSTI_LOG_INFO;

    optind = 1;
    while ((c = getopt_long(argc, argv, "l:o:vqh", longopts, NULL)) != -1) {
        switch (c) {
        case 'l':  o.listen = optarg; break;
        case 'o':  o.output = optarg; break;
        case 1001: o.mode = 1; break;
        case 1002: o.mode = 2; break;
        case 1003: o.iface = optarg; break;
        case 1004: o.rcvbuf = atoi(optarg); break;
        case 1005: o.timeout_ms = (int)(atof(optarg) * 1000.0); break;
        case 1006: o.duration = atof(optarg); break;
        case 1007: o.stats_interval = atof(optarg); break;
        case 'v':  verbosity++; break;
        case 'q':  verbosity = VSTI_LOG_ERROR; break;
        case 'h':  usage(); return 0;
        default:   usage(); return 2;
        }
    }

    vsti_log_set_level((vsti_log_level_t)(verbosity > VSTI_LOG_DEBUG
                                          ? VSTI_LOG_DEBUG : verbosity));

    if (o.listen == NULL || o.output == NULL) {
        VSTI_ERR("-l/--listen e -o/--output sao obrigatorios");
        usage();
        return 2;
    }

    char host[256], port[32];
    if (!vsti_parse_endpoint(o.listen, host, sizeof(host), port, sizeof(port))) {
        VSTI_ERR("alvo de escuta invalido: %s", o.listen);
        return 2;
    }

    vsti_net_opts_t nopts;
    vsti_net_opts_default(&nopts);
    nopts.host   = (host[0] != '\0') ? host : NULL;
    nopts.port   = port;
    nopts.iface  = o.iface;
    nopts.rcvbuf = o.rcvbuf;
    nopts.reuse  = true;

    vsti_sock_t sock;
    char errbuf[256];
    if (vsti_sock_open_rx(&sock, &nopts, errbuf, sizeof(errbuf)) != 0) {
        VSTI_ERR("socket de entrada: %s", errbuf);
        return 1;
    }

    FILE *out = NULL;
    if (strcmp(o.output, "-") == 0) {
        out = stdout;
    } else {
        out = fopen(o.output, "wb");
        if (out == NULL) {
            VSTI_ERR("nao foi possivel criar '%s'", o.output);
            vsti_sock_close(&sock);
            return 1;
        }
    }

    VSTI_INFO("escutando %s%s%s -> %s",
              host[0] ? host : "*", host[0] ? ":" : "", port,
              (out == stdout) ? "(stdout)" : o.output);
    if (sock.is_multicast) {
        VSTI_INFO("inscrito no grupo multicast %s", host);
    }

    /* ---- Laco de recepcao ---- */

    uint8_t buf[RX_BUFSZ];
    vsti_rtp_stats_t rstats;
    vsti_rtp_stats_init(&rstats);

    vsti_cc_state_t cc[VSTI_PID_COUNT];
    memset(cc, 0, sizeof(cc));

    uint64_t datagrams = 0, ts_packets = 0, bytes_written = 0;
    uint64_t cc_errors = 0, malformed = 0;
    uint32_t ssrc_seen = 0;
    bool     ssrc_known = false;
    bool     is_rtp = (o.mode == 1);
    bool     mode_decided = (o.mode != 0);

    const uint64_t t_start = vsti_now_ns();
    uint64_t t_last_stats  = t_start;
    uint64_t t_last_data   = t_start;

    vsti_install_signal_handlers();

    int rc = 0;

    for (;;) {
        if (g_vsti_stop) {
            VSTI_INFO("interrupcao recebida, encerrando");
            break;
        }

        const uint64_t now = vsti_now_ns();

        if (o.duration > 0.0 && (double)(now - t_start) / 1e9 >= o.duration) {
            break;
        }
        if (o.timeout_ms > 0 &&
            (double)(now - t_last_data) / 1e6 >= (double)o.timeout_ms) {
            VSTI_WARN("nenhum dado por %.1f s, encerrando", o.timeout_ms / 1000.0);
            break;
        }

        /*
         * Espera limitada a 200 ms mesmo sem timeout configurado, para que o
         * laco reavalie sinais e prazos com regularidade.
         */
        const ssize_t n = vsti_sock_recv(&sock, buf, sizeof(buf), 200);
        if (n < 0) {
            VSTI_ERR("falha na recepcao");
            rc = 1;
            break;
        }
        if (n == 0) {
            continue; /* Timeout do poll: nada a fazer */
        }

        t_last_data = vsti_now_ns();
        datagrams++;

        if (!mode_decided) {
            is_rtp = looks_like_rtp(buf, (size_t)n);
            mode_decided = true;
            VSTI_INFO("formato detectado: %s (%zd bytes no primeiro datagrama)",
                      is_rtp ? "RTP PT=33" : "UDP puro", n);
        }

        const uint8_t *payload = buf;
        size_t payload_len = (size_t)n;

        if (is_rtp) {
            vsti_rtp_header_t rh;
            if (!vsti_rtp_parse(buf, (size_t)n, &rh)) {
                malformed++;
                continue;
            }
            if (!ssrc_known) {
                ssrc_known = true;
                ssrc_seen  = rh.ssrc;
                VSTI_INFO("SSRC 0x%08X", rh.ssrc);
            } else if (rh.ssrc != ssrc_seen) {
                /*
                 * Outro emissor entrou no mesmo grupo/porta. Misturar as duas
                 * fontes produziria um arquivo corrompido, entao descartamos o
                 * intruso e seguimos com a fonte original.
                 */
                continue;
            }

            vsti_rtp_stats_update(&rstats, rh.sequence);

            payload     = buf + rh.header_size;
            payload_len = (size_t)n - rh.header_size;
        }

        if (payload_len == 0 || payload_len % VSTI_TS_PACKET_SIZE != 0) {
            malformed++;
            continue;
        }

        /* Verificacao de continuidade sobre o que efetivamente chegou. */
        for (size_t i = 0; i < payload_len; i += VSTI_TS_PACKET_SIZE) {
            vsti_ts_header_t h;
            if (vsti_ts_parse(payload + i, &h)) {
                if (vsti_cc_check(&cc[h.pid], &h) == VSTI_CC_ERROR) {
                    cc_errors++;
                }
            } else {
                malformed++;
            }
            ts_packets++;
        }

        if (fwrite(payload, 1, payload_len, out) != payload_len) {
            VSTI_ERR("falha ao gravar a saida");
            rc = 1;
            break;
        }
        bytes_written += payload_len;

        if (o.stats_interval > 0.0) {
            const uint64_t t = vsti_now_ns();
            if ((double)(t - t_last_stats) / 1e9 >= o.stats_interval) {
                t_last_stats = t;
                const double elapsed = (double)(t - t_start) / 1e9;

                char rate[64], wb[64], dur[32];
                vsti_fmt_bitrate((uint64_t)((double)bytes_written * 8.0 / elapsed),
                                 rate, sizeof(rate));
                vsti_fmt_bytes(bytes_written, wb, sizeof(wb));
                vsti_fmt_duration(elapsed, dur, sizeof(dur));

                if (is_rtp) {
                    VSTI_INFO("%s | %s | %s | perdidos: %llu | CC err: %llu",
                              dur, wb, rate,
                              (unsigned long long)rstats.lost,
                              (unsigned long long)cc_errors);
                } else {
                    VSTI_INFO("%s | %s | %s | CC err: %llu",
                              dur, wb, rate, (unsigned long long)cc_errors);
                }
            }
        }
    }

    if (out != stdout) {
        fclose(out);
    } else {
        fflush(stdout);
    }
    vsti_sock_close(&sock);

    /* ---- Resumo final ---- */

    const double elapsed = (double)(vsti_now_ns() - t_start) / 1e9;
    char wb[64], rate[64], dur[32];
    vsti_fmt_bytes(bytes_written, wb, sizeof(wb));
    vsti_fmt_bitrate(elapsed > 0 ? (uint64_t)((double)bytes_written * 8.0 / elapsed) : 0,
                     rate, sizeof(rate));
    vsti_fmt_duration(elapsed, dur, sizeof(dur));

    VSTI_INFO("---");
    VSTI_INFO("duracao ........ %s", dur);
    VSTI_INFO("datagramas ..... %llu", (unsigned long long)datagrams);
    VSTI_INFO("pacotes TS ..... %llu", (unsigned long long)ts_packets);
    VSTI_INFO("gravado ........ %s (%s medios)", wb, rate);
    VSTI_INFO("erros de CC .... %llu", (unsigned long long)cc_errors);
    VSTI_INFO("malformados .... %llu", (unsigned long long)malformed);

    if (is_rtp && rstats.received > 0) {
        const double loss_pct = 100.0 * (double)rstats.lost /
                                (double)(rstats.received + rstats.lost);
        VSTI_INFO("RTP recebidos .. %llu", (unsigned long long)rstats.received);
        VSTI_INFO("RTP perdidos ... %llu (%.4f%%)",
                  (unsigned long long)rstats.lost, loss_pct);
        VSTI_INFO("RTP reordenados  %llu", (unsigned long long)rstats.reordered);
        VSTI_INFO("RTP duplicados . %llu", (unsigned long long)rstats.duplicates);
    }

    return rc;
}
