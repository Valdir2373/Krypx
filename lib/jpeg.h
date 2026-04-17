
#ifndef _JPEG_H
#define _JPEG_H

#include <types.h>

/* Decode a JPEG file in memory. Returns kmalloc'd 0x00RRGGBB pixel buffer,
   or NULL on error. Caller must kfree the buffer. */
uint32_t *jpeg_decode(const uint8_t *data, uint32_t size, int *out_w, int *out_h);

#endif
