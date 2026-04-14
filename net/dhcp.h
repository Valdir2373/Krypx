/*
 * net/dhcp.h — DHCP client (obtém IP automaticamente via DHCP)
 */
#ifndef _DHCP_H
#define _DHCP_H

#include <types.h>

/* Inicia negociação DHCP (DISCOVER → OFFER → REQUEST → ACK)
 * Bloqueia até obter configuração ou timeout.
 * Retorna true se obteve IP com sucesso. */
bool dhcp_request(void);

void dhcp_init(void);

#endif /* _DHCP_H */
