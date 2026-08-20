/*
 * vsti/net.h - Sockets UDP para transporte de TS sobre IP.
 *
 * Suporta unicast e multicast, IPv4 e IPv6, com os ajustes de socket que
 * importam em rede de contribuicao de video: TTL de multicast, escolha da
 * interface de saida, marcacao DSCP e dimensionamento dos buffers do kernel.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#ifndef VSTI_NET_H
#define VSTI_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

typedef struct {
    int  fd;
    bool is_multicast;
    struct sockaddr_storage peer;
    socklen_t peer_len;
    int  family;
} vsti_sock_t;

typedef struct {
    const char *host;      /* Destino: IP unicast ou grupo multicast */
    const char *port;      /* Porta como string, para getaddrinfo */
    const char *iface;     /* Interface de saida/entrada (nome ou IP), opcional */
    int  ttl;              /* TTL/hop limit; <= 0 usa o padrao do sistema */
    int  dscp;             /* Valor DSCP 0-63; < 0 nao altera */
    int  sndbuf;           /* Bytes; <= 0 nao altera */
    int  rcvbuf;           /* Bytes; <= 0 nao altera */
    bool loopback;         /* Permitir receber o proprio trafego multicast */
    bool reuse;            /* SO_REUSEADDR/SO_REUSEPORT na recepcao */
} vsti_net_opts_t;

void vsti_net_opts_default(vsti_net_opts_t *o);

/*
 * Abre um socket de transmissao.
 *
 * O endereco de destino e resolvido uma unica vez e guardado, e o socket NAO e
 * conectado quando o destino e multicast — em alguns sistemas connect() em
 * grupo multicast interfere com a selecao de interface. Para unicast usamos
 * connect(), o que permite send() em vez de sendto() e evita repetir a copia
 * do endereco a cada datagrama.
 *
 * Retorna 0 em caso de sucesso, ou um valor negativo em erro (mensagem em
 * `errbuf`).
 */
int vsti_sock_open_tx(vsti_sock_t *s, const vsti_net_opts_t *o,
                      char *errbuf, size_t errsz);

/*
 * Abre um socket de recepcao, entrando no grupo multicast se `host` for um
 * endereco de grupo. Se `host` for NULL ou vazio, escuta em todas as
 * interfaces (INADDR_ANY).
 */
int vsti_sock_open_rx(vsti_sock_t *s, const vsti_net_opts_t *o,
                      char *errbuf, size_t errsz);

/* Envia um datagrama. Retorna bytes enviados ou -1. */
ssize_t vsti_sock_send(vsti_sock_t *s, const void *buf, size_t len);

/*
 * Recebe um datagrama, com espera limitada.
 * `timeout_ms` negativo bloqueia indefinidamente.
 * Retorna bytes recebidos, 0 em timeout, ou -1 em erro.
 */
ssize_t vsti_sock_recv(vsti_sock_t *s, void *buf, size_t len, int timeout_ms);

void vsti_sock_close(vsti_sock_t *s);

/* Formata o peer atual como "host:porta" para exibicao. */
void vsti_sock_peer_str(const vsti_sock_t *s, char *out, size_t outsz);

#endif /* VSTI_NET_H */
