/*
 * vsti/log.h - Registro de eventos em stderr com niveis.
 *
 * stdout fica reservado para dados (relatorios, JSON, TS em pipe), entao todo
 * log vai para stderr. Isso permite compor a ferramenta em pipeline sem que as
 * mensagens contaminem a saida.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_LOG_H
#define VSTI_LOG_H

#include <stdbool.h>

typedef enum {
    VSTI_LOG_ERROR = 0,
    VSTI_LOG_WARN  = 1,
    VSTI_LOG_INFO  = 2,
    VSTI_LOG_DEBUG = 3
} vsti_log_level_t;

void vsti_log_set_level(vsti_log_level_t lvl);
vsti_log_level_t vsti_log_get_level(void);

/* Desliga as cores ANSI (automatico quando stderr nao e um terminal). */
void vsti_log_set_color(bool enable);

void vsti_log(vsti_log_level_t lvl, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define VSTI_ERR(...)  vsti_log(VSTI_LOG_ERROR, __VA_ARGS__)
#define VSTI_WARN(...) vsti_log(VSTI_LOG_WARN,  __VA_ARGS__)
#define VSTI_INFO(...) vsti_log(VSTI_LOG_INFO,  __VA_ARGS__)
#define VSTI_DBG(...)  vsti_log(VSTI_LOG_DEBUG, __VA_ARGS__)

#endif /* VSTI_LOG_H */
