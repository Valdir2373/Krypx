

#include <net/dns.h>
#include <net/udp.h>
#include <net/net.h>
#include <net/ethernet.h>
#include <drivers/e1000.h>
#include <lib/string.h>
#include <types.h>

#define DNS_PORT      53
#define DNS_CLIENT_PORT 1053


typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_hdr_t;

static uint32_t dns_resolved_ip = 0;
static bool     dns_got_reply   = false;
static uint16_t dns_query_id    = 0;


static uint16_t dns_encode_name(const char *name, uint8_t *out) {
    uint16_t pos = 0;
    while (*name) {
        const char *dot = name;
        while (*dot && *dot != '.') dot++;
        uint8_t len = (uint8_t)(dot - name);
        out[pos++] = len;
        uint8_t i;
        for (i = 0; i < len; i++) out[pos++] = (uint8_t)name[i];
        name = dot;
        if (*name == '.') name++;
    }
    out[pos++] = 0;
    return pos;
}


static void dns_recv_cb(uint32_t src_ip, uint16_t src_port,
                         const void *data, uint16_t len)
{
    (void)src_ip; (void)src_port;
    if (len < sizeof(dns_hdr_t)) return;
    const dns_hdr_t *hdr = (const dns_hdr_t *)data;
    if (ntohs(hdr->id) != dns_query_id) return;
    if (ntohs(hdr->ancount) == 0) return;

    
    const uint8_t *p = (const uint8_t *)data + sizeof(dns_hdr_t);
    const uint8_t *end = (const uint8_t *)data + len;
    uint16_t qdcount = ntohs(hdr->qdcount);
    uint16_t q;
    for (q = 0; q < qdcount; q++) {
        
        while (p < end && *p != 0) {
            if ((*p & 0xC0) == 0xC0) { p += 2; goto next_q; }
            p += *p + 1;
        }
        p++;  
next_q:
        p += 4;  
    }

    
    uint16_t ancount = ntohs(hdr->ancount);
    uint16_t a;
    for (a = 0; a < ancount && p < end; a++) {
        
        if ((*p & 0xC0) == 0xC0) p += 2;
        else {
            while (p < end && *p != 0) p += *p + 1;
            p++;
        }
        if (p + 10 > end) break;
        uint16_t rtype  = (uint16_t)((p[0] << 8) | p[1]);
        
        uint16_t rdlen  = (uint16_t)((p[8] << 8) | p[9]);
        p += 10;
        if (rtype == 1 && rdlen == 4 && p + 4 <= end) {
            
            dns_resolved_ip = *(uint32_t *)p;
            dns_got_reply   = true;
            return;
        }
        p += rdlen;
    }
}


void dns_init(void) {
    dns_resolved_ip = 0;
    dns_got_reply   = false;
    dns_query_id    = 0;
    udp_register_port(DNS_CLIENT_PORT, dns_recv_cb);
}

uint32_t dns_resolve(const char *hostname) {
    if (!net_dns) return 0;

    static uint8_t buf[512];
    dns_hdr_t *hdr = (dns_hdr_t *)buf;
    dns_query_id++;
    hdr->id      = htons(dns_query_id);
    hdr->flags   = htons(0x0100);  
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    uint8_t *p = buf + sizeof(dns_hdr_t);
    p += dns_encode_name(hostname, p);
    p[0] = 0; p[1] = 1;  
    p[2] = 0; p[3] = 1;  
    p += 4;

    uint16_t total = (uint16_t)(p - buf);

    dns_resolved_ip = 0;
    dns_got_reply   = false;

    udp_send(net_dns, DNS_CLIENT_PORT, DNS_PORT, buf, total);

    uint32_t timeout = 30000000;
    while (!dns_got_reply && timeout--) {
        uint8_t tmp[2048];
        uint16_t rlen = e1000_recv(tmp, sizeof(tmp));
        if (rlen > 0) eth_recv(tmp, rlen);
        __asm__ volatile("nop");
    }
    return dns_resolved_ip;
}
