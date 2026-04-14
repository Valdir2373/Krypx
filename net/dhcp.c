/*
 * net/dhcp.c — DHCP client
 * Implementa DISCOVER → OFFER → REQUEST → ACK para obter IP automaticamente.
 * Usa UDP porta 68 (client) → porta 67 (server).
 */

#include <net/dhcp.h>
#include <net/udp.h>
#include <net/net.h>
#include <net/ethernet.h>
#include <drivers/e1000.h>
#include <lib/string.h>
#include <types.h>

#define DHCP_PORT_CLIENT  68
#define DHCP_PORT_SERVER  67

#define DHCP_MAGIC_COOKIE 0x63825363  /* little-endian no pacote: 63 82 53 63 */

/* Opcodes */
#define DHCP_BOOTREQUEST  1
#define DHCP_BOOTREPLY    2

/* Message types (option 53) */
#define DHCP_DISCOVER  1
#define DHCP_OFFER     2
#define DHCP_REQUEST   3
#define DHCP_DECLINE   4
#define DHCP_ACK       5
#define DHCP_NAK       6
#define DHCP_RELEASE   7

/* Estrutura DHCP (simplificada) */
typedef struct {
    uint8_t  op;
    uint8_t  htype;   /* 1 = Ethernet */
    uint8_t  hlen;    /* 6 */
    uint8_t  hops;
    uint32_t xid;     /* transaction ID */
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;  /* client IP */
    uint32_t yiaddr;  /* your IP (oferta do server) */
    uint32_t siaddr;  /* server IP */
    uint32_t giaddr;  /* gateway IP */
    uint8_t  chaddr[16]; /* client hardware addr */
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
    uint8_t  options[308]; /* variável, terminado por 0xFF */
} __attribute__((packed)) dhcp_pkt_t;

/* Estado da negociação DHCP */
static bool     dhcp_got_offer  = false;
static bool     dhcp_got_ack    = false;
static uint32_t dhcp_offered_ip = 0;
static uint32_t dhcp_server_ip  = 0;
static uint32_t dhcp_mask       = 0;
static uint32_t dhcp_gateway    = 0;
static uint32_t dhcp_dns        = 0;

/* ============================================================
 * Parser de opções DHCP
 * ============================================================ */
static uint8_t dhcp_get_msg_type(const dhcp_pkt_t *pkt) {
    const uint8_t *opt = pkt->options;
    while (*opt != 0xFF) {
        uint8_t code = opt[0];
        uint8_t len  = opt[1];
        if (code == 53 && len == 1) return opt[2];
        opt += 2 + len;
    }
    return 0;
}

static void dhcp_parse_options(const dhcp_pkt_t *pkt) {
    const uint8_t *opt = pkt->options;
    while (*opt != 0xFF) {
        uint8_t code = opt[0];
        if (code == 0) { opt++; continue; }
        uint8_t len = opt[1];
        switch (code) {
            case 1: /* Subnet mask */
                if (len == 4) memcpy(&dhcp_mask, opt+2, 4);
                break;
            case 3: /* Router/gateway */
                if (len >= 4) memcpy(&dhcp_gateway, opt+2, 4);
                break;
            case 6: /* DNS server */
                if (len >= 4) memcpy(&dhcp_dns, opt+2, 4);
                break;
            case 54: /* Server identifier */
                if (len == 4) memcpy(&dhcp_server_ip, opt+2, 4);
                break;
            default: break;
        }
        opt += 2 + len;
    }
}

/* ============================================================
 * Callback UDP para porta 68
 * ============================================================ */
static void dhcp_recv_cb(uint32_t src_ip, uint16_t src_port,
                          const void *data, uint16_t len)
{
    (void)src_ip; (void)src_port;
    if (len < sizeof(dhcp_pkt_t)) return;
    const dhcp_pkt_t *pkt = (const dhcp_pkt_t *)data;
    if (pkt->op != DHCP_BOOTREPLY) return;
    if (ntohl(pkt->magic) != DHCP_MAGIC_COOKIE) return;

    uint8_t mtype = dhcp_get_msg_type(pkt);
    if (mtype == DHCP_OFFER && !dhcp_got_offer) {
        dhcp_offered_ip = pkt->yiaddr;
        dhcp_parse_options(pkt);
        dhcp_got_offer = true;
    } else if (mtype == DHCP_ACK && dhcp_got_offer) {
        dhcp_offered_ip = pkt->yiaddr;
        dhcp_parse_options(pkt);
        dhcp_got_ack = true;
    }
}

/* ============================================================
 * Monta e envia pacote DHCP
 * ============================================================ */
static void dhcp_send(uint8_t msg_type, uint32_t xid) {
    dhcp_pkt_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.op    = DHCP_BOOTREQUEST;
    pkt.htype = 1;
    pkt.hlen  = 6;
    pkt.xid   = xid;
    pkt.flags = htons(0x8000);  /* broadcast flag */
    MAC_COPY(pkt.chaddr, net_mac);
    pkt.magic = htonl(DHCP_MAGIC_COOKIE);

    uint8_t *opt = pkt.options;
    /* Option 53: DHCP Message Type */
    *opt++ = 53; *opt++ = 1; *opt++ = msg_type;
    if (msg_type == DHCP_REQUEST) {
        /* Option 50: Requested IP */
        *opt++ = 50; *opt++ = 4;
        memcpy(opt, &dhcp_offered_ip, 4); opt += 4;
        /* Option 54: Server Identifier */
        *opt++ = 54; *opt++ = 4;
        memcpy(opt, &dhcp_server_ip, 4); opt += 4;
    }
    /* Option 55: Parameter Request List */
    *opt++ = 55; *opt++ = 3; *opt++ = 1; *opt++ = 3; *opt++ = 6;
    /* End */
    *opt++ = 0xFF;

    udp_send(IP_BROADCAST, DHCP_PORT_CLIENT, DHCP_PORT_SERVER,
             &pkt, sizeof(pkt));
}

/* ============================================================
 * API pública
 * ============================================================ */
void dhcp_init(void) {
    dhcp_got_offer  = false;
    dhcp_got_ack    = false;
    dhcp_offered_ip = 0;
    dhcp_server_ip  = 0;
    dhcp_mask       = 0;
    dhcp_gateway    = 0;
    dhcp_dns        = 0;
    udp_register_port(DHCP_PORT_CLIENT, dhcp_recv_cb);
}

bool dhcp_request(void) {
    uint32_t xid = 0xDEADBEEF;

    /* DISCOVER */
    dhcp_send(DHCP_DISCOVER, xid);

    /* Aguarda OFFER */
    uint32_t timeout = 50000000;
    while (!dhcp_got_offer && timeout--) {
        uint8_t tmp[2048];
        uint16_t rlen = e1000_recv(tmp, sizeof(tmp));
        if (rlen > 0) eth_recv(tmp, rlen);
        __asm__ volatile("nop");
    }
    if (!dhcp_got_offer) return false;

    /* REQUEST */
    dhcp_send(DHCP_REQUEST, xid);

    /* Aguarda ACK */
    timeout = 50000000;
    while (!dhcp_got_ack && timeout--) {
        uint8_t tmp[2048];
        uint16_t rlen = e1000_recv(tmp, sizeof(tmp));
        if (rlen > 0) eth_recv(tmp, rlen);
        __asm__ volatile("nop");
    }
    if (!dhcp_got_ack) return false;

    /* Configura rede global */
    net_ip      = dhcp_offered_ip;
    net_mask    = dhcp_mask    ? dhcp_mask    : IP4(255,255,255,0);
    net_gateway = dhcp_gateway ? dhcp_gateway : 0;
    net_dns     = dhcp_dns     ? dhcp_dns     : 0;

    return true;
}
