/*
 * mm/pmm.h — Physical Memory Manager (PMM)
 * Gerencia páginas físicas de 4 KB via bitmap.
 * 1 bit por página: 0 = livre, 1 = usada.
 */
#ifndef _PMM_H
#define _PMM_H

#include <types.h>
#include <multiboot.h>

#define PAGE_SIZE   4096
#define PAGE_SHIFT  12

/* Inicializa o PMM lendo o mapa de memória do GRUB */
void pmm_init(multiboot_info_t *mbi);

/* Aloca uma página física — retorna endereço físico ou 0 se sem memória */
uint32_t pmm_alloc_page(void);

/* Libera uma página física */
void pmm_free_page(uint32_t addr);

/* Marca um range de páginas como usado (para reservar kernel, MMIO, etc.) */
void pmm_mark_used(uint32_t addr, uint32_t size);

/* Marca um range como livre */
void pmm_mark_free(uint32_t addr, uint32_t size);

/* Retorna o número de páginas livres */
uint32_t pmm_get_free_pages(void);

/* Retorna total de páginas */
uint32_t pmm_get_total_pages(void);

/* Imprime resumo da memória (debug via VGA) */
void pmm_print_info(void);

#endif /* _PMM_H */
