

#include <net/icmp.h>
#include <net/ip.h>
#include <net/net.h>
#include <lib/string.h>
#include <types.h>

static volatile bool ping_reply_received = false;

void icmp_init(void) {}

bool icmp_get_ping_reply(void) { return ping_reply_received; }
void icmp_clear_ping_reply(void) { ping_reply_received = false; }

void icmp_send_echo(uint32_t dst_ip, uint16_t id, uint16_t seq) {
    static uint8_t buf[sizeof(icmp_hdr_t) + 32];
    icmp_hdr_t *hdr = (icmp_hdr_t *)buf;
    hdr->type     = ICMP_ECHO_REQUEST;
    hdr->code     = 0;
    hdr->checksum = 0;
    hdr->id       = htons(id);
    hdr->seq      = htons(seq);
    
    uint8_t *data = buf + sizeof(icmp_hdr_t);
    uint32_t i;
    for (i = 0; i < 32; i++) data[i] = (uint8_t)i;
    hdr->checksum = net_checksum(buf, sizeof(buf));
    ip_send(dst_ip, IP_PROTO_ICMP, buf, sizeof(buf));
}

void icmp_recv(const void *pkt, uint16_t len, uint32_t src_ip) {
    if (len < sizeof(icmp_hdr_t)) return;
    const icmp_hdr_t *hdr = (const icmp_hdr_t *)pkt;

    if (hdr->type == ICMP_ECHO_REPLY) {
        ping_reply_received = true;
        return;
    }

    if (hdr->type == ICMP_ECHO_REQUEST) {
        
        static uint8_t reply[1500];
        if (len > sizeof(reply)) return;
        memcpy(reply, pkt, len);
        icmp_hdr_t *rep = (icmp_hdr_t *)reply;
        rep->type     = ICMP_ECHO_REPLY;
        rep->checksum = 0;
        rep->checksum = net_checksum(reply, len);
        ip_send(src_ip, IP_PROTO_ICMP, reply, len);
    }
    
}
