/*
 * net/icmp.h — Internet Control Message Protocol (ping)
 */
#ifndef _ICMP_H
#define _ICMP_H

#include <types.h>

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_hdr_t;

/* Envia um ICMP echo request (ping) */
void icmp_send_echo(uint32_t dst_ip, uint16_t id, uint16_t seq);

/* Handler chamado por ip_recv */
void icmp_recv(const void *pkt, uint16_t len, uint32_t src_ip);

void icmp_init(void);

#endif /* _ICMP_H */
