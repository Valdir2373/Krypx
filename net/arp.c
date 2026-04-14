/*
 * net/arp.c — Address Resolution Protocol (IPv4 ↔ MAC)
 * Mantém cache ARP de até ARP_CACHE_SIZE entradas.
 * Para resolver um IP: envia ARP request e aguarda reply por polling.
 */

#include <net/arp.h>
#include <net/ethernet.h>
#include <net/net.h>
#include <drivers/e1000.h>
#include <lib/string.h>
#include <types.h>

/* ============================================================
 * Estrutura de pacote ARP (para IPv4 sobre Ethernet)
 * ============================================================ */
typedef struct {
    uint16_t htype;   /* Hardware type: 1 = Ethernet */
    uint16_t ptype;   /* Protocol type: 0x0800 = IPv4 */
    uint8_t  hlen;    /* Hardware addr length: 6 */
    uint8_t  plen;    /* Protocol addr length: 4 */
    uint16_t oper;    /* 1 = request, 2 = reply */
    uint8_t  sha[6];  /* Sender hardware addr */
    uint32_t spa;     /* Sender protocol addr */
    uint8_t  tha[6];  /* Target hardware addr */
    uint32_t tpa;     /* Target protocol addr */
} __attribute__((packed)) arp_packet_t;

#define ARP_REQUEST 1
#define ARP_REPLY   2

/* ============================================================
 * Cache ARP
 * ============================================================ */
typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    bool     valid;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];
static uint32_t    arp_cache_idx = 0;

void arp_init(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
    arp_cache_idx = 0;
}

/* Procura MAC no cache para o IP dado */
static const uint8_t *arp_cache_lookup(uint32_t ip) {
    uint32_t i;
    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip)
            return arp_cache[i].mac;
    }
    return NULL;
}

/* Insere ou atualiza entrada no cache */
static void arp_cache_insert(uint32_t ip, const uint8_t *mac) {
    uint32_t i;
    /* Atualiza se já existe */
    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            MAC_COPY(arp_cache[i].mac, mac);
            return;
        }
    }
    /* Insere em slot circular */
    uint32_t idx = arp_cache_idx % ARP_CACHE_SIZE;
    arp_cache[idx].ip    = ip;
    arp_cache[idx].valid = true;
    MAC_COPY(arp_cache[idx].mac, mac);
    arp_cache_idx++;
}

/* ============================================================
 * Envio de ARP request
 * ============================================================ */
void arp_request(uint32_t target_ip) {
    arp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.htype = htons(1);
    pkt.ptype = htons(0x0800);
    pkt.hlen  = 6;
    pkt.plen  = 4;
    pkt.oper  = htons(ARP_REQUEST);
    MAC_COPY(pkt.sha, net_mac);
    pkt.spa   = net_ip;
    memset(pkt.tha, 0, 6);
    pkt.tpa   = target_ip;

    static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    eth_send(bcast, ETHERTYPE_ARP, &pkt, sizeof(pkt));
}

/* ============================================================
 * Recepção de pacotes ARP
 * ============================================================ */
void arp_recv(const void *pkt, uint16_t len) {
    if (len < sizeof(arp_packet_t)) return;
    const arp_packet_t *a = (const arp_packet_t *)pkt;

    if (ntohs(a->htype) != 1)      return;   /* não Ethernet */
    if (ntohs(a->ptype) != 0x0800) return;   /* não IPv4 */
    if (a->hlen != 6 || a->plen != 4) return;

    uint16_t oper = ntohs(a->oper);

    /* Aprende o sender de qualquer pacote ARP */
    arp_cache_insert(a->spa, a->sha);

    if (oper == ARP_REQUEST && a->tpa == net_ip) {
        /* Alguém quer nosso MAC — responder */
        arp_packet_t rep;
        memset(&rep, 0, sizeof(rep));
        rep.htype = htons(1);
        rep.ptype = htons(0x0800);
        rep.hlen  = 6;
        rep.plen  = 4;
        rep.oper  = htons(ARP_REPLY);
        MAC_COPY(rep.sha, net_mac);
        rep.spa   = net_ip;
        MAC_COPY(rep.tha, a->sha);
        rep.tpa   = a->spa;
        eth_send(a->sha, ETHERTYPE_ARP, &rep, sizeof(rep));
    }
    /* Se for ARP_REPLY, o aprendizado acima já foi suficiente */
}

static const uint8_t arp_broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* ============================================================
 * Resolve IP → MAC (bloqueante com polling)
 * ============================================================ */
const uint8_t *arp_resolve(uint32_t ip) {
    /* Broadcast: retorna MAC broadcast */
    if (ip == 0xFFFFFFFF) return arp_broadcast_mac;

    /* IP local: retorna MAC próprio */
    if (ip == net_ip) return net_mac;

    /* Já no cache? */
    const uint8_t *cached = arp_cache_lookup(ip);
    if (cached) return cached;

    /* Envia request e aguarda reply com timeout por polling */
    uint32_t retries;
    for (retries = 0; retries < 3; retries++) {
        arp_request(ip);

        /* ~500 ms de espera por polling (~50M iterações a ~100 MIPS) */
        uint32_t timeout = 5000000;
        while (timeout--) {
            /* O driver e1000 usa callback, logo cada frame recebido
             * durante e1000_poll() aciona eth_recv → arp_recv e
             * insere no cache automaticamente */
            uint8_t tmp[2048];
            uint16_t rlen = e1000_recv(tmp, sizeof(tmp));
            if (rlen > 0) {
                eth_recv(tmp, rlen);
                cached = arp_cache_lookup(ip);
                if (cached) return cached;
            }
            __asm__ volatile("nop");
        }
    }
    return NULL;
}
