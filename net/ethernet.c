/*
 * net/ethernet.c — Frame Ethernet II
 * Encapsula payloads (ARP, IP) em frames Ethernet e os envia via e1000.
 * Recebe frames do driver e despacha para ARP ou IP conforme ethertype.
 */

#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/net.h>
#include <drivers/e1000.h>
#include <lib/string.h>
#include <types.h>

/* Buffer estático para montagem do frame */
static uint8_t eth_tx_buf[ETH_MAX_FRAME];

void eth_init(void) {
    /* Registra callback de recepção no driver e1000 */
    e1000_set_recv_callback(eth_recv);
}

bool eth_send(const uint8_t *dst_mac, uint16_t ethertype,
              const void *payload, uint16_t plen)
{
    if (!e1000_ready()) return false;
    if (plen + ETH_HDR_LEN > ETH_MAX_FRAME) return false;

    eth_frame_t *hdr = (eth_frame_t *)eth_tx_buf;
    MAC_COPY(hdr->dst, dst_mac);
    MAC_COPY(hdr->src, net_mac);
    hdr->type = htons(ethertype);

    memcpy(eth_tx_buf + ETH_HDR_LEN, payload, plen);

    return e1000_send(eth_tx_buf, (uint16_t)(ETH_HDR_LEN + plen));
}

void eth_recv(const void *frame, uint16_t len) {
    if (len < ETH_HDR_LEN) return;

    const eth_frame_t *hdr = (const eth_frame_t *)frame;
    uint16_t etype = ntohs(hdr->type);
    const uint8_t *payload = (const uint8_t *)frame + ETH_HDR_LEN;
    uint16_t plen = (uint16_t)(len - ETH_HDR_LEN);

    switch (etype) {
        case ETHERTYPE_ARP:
            arp_recv(payload, plen);
            break;
        case ETHERTYPE_IP:
            ip_recv(payload, plen);
            break;
        default:
            break;
    }
}
