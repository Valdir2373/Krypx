
#ifndef _PMM_H
#define _PMM_H

#include <types.h>
#include <multiboot.h>

#define PAGE_SIZE   4096ULL
#define PAGE_SHIFT  12

void     pmm_init(multiboot_info_t *mbi);
uint64_t pmm_alloc_page(void);
void     pmm_free_page(uint64_t addr);
void     pmm_mark_used(uint64_t addr, uint64_t size);
void     pmm_mark_free(uint64_t addr, uint64_t size);
uint64_t pmm_get_free_pages(void);
uint64_t pmm_get_total_pages(void);
void     pmm_print_info(void);

#endif
