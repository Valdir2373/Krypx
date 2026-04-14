/*
 * net/udp.c — User Datagram Protocol
 * Encapsula dados em segmentos UDP e os passa para a camada IP.
 * Permite registrar callbacks para portas específicas (ex: DHCP porta 68).
 */

#include <net/udp.h>
#include <net/ip.h>
#include <net/net.h>
#include <lib/string.h>
#include <types.h>

#define UDP_PORT_TABLE_SIZE 16

typedef struct {
    uint16_t       port;
    udp_recv_cb_t  cb;
    bool           used;
} udp_port_entry_t;

static udp_port_entry_t port_table[UDP_PORT_TABLE_SIZE];

void udp_init(void) {
    memset(port_table, 0, sizeof(port_table));
}

void udp_register_port(uint16_t port, udp_recv_cb_t cb) {
    uint32_t i;
    for (i = 0; i < UDP_PORT_TABLE_SIZE; i++) {
        if (!port_table[i].used) {
            port_table[i].port = port;
            port_table[i].cb   = cb;
            port_table[i].used = true;
            return;
        }
    }
}

bool udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void *data, uint16_t len)
{
    static uint8_t buf[1500];
    uint16_t total = (uint16_t)(UDP_HDR_LEN + len);
    if (total > sizeof(buf)) return false;

    udp_hdr_t *hdr = (udp_hdr_t *)buf;
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length   = htons(total);
    hdr->checksum = 0;   /* Checksum opcional para IPv4 UDP */

    memcpy(buf + UDP_HDR_LEN, data, len);
    return ip_send(dst_ip, IP_PROTO_UDP, buf, total);
}

void udp_recv(const void *pkt, uint16_t len, uint32_t src_ip) {
    if (len < UDP_HDR_LEN) return;
    const udp_hdr_t *hdr = (const udp_hdr_t *)pkt;
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint16_t src_port = ntohs(hdr->src_port);
    const uint8_t *data = (const uint8_t *)pkt + UDP_HDR_LEN;
    uint16_t data_len = (uint16_t)(ntohs(hdr->length) - UDP_HDR_LEN);
    if (data_len > len - UDP_HDR_LEN)
        data_len = (uint16_t)(len - UDP_HDR_LEN);

    uint32_t i;
    for (i = 0; i < UDP_PORT_TABLE_SIZE; i++) {
        if (port_table[i].used && port_table[i].port == dst_port) {
            port_table[i].cb(src_ip, src_port, data, data_len);
            return;
        }
    }
}
