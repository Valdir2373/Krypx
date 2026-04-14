/*
 * net/netif.c — Interface de rede abstrata
 * Ponto de entrada para inicializar toda a pilha TCP/IP.
 * Inicializa: PCI → e1000 → Ethernet → ARP → IP → ICMP → UDP → TCP → DHCP → DNS
 */

#include <net/netif.h>
#include <net/net.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/dhcp.h>
#include <net/dns.h>
#include <net/socket.h>
#include <drivers/pci.h>
#include <drivers/e1000.h>
#include <io.h>
#include <lib/string.h>
#include <types.h>

static bool net_up = false;

/* Debug serial inline */
static void net_ser_putc(char c) { while (!(inb(0x3FD)&0x20)); outb(0x3F8,c); }
static void net_ser_puts(const char *s) { while (*s) net_ser_putc(*s++); }

void netif_init(void) {
    net_ser_puts("[NET] Iniciando pilha de rede...\r\n");

    if (!e1000_init()) {
        net_ser_puts("[NET] e1000_init() falhou - NIC nao detectada\r\n");
        return;
    }
    net_ser_puts("[NET] e1000 detectado OK\r\n");

    const uint8_t *mac = e1000_get_mac();
    MAC_COPY(net_mac, mac);

    arp_init();
    ip_init();
    icmp_init();
    udp_init();
    tcp_init();
    dhcp_init();
    dns_init();
    socket_init();
    eth_init();

    net_ser_puts("[NET] Enviando DHCP DISCOVER...\r\n");
    if (dhcp_request()) {
        net_ser_puts("[NET] DHCP OK!\r\n");
        net_up = true;
    } else {
        net_ser_puts("[NET] DHCP falhou\r\n");
    }
}

void netif_poll(void) {
    if (!e1000_ready()) return;
    uint8_t buf[2048];
    uint16_t len;
    while ((len = e1000_recv(buf, sizeof(buf))) > 0) {
        eth_recv(buf, len);
    }
}

bool netif_is_up(void) {
    return net_up;
}
