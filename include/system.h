/*
 * system.h — Definições globais do sistema Krypx
 */
#ifndef _SYSTEM_H
#define _SYSTEM_H

#include <types.h>

/* Versão do Krypx */
#define KRYPX_VERSION_MAJOR  0
#define KRYPX_VERSION_MINOR  1
#define KRYPX_VERSION_PATCH  0
#define KRYPX_VERSION_STR    "0.1.0"
#define KRYPX_NAME           "Krypx"

/* Layout de memória virtual */
#define KERNEL_VIRTUAL_BASE  0x00100000   /* 1 MB — onde o kernel é carregado */
#define KERNEL_HEAP_START    0xC0000000   /* Início do heap do kernel */
#define KERNEL_HEAP_SIZE     (16 * 1024 * 1024)  /* 16 MB de heap inicial */

/* Tamanho de página */
#define PAGE_SIZE            4096
#define PAGE_SHIFT           12

/* Macros úteis */
#define ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define PAGE_ALIGN(x)        ALIGN_UP(x, PAGE_SIZE)

#define ARRAY_SIZE(arr)      (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b)            ((a) < (b) ? (a) : (b))
#define MAX(a, b)            ((a) > (b) ? (a) : (b))
#define ABS(x)               ((x) < 0 ? -(x) : (x))

/* Halt da CPU */
static inline void cpu_halt(void) {
    __asm__ volatile ("cli; hlt");
}

/* Desabilita interrupções */
static inline void cli(void) {
    __asm__ volatile ("cli");
}

/* Habilita interrupções */
static inline void sti(void) {
    __asm__ volatile ("sti");
}

/* Lê CR2 (endereço que causou page fault) */
static inline uint32_t read_cr2(void) {
    uint32_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

/* Lê CR3 (page directory base) */
static inline uint32_t read_cr3(void) {
    uint32_t val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

/* Escreve CR3 (troca page directory) */
static inline void write_cr3(uint32_t val) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(val));
}

/* Declarações externas das funções principais */
void kernel_main(uint32_t magic, uint32_t mbi_addr);
void kernel_panic(const char *msg);

#endif /* _SYSTEM_H */
