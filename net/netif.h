/*
 * net/netif.h — Interface de rede abstrata
 * Inicializa toda a pilha TCP/IP do Krypx.
 */
#ifndef _NETIF_H
#define _NETIF_H

#include <types.h>

/* Inicializa toda a pilha de rede (driver + protocolo) */
void netif_init(void);

/* Processa pacotes pendentes (polling — chamar periodicamente) */
void netif_poll(void);

/* Retorna true se a rede está up e configurada */
bool netif_is_up(void);

#endif /* _NETIF_H */
