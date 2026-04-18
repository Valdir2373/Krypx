
#ifndef _ARP_H
#define _ARP_H

#include <types.h>

#define ARP_CACHE_SIZE 16



const uint8_t *arp_resolve(uint32_t ip);


void arp_recv(const void *pkt, uint16_t len);


void arp_request(uint32_t target_ip);


void arp_init(void);

#endif 
