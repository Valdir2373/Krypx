

#include <security/aslr.h>
#include <types.h>

static uint32_t aslr_rand_state = 0xDEADBEEF;

void aslr_init(uint32_t seed) {
    aslr_rand_state = seed ^ 0xA5A5A5A5;
    if (!aslr_rand_state) aslr_rand_state = 0xCAFEBABE;
}

static uint32_t aslr_rand(void) {
    
    aslr_rand_state = aslr_rand_state * 1664525u + 1013904223u;
    return aslr_rand_state;
}

void aslr_randomize(uint32_t *stack_base, uint32_t *heap_base) {
    
    uint32_t sr = aslr_rand();
    *stack_base = 0x70000000 + ((sr & 0x0FFF0000));

    
    uint32_t hr = aslr_rand();
    *heap_base  = 0x40000000 + ((hr & 0x0FFF0000));
}
