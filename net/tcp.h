
#ifndef _TCP_H
#define _TCP_H

#include <types.h>

#define TCP_HDR_LEN 20


#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10
#define TCP_URG  0x20

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;  
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_hdr_t;


typedef enum {
    TCP_CLOSED = 0,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
} tcp_state_t;

#define TCP_MAX_CONNS    8
#define TCP_RECV_BUF_SZ  4096

typedef struct tcp_conn {
    bool        used;
    tcp_state_t state;
    uint32_t    remote_ip;
    uint16_t    remote_port;
    uint16_t    local_port;
    uint32_t    seq;           
    uint32_t    ack;           
    uint8_t     recv_buf[TCP_RECV_BUF_SZ];
    uint16_t    recv_head;
    uint16_t    recv_tail;
    uint16_t    recv_len;
} tcp_conn_t;


int tcp_connect(uint32_t dst_ip, uint16_t dst_port);


int tcp_send(int conn_id, const void *data, uint16_t len);


int tcp_recv(int conn_id, void *buf, uint16_t max_len);


void tcp_close(int conn_id);


void tcp_recv_pkt(const void *pkt, uint16_t len, uint32_t src_ip);

void tcp_init(void);

#endif 
