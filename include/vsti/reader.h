/*
 * vsti/reader.h - Leitura de transport stream a partir de arquivo ou stdin.
 *
 * Entrega sempre pacotes de 188 bytes alinhados, independentemente de a fonte
 * usar 188, 192 (com timecode), 204 ou 208 bytes (com Reed-Solomon).
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_READER_H
#define VSTI_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "vsti/ts.h"

/*
 * Buffer de leitura. 64 KiB e um bom compromisso: grande o bastante para que a
 * chamada de sistema seja amortizada por ~348 pacotes, pequeno o bastante para
 * caber com folga no cache L2 e nao inflar a latencia de partida.
 */
#define VSTI_READER_BUFSZ 65536

typedef struct {
    FILE   *fp;
    bool    owns_fp;
    bool    loop;            /* Reiniciar do inicio ao chegar no fim */
    bool    eof;

    uint8_t buf[VSTI_READER_BUFSZ];
    size_t  fill;            /* Bytes validos em buf */
    size_t  pos;             /* Proximo byte a consumir */

    size_t  packet_size;     /* Tamanho detectado na fonte */
    size_t  sync_offset;     /* Deslocamento do sync dentro de cada bloco */
    bool    aligned;

    uint64_t packets_read;
    uint64_t bytes_read;
    uint64_t resyncs;        /* Quantas vezes foi preciso reencontrar o sync */
} vsti_reader_t;

/*
 * Abre um arquivo. Se `path` for "-" ou NULL, le de stdin (e `loop` passa a
 * ser ignorado, ja que um pipe nao pode ser rebobinado).
 * Retorna 0 em sucesso.
 */
int vsti_reader_open(vsti_reader_t *r, const char *path, bool loop);

/*
 * Le o proximo pacote de 188 bytes para `out`.
 * Retorna 1 em sucesso, 0 no fim do fluxo, -1 em erro de leitura.
 *
 * Perda de sincronismo no meio do fluxo (comum em captura de ASI com erro) e
 * tratada internamente: o leitor procura o proximo alinhamento valido e
 * contabiliza o evento em `resyncs`, em vez de abortar.
 */
int vsti_reader_next(vsti_reader_t *r, uint8_t out[VSTI_TS_PACKET_SIZE]);

void vsti_reader_close(vsti_reader_t *r);

/* Tamanho total do arquivo, ou 0 se a fonte nao for seekable. */
uint64_t vsti_reader_size(const vsti_reader_t *r);

#endif /* VSTI_READER_H */
