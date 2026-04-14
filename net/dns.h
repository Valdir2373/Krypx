/*
 * net/dns.h — DNS resolver (consulta A record)
 */
#ifndef _DNS_H
#define _DNS_H

#include <types.h>

/* Resolve hostname para IPv4. Retorna IP em ordem de rede, ou 0 se falhar. */
uint32_t dns_resolve(const char *hostname);

void dns_init(void);

#endif /* _DNS_H */
