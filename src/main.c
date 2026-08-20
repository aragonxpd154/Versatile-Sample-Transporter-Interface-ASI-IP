/*
 * src/main.c - Ponto de entrada e despacho de subcomandos.
 *
 * VSTI — Versatile Sample Transporter Interface
 * Encapsulamento de MPEG-2 Transport Stream sobre IP.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#define _POSIX_C_SOURCE 200809L

#include "cli.h"

#include "vsti/log.h"

#include <stdio.h>
#include <string.h>

static void usage(void)
{
    fputs(
"vsti " VSTI_VERSION " — Versatile Sample Transporter Interface\n"
"Encapsulamento de MPEG-2 Transport Stream sobre IP.\n"
"\n"
"Uso: vsti COMANDO [opcoes]\n"
"\n"
"Comandos:\n"
"  stream    Le um arquivo TS e o emite em RTP/UDP ou UDP puro\n"
"  receive   Recebe RTP/UDP ou UDP puro e grava um arquivo TS\n"
"  analyze   Percorre um TS e relata PSI/SI, PIDs, taxa e erros\n"
"  version   Mostra a versao\n"
"  help      Mostra esta ajuda\n"
"\n"
"Use 'vsti COMANDO --help' para as opcoes de cada comando.\n"
"\n"
"Exemplo de ciclo completo em uma maquina so:\n"
"  vsti receive -l 5000 -o volta.ts --timeout 3 &\n"
"  vsti stream  -i original.ts -d 127.0.0.1:5000\n"
"  cmp original.ts volta.ts\n",
        stderr);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage();
        return 2;
    }

    const char *cmd = argv[1];

    /*
     * Repassamos argv a partir do subcomando para que getopt_long veja
     * argv[0] como o nome do comando. Cada subcomando reinicia optind, o que
     * mantem os analisadores de opcoes independentes entre si.
     */
    if (strcmp(cmd, "stream") == 0) {
        return vsti_cmd_stream(argc - 1, argv + 1);
    }
    if (strcmp(cmd, "receive") == 0) {
        return vsti_cmd_receive(argc - 1, argv + 1);
    }
    if (strcmp(cmd, "analyze") == 0) {
        return vsti_cmd_analyze(argc - 1, argv + 1);
    }
    if (strcmp(cmd, "version") == 0 ||
        strcmp(cmd, "--version") == 0 ||
        strcmp(cmd, "-V") == 0) {
        printf("vsti %s\n", VSTI_VERSION);
        return 0;
    }
    if (strcmp(cmd, "help") == 0 ||
        strcmp(cmd, "--help") == 0 ||
        strcmp(cmd, "-h") == 0) {
        usage();
        return 0;
    }

    VSTI_ERR("comando desconhecido: %s", cmd);
    usage();
    return 2;
}
