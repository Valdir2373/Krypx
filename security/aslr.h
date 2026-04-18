
#ifndef _ASLR_H
#define _ASLR_H

#include <types.h>


void aslr_randomize(uint32_t *stack_base, uint32_t *heap_base);


void aslr_init(uint32_t seed);

#endif 
