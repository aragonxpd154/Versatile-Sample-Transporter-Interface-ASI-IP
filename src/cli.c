/*
 * src/cli.c - Utilitarios compartilhados entre os subcomandos.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#define _POSIX_C_SOURCE 200809L

#include "cli.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

volatile sig_atomic_t g_vsti_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_vsti_stop = 1;
}

void vsti_install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);

    /*
     * Sem SA_RESTART de proposito: queremos que recv() e nanosleep() retornem
     * EINTR para que o laco principal perceba o pedido de parada de imediato,
     * em vez de so na proxima iteracao completa.
     */
    sa.sa_flags = 0;

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /*
     * SIGPIPE ignorado: escrever em socket fechado deve devolver EPIPE para o
     * codigo tratar, e nao derrubar o processo silenciosamente.
     */
    signal(SIGPIPE, SIG_IGN);
}

bool vsti_parse_endpoint(const char *in,
                         char *host, size_t host_sz,
                         char *port, size_t port_sz)
{
    if (in == NULL || host == NULL || port == NULL) {
        return false;
    }

    host[0] = '\0';
    port[0] = '\0';

    /* Forma IPv6 entre colchetes. */
    if (in[0] == '[') {
        const char *close = strchr(in, ']');
        if (close == NULL || close[1] != ':') {
            return false;
        }
        const size_t hlen = (size_t)(close - in - 1);
        if (hlen == 0 || hlen >= host_sz) {
            return false;
        }
        memcpy(host, in + 1, hlen);
        host[hlen] = '\0';
        snprintf(port, port_sz, "%s", close + 2);
        return port[0] != '\0';
    }

    const char *colon = strrchr(in, ':');
    if (colon == NULL) {
        /* Somente a porta: usado em "receive --listen 1234". */
        for (const char *p = in; *p; ++p) {
            if (!isdigit((unsigned char)*p)) {
                return false;
            }
        }
        snprintf(port, port_sz, "%s", in);
        return in[0] != '\0';
    }

    const size_t hlen = (size_t)(colon - in);
    if (hlen >= host_sz) {
        return false;
    }
    memcpy(host, in, hlen);
    host[hlen] = '\0';
    snprintf(port, port_sz, "%s", colon + 1);
    return port[0] != '\0';
}

uint64_t vsti_parse_bitrate(const char *s)
{
    if (s == NULL || *s == '\0') {
        return 0;
    }

    char *end = NULL;
    const double v = strtod(s, &end);
    if (end == s || v <= 0.0) {
        return 0;
    }

    double mult = 1.0;
    while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }

    if (end != NULL && *end != '\0') {
        switch (*end) {
        case 'k': case 'K': mult = 1e3; break;
        case 'm': case 'M': mult = 1e6; break;
        case 'g': case 'G': mult = 1e9; break;
        default: return 0;
        }
        ++end;
        /* Aceita "M", "Mb", "Mbit", "Mbps" — tudo significa o mesmo aqui. */
        if (*end != '\0' &&
            strcasecmp(end, "b")    != 0 &&
            strcasecmp(end, "bit")  != 0 &&
            strcasecmp(end, "bps")  != 0 &&
            strcasecmp(end, "bit/s")!= 0) {
            return 0;
        }
    }

    return (uint64_t)(v * mult + 0.5);
}

void vsti_fmt_bitrate(uint64_t bps, char *out, size_t outsz)
{
    if (bps >= 1000000000ull) {
        snprintf(out, outsz, "%.3f Gbit/s", (double)bps / 1e9);
    } else if (bps >= 1000000ull) {
        snprintf(out, outsz, "%.3f Mbit/s", (double)bps / 1e6);
    } else if (bps >= 1000ull) {
        snprintf(out, outsz, "%.2f kbit/s", (double)bps / 1e3);
    } else {
        snprintf(out, outsz, "%llu bit/s", (unsigned long long)bps);
    }
}

void vsti_fmt_bytes(uint64_t bytes, char *out, size_t outsz)
{
    if (bytes >= (1ull << 30)) {
        snprintf(out, outsz, "%.2f GiB", (double)bytes / (double)(1ull << 30));
    } else if (bytes >= (1ull << 20)) {
        snprintf(out, outsz, "%.2f MiB", (double)bytes / (double)(1ull << 20));
    } else if (bytes >= (1ull << 10)) {
        snprintf(out, outsz, "%.2f KiB", (double)bytes / (double)(1ull << 10));
    } else {
        snprintf(out, outsz, "%llu B", (unsigned long long)bytes);
    }
}

void vsti_fmt_duration(double seconds, char *out, size_t outsz)
{
    if (seconds < 0) {
        seconds = 0;
    }
    const unsigned long long total = (unsigned long long)seconds;
    const unsigned long long h = total / 3600ull;
    const unsigned long long m = (total % 3600ull) / 60ull;
    const double s = seconds - (double)(h * 3600ull + m * 60ull);
    snprintf(out, outsz, "%02llu:%02llu:%06.3f", h, m, s);
}

void vsti_json_escape(const char *in, char *out, size_t outsz)
{
    size_t w = 0;

    if (out == NULL || outsz < 3) {
        if (out && outsz > 0) out[0] = '\0';
        return;
    }

    out[w++] = '"';

    for (const unsigned char *p = (const unsigned char *)in; in && *p; ++p) {
        /* Reserva espaco para o pior caso (\uXXXX = 6 bytes) e o fecho. */
        if (w + 8 >= outsz) {
            break;
        }
        switch (*p) {
        case '"':  out[w++] = '\\'; out[w++] = '"';  break;
        case '\\': out[w++] = '\\'; out[w++] = '\\'; break;
        case '\n': out[w++] = '\\'; out[w++] = 'n';  break;
        case '\r': out[w++] = '\\'; out[w++] = 'r';  break;
        case '\t': out[w++] = '\\'; out[w++] = 't';  break;
        case '\b': out[w++] = '\\'; out[w++] = 'b';  break;
        case '\f': out[w++] = '\\'; out[w++] = 'f';  break;
        default:
            if (*p < 0x20) {
                /*
                 * Demais caracteres de controle viram escape \u. Bytes >= 0x80
                 * passam intactos: a entrada ja e UTF-8 valido, produzido pelo
                 * decodificador de texto ARIB.
                 */
                w += (size_t)snprintf(out + w, outsz - w, "\\u%04x", *p);
            } else {
                out[w++] = (char)*p;
            }
            break;
        }
    }

    out[w++] = '"';
    out[w]   = '\0';
}
