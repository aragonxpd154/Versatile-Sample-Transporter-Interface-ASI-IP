/*
 * vsti/ts.h - Camada de pacotes MPEG-2 Transport Stream (ISO/IEC 13818-1).
 *
 * Esta camada nao aloca memoria e nao faz I/O: ela apenas interpreta um buffer
 * de 188 bytes. Isso mantem o parser testavel de forma isolada e permite que o
 * mesmo codigo rode sobre arquivo, socket ou memoria mapeada.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_TS_H
#define VSTI_TS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Tamanho canonico de um pacote de transporte MPEG-2. */
#define VSTI_TS_PACKET_SIZE 188
/* Variantes com codigo corretor de erro Reed-Solomon (DVB-ASI / M2TS). */
#define VSTI_TS_PACKET_SIZE_204 204
#define VSTI_TS_PACKET_SIZE_208 208
/* Variante com timecode de 4 bytes usada em Blu-ray / m2ts. */
#define VSTI_TS_PACKET_SIZE_192 192

#define VSTI_TS_SYNC_BYTE 0x47u

/* PIDs reservados pelas normas MPEG-2, DVB e ARIB (ISDB-Tb). */
#define VSTI_PID_PAT   0x0000u /* Program Association Table */
#define VSTI_PID_CAT   0x0001u /* Conditional Access Table */
#define VSTI_PID_TSDT  0x0002u /* Transport Stream Description Table */
#define VSTI_PID_NIT   0x0010u /* Network Information Table */
#define VSTI_PID_SDT   0x0011u /* Service Description Table / BAT */
#define VSTI_PID_EIT   0x0012u /* Event Information Table */
#define VSTI_PID_RST   0x0013u /* Running Status Table */
#define VSTI_PID_TDT   0x0014u /* Time & Date Table / TOT */
#define VSTI_PID_BIT   0x0024u /* Broadcaster Information Table (ARIB) */
#define VSTI_PID_SDTT  0x0023u /* Software Download Trigger Table (ARIB) */
#define VSTI_PID_CDT   0x0029u /* Common Data Table (ARIB) */
#define VSTI_PID_NULL  0x1FFFu /* Pacotes de enchimento (stuffing) */

#define VSTI_PID_MAX   0x1FFFu
#define VSTI_PID_COUNT 0x2000u

/* Codigos de adaptation_field_control (ISO/IEC 13818-1 Tabela 2-5). */
typedef enum {
    VSTI_AFC_RESERVED      = 0, /* Invalido: decodificador deve descartar */
    VSTI_AFC_PAYLOAD_ONLY  = 1,
    VSTI_AFC_ADAPT_ONLY    = 2,
    VSTI_AFC_ADAPT_PAYLOAD = 3
} vsti_afc_t;

/*
 * Relogio de programa. O PCR e transmitido em duas partes: uma base de 33 bits
 * a 90 kHz e uma extensao de 9 bits a 27 MHz. O valor unificado em 27 MHz e
 * base * 300 + ext, e e ele que usamos para calcular bitrate e pacing.
 */
typedef struct {
    uint64_t base; /* 33 bits, unidade de 1/90000 s */
    uint16_t ext;  /* 9 bits, unidade de 1/27000000 s */
} vsti_pcr_t;

/* Frequencia do relogio de sistema MPEG-2, em Hz. */
#define VSTI_SYSTEM_CLOCK_HZ 27000000ull
/* Frequencia do relogio de apresentacao (PTS/DTS e timestamp RTP). */
#define VSTI_PTS_CLOCK_HZ 90000ull
/* Modulo de contagem do PCR base: 2^33. */
#define VSTI_PCR_BASE_MODULO (1ull << 33)
/* Modulo do PCR completo em 27 MHz. */
#define VSTI_PCR_FULL_MODULO (VSTI_PCR_BASE_MODULO * 300ull)

/* Converte o par base/extensao para um unico contador de 27 MHz. */
static inline uint64_t vsti_pcr_to_27mhz(vsti_pcr_t pcr)
{
    return pcr.base * 300ull + (uint64_t)pcr.ext;
}

/*
 * Cabecalho de um pacote de transporte ja decodificado.
 * Os campos seguem os nomes da norma para facilitar a conferencia.
 */
typedef struct {
    uint16_t pid;
    uint8_t  continuity_counter;      /* 4 bits, incrementa so quando ha payload */
    uint8_t  transport_scrambling;    /* 2 bits, != 0 indica payload cifrado */
    vsti_afc_t adaptation_control;
    bool     transport_error;         /* TEI: erro irrecuperavel sinalizado a montante */
    bool     payload_unit_start;      /* PUSI: inicio de secao PSI ou de PES */
    bool     transport_priority;

    /* Campos de adaptacao (validos apenas se has_adaptation). */
    bool     has_adaptation;
    uint8_t  adaptation_length;       /* Byte de comprimento, sem contar ele mesmo */
    bool     discontinuity;
    bool     random_access;
    bool     has_pcr;
    vsti_pcr_t pcr;
    bool     has_opcr;
    vsti_pcr_t opcr;

    /* Recorte do payload dentro do pacote original. */
    bool     has_payload;
    uint8_t  payload_offset;          /* Deslocamento a partir do byte de sync */
    uint8_t  payload_length;
} vsti_ts_header_t;

/*
 * Decodifica o cabecalho de um pacote de 188 bytes.
 * Retorna true se o pacote e estruturalmente valido (sync correto, campo de
 * adaptacao dentro dos limites). Um pacote com TEI ligado ainda e considerado
 * valido aqui: cabe ao chamador decidir se descarta, porque em analise de
 * qualidade de sinal esses pacotes precisam ser contados, nao ignorados.
 */
bool vsti_ts_parse(const uint8_t *pkt, vsti_ts_header_t *out);

/*
 * Verifica apenas o byte de sincronismo. Barato o suficiente para ser chamado
 * em loop apertado antes de decodificar o cabecalho inteiro.
 */
static inline bool vsti_ts_has_sync(const uint8_t *pkt)
{
    return pkt[0] == VSTI_TS_SYNC_BYTE;
}

/* Extrai o PID sem decodificar o restante do cabecalho. */
static inline uint16_t vsti_ts_pid(const uint8_t *pkt)
{
    return (uint16_t)(((pkt[1] & 0x1Fu) << 8) | pkt[2]);
}

/*
 * Procura o alinhamento de pacotes em um buffer arbitrario.
 *
 * Um fluxo capturado raramente comeca em um limite de pacote, e capturas de
 * ASI podem vir com 188, 192, 204 ou 208 bytes por pacote. Esta funcao testa
 * cada tamanho conhecido e exige varios sync bytes consecutivos no mesmo
 * espacamento antes de declarar o alinhamento, o que evita casar com um 0x47
 * que apareca por acaso dentro de um payload de video.
 *
 * Em caso de sucesso grava o deslocamento e o tamanho detectados e retorna
 * true. Precisa de pelo menos VSTI_TS_SYNC_RUN pacotes para decidir.
 */
#define VSTI_TS_SYNC_RUN 5

bool vsti_ts_find_alignment(const uint8_t *buf, size_t len,
                            size_t *out_offset, size_t *out_packet_size);

/*
 * Avalia a continuidade de um PID.
 *
 * O continuity_counter e um contador de 4 bits que so avanca em pacotes que
 * carregam payload. Duplicatas exatas (mesmo CC repetido uma vez) sao legais
 * pela norma, e pacotes marcados com discontinuity_indicator devem reiniciar a
 * contagem sem gerar erro. Este estado guarda o necessario para distinguir os
 * tres casos.
 */
typedef struct {
    uint8_t last_cc;
    bool    seen;       /* Ja recebemos ao menos um pacote deste PID */
    bool    last_had_payload;
} vsti_cc_state_t;

typedef enum {
    VSTI_CC_OK,           /* Sequencia correta */
    VSTI_CC_FIRST,        /* Primeiro pacote do PID, nada a comparar */
    VSTI_CC_DUPLICATE,    /* Duplicata legal (mesmo CC, permitida uma vez) */
    VSTI_CC_DISCONTINUITY,/* Descontinuidade sinalizada, esperada */
    VSTI_CC_ERROR         /* Salto real: pacotes perdidos */
} vsti_cc_result_t;

vsti_cc_result_t vsti_cc_check(vsti_cc_state_t *st, const vsti_ts_header_t *hdr);

#endif /* VSTI_TS_H */
