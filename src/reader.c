/*
 * src/reader.c - Leitor de transport stream com deteccao de alinhamento.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#define _POSIX_C_SOURCE 200809L

#include "vsti/reader.h"

#include <string.h>
#include <sys/stat.h>

int vsti_reader_open(vsti_reader_t *r, const char *path, bool loop)
{
    memset(r, 0, sizeof(*r));

    if (path == NULL || strcmp(path, "-") == 0) {
        r->fp      = stdin;
        r->owns_fp = false;
        r->loop    = false; /* Um pipe nao volta ao inicio */
    } else {
        r->fp = fopen(path, "rb");
        if (r->fp == NULL) {
            return -1;
        }
        r->owns_fp = true;
        r->loop    = loop;
    }

    r->packet_size = VSTI_TS_PACKET_SIZE;
    r->sync_offset = 0;
    r->aligned     = false;
    return 0;
}

void vsti_reader_close(vsti_reader_t *r)
{
    if (r->fp != NULL && r->owns_fp) {
        fclose(r->fp);
    }
    r->fp = NULL;
}

uint64_t vsti_reader_size(const vsti_reader_t *r)
{
    struct stat st;
    if (r->fp == NULL || fstat(fileno(r->fp), &st) != 0) {
        return 0;
    }
    return S_ISREG(st.st_mode) ? (uint64_t)st.st_size : 0;
}

/*
 * Compacta o buffer e completa com dados novos do arquivo.
 * Retorna quantos bytes estao disponiveis apos o preenchimento.
 */
static size_t refill(vsti_reader_t *r)
{
    if (r->pos > 0) {
        const size_t remain = r->fill - r->pos;
        if (remain > 0) {
            memmove(r->buf, r->buf + r->pos, remain);
        }
        r->fill = remain;
        r->pos  = 0;
    }

    while (r->fill < VSTI_READER_BUFSZ && !r->eof) {
        const size_t n = fread(r->buf + r->fill, 1, VSTI_READER_BUFSZ - r->fill, r->fp);
        if (n == 0) {
            if (r->loop && r->owns_fp) {
                /*
                 * Rebobinar em vez de encerrar. Usado para gerar um fluxo
                 * continuo a partir de uma captura curta, cenario tipico de
                 * bancada de teste.
                 */
                if (fseek(r->fp, 0, SEEK_SET) == 0) {
                    clearerr(r->fp);
                    continue;
                }
            }
            r->eof = true;
            break;
        }
        r->fill += n;
        r->bytes_read += n;
    }

    return r->fill - r->pos;
}

int vsti_reader_next(vsti_reader_t *r, uint8_t out[VSTI_TS_PACKET_SIZE])
{
    for (;;) {
        size_t avail = r->fill - r->pos;

        /*
         * Garantimos folga de um bloco inteiro antes de tentar consumir, para
         * que a deteccao de alinhamento tenha material suficiente com que
         * trabalhar quando precisar reencontrar o sync.
         */
        if (avail < r->packet_size) {
            avail = refill(r);
            if (avail < VSTI_TS_PACKET_SIZE) {
                return 0; /* Fim do fluxo */
            }
        }

        if (!r->aligned) {
            size_t off = 0, psize = 0;
            if (!vsti_ts_find_alignment(r->buf + r->pos, avail, &off, &psize)) {
                /*
                 * Nao ha alinhamento no que temos. Descartamos quase todo o
                 * buffer, preservando uma cauda do tamanho de um bloco para nao
                 * cortar um padrao que estivesse comecando no fim.
                 */
                const size_t keep = (avail > VSTI_TS_PACKET_SIZE_208)
                                  ? VSTI_TS_PACKET_SIZE_208 : avail;
                r->pos = r->fill - keep;
                if (r->eof) {
                    return 0;
                }
                if (refill(r) < VSTI_TS_PACKET_SIZE) {
                    return 0;
                }
                continue;
            }
            r->pos        += off;
            r->packet_size = psize;
            r->aligned     = true;
            avail          = r->fill - r->pos;
            if (avail < VSTI_TS_PACKET_SIZE) {
                if (refill(r) < VSTI_TS_PACKET_SIZE) {
                    return 0;
                }
            }
        }

        /* Neste ponto r->pos deve apontar para um byte de sync. */
        if (r->buf[r->pos] != VSTI_TS_SYNC_BYTE) {
            /*
             * Perdemos o alinhamento no meio do fluxo. Avancamos um byte e
             * pedimos nova deteccao; contabilizar o evento permite ao
             * analisador reportar a qualidade da captura.
             */
            r->aligned = false;
            r->pos++;
            r->resyncs++;
            continue;
        }

        if (r->fill - r->pos < VSTI_TS_PACKET_SIZE) {
            if (refill(r) < VSTI_TS_PACKET_SIZE) {
                return 0;
            }
            continue;
        }

        memcpy(out, r->buf + r->pos, VSTI_TS_PACKET_SIZE);

        /*
         * Avanca um bloco completo da fonte. Nas variantes de 204/208 bytes os
         * bytes extras sao paridade Reed-Solomon, que descartamos; na de 192,
         * o timecode de 4 bytes ja foi pulado pela deteccao de alinhamento e o
         * avanco de packet_size posiciona no timecode seguinte.
         */
        const size_t advance = (r->packet_size > VSTI_TS_PACKET_SIZE)
                             ? r->packet_size : VSTI_TS_PACKET_SIZE;

        if (r->fill - r->pos >= advance) {
            r->pos += advance;
        } else {
            r->pos = r->fill;
        }

        r->packets_read++;
        return 1;
    }
}
