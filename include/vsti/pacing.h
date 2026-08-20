/*
 * vsti/pacing.h - Controle de ritmo de emissao (traffic shaping).
 *
 * Um transport stream so e util no destino se chegar na mesma taxa em que foi
 * produzido. Enviar um arquivo "o mais rapido possivel" estoura o buffer de
 * qualquer decodificador; enviar devagar demais o esvazia. Este modulo cuida
 * de espacar os datagramas no tempo com precisao suficiente para que o buffer
 * do receptor permaneca estavel.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_PACING_H
#define VSTI_PACING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vsti/ts.h"

typedef enum {
    VSTI_PACE_NONE,   /* Envia sem espera: util para testes de vazao */
    VSTI_PACE_CBR,    /* Taxa constante informada pelo usuario */
    VSTI_PACE_PCR     /* Taxa derivada dos PCRs do proprio fluxo */
} vsti_pace_mode_t;

typedef struct {
    vsti_pace_mode_t mode;
    uint64_t bitrate_bps;      /* Taxa alvo em bits por segundo */

    /*
     * Deadline absoluto do proximo envio, em nanossegundos de CLOCK_MONOTONIC.
     *
     * Trabalhar com deadline absoluto, e nao com "dormir X ns", e o que impede
     * o acumulo de erro: cada sleep real dura um pouco mais que o pedido, e ao
     * longo de uma hora de transmissao esse excesso viraria segundos de atraso.
     */
    uint64_t next_deadline_ns;
    bool     started;

    /*
     * Residuo fracionario em nanossegundos multiplicado por 2^16.
     *
     * O intervalo entre datagramas raramente e inteiro em nanossegundos
     * (1316 bytes a 17,27 Mbps da 609.451,7... ns). Guardar a parte fracionaria
     * em ponto fixo mantem a taxa media exata sem recorrer a ponto flutuante no
     * caminho critico.
     */
    uint64_t frac_accum;

    /*
     * Abaixo deste limiar nao vale a pena chamar o kernel: a latencia de
     * agendamento seria maior que a espera desejada. Acumulamos o credito e
     * dormimos de uma vez quando ele ultrapassa o limiar.
     */
    uint64_t min_sleep_ns;

    /* Estatisticas de qualidade do pacing. */
    uint64_t sleeps;
    uint64_t late_events;      /* Vezes em que ja estavamos atrasados */
    uint64_t max_lateness_ns;
} vsti_pacer_t;

/*
 * Inicializa o controlador.
 * `bitrate_bps` e ignorado quando o modo e VSTI_PACE_NONE.
 */
void vsti_pacer_init(vsti_pacer_t *p, vsti_pace_mode_t mode, uint64_t bitrate_bps);

/* Ajusta a taxa em tempo de execucao (usado pelo modo PCR). */
void vsti_pacer_set_bitrate(vsti_pacer_t *p, uint64_t bitrate_bps);

/*
 * Espera ate o instante correto de enviar `bytes` bytes.
 * Deve ser chamada imediatamente antes de cada envio.
 */
void vsti_pacer_wait(vsti_pacer_t *p, size_t bytes);

/* Relogio monotonico em nanossegundos. */
uint64_t vsti_now_ns(void);

/* ------------------------------------------------------------------ */
/* Estimador de bitrate a partir do PCR                                 */
/* ------------------------------------------------------------------ */

/*
 * Mede a taxa real do fluxo observando a distancia entre PCRs consecutivos do
 * mesmo PID e a quantidade de bytes transportados entre eles.
 *
 * bitrate = bytes_entre_pcrs * 8 * 27e6 / delta_pcr_em_unidades_de_27MHz
 *
 * O estimador ignora intervalos absurdamente curtos ou longos, que indicam
 * descontinuidade de relogio (troca de fonte, corte de arquivo) e produziriam
 * uma taxa sem sentido.
 */
typedef struct {
    uint16_t pcr_pid;        /* 0x1FFF = ainda nao definido */
    bool     have_last;
    uint64_t last_pcr27;
    uint64_t bytes_since;
    uint64_t bitrate_bps;    /* Ultima estimativa valida */
    uint64_t samples;
} vsti_bitrate_est_t;

void vsti_bitrate_est_init(vsti_bitrate_est_t *e, uint16_t pcr_pid);

/*
 * Alimenta o estimador com um pacote. `pkt_bytes` normalmente e 188.
 * Retorna true quando uma nova estimativa foi produzida.
 */
bool vsti_bitrate_est_feed(vsti_bitrate_est_t *e,
                           const vsti_ts_header_t *hdr,
                           size_t pkt_bytes);

#endif /* VSTI_PACING_H */
