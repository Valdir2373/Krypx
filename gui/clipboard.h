
#ifndef _CLIPBOARD_H
#define _CLIPBOARD_H

#include <types.h>

void        clipboard_set(const char *data, int len);
const char *clipboard_get(void);
int         clipboard_size(void);
void        clipboard_clear(void);

#endif
