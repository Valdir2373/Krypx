
#ifndef _HEAP_H
#define _HEAP_H

#include <types.h>


void heap_init(uint32_t start, uint32_t size);


void *kmalloc(size_t size);


void *kmalloc_aligned(size_t size);


void kfree(void *ptr);


void *krealloc(void *ptr, size_t size);


void *kzalloc(size_t size);


void heap_print_info(void);

#endif 
