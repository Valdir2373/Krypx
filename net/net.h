/*
 * net/net.h — Definições comuns para a pilha TCP/IP do Krypx
 */
#ifndef _NET_H
#define _NET_H

#include <types.h>

/* Converte entre big-endian (rede) e little-endian (x86) */
static inline uint16_t htons(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}
static inline uint16_t ntohs(uint16_t x) { return htons(x); }
static inline uint32_t htonl(uint32_t x) {
    return ((x >> 24) & 0xFF)
         | (((x >> 16) & 0xFF) << 8)
         | (((x >>  8) & 0xFF) << 16)
         | ((x & 0xFF) << 24);
}
static inline uint32_t ntohl(uint32_t x) { return htonl(x); }

/* Endereço IPv4 como uint32_t em ordem de rede */
#define IP4(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b)<<8) | ((uint32_t)(c)<<16) | ((uint32_t)(d)<<24))
#define IP_BROADCAST  IP4(255,255,255,255)

/* Copia endereço MAC */
#define MAC_COPY(dst, src) do { \
    (dst)[0]=(src)[0]; (dst)[1]=(src)[1]; (dst)[2]=(src)[2]; \
    (dst)[3]=(src)[3]; (dst)[4]=(src)[4]; (dst)[5]=(src)[5]; \
} while(0)
#define MAC_BCAST "\xFF\xFF\xFF\xFF\xFF\xFF"

/* Checksum IP/TCP/UDP (complemento de um dos bytes em big-endian) */
uint16_t net_checksum(const void *data, uint32_t len);

/* Estado de rede global */
extern uint8_t  net_mac[6];      /* MAC da NIC */
extern uint32_t net_ip;          /* IP local (em ordem de rede) */
extern uint32_t net_gateway;     /* Gateway (em ordem de rede) */
extern uint32_t net_mask;        /* Máscara de rede */
extern uint32_t net_dns;         /* Servidor DNS */

bool net_is_configured(void);

#endif /* _NET_H */
