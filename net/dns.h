
#ifndef _DNS_H
#define _DNS_H

#include <types.h>


uint32_t dns_resolve(const char *hostname);

void dns_init(void);

#endif 
