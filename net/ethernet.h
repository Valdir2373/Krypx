/*
 * net/ethernet.h — Frame Ethernet II
 */
#ifndef _ETHERNET_H
#define _ETHERNET_H

#include <types.h>
#include <net/net.h>

#define ETH_HDR_LEN   14
#define ETH_MAX_FRAME 1514

#define ETHERTYPE_IP  0x0800
#define ETHERTYPE_ARP 0x0806

typedef struct {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;   /* big-endian */
} __attribute__((packed)) eth_frame_t;

/* Envia um frame Ethernet */
bool eth_send(const uint8_t *dst_mac, uint16_t ethertype,
              const void *payload, uint16_t plen);

/* Processa frame recebido (chamado pelo e1000 callback) */
void eth_recv(const void *frame, uint16_t len);

void eth_init(void);

#endif /* _ETHERNET_H */
