
#ifndef _SOCKET_H
#define _SOCKET_H

#include <types.h>


#define AF_INET    2


#define SOCK_STREAM 1
#define SOCK_DGRAM  2


#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

typedef struct {
    uint16_t family;    
    uint16_t port;      
    uint32_t addr;      
    uint8_t  pad[8];
} sockaddr_in_t;


int socket_create(int domain, int type, int protocol);


int socket_connect(int sockfd, uint32_t addr, uint16_t port);


int socket_send(int sockfd, const void *data, uint16_t len);


int socket_recv(int sockfd, void *buf, uint16_t max_len);


int socket_sendto(int sockfd, const void *data, uint16_t len,
                  uint32_t dst_addr, uint16_t dst_port);


int socket_close(int sockfd);

void socket_init(void);

#endif 
