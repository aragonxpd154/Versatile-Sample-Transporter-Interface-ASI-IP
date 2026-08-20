/*
 * src/log.c - Registro de eventos.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#define _POSIX_C_SOURCE 200809L

#include "vsti/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

static vsti_log_level_t g_level = VSTI_LOG_INFO;
static int g_color = -1; /* -1 = ainda nao decidido */

void vsti_log_set_level(vsti_log_level_t lvl) { g_level = lvl; }
vsti_log_level_t vsti_log_get_level(void)     { return g_level; }
void vsti_log_set_color(bool enable)          { g_color = enable ? 1 : 0; }

static const char *prefix_for(vsti_log_level_t lvl, bool color)
{
    if (color) {
        switch (lvl) {
        case VSTI_LOG_ERROR: return "\033[1;31merro\033[0m";
        case VSTI_LOG_WARN:  return "\033[1;33maviso\033[0m";
        case VSTI_LOG_INFO:  return "\033[1;36minfo\033[0m";
        default:             return "\033[1;35mdebug\033[0m";
        }
    }
    switch (lvl) {
    case VSTI_LOG_ERROR: return "erro";
    case VSTI_LOG_WARN:  return "aviso";
    case VSTI_LOG_INFO:  return "info";
    default:             return "debug";
    }
}

void vsti_log(vsti_log_level_t lvl, const char *fmt, ...)
{
    if (lvl > g_level) {
        return;
    }

    if (g_color < 0) {
        /*
         * Cor so faz sentido em terminal. Quando a saida vai para arquivo ou
         * pipe (redirecionamento, systemd, CI), os codigos ANSI viram lixo.
         */
        g_color = isatty(STDERR_FILENO) ? 1 : 0;
    }

    fprintf(stderr, "vsti: %s: ", prefix_for(lvl, g_color == 1));

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
}
