/*
 * mm/vmm.h — Virtual Memory Manager (VMM)
 * Paginação x86: Page Directory (1024 entries) + Page Tables (1024 entries).
 */
#ifndef _VMM_H
#define _VMM_H

#include <types.h>

/* Flags de página */
#define PAGE_PRESENT   (1 << 0)
#define PAGE_WRITABLE  (1 << 1)
#define PAGE_USER      (1 << 2)
#define PAGE_NOCACHE   (1 << 4)
#define PAGE_4MB       (1 << 7)   /* PSE: 4 MB page */

/* Page Directory e Page Table são apenas arrays de uint32_t[1024] */
typedef uint32_t page_dir_t;    /* Ponteiro para array de 1024 uint32_t */
typedef uint32_t page_table_t;  /* Ponteiro para array de 1024 uint32_t */

/* Inicializa paginação com identity map do kernel */
void vmm_init(void);

/* Mapeia virtual→físico com flags */
void vmm_map_page(page_dir_t *dir, uint32_t virt, uint32_t phys, uint32_t flags);

/* Remove mapeamento */
void vmm_unmap_page(page_dir_t *dir, uint32_t virt);

/* Traduz virtual→físico (0 se não mapeado) */
uint32_t vmm_get_physical(page_dir_t *dir, uint32_t virt);

/* Cria novo espaço de endereçamento */
page_dir_t *vmm_create_address_space(void);

/* Troca CR3 */
void vmm_switch_address_space(page_dir_t *dir);

/* Retorna diretório atual */
page_dir_t *vmm_get_current_dir(void);

/* Mapeia range físico em virtual */
void vmm_map_range(page_dir_t *dir, uint32_t virt, uint32_t phys,
                   uint32_t size, uint32_t flags);

#endif /* _VMM_H */
