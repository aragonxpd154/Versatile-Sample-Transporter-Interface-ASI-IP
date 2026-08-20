/*
 * src/pacing.c - Controle de ritmo de emissao e estimativa de bitrate.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#define _POSIX_C_SOURCE 200809L

#include "vsti/pacing.h"

#include <errno.h>
#include <string.h>
#include <time.h>

uint64_t vsti_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void vsti_pacer_init(vsti_pacer_t *p, vsti_pace_mode_t mode, uint64_t bitrate_bps)
{
    memset(p, 0, sizeof(*p));
    p->mode        = mode;
    p->bitrate_bps = bitrate_bps;

    /*
     * 250 us e um meio-termo pratico: menor que isso e a granularidade do
     * agendador do Linux (mesmo com HRTIMER) domina a espera, maior que isso
     * comeca a produzir rajadas visiveis em analisadores de jitter.
     */
    p->min_sleep_ns = 250000ull;
}

void vsti_pacer_set_bitrate(vsti_pacer_t *p, uint64_t bitrate_bps)
{
    if (bitrate_bps > 0) {
        p->bitrate_bps = bitrate_bps;
    }
}

/* Dorme ate um instante absoluto de CLOCK_MONOTONIC. */
static void sleep_until_ns(uint64_t deadline_ns)
{
    struct timespec ts;
    ts.tv_sec  = (time_t)(deadline_ns / 1000000000ull);
    ts.tv_nsec = (long)(deadline_ns % 1000000000ull);

    /*
     * clock_nanosleep com TIMER_ABSTIME e reiniciado com o mesmo deadline em
     * caso de EINTR, entao um sinal nao encurta nem alonga a espera.
     */
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR) {
        /* Reiniciar com o mesmo alvo absoluto. */
    }
}

void vsti_pacer_wait(vsti_pacer_t *p, size_t bytes)
{
    if (p->mode == VSTI_PACE_NONE || p->bitrate_bps == 0) {
        return;
    }

    const uint64_t now = vsti_now_ns();

    if (!p->started) {
        p->started          = true;
        p->next_deadline_ns = now;
        p->frac_accum       = 0;
        return;
    }

    /*
     * Intervalo ideal para transmitir `bytes` na taxa alvo:
     *   ns = bytes * 8 * 1e9 / bitrate
     * Calculado em ponto fixo 16.16 para preservar a fracao.
     */
    const uint64_t numer = (uint64_t)bytes * 8ull * 1000000000ull;
    const uint64_t scaled = (numer << 16) / p->bitrate_bps;

    p->frac_accum += scaled;
    const uint64_t interval_ns = p->frac_accum >> 16;
    p->frac_accum &= 0xFFFFull;

    p->next_deadline_ns += interval_ns;

    if (p->next_deadline_ns > now) {
        const uint64_t wait = p->next_deadline_ns - now;
        if (wait >= p->min_sleep_ns) {
            sleep_until_ns(p->next_deadline_ns);
            p->sleeps++;
        }
        /*
         * Abaixo do limiar nao dormimos: o credito fica registrado no deadline
         * e sera consumido nas proximas iteracoes, produzindo uma espera maior
         * e mais eficiente mais adiante.
         */
    } else {
        const uint64_t late = now - p->next_deadline_ns;
        p->late_events++;
        if (late > p->max_lateness_ns) {
            p->max_lateness_ns = late;
        }

        /*
         * Atraso grande significa que a maquina nao esta acompanhando (disco
         * lento, CPU saturada, socket bloqueando). Insistir em recuperar o
         * tempo perdido geraria uma rajada que so pioraria a situacao no
         * receptor, entao realinhamos o deadline com o presente.
         */
        if (late > 100000000ull) { /* 100 ms */
            p->next_deadline_ns = now;
            p->frac_accum       = 0;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Estimador de bitrate                                                 */
/* ------------------------------------------------------------------ */

void vsti_bitrate_est_init(vsti_bitrate_est_t *e, uint16_t pcr_pid)
{
    memset(e, 0, sizeof(*e));
    e->pcr_pid = pcr_pid;
}

bool vsti_bitrate_est_feed(vsti_bitrate_est_t *e,
                           const vsti_ts_header_t *hdr,
                           size_t pkt_bytes)
{
    e->bytes_since += pkt_bytes;

    if (!hdr->has_pcr) {
        return false;
    }

    /*
     * Se nenhum PID foi fixado, adotamos o primeiro que apresentar PCR. Em um
     * SPTS isso e sempre o PID correto; em MPTS o chamador deve informar o
     * PCR_PID obtido da PMT do programa de interesse.
     */
    if (e->pcr_pid == VSTI_PID_NULL || e->pcr_pid == 0) {
        e->pcr_pid = hdr->pid;
    }
    if (hdr->pid != e->pcr_pid) {
        return false;
    }

    const uint64_t pcr27 = vsti_pcr_to_27mhz(hdr->pcr);

    if (!e->have_last) {
        e->have_last  = true;
        e->last_pcr27 = pcr27;
        e->bytes_since = 0;
        return false;
    }

    /*
     * O PCR conta em modulo 2^33 * 300. A subtracao em aritmetica sem sinal
     * seguida da mascara do modulo trata o wrap-around naturalmente.
     */
    uint64_t delta = (pcr27 >= e->last_pcr27)
                   ? (pcr27 - e->last_pcr27)
                   : (VSTI_PCR_FULL_MODULO - e->last_pcr27 + pcr27);

    const uint64_t bytes = e->bytes_since;

    e->last_pcr27  = pcr27;
    e->bytes_since = 0;

    /*
     * Filtro de sanidade. A norma exige PCR a cada 100 ms no maximo; abaixo de
     * 1 ms a medida fica dominada por granularidade e acima de 1 s quase
     * certamente houve descontinuidade de relogio ou emenda de arquivo.
     */
    const uint64_t min_delta = VSTI_SYSTEM_CLOCK_HZ / 1000ull;  /* 1 ms */
    const uint64_t max_delta = VSTI_SYSTEM_CLOCK_HZ;            /* 1 s  */

    if (delta < min_delta || delta > max_delta || bytes == 0) {
        return false;
    }

    e->bitrate_bps = (bytes * 8ull * VSTI_SYSTEM_CLOCK_HZ) / delta;
    e->samples++;
    return true;
}
