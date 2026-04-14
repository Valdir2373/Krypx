/*
 * net/socket.h — Abstração de sockets (BSD-like)
 * Suporta SOCK_STREAM (TCP) e SOCK_DGRAM (UDP) sobre AF_INET.
 */
#ifndef _SOCKET_H
#define _SOCKET_H

#include <types.h>

/* Domains */
#define AF_INET    2

/* Types */
#define SOCK_STREAM 1
#define SOCK_DGRAM  2

/* Protocolos */
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

typedef struct {
    uint16_t family;    /* AF_INET */
    uint16_t port;      /* big-endian */
    uint32_t addr;      /* IPv4 em ordem de rede */
    uint8_t  pad[8];
} sockaddr_in_t;

/* Cria socket. Retorna file descriptor (>=0) ou -1 em erro. */
int socket_create(int domain, int type, int protocol);

/* Conecta socket TCP a um endereço remoto. */
int socket_connect(int sockfd, uint32_t addr, uint16_t port);

/* Envia dados. Retorna bytes enviados ou -1. */
int socket_send(int sockfd, const void *data, uint16_t len);

/* Recebe dados. Retorna bytes recebidos ou -1/0. */
int socket_recv(int sockfd, void *buf, uint16_t max_len);

/* Envia datagrama UDP para destino específico. */
int socket_sendto(int sockfd, const void *data, uint16_t len,
                  uint32_t dst_addr, uint16_t dst_port);

/* Fecha socket. */
int socket_close(int sockfd);

void socket_init(void);

#endif /* _SOCKET_H */
