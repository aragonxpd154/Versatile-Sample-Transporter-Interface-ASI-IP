/*
 * tests/test_crc32.c - CRC-32/MPEG-2.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#include "test_util.h"
#include "vsti/psi.h"

int main(void)
{
    /*
     * Vetor canonico do CRC-32/MPEG-2: a string "123456789" produz 0x0376E6E7.
     * E o mesmo valor publicado no catalogo de CRCs de Greg Cook, o que amarra
     * nossa implementacao a uma referencia independente.
     */
    const unsigned char check[] = "123456789";
    T_EQ_U(vsti_crc32_mpeg2(check, 9), 0x0376E6E7u, "vetor \"123456789\"");

    /* Buffer vazio deve devolver o valor inicial intacto. */
    T_EQ_U(vsti_crc32_mpeg2((const unsigned char *)"", 0), 0xFFFFFFFFu, "vazio");

    /*
     * Propriedade que o remontador de secoes usa: acrescentar o CRC ao fim dos
     * dados faz o CRC do conjunto resultar em zero. Montamos uma secao PAT
     * minima e verificamos que ela se autovalida.
     */
    unsigned char sec[] = {
        0x00,             /* table_id = PAT */
        0xB0, 0x0D,       /* syntax=1, section_length = 13 */
        0x00, 0x01,       /* transport_stream_id = 1 */
        0xC1,             /* version 0, current_next = 1 */
        0x00, 0x00,       /* section_number, last_section_number */
        0x00, 0x01,       /* program_number = 1 */
        0xEF, 0xC8,       /* PMT PID = 0x1FC8 */
        0x00, 0x00, 0x00, 0x00 /* espaco para o CRC */
    };
    const size_t body = sizeof(sec) - 4;

    const uint32_t crc = vsti_crc32_mpeg2(sec, body);
    sec[body + 0] = (unsigned char)(crc >> 24);
    sec[body + 1] = (unsigned char)(crc >> 16);
    sec[body + 2] = (unsigned char)(crc >> 8);
    sec[body + 3] = (unsigned char)(crc);

    T_EQ_U(vsti_crc32_mpeg2(sec, sizeof(sec)), 0u,
           "secao com CRC anexado valida para zero");

    /* Um unico bit trocado deve quebrar a verificacao. */
    sec[5] ^= 0x02;
    T_CHECK(vsti_crc32_mpeg2(sec, sizeof(sec)) != 0u,
            "bit corrompido deve invalidar o CRC");

    return t_report("crc32");
}
