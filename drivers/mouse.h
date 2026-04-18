
#ifndef _MOUSE_H
#define _MOUSE_H

#include <types.h>


void mouse_init(void);


int     mouse_get_x(void);
int     mouse_get_y(void);


uint8_t mouse_get_buttons(void);
uint8_t mouse_get_prev_buttons(void);

#endif 
