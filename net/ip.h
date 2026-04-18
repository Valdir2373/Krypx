
#ifndef _IP_H
#define _IP_H

#include <types.h>
#include <net/net.h>

#define IP_HDR_LEN  20
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

typedef struct {
    uint8_t  ver_ihl;      
    uint8_t  tos;
    uint16_t total_len;    
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src;          
    uint32_t dst;
} __attribute__((packed)) ip_hdr_t;


bool ip_send(uint32_t dst_ip, uint8_t protocol,
             const void *payload, uint16_t plen);


void ip_recv(const void *pkt, uint16_t len);

void ip_init(void);

#endif 
