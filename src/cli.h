/*
 * src/cli.h - Utilitarios compartilhados entre os subcomandos.
 *
 * Cabecalho interno: nao faz parte da API publica da biblioteca.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_CLI_H
#define VSTI_CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>

#define VSTI_VERSION "1.0.0"

/*
 * Sinalizador de parada acionado por SIGINT/SIGTERM.
 *
 * sig_atomic_t volatil e o unico tipo que a norma C garante ser seguro para
 * escrever dentro de um handler e ler no fluxo principal.
 */
extern volatile sig_atomic_t g_vsti_stop;

/* Instala os handlers de SIGINT e SIGTERM. */
void vsti_install_signal_handlers(void);

/*
 * Separa uma string "host:porta" em partes.
 *
 * Aceita as formas:
 *   239.1.2.3:1234      IPv4 ou nome
 *   [ff02::1]:1234      IPv6 entre colchetes
 *   1234                somente a porta (host fica vazio)
 *
 * Retorna true em sucesso.
 */
bool vsti_parse_endpoint(const char *in,
                         char *host, size_t host_sz,
                         char *port, size_t port_sz);

/*
 * Interpreta um valor de taxa aceitando sufixos: "17274150", "17.27M",
 * "2500k", "1G". Sufixos sao potencias de 1000, como e convencao em
 * telecomunicacoes (e nao 1024, como em armazenamento).
 * Retorna 0 se a string for invalida.
 */
uint64_t vsti_parse_bitrate(const char *s);

/* Formata uma taxa em bits/s de forma legivel ("17,27 Mbit/s"). */
void vsti_fmt_bitrate(uint64_t bps, char *out, size_t outsz);

/* Formata uma quantidade de bytes ("14,1 MiB"). */
void vsti_fmt_bytes(uint64_t bytes, char *out, size_t outsz);

/* Formata uma duracao em segundos ("01:23:45"). */
void vsti_fmt_duration(double seconds, char *out, size_t outsz);

/*
 * Escapa uma string para uso em JSON, escrevendo entre aspas.
 * Trata aspas, barra invertida, caracteres de controle e mantem UTF-8 valido
 * como esta.
 */
void vsti_json_escape(const char *in, char *out, size_t outsz);

/* Pontos de entrada dos subcomandos. */
int vsti_cmd_stream(int argc, char **argv);
int vsti_cmd_receive(int argc, char **argv);
int vsti_cmd_analyze(int argc, char **argv);

#endif /* VSTI_CLI_H */
