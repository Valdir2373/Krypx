
#ifndef _UDP_H
#define _UDP_H

#include <types.h>

#define UDP_HDR_LEN 8

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;    
    uint16_t checksum;
} __attribute__((packed)) udp_hdr_t;


bool udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void *data, uint16_t len);


void udp_recv(const void *pkt, uint16_t len, uint32_t src_ip);


typedef void (*udp_recv_cb_t)(uint32_t src_ip, uint16_t src_port,
                               const void *data, uint16_t len);
void udp_register_port(uint16_t port, udp_recv_cb_t cb);

void udp_init(void);

#endif 
