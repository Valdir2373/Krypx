
#ifndef _PNG_H
#define _PNG_H

#include <types.h>

/* Decode a PNG file in memory. Returns kmalloc'd 0x00RRGGBB pixel buffer,
   or NULL on error. Caller must kfree the buffer. */
uint32_t *png_decode(const uint8_t *data, uint32_t size, int *out_w, int *out_h);

#endif
