/*
 * net/net.c — Estado global da rede e função de checksum
 * Contém as variáveis globais de configuração de rede (MAC, IP, máscara, etc.)
 * e a função net_checksum usada por IP, ICMP, TCP, UDP.
 */

#include <net/net.h>
#include <types.h>

/* Estado de rede global */
uint8_t  net_mac[6]    = {0};
uint32_t net_ip        = 0;
uint32_t net_gateway   = 0;
uint32_t net_mask      = 0;
uint32_t net_dns       = 0;

bool net_is_configured(void) {
    return net_ip != 0;
}

/* ============================================================
 * Checksum RFC 1071 (complemento de um sobre palavras de 16 bits)
 * Usado por IP, ICMP, TCP, UDP
 * ============================================================ */
uint16_t net_checksum(const void *data, uint32_t len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    /* byte ímpar final */
    if (len == 1) {
        sum += *(const uint8_t *)p;
    }

    /* Dobra os carry bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}
