/*
 * net/icmp.c — ICMP: echo request/reply (ping)
 * Responde automaticamente a echo requests recebidos.
 * Permite enviar pings para IPs remotos.
 */

#include <net/icmp.h>
#include <net/ip.h>
#include <net/net.h>
#include <lib/string.h>
#include <types.h>

void icmp_init(void) {}

void icmp_send_echo(uint32_t dst_ip, uint16_t id, uint16_t seq) {
    static uint8_t buf[sizeof(icmp_hdr_t) + 32];
    icmp_hdr_t *hdr = (icmp_hdr_t *)buf;
    hdr->type     = ICMP_ECHO_REQUEST;
    hdr->code     = 0;
    hdr->checksum = 0;
    hdr->id       = htons(id);
    hdr->seq      = htons(seq);
    /* Payload de 32 bytes com padrão */
    uint8_t *data = buf + sizeof(icmp_hdr_t);
    uint32_t i;
    for (i = 0; i < 32; i++) data[i] = (uint8_t)i;
    hdr->checksum = net_checksum(buf, sizeof(buf));
    ip_send(dst_ip, IP_PROTO_ICMP, buf, sizeof(buf));
}

void icmp_recv(const void *pkt, uint16_t len, uint32_t src_ip) {
    if (len < sizeof(icmp_hdr_t)) return;
    const icmp_hdr_t *hdr = (const icmp_hdr_t *)pkt;

    if (hdr->type == ICMP_ECHO_REQUEST) {
        /* Responder com echo reply: copiar payload e trocar tipo */
        static uint8_t reply[1500];
        if (len > sizeof(reply)) return;
        memcpy(reply, pkt, len);
        icmp_hdr_t *rep = (icmp_hdr_t *)reply;
        rep->type     = ICMP_ECHO_REPLY;
        rep->checksum = 0;
        rep->checksum = net_checksum(reply, len);
        ip_send(src_ip, IP_PROTO_ICMP, reply, len);
    }
    /* Ignorar outros tipos (echo reply, unreachable, etc.) */
}
