/*
 * mm/heap.h — Kernel Heap: kmalloc / kfree
 * Alocador de heap simples com blocos de header.
 * Usa o PMM para expandir a heap quando necessário.
 */
#ifndef _HEAP_H
#define _HEAP_H

#include <types.h>

/* Inicializa o heap do kernel */
void heap_init(uint32_t start, uint32_t size);

/* Aloca size bytes no heap — retorna ponteiro ou NULL */
void *kmalloc(size_t size);

/* Aloca size bytes alinhado a 4 KB (para page tables) */
void *kmalloc_aligned(size_t size);

/* Libera memória alocada por kmalloc */
void kfree(void *ptr);

/* Realoca bloco */
void *krealloc(void *ptr, size_t size);

/* Zera e aloca (como calloc) */
void *kzalloc(size_t size);

/* Imprime estado do heap (debug) */
void heap_print_info(void);

#endif /* _HEAP_H */
