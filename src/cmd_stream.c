/*
 * src/cmd_stream.c - Subcomando "stream": TS de arquivo para RTP/UDP.
 *
 * Este e o caminho principal do projeto: le um transport stream, agrupa os
 * pacotes em datagramas, encapsula em RTP e emite na rede no ritmo correto.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#define _POSIX_C_SOURCE 200809L

#include "cli.h"

#include "vsti/log.h"
#include "vsti/net.h"
#include "vsti/pacing.h"
#include "vsti/reader.h"
#include "vsti/rtp.h"
#include "vsti/ts.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Taxa usada apenas ate a primeira medida de PCR chegar. 17,27 Mbit/s e a taxa
 * util de um canal ISDB-Tb completo em 64QAM 3/4 — chute razoavel para o
 * material com que este projeto trabalha, e irrelevante depois de alguns
 * milissegundos de fluxo.
 */
#define BOOTSTRAP_BITRATE 17274150ull

typedef struct {
    const char *input;
    const char *dest;
    const char *iface;
    uint64_t    bitrate;
    const char *pace;
    int         ts_per_packet;
    int         ttl;
    int         dscp;
    int         sndbuf;
    long        ssrc;
    long        seq;
    bool        raw;
    bool        loop;
    double      duration;
    double      stats_interval;
} stream_opts_t;

static void usage(void)
{
    fputs(
"Uso: vsti stream -i ARQUIVO -d HOST:PORTA [opcoes]\n"
"\n"
"Le um MPEG-2 Transport Stream e o emite encapsulado sobre UDP.\n"
"\n"
"Obrigatorios:\n"
"  -i, --input ARQ       Arquivo .ts/.mpg de entrada ('-' para stdin)\n"
"  -d, --dest HOST:PORTA Destino unicast ou grupo multicast\n"
"\n"
"Encapsulamento:\n"
"      --raw             UDP puro, sem cabecalho RTP (padrao: RTP)\n"
"      --pkts N          Pacotes TS por datagrama, 1 a 7 (padrao: 7)\n"
"      --ssrc N          SSRC do RTP (padrao: aleatorio)\n"
"      --seq N           Sequencia RTP inicial (padrao: aleatoria)\n"
"\n"
"Ritmo de emissao:\n"
"      --pace MODO       pcr | cbr | none (padrao: pcr)\n"
"      --bitrate TAXA    Taxa alvo para o modo cbr (ex.: 17.27M, 2500k)\n"
"\n"
"Rede:\n"
"      --iface NOME      Interface de saida (nome ou IP local)\n"
"      --ttl N           TTL / hop limit (padrao: 8)\n"
"      --dscp N          Valor DSCP 0-63 (ex.: 34 para AF41)\n"
"      --sndbuf BYTES    Tamanho do buffer de envio do socket\n"
"\n"
"Execucao:\n"
"      --loop            Reiniciar o arquivo ao chegar no fim\n"
"      --duration SEG    Encerrar apos N segundos\n"
"      --stats SEG       Intervalo do relatorio de progresso (padrao: 2)\n"
"  -v, --verbose         Aumenta o nivel de log (pode repetir)\n"
"  -q, --quiet           Somente erros\n"
"  -h, --help            Esta ajuda\n"
"\n"
"Exemplos:\n"
"  vsti stream -i captura.ts -d 239.1.1.1:1234 --iface eth0\n"
"  vsti stream -i captura.ts -d 127.0.0.1:5000 --pace cbr --bitrate 4M\n"
"  vsti stream -i - -d 239.1.1.1:1234 --raw --pkts 7\n",
        stderr);
}

/*
 * Estima a taxa do arquivo lendo um trecho do inicio.
 *
 * Ter uma taxa correta antes do primeiro datagrama importa: os primeiros
 * segundos sao justamente quando o buffer do receptor esta se enchendo, e sair
 * emitindo com a taxa errada nesse momento e o que provoca aquele travamento
 * inicial classico. Como so lemos e rebobinamos, o custo e desprezivel.
 */
static uint64_t prescan_bitrate(const char *path, uint64_t *out_pcr_pid)
{
    vsti_reader_t r;
    if (vsti_reader_open(&r, path, false) != 0) {
        return 0;
    }

    vsti_bitrate_est_t est;
    vsti_bitrate_est_init(&est, VSTI_PID_NULL);

    uint8_t pkt[VSTI_TS_PACKET_SIZE];
    uint64_t sum = 0, n = 0;
    size_t scanned = 0;

    /* Limite de 200 mil pacotes (~37 MB) para nao penalizar arquivos enormes. */
    while (scanned < 200000 && vsti_reader_next(&r, pkt) == 1) {
        vsti_ts_header_t h;
        if (vsti_ts_parse(pkt, &h)) {
            if (vsti_bitrate_est_feed(&est, &h, VSTI_TS_PACKET_SIZE)) {
                sum += est.bitrate_bps;
                n++;
                if (n >= 32) {
                    break; /* 32 amostras ja dao uma media estavel */
                }
            }
        }
        scanned++;
    }

    vsti_reader_close(&r);

    if (out_pcr_pid != NULL) {
        *out_pcr_pid = est.pcr_pid;
    }
    return (n > 0) ? (sum / n) : 0;
}

int vsti_cmd_stream(int argc, char **argv)
{
    stream_opts_t o;
    memset(&o, 0, sizeof(o));
    o.ts_per_packet  = VSTI_RTP_TS_PER_PACKET;
    o.ttl            = 8;
    o.dscp           = -1;
    o.ssrc           = 0;
    o.seq            = -1;
    o.pace           = "pcr";
    o.stats_interval = 2.0;

    static const struct option longopts[] = {
        { "input",    required_argument, 0, 'i' },
        { "dest",     required_argument, 0, 'd' },
        { "raw",      no_argument,       0, 1001 },
        { "pkts",     required_argument, 0, 1002 },
        { "ssrc",     required_argument, 0, 1003 },
        { "seq",      required_argument, 0, 1004 },
        { "pace",     required_argument, 0, 1005 },
        { "bitrate",  required_argument, 0, 1006 },
        { "iface",    required_argument, 0, 1007 },
        { "ttl",      required_argument, 0, 1008 },
        { "dscp",     required_argument, 0, 1009 },
        { "sndbuf",   required_argument, 0, 1010 },
        { "loop",     no_argument,       0, 1011 },
        { "duration", required_argument, 0, 1012 },
        { "stats",    required_argument, 0, 1013 },
        { "verbose",  no_argument,       0, 'v' },
        { "quiet",    no_argument,       0, 'q' },
        { "help",     no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };

    int c;
    int verbosity = VSTI_LOG_INFO;

    optind = 1;
    while ((c = getopt_long(argc, argv, "i:d:vqh", longopts, NULL)) != -1) {
        switch (c) {
        case 'i':  o.input = optarg; break;
        case 'd':  o.dest  = optarg; break;
        case 1001: o.raw   = true;   break;
        case 1002: o.ts_per_packet = atoi(optarg); break;
        case 1003: o.ssrc  = strtol(optarg, NULL, 0); break;
        case 1004: o.seq   = strtol(optarg, NULL, 0); break;
        case 1005: o.pace  = optarg; break;
        case 1006: o.bitrate = vsti_parse_bitrate(optarg);
                   if (o.bitrate == 0) {
                       VSTI_ERR("taxa invalida: %s", optarg);
                       return 2;
                   }
                   break;
        case 1007: o.iface  = optarg; break;
        case 1008: o.ttl    = atoi(optarg); break;
        case 1009: o.dscp   = atoi(optarg); break;
        case 1010: o.sndbuf = atoi(optarg); break;
        case 1011: o.loop   = true; break;
        case 1012: o.duration = atof(optarg); break;
        case 1013: o.stats_interval = atof(optarg); break;
        case 'v':  verbosity++; break;
        case 'q':  verbosity = VSTI_LOG_ERROR; break;
        case 'h':  usage(); return 0;
        default:   usage(); return 2;
        }
    }

    vsti_log_set_level((vsti_log_level_t)(verbosity > VSTI_LOG_DEBUG
                                          ? VSTI_LOG_DEBUG : verbosity));

    if (o.input == NULL || o.dest == NULL) {
        VSTI_ERR("-i/--input e -d/--dest sao obrigatorios");
        usage();
        return 2;
    }
    if (o.ts_per_packet < 1 || o.ts_per_packet > VSTI_RTP_MAX_TS_PER_PACKET) {
        VSTI_ERR("--pkts deve estar entre 1 e %d", VSTI_RTP_MAX_TS_PER_PACKET);
        return 2;
    }

    vsti_pace_mode_t pace_mode;
    if (strcmp(o.pace, "pcr") == 0) {
        pace_mode = VSTI_PACE_PCR;
    } else if (strcmp(o.pace, "cbr") == 0) {
        pace_mode = VSTI_PACE_CBR;
        if (o.bitrate == 0) {
            VSTI_ERR("--pace cbr exige --bitrate");
            return 2;
        }
    } else if (strcmp(o.pace, "none") == 0) {
        pace_mode = VSTI_PACE_NONE;
    } else {
        VSTI_ERR("modo de pacing desconhecido: %s (use pcr, cbr ou none)", o.pace);
        return 2;
    }

    char host[256], port[32];
    if (!vsti_parse_endpoint(o.dest, host, sizeof(host), port, sizeof(port)) ||
        host[0] == '\0') {
        VSTI_ERR("destino invalido: %s (esperado HOST:PORTA)", o.dest);
        return 2;
    }

    /* ---- Estimativa inicial de taxa ---- */

    uint64_t bitrate = o.bitrate;
    if (pace_mode == VSTI_PACE_PCR) {
        uint64_t pcr_pid = 0;
        const uint64_t measured = prescan_bitrate(o.input, &pcr_pid);
        if (measured > 0) {
            bitrate = measured;
            char b[64];
            vsti_fmt_bitrate(bitrate, b, sizeof(b));
            VSTI_INFO("taxa medida no arquivo: %s (PCR no PID 0x%04X)",
                      b, (unsigned)pcr_pid);
        } else {
            bitrate = (o.bitrate > 0) ? o.bitrate : BOOTSTRAP_BITRATE;
            VSTI_WARN("nenhum PCR utilizavel na sondagem inicial; "
                      "partindo de %llu bit/s e ajustando em tempo real",
                      (unsigned long long)bitrate);
        }
    }
    if (bitrate == 0) {
        bitrate = BOOTSTRAP_BITRATE;
    }

    /* ---- Abertura da entrada e do socket ---- */

    vsti_reader_t reader;
    if (vsti_reader_open(&reader, o.input, o.loop) != 0) {
        VSTI_ERR("nao foi possivel abrir '%s'", o.input);
        return 1;
    }

    vsti_net_opts_t nopts;
    vsti_net_opts_default(&nopts);
    nopts.host   = host;
    nopts.port   = port;
    nopts.iface  = o.iface;
    nopts.ttl    = o.ttl;
    nopts.dscp   = o.dscp;
    nopts.sndbuf = o.sndbuf;

    vsti_sock_t sock;
    char errbuf[256];
    if (vsti_sock_open_tx(&sock, &nopts, errbuf, sizeof(errbuf)) != 0) {
        VSTI_ERR("socket de saida: %s", errbuf);
        vsti_reader_close(&reader);
        return 1;
    }

    char peer[128];
    vsti_sock_peer_str(&sock, peer, sizeof(peer));

    {
        char b[64];
        vsti_fmt_bitrate(bitrate, b, sizeof(b));
        VSTI_INFO("emitindo %s para %s (%s, %d pacote%s TS/datagrama, %s)",
                  o.input, peer,
                  sock.is_multicast ? "multicast" : "unicast",
                  o.ts_per_packet, o.ts_per_packet > 1 ? "s" : "",
                  o.raw ? "UDP puro" : "RTP PT=33");
        VSTI_INFO("ritmo: %s a %s",
                  pace_mode == VSTI_PACE_PCR ? "PCR adaptativo" :
                  pace_mode == VSTI_PACE_CBR ? "CBR" : "sem controle", b);
    }

    /* ---- Estado de emissao ---- */

    vsti_rtp_session_t rtp;
    vsti_rtp_init(&rtp, (uint32_t)o.ssrc, (int32_t)o.seq);

    vsti_pacer_t pacer;
    vsti_pacer_init(&pacer, pace_mode, bitrate);

    vsti_bitrate_est_t est;
    vsti_bitrate_est_init(&est, VSTI_PID_NULL);

    uint8_t datagram[VSTI_RTP_MAX_DATAGRAM];
    const size_t hdr_size    = o.raw ? 0 : VSTI_RTP_HEADER_SIZE;
    size_t in_datagram = 0;

    /*
     * Relogio de 90 kHz do RTP. Mantido em ponto fixo (deslocado 16 bits) para
     * que o avanco fracionario por datagrama nao acumule erro ao longo de horas
     * de transmissao.
     */
    uint64_t ts90_fixed = 0;
    /* Timestamp congelado no instante em que o datagrama comecou a ser montado. */
    uint64_t dg_ts90    = 0;
    bool     dg_started = false;

    uint64_t datagrams = 0, ts_packets = 0, bytes_sent = 0;
    /*
     * Duas contagens distintas: o total, que vai para o resumo, e a sequencia
     * consecutiva, que dispara a desistencia. Um socket unicast apontado para
     * uma porta sem ouvinte recebe ICMP port-unreachable e alterna entre
     * sucesso e ECONNREFUSED — situacao que precisa aparecer no relatorio, mas
     * nao deve abortar a transmissao.
     */
    uint64_t send_errors_total = 0, send_errors_run = 0;
    uint64_t cc_errors = 0;
    vsti_cc_state_t cc[VSTI_PID_COUNT];
    memset(cc, 0, sizeof(cc));

    const uint64_t t_start = vsti_now_ns();
    uint64_t t_last_stats  = t_start;

    vsti_install_signal_handlers();

    int rc = 0;

    for (;;) {
        if (g_vsti_stop) {
            VSTI_INFO("interrupcao recebida, encerrando");
            break;
        }
        if (o.duration > 0.0) {
            const double elapsed = (double)(vsti_now_ns() - t_start) / 1e9;
            if (elapsed >= o.duration) {
                break;
            }
        }

        /*
         * O timestamp RTP deve descrever o instante de amostragem do primeiro
         * byte do payload (RFC 2250, secao 2). Congelamos o valor aqui, no
         * inicio do datagrama, para que um PCR que chegue nos pacotes seguintes
         * atualize o relogio sem falsear o timestamp deste datagrama.
         */
        if (in_datagram == 0) {
            dg_ts90    = ts90_fixed;
            dg_started = true;
        }

        uint8_t *slot = datagram + hdr_size + in_datagram * VSTI_TS_PACKET_SIZE;
        const int r = vsti_reader_next(&reader, slot);
        if (r == 0) {
            break; /* Fim do arquivo */
        }
        if (r < 0) {
            VSTI_ERR("erro de leitura na entrada");
            rc = 1;
            break;
        }

        vsti_ts_header_t h;
        if (vsti_ts_parse(slot, &h)) {
            if (vsti_cc_check(&cc[h.pid], &h) == VSTI_CC_ERROR) {
                cc_errors++;
            }

            /*
             * Sempre que passa um PCR, travamos o relogio RTP nele. Entre PCRs
             * o relogio avanca por interpolacao a partir da taxa corrente, o
             * que e exatamente o que a RFC 2250 descreve para MP2T.
             */
            if (h.has_pcr && (est.pcr_pid == VSTI_PID_NULL || h.pid == est.pcr_pid)) {
                ts90_fixed = (uint64_t)vsti_rtp_ts_from_pcr27(vsti_pcr_to_27mhz(h.pcr)) << 16;
                if (in_datagram == 0) {
                    /*
                     * O PCR estava no primeiro pacote do datagrama, entao ele
                     * descreve exatamente este payload: vale usa-lo direto.
                     */
                    dg_ts90 = ts90_fixed;
                }
            }

            if (pace_mode == VSTI_PACE_PCR &&
                vsti_bitrate_est_feed(&est, &h, VSTI_TS_PACKET_SIZE)) {
                /*
                 * Ajuste suave da taxa: media exponencial com peso 1/8 na
                 * amostra nova. Reagir instantaneamente a cada medida faria o
                 * pacing oscilar junto com o ruido de medicao do PCR.
                 */
                const uint64_t cur = pacer.bitrate_bps;
                const uint64_t nb  = (cur * 7ull + est.bitrate_bps) / 8ull;
                vsti_pacer_set_bitrate(&pacer, nb);
            }
        }

        in_datagram++;
        ts_packets++;

        if (in_datagram < (size_t)o.ts_per_packet) {
            continue;
        }

        /* ---- Datagrama completo: encapsular e enviar ---- */

        const size_t payload_len = in_datagram * VSTI_TS_PACKET_SIZE;
        const size_t total_len   = hdr_size + payload_len;

        if (!o.raw) {
            rtp.timestamp = (uint32_t)(dg_ts90 >> 16);
            vsti_rtp_write_header(&rtp, datagram, false);
        }

        vsti_pacer_wait(&pacer, total_len);

        const ssize_t sent = vsti_sock_send(&sock, datagram, total_len);
        if (sent < 0) {
            send_errors_total++;
            send_errors_run++;
            /*
             * Erros de envio isolados (ENOBUFS em rajada, EPERM momentaneo do
             * firewall) nao devem derrubar a transmissao. So desistimos se o
             * problema for persistente.
             */
            if (send_errors_run > 1000) {
                VSTI_ERR("mais de 1000 falhas de envio consecutivas, abortando");
                rc = 1;
                break;
            }
        } else {
            bytes_sent += (uint64_t)sent;
            send_errors_run = 0;
        }

        datagrams++;

        /*
         * Avanco do relogio de 90 kHz para o proximo datagrama:
         *   ticks = bytes * 8 * 90000 / bitrate
         * Se ainda nao vimos nenhum PCR, o relogio simplesmente comeca em zero
         * e avanca por essa mesma regra, o que ja e valido para o receptor.
         */
        {
            const uint64_t br = (pacer.bitrate_bps > 0) ? pacer.bitrate_bps : bitrate;
            ts90_fixed += ((payload_len * 8ull * VSTI_PTS_CLOCK_HZ) << 16) / br;
        }

        in_datagram = 0;
        dg_started  = false;

        /* ---- Relatorio periodico ---- */

        if (o.stats_interval > 0.0) {
            const uint64_t now = vsti_now_ns();
            if ((double)(now - t_last_stats) / 1e9 >= o.stats_interval) {
                t_last_stats = now;
                const double elapsed = (double)(now - t_start) / 1e9;

                char rate[64], sentb[64], dur[32];
                vsti_fmt_bitrate((uint64_t)((double)bytes_sent * 8.0 / elapsed),
                                 rate, sizeof(rate));
                vsti_fmt_bytes(bytes_sent, sentb, sizeof(sentb));
                vsti_fmt_duration(elapsed, dur, sizeof(dur));

                VSTI_INFO("%s | %llu datagramas | %s | %s | CC err: %llu",
                          dur, (unsigned long long)datagrams, sentb, rate,
                          (unsigned long long)cc_errors);
            }
        }
    }

    /* ---- Descarga do datagrama parcial ---- */

    if (in_datagram > 0 && rc == 0) {
        const size_t payload_len = in_datagram * VSTI_TS_PACKET_SIZE;
        if (!o.raw) {
            rtp.timestamp = (uint32_t)((dg_started ? dg_ts90 : ts90_fixed) >> 16);
            vsti_rtp_write_header(&rtp, datagram, true); /* marker: fim do fluxo */
        }
        vsti_pacer_wait(&pacer, hdr_size + payload_len);
        const ssize_t sent = vsti_sock_send(&sock, datagram, hdr_size + payload_len);
        if (sent > 0) {
            bytes_sent += (uint64_t)sent;
            datagrams++;
        }
    }

    /* ---- Resumo final ---- */

    const double elapsed = (double)(vsti_now_ns() - t_start) / 1e9;
    char sentb[64], rate[64], dur[32];
    vsti_fmt_bytes(bytes_sent, sentb, sizeof(sentb));
    vsti_fmt_bitrate(elapsed > 0 ? (uint64_t)((double)bytes_sent * 8.0 / elapsed) : 0,
                     rate, sizeof(rate));
    vsti_fmt_duration(elapsed, dur, sizeof(dur));

    VSTI_INFO("---");
    VSTI_INFO("duracao ....... %s", dur);
    VSTI_INFO("pacotes TS .... %llu", (unsigned long long)ts_packets);
    VSTI_INFO("datagramas .... %llu", (unsigned long long)datagrams);
    VSTI_INFO("enviado ....... %s (%s medios)", sentb, rate);
    VSTI_INFO("erros de CC ... %llu", (unsigned long long)cc_errors);
    VSTI_INFO("resyncs ....... %llu", (unsigned long long)reader.resyncs);
    if (send_errors_total > 0) {
        VSTI_WARN("falhas de envio %llu de %llu datagramas — verifique se ha "
                  "receptor no destino e se o buffer do socket e suficiente",
                  (unsigned long long)send_errors_total,
                  (unsigned long long)(datagrams + send_errors_total));
    }
    if (pacer.late_events > 0) {
        VSTI_INFO("atrasos ....... %llu (maximo %.2f ms)",
                  (unsigned long long)pacer.late_events,
                  (double)pacer.max_lateness_ns / 1e6);
    }

    vsti_sock_close(&sock);
    vsti_reader_close(&reader);
    return rc;
}
