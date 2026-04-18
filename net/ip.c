

#include <net/ip.h>
#include <net/net.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <lib/string.h>
#include <types.h>


void icmp_recv(const void *pkt, uint16_t len, uint32_t src_ip);
void udp_recv(const void *pkt, uint16_t len, uint32_t src_ip);
void tcp_recv_pkt(const void *pkt, uint16_t len, uint32_t src_ip);

static uint16_t ip_id_counter = 1;

void ip_init(void) {
    ip_id_counter = 1;
}


bool ip_send(uint32_t dst_ip, uint8_t protocol,
             const void *payload, uint16_t plen)
{
    
    static uint8_t ip_buf[1500];

    uint16_t total = (uint16_t)(IP_HDR_LEN + plen);
    if (total > sizeof(ip_buf)) return false;

    ip_hdr_t *hdr = (ip_hdr_t *)ip_buf;
    hdr->ver_ihl    = 0x45;   
    hdr->tos        = 0;
    hdr->total_len  = htons(total);
    hdr->id         = htons(ip_id_counter++);
    hdr->flags_frag = 0;
    hdr->ttl        = 64;
    hdr->protocol   = protocol;
    hdr->checksum   = 0;
    hdr->src        = net_ip;
    hdr->dst        = dst_ip;
    hdr->checksum   = net_checksum(hdr, IP_HDR_LEN);

    memcpy(ip_buf + IP_HDR_LEN, payload, plen);

    
    uint32_t next_hop = dst_ip;
    if ((dst_ip & net_mask) != (net_ip & net_mask))
        next_hop = net_gateway;

    const uint8_t *dst_mac = arp_resolve(next_hop);
    if (!dst_mac) return false;

    return eth_send(dst_mac, ETHERTYPE_IP, ip_buf, total);
}


void ip_recv(const void *pkt, uint16_t len) {
    if (len < IP_HDR_LEN) return;
    const ip_hdr_t *hdr = (const ip_hdr_t *)pkt;

    uint8_t ihl = (hdr->ver_ihl & 0x0F) * 4;
    if (ihl < IP_HDR_LEN || ihl > len) return;

    
    if (hdr->dst != net_ip && hdr->dst != IP_BROADCAST) return;

    uint32_t src_ip = hdr->src;
    const uint8_t *upper = (const uint8_t *)pkt + ihl;
    uint16_t upper_len = (uint16_t)(ntohs(hdr->total_len) - ihl);
    if (upper_len > len - ihl) upper_len = (uint16_t)(len - ihl);

    switch (hdr->protocol) {
        case IP_PROTO_ICMP:
            icmp_recv(upper, upper_len, src_ip);
            break;
        case IP_PROTO_UDP:
            udp_recv(upper, upper_len, src_ip);
            break;
        case IP_PROTO_TCP:
            tcp_recv_pkt(upper, upper_len, src_ip);
            break;
        default:
            break;
    }
}
