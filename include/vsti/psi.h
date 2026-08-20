/*
 * vsti/psi.h - Remontagem de secoes PSI/SI e parsers de tabela.
 *
 * "PSI" (Program Specific Information) e o conjunto de tabelas MPEG-2 base
 * (PAT, PMT, CAT). "SI" (Service Information) e a extensao definida por DVB e
 * por ARIB STD-B10 para ISDB-Tb (SDT, EIT, NIT, TOT...). As duas famílias
 * compartilham o mesmo formato de secao, entao a remontagem e comum.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_PSI_H
#define VSTI_PSI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vsti/ts.h"

/*
 * Uma secao pode declarar section_length de ate 12 bits (4093), mais os 3
 * bytes de cabecalho que antecedem o campo. 4096 cobre o pior caso.
 */
#define VSTI_SECTION_MAX 4096

/* table_id das tabelas que este projeto interpreta. */
#define VSTI_TID_PAT       0x00u
#define VSTI_TID_CAT       0x01u
#define VSTI_TID_PMT       0x02u
#define VSTI_TID_NIT_ACT   0x40u
#define VSTI_TID_NIT_OTH   0x41u
#define VSTI_TID_SDT_ACT   0x42u
#define VSTI_TID_SDT_OTH   0x46u
#define VSTI_TID_BAT       0x4Au
#define VSTI_TID_EIT_ACT_PF 0x4Eu
#define VSTI_TID_EIT_OTH_PF 0x4Fu
#define VSTI_TID_TDT       0x70u
#define VSTI_TID_TOT       0x73u

/* EIT de agenda ocupa a faixa 0x50-0x5F (atual) e 0x60-0x6F (outros TS). */
static inline bool vsti_tid_is_eit_schedule(uint8_t tid)
{
    return (tid >= 0x50u && tid <= 0x6Fu);
}

static inline bool vsti_tid_is_eit(uint8_t tid)
{
    return tid == VSTI_TID_EIT_ACT_PF || tid == VSTI_TID_EIT_OTH_PF ||
           vsti_tid_is_eit_schedule(tid);
}

/*
 * CRC-32/MPEG-2: polinomio 0x04C11DB7, valor inicial 0xFFFFFFFF, sem reflexao
 * de entrada nem de saida, sem XOR final. E o mesmo CRC usado por todas as
 * tabelas PSI/SI com section_syntax_indicator igual a 1.
 *
 * Uma secao integra satisfaz crc32(secao_inteira_incluindo_o_crc) == 0, o que
 * torna a verificacao um unico calculo sem comparacao explicita.
 */
uint32_t vsti_crc32_mpeg2(const uint8_t *data, size_t len);

/* Cabecalho comum das secoes com sintaxe estendida. */
typedef struct {
    uint8_t  table_id;
    bool     section_syntax_indicator;
    uint16_t section_length;      /* Bytes apos este campo, incluindo o CRC */
    uint16_t table_id_extension;  /* transport_stream_id, program_number, service_id... */
    uint8_t  version_number;      /* 5 bits: muda quando o conteudo muda */
    bool     current_next;        /* true = tabela em vigor agora */
    uint8_t  section_number;
    uint8_t  last_section_number;
} vsti_section_header_t;

/*
 * Callback chamado para cada secao completa e com CRC valido.
 * `data` aponta para o inicio da secao (byte de table_id) e `len` cobre a
 * secao inteira, incluindo os 4 bytes de CRC.
 */
typedef void (*vsti_section_cb)(uint16_t pid,
                                const vsti_section_header_t *hdr,
                                const uint8_t *data, size_t len,
                                void *user);

/*
 * Montador de secoes de um unico PID.
 *
 * O protocolo aqui tem duas sutilezas que geram a maior parte dos bugs em
 * implementacoes ingenuas:
 *
 *  1. Quando PUSI esta ligado, o primeiro byte do payload e um pointer_field
 *     que diz quantos bytes de *cauda da secao anterior* vem antes do inicio
 *     da nova secao. Ignorar isso corta a ultima secao de cada ciclo.
 *
 *  2. Um unico pacote pode conter o fim de uma secao e o inicio (ou ate o
 *     total) de varias outras. E preciso iterar, nao tratar so a primeira.
 */
typedef struct {
    uint8_t  buf[VSTI_SECTION_MAX];
    size_t   len;       /* Bytes ja acumulados */
    size_t   expected;  /* Tamanho total esperado, 0 enquanto desconhecido */
    bool     active;    /* Estamos no meio de uma secao */
    uint16_t pid;
} vsti_section_asm_t;

void vsti_section_asm_init(vsti_section_asm_t *asmb, uint16_t pid);
void vsti_section_asm_reset(vsti_section_asm_t *asmb);

/*
 * Alimenta o montador com o payload de um pacote TS.
 * Deve ser chamado apenas para pacotes que carreguem payload.
 */
void vsti_section_asm_feed(vsti_section_asm_t *asmb,
                           const uint8_t *payload, size_t len,
                           bool payload_unit_start,
                           vsti_section_cb cb, void *user);

/* Decodifica o cabecalho comum. Retorna false se a secao for curta demais. */
bool vsti_section_parse_header(const uint8_t *data, size_t len,
                               vsti_section_header_t *out);

/* ------------------------------------------------------------------ */
/* PAT - Program Association Table                                      */
/* ------------------------------------------------------------------ */

#define VSTI_MAX_PROGRAMS 256

typedef struct {
    uint16_t program_number; /* 0 = ponteiro para a NIT */
    uint16_t pid;            /* PID da PMT (ou da NIT se program_number == 0) */
} vsti_pat_entry_t;

typedef struct {
    uint16_t transport_stream_id;
    uint8_t  version;
    size_t   count;
    vsti_pat_entry_t entries[VSTI_MAX_PROGRAMS];
} vsti_pat_t;

bool vsti_parse_pat(const uint8_t *sec, size_t len, vsti_pat_t *out);

/* ------------------------------------------------------------------ */
/* PMT - Program Map Table                                              */
/* ------------------------------------------------------------------ */

#define VSTI_MAX_STREAMS 64

typedef struct {
    uint8_t  stream_type;
    uint16_t elementary_pid;
    uint16_t es_info_length;
    /* Recorte dos descritores dentro da secao original, sem copia. */
    const uint8_t *es_descriptors;
} vsti_pmt_stream_t;

typedef struct {
    uint16_t program_number;
    uint8_t  version;
    uint16_t pcr_pid;
    uint16_t program_info_length;
    const uint8_t *program_descriptors;
    size_t   stream_count;
    vsti_pmt_stream_t streams[VSTI_MAX_STREAMS];
} vsti_pmt_t;

bool vsti_parse_pmt(const uint8_t *sec, size_t len, vsti_pmt_t *out);

/* Nome legivel de um stream_type (ISO/IEC 13818-1 Tabela 2-34 + ARIB). */
const char *vsti_stream_type_name(uint8_t stream_type);

/* ------------------------------------------------------------------ */
/* SDT - Service Description Table                                      */
/* ------------------------------------------------------------------ */

#define VSTI_MAX_SERVICES 64

typedef struct {
    uint16_t service_id;
    bool     eit_schedule_flag;
    bool     eit_present_following_flag;
    uint8_t  running_status;  /* 4 = em execucao */
    bool     free_ca_mode;    /* true = servico cifrado */
    uint16_t descriptors_length;
    const uint8_t *descriptors;
} vsti_sdt_service_t;

typedef struct {
    uint16_t transport_stream_id;
    uint16_t original_network_id;
    uint8_t  version;
    size_t   count;
    vsti_sdt_service_t services[VSTI_MAX_SERVICES];
} vsti_sdt_t;

bool vsti_parse_sdt(const uint8_t *sec, size_t len, vsti_sdt_t *out);

/* ------------------------------------------------------------------ */
/* EIT - Event Information Table                                        */
/* ------------------------------------------------------------------ */

#define VSTI_MAX_EVENTS 128

typedef struct {
    uint16_t event_id;
    /* start_time em MJD + BCD, ja convertido para UTC epoch (0 se invalido). */
    int64_t  start_time_utc;
    uint32_t duration_seconds;
    uint8_t  running_status;
    bool     free_ca_mode;
    uint16_t descriptors_length;
    const uint8_t *descriptors;
} vsti_eit_event_t;

typedef struct {
    uint16_t service_id;
    uint16_t transport_stream_id;
    uint16_t original_network_id;
    uint8_t  table_id;
    uint8_t  version;
    size_t   count;
    vsti_eit_event_t events[VSTI_MAX_EVENTS];
} vsti_eit_t;

bool vsti_parse_eit(const uint8_t *sec, size_t len, vsti_eit_t *out);

/*
 * Converte a data/hora de 5 bytes usada por DVB/ARIB (MJD de 16 bits seguido
 * de HH:MM:SS em BCD) para segundos desde a epoch Unix.
 * Retorna 0 se o campo estiver marcado como indefinido (todos os bits em 1).
 */
int64_t vsti_mjd_bcd_to_unix(const uint8_t *p);

/* Converte HH:MM:SS em BCD (3 bytes) para segundos. */
uint32_t vsti_bcd_duration_to_seconds(const uint8_t *p);

#endif /* VSTI_PSI_H */
