
#ifndef _NETIF_H
#define _NETIF_H

#include <types.h>


void netif_init(void);


void netif_poll(void);


bool netif_is_up(void);

#endif 
