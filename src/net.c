/*
 * src/net.c - Implementacao dos sockets UDP.
 *
 * Copyright (c) 2026 Marcos Silva. MIT License.
 */
#define _GNU_SOURCE

#include "vsti/net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void set_err(char *buf, size_t sz, const char *fmt, ...)
{
    if (buf == NULL || sz == 0) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sz, fmt, ap);
    va_end(ap);
}

void vsti_net_opts_default(vsti_net_opts_t *o)
{
    memset(o, 0, sizeof(*o));
    o->ttl      = 8;   /* Suficiente para atravessar alguns roteadores sem vazar */
    o->dscp     = -1;
    o->sndbuf   = 0;
    o->rcvbuf   = 0;
    o->loopback = false;
    o->reuse    = true;
}

/* Detecta se um endereco resolvido pertence a faixa multicast. */
static bool addr_is_multicast(const struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)sa;
        return IN_MULTICAST(ntohl(in->sin_addr.s_addr));
    }
    if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)sa;
        return IN6_IS_ADDR_MULTICAST(&in6->sin6_addr);
    }
    return false;
}

/*
 * Resolve o nome da interface para um indice e, no caso IPv4, tambem para o
 * endereco local correspondente.
 *
 * Aceitamos tanto o nome ("eth0") quanto o IP local ("192.168.0.10") porque em
 * servidor de broadcast as duas convencoes aparecem: scripts antigos usam IP,
 * ferramentas modernas usam nome.
 */
static bool resolve_iface(const char *iface, unsigned *out_index,
                          struct in_addr *out_addr)
{
    if (iface == NULL || *iface == '\0') {
        return false;
    }

    struct in_addr a;
    if (inet_pton(AF_INET, iface, &a) == 1) {
        if (out_addr)  *out_addr = a;
        if (out_index) *out_index = 0;
        return true;
    }

    const unsigned idx = if_nametoindex(iface);
    if (idx == 0) {
        return false;
    }
    if (out_index) *out_index = idx;
    if (out_addr)  out_addr->s_addr = htonl(INADDR_ANY);
    return true;
}

static int apply_common_opts(int fd, int family, const vsti_net_opts_t *o,
                             char *errbuf, size_t errsz)
{
    if (o->dscp >= 0) {
        /*
         * DSCP ocupa os 6 bits mais significativos do byte de ToS/Traffic
         * Class, por isso o deslocamento de 2. Video de contribuicao costuma
         * usar AF41 (34) ou CS4 (32).
         */
        const int tos = (o->dscp & 0x3F) << 2;
        if (family == AF_INET) {
            if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
                set_err(errbuf, errsz, "IP_TOS: %s", strerror(errno));
                return -1;
            }
        } else {
            if (setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &tos, sizeof(tos)) < 0) {
                set_err(errbuf, errsz, "IPV6_TCLASS: %s", strerror(errno));
                return -1;
            }
        }
    }

    if (o->sndbuf > 0) {
        /*
         * Falha aqui nao e fatal: o kernel pode recusar valores acima de
         * net.core.wmem_max e ainda assim entregar um buffer utilizavel.
         */
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &o->sndbuf, sizeof(o->sndbuf));
    }
    if (o->rcvbuf > 0) {
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &o->rcvbuf, sizeof(o->rcvbuf));
    }
    return 0;
}

int vsti_sock_open_tx(vsti_sock_t *s, const vsti_net_opts_t *o,
                      char *errbuf, size_t errsz)
{
    struct addrinfo hints, *res = NULL;

    memset(s, 0, sizeof(*s));
    s->fd = -1;

    if (o->host == NULL || o->port == NULL) {
        set_err(errbuf, errsz, "destino nao informado");
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    const int gai = getaddrinfo(o->host, o->port, &hints, &res);
    if (gai != 0 || res == NULL) {
        set_err(errbuf, errsz, "getaddrinfo(%s:%s): %s",
                o->host, o->port, gai_strerror(gai));
        return -1;
    }

    const int fd = socket(res->ai_family, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        set_err(errbuf, errsz, "socket: %s", strerror(errno));
        freeaddrinfo(res);
        return -1;
    }

    s->family       = res->ai_family;
    s->is_multicast = addr_is_multicast(res->ai_addr);
    memcpy(&s->peer, res->ai_addr, res->ai_addrlen);
    s->peer_len = res->ai_addrlen;

    if (apply_common_opts(fd, s->family, o, errbuf, errsz) < 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    if (s->is_multicast) {
        const int ttl = (o->ttl > 0) ? o->ttl : 8;
        const int lb  = o->loopback ? 1 : 0;

        if (s->family == AF_INET) {
            const unsigned char ttl8 = (unsigned char)ttl;
            const unsigned char lb8  = (unsigned char)lb;
            if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl8, sizeof(ttl8)) < 0) {
                set_err(errbuf, errsz, "IP_MULTICAST_TTL: %s", strerror(errno));
                close(fd); freeaddrinfo(res); return -1;
            }
            (void)setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &lb8, sizeof(lb8));

            struct in_addr ifaddr;
            unsigned ifidx = 0;
            if (resolve_iface(o->iface, &ifidx, &ifaddr)) {
                struct ip_mreqn mreq;
                memset(&mreq, 0, sizeof(mreq));
                mreq.imr_address      = ifaddr;
                mreq.imr_ifindex      = (int)ifidx;
                if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0) {
                    set_err(errbuf, errsz, "IP_MULTICAST_IF(%s): %s",
                            o->iface, strerror(errno));
                    close(fd); freeaddrinfo(res); return -1;
                }
            }
        } else {
            if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl)) < 0) {
                set_err(errbuf, errsz, "IPV6_MULTICAST_HOPS: %s", strerror(errno));
                close(fd); freeaddrinfo(res); return -1;
            }
            (void)setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &lb, sizeof(lb));

            unsigned ifidx = 0;
            if (resolve_iface(o->iface, &ifidx, NULL) && ifidx != 0) {
                if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                               &ifidx, sizeof(ifidx)) < 0) {
                    set_err(errbuf, errsz, "IPV6_MULTICAST_IF(%s): %s",
                            o->iface, strerror(errno));
                    close(fd); freeaddrinfo(res); return -1;
                }
            }
        }
    } else {
        /*
         * Em unicast conectamos o socket: alem de simplificar o envio, isso faz
         * o kernel reportar ICMP port-unreachable como erro em send(), o que da
         * um diagnostico util quando o receptor nao esta no ar.
         */
        if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
            set_err(errbuf, errsz, "connect: %s", strerror(errno));
            close(fd); freeaddrinfo(res); return -1;
        }
        if (o->ttl > 0) {
            if (s->family == AF_INET) {
                (void)setsockopt(fd, IPPROTO_IP, IP_TTL, &o->ttl, sizeof(o->ttl));
            } else {
                (void)setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
                                 &o->ttl, sizeof(o->ttl));
            }
        }
    }

    freeaddrinfo(res);
    s->fd = fd;
    return 0;
}

int vsti_sock_open_rx(vsti_sock_t *s, const vsti_net_opts_t *o,
                      char *errbuf, size_t errsz)
{
    struct addrinfo hints, *res = NULL;
    const bool have_host = (o->host != NULL && o->host[0] != '\0');

    memset(s, 0, sizeof(*s));
    s->fd = -1;

    if (o->port == NULL) {
        set_err(errbuf, errsz, "porta nao informada");
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags    = have_host ? 0 : AI_PASSIVE;

    const int gai = getaddrinfo(have_host ? o->host : NULL, o->port, &hints, &res);
    if (gai != 0 || res == NULL) {
        set_err(errbuf, errsz, "getaddrinfo(%s:%s): %s",
                have_host ? o->host : "*", o->port, gai_strerror(gai));
        return -1;
    }

    const int fd = socket(res->ai_family, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        set_err(errbuf, errsz, "socket: %s", strerror(errno));
        freeaddrinfo(res);
        return -1;
    }

    s->family       = res->ai_family;
    s->is_multicast = have_host && addr_is_multicast(res->ai_addr);

    if (o->reuse) {
        const int one = 1;
        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#ifdef SO_REUSEPORT
        /*
         * SO_REUSEPORT permite que varios analisadores escutem o mesmo grupo
         * simultaneamente, cenario comum em sala de monitoramento.
         */
        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
#endif
    }

    if (apply_common_opts(fd, s->family, o, errbuf, errsz) < 0) {
        close(fd); freeaddrinfo(res); return -1;
    }

    /*
     * Ao entrar em um grupo multicast fazemos bind no proprio endereco do
     * grupo, e nao em INADDR_ANY. Isso filtra no kernel o trafego de outros
     * grupos que compartilhem a mesma porta — sem isso, um analisador ligado
     * na porta 1234 receberia todos os fluxos da rede que usam essa porta.
     */
    struct sockaddr_storage bind_addr;
    memcpy(&bind_addr, res->ai_addr, res->ai_addrlen);
    const socklen_t bind_len = res->ai_addrlen;

    if (bind(fd, (struct sockaddr *)&bind_addr, bind_len) < 0) {
        set_err(errbuf, errsz, "bind: %s", strerror(errno));
        close(fd); freeaddrinfo(res); return -1;
    }

    if (s->is_multicast) {
        if (s->family == AF_INET) {
            struct ip_mreqn mreq;
            struct in_addr ifaddr;
            unsigned ifidx = 0;

            memset(&mreq, 0, sizeof(mreq));
            mreq.imr_multiaddr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
            if (resolve_iface(o->iface, &ifidx, &ifaddr)) {
                mreq.imr_address = ifaddr;
                mreq.imr_ifindex = (int)ifidx;
            } else {
                mreq.imr_address.s_addr = htonl(INADDR_ANY);
                mreq.imr_ifindex        = 0;
            }
            if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                           &mreq, sizeof(mreq)) < 0) {
                set_err(errbuf, errsz, "IP_ADD_MEMBERSHIP: %s", strerror(errno));
                close(fd); freeaddrinfo(res); return -1;
            }
        } else {
            struct ipv6_mreq mreq6;
            unsigned ifidx = 0;
            memset(&mreq6, 0, sizeof(mreq6));
            mreq6.ipv6mr_multiaddr =
                ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
            (void)resolve_iface(o->iface, &ifidx, NULL);
            mreq6.ipv6mr_interface = ifidx;
            if (setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                           &mreq6, sizeof(mreq6)) < 0) {
                set_err(errbuf, errsz, "IPV6_ADD_MEMBERSHIP: %s", strerror(errno));
                close(fd); freeaddrinfo(res); return -1;
            }
        }
    }

    freeaddrinfo(res);
    s->fd = fd;
    return 0;
}

ssize_t vsti_sock_send(vsti_sock_t *s, const void *buf, size_t len)
{
    if (s == NULL || s->fd < 0) {
        return -1;
    }
    if (s->is_multicast) {
        return sendto(s->fd, buf, len, 0,
                      (const struct sockaddr *)&s->peer, s->peer_len);
    }
    return send(s->fd, buf, len, 0);
}

ssize_t vsti_sock_recv(vsti_sock_t *s, void *buf, size_t len, int timeout_ms)
{
    if (s == NULL || s->fd < 0) {
        return -1;
    }

    if (timeout_ms >= 0) {
        struct pollfd pfd = { .fd = s->fd, .events = POLLIN, .revents = 0 };
        const int r = poll(&pfd, 1, timeout_ms);
        if (r == 0) {
            return 0; /* Timeout */
        }
        if (r < 0) {
            return (errno == EINTR) ? 0 : -1;
        }
    }

    const ssize_t n = recv(s->fd, buf, len, 0);
    if (n < 0 && errno == EINTR) {
        return 0;
    }
    return n;
}

void vsti_sock_close(vsti_sock_t *s)
{
    if (s != NULL && s->fd >= 0) {
        close(s->fd);
        s->fd = -1;
    }
}

void vsti_sock_peer_str(const vsti_sock_t *s, char *out, size_t outsz)
{
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];

    if (out == NULL || outsz == 0) {
        return;
    }
    if (s == NULL || s->peer_len == 0) {
        snprintf(out, outsz, "(nao conectado)");
        return;
    }

    if (getnameinfo((const struct sockaddr *)&s->peer, s->peer_len,
                    host, sizeof(host), serv, sizeof(serv),
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        snprintf(out, outsz, "(desconhecido)");
        return;
    }

    if (s->family == AF_INET6) {
        snprintf(out, outsz, "[%s]:%s", host, serv);
    } else {
        snprintf(out, outsz, "%s:%s", host, serv);
    }
}
