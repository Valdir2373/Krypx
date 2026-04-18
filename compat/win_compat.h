
#ifndef _COMPAT_WIN_H
#define _COMPAT_WIN_H

#include <types.h>


void win_compat_init(void);


bool win_compat_load(const uint8_t *data, size_t size);

#endif 
