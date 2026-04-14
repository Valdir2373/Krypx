/*
 * net/socket.c — Implementação de sockets BSD-like
 * Mapeia operações de socket para TCP ou UDP conforme o tipo.
 */

#include <net/socket.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/net.h>
#include <lib/string.h>
#include <types.h>

#define MAX_SOCKETS 16

typedef enum {
    SOCK_FREE = 0,
    SOCK_TCP,
    SOCK_UDP
} sock_type_internal_t;

typedef struct {
    sock_type_internal_t type;
    int tcp_conn;          /* ID de conexão TCP, -1 se não conectado */
    uint16_t udp_port;     /* Porta local UDP */
    bool used;
} sock_entry_t;

static sock_entry_t sockets[MAX_SOCKETS];
static uint16_t udp_ephemeral = 50000;

void socket_init(void) {
    memset(sockets, 0, sizeof(sockets));
    udp_ephemeral = 50000;
}

int socket_create(int domain, int type, int protocol) {
    (void)protocol;
    if (domain != AF_INET) return -1;

    uint32_t i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].used) {
            sockets[i].used     = true;
            sockets[i].tcp_conn = -1;
            if (type == SOCK_STREAM) {
                sockets[i].type = SOCK_TCP;
            } else if (type == SOCK_DGRAM) {
                sockets[i].type     = SOCK_UDP;
                sockets[i].udp_port = udp_ephemeral++;
            } else {
                sockets[i].used = false;
                return -1;
            }
            return (int)i;
        }
    }
    return -1;
}

int socket_connect(int sockfd, uint32_t addr, uint16_t port) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    sock_entry_t *s = &sockets[sockfd];
    if (!s->used || s->type != SOCK_TCP) return -1;

    s->tcp_conn = tcp_connect(addr, port);
    return (s->tcp_conn >= 0) ? 0 : -1;
}

int socket_send(int sockfd, const void *data, uint16_t len) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    sock_entry_t *s = &sockets[sockfd];
    if (!s->used) return -1;

    if (s->type == SOCK_TCP) {
        return tcp_send(s->tcp_conn, data, len);
    }
    return -1;
}

int socket_sendto(int sockfd, const void *data, uint16_t len,
                  uint32_t dst_addr, uint16_t dst_port)
{
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    sock_entry_t *s = &sockets[sockfd];
    if (!s->used || s->type != SOCK_UDP) return -1;

    bool ok = udp_send(dst_addr, s->udp_port, dst_port, data, len);
    return ok ? (int)len : -1;
}

int socket_recv(int sockfd, void *buf, uint16_t max_len) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    sock_entry_t *s = &sockets[sockfd];
    if (!s->used) return -1;

    if (s->type == SOCK_TCP) {
        return tcp_recv(s->tcp_conn, buf, max_len);
    }
    return -1;
}

int socket_close(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    sock_entry_t *s = &sockets[sockfd];
    if (!s->used) return -1;

    if (s->type == SOCK_TCP && s->tcp_conn >= 0) {
        tcp_close(s->tcp_conn);
    }
    memset(s, 0, sizeof(*s));
    return 0;
}
