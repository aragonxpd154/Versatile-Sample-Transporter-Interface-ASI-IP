/*
 * tests/test_util.h - Micro-harness de testes.
 *
 * Sem dependencia externa de proposito: a suite precisa rodar em qualquer
 * maquina com um compilador C, inclusive no runner do CI, sem instalar nada.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_TEST_UTIL_H
#define VSTI_TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int t_failures = 0;
static int t_checks   = 0;

#define T_CHECK(cond, ...)                                          \
    do {                                                            \
        t_checks++;                                                 \
        if (!(cond)) {                                              \
            t_failures++;                                           \
            fprintf(stderr, "  FALHA %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                           \
            fprintf(stderr, "\n");                                  \
        }                                                           \
    } while (0)

#define T_EQ_U(actual, expected, label)                                     \
    do {                                                                    \
        const unsigned long long _a = (unsigned long long)(actual);         \
        const unsigned long long _e = (unsigned long long)(expected);       \
        T_CHECK(_a == _e, "%s: esperado %llu, obtido %llu", label, _e, _a);  \
    } while (0)

#define T_EQ_STR(actual, expected, label)                                   \
    do {                                                                    \
        const char *_a = (actual);                                          \
        const char *_e = (expected);                                        \
        T_CHECK(strcmp(_a, _e) == 0,                                        \
                "%s: esperado \"%s\", obtido \"%s\"", label, _e, _a);       \
    } while (0)

#define T_TRUE(cond, label)  T_CHECK((cond), "%s: esperado verdadeiro", label)
#define T_FALSE(cond, label) T_CHECK(!(cond), "%s: esperado falso", label)

static inline int t_report(const char *suite)
{
    if (t_failures == 0) {
        fprintf(stderr, "[%s] %d verificacoes, tudo certo\n", suite, t_checks);
        return 0;
    }
    fprintf(stderr, "[%s] %d de %d verificacoes falharam\n",
            suite, t_failures, t_checks);
    return 1;
}

#endif /* VSTI_TEST_UTIL_H */
