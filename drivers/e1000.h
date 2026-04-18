
#ifndef _E1000_H
#define _E1000_H

#include <types.h>

#define E1000_VENDOR  0x8086
#define E1000_DEVICE  0x100E


bool e1000_init(void);


bool e1000_ready(void);


const uint8_t *e1000_get_mac(void);


bool e1000_send(const void *data, uint16_t len);



uint16_t e1000_recv(void *buf, uint16_t max_len);


void e1000_irq_handler(void);


typedef void (*e1000_recv_cb_t)(const void *frame, uint16_t len);
void e1000_set_recv_callback(e1000_recv_cb_t cb);

#endif 
