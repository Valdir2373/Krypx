/*
 * net/arp.h — Address Resolution Protocol (IPv4 ↔ MAC)
 */
#ifndef _ARP_H
#define _ARP_H

#include <types.h>

#define ARP_CACHE_SIZE 16

/* Resolve IP para MAC. Envia ARP request e aguarda reply (com timeout). */
/* Retorna ponteiro para MAC (6 bytes) no cache, ou NULL se timeout. */
const uint8_t *arp_resolve(uint32_t ip);

/* Processa um pacote ARP recebido */
void arp_recv(const void *pkt, uint16_t len);

/* Envia ARP request para o IP alvo */
void arp_request(uint32_t target_ip);

/* Inicializa cache ARP */
void arp_init(void);

#endif /* _ARP_H */
