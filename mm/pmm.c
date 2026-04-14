/*
 * mm/pmm.c — Physical Memory Manager
 * Bitmap de páginas físicas de 4 KB.
 * O bitmap fica no primeiro bloco de memória livre acima do kernel.
 */

#include <mm/pmm.h>
#include <drivers/vga.h>
#include <types.h>

/* Endereço do símbolo definido no linker script — fim do kernel */
extern uint32_t kernel_end;

/* ============================================================
 * Bitmap estático para até 4 GB / 4 KB = 1.048.576 páginas
 * = 128 KB de bitmap. Colocamos em BSS para não inflar o kernel.
 * ============================================================ */
#define MAX_PAGES    (1024 * 1024)          /* 4 GB / 4 KB */
#define BITMAP_SIZE  (MAX_PAGES / 32)       /* uint32_t words */

static uint32_t bitmap[BITMAP_SIZE];
static uint32_t total_pages = 0;
static uint32_t free_pages  = 0;

/* ---- Helpers de bitmap ---- */

static inline void bitmap_set(uint32_t page) {
    bitmap[page / 32] |= (1u << (page % 32));
}

static inline void bitmap_clear(uint32_t page) {
    bitmap[page / 32] &= ~(1u << (page % 32));
}

static inline bool bitmap_test(uint32_t page) {
    return (bitmap[page / 32] >> (page % 32)) & 1;
}

/* ---- Marca range de endereços físicos ---- */

void pmm_mark_used(uint32_t addr, uint32_t size) {
    uint32_t page_start = addr / PAGE_SIZE;
    uint32_t page_end   = (addr + size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t i;
    for (i = page_start; i < page_end && i < MAX_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            if (free_pages > 0) free_pages--;
        }
    }
}

void pmm_mark_free(uint32_t addr, uint32_t size) {
    uint32_t page_start = addr / PAGE_SIZE;
    uint32_t page_end   = (addr + size) / PAGE_SIZE;
    uint32_t i;
    for (i = page_start; i < page_end && i < MAX_PAGES; i++) {
        if (bitmap_test(i)) {
            bitmap_clear(i);
            free_pages++;
        }
    }
}

/* ============================================================
 * pmm_init — Lê o mapa de memória do GRUB e inicializa o PMM
 * ============================================================ */
void pmm_init(multiboot_info_t *mbi) {
    uint32_t i;

    /* 1. Começa com tudo marcado como usado */
    for (i = 0; i < BITMAP_SIZE; i++) bitmap[i] = 0xFFFFFFFF;

    /* 2. Marca regiões disponíveis conforme o mapa do GRUB */
    if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)mbi->mmap_addr;
        uint32_t end = mbi->mmap_addr + mbi->mmap_length;

        while ((uint32_t)entry < end) {
            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                /* Ignora memória acima de 4 GB (32-bit) */
                if (entry->base_addr < 0xFFFFFFFFULL) {
                    uint32_t base = (uint32_t)entry->base_addr;
                    uint32_t len  = (entry->length > 0xFFFFFFFFULL)
                                    ? 0xFFFFFFFF - base
                                    : (uint32_t)entry->length;

                    /* Conta total de páginas (primeira vez) */
                    total_pages += len / PAGE_SIZE;
                    free_pages  += len / PAGE_SIZE;

                    /* Marca como livre no bitmap */
                    pmm_mark_free(base, len);
                }
            }
            entry = (multiboot_mmap_entry_t *)((uint32_t)entry + entry->size + 4);
        }
    } else if (mbi->flags & MULTIBOOT_INFO_MEMORY) {
        /* Fallback: usa mem_upper se não tiver mmap */
        total_pages = (mbi->mem_upper * 1024) / PAGE_SIZE;
        free_pages  = total_pages;
        pmm_mark_free(0x100000, mbi->mem_upper * 1024);
    }

    /* 3. Reserva o primeiro MB (BIOS, VGA, hardware) */
    pmm_mark_used(0x000000, 0x100000);

    /* 4. Reserva o kernel */
    uint32_t kend = (uint32_t)&kernel_end;
    pmm_mark_used(0x100000, kend - 0x100000);

    /* 5. Reserva a região do bitmap (pode estar no BSS do kernel, já incluso acima) */
}

/* ============================================================
 * Aloca uma página (first-fit no bitmap)
 * ============================================================ */
uint32_t pmm_alloc_page(void) {
    uint32_t i, bit;
    if (free_pages == 0) return 0;

    for (i = 0; i < BITMAP_SIZE; i++) {
        if (bitmap[i] == 0xFFFFFFFF) continue;  /* todos usados — pula */

        /* Encontra primeiro bit 0 (livre) */
        for (bit = 0; bit < 32; bit++) {
            if (!((bitmap[i] >> bit) & 1)) {
                uint32_t page = i * 32 + bit;
                bitmap_set(page);
                free_pages--;
                return page * PAGE_SIZE;
            }
        }
    }
    return 0;  /* Sem memória */
}

void pmm_free_page(uint32_t addr) {
    uint32_t page = addr / PAGE_SIZE;
    if (page >= MAX_PAGES) return;
    if (!bitmap_test(page)) return;  /* Já estava livre — double-free */
    bitmap_clear(page);
    free_pages++;
}

uint32_t pmm_get_free_pages(void) { return free_pages; }
uint32_t pmm_get_total_pages(void) { return total_pages; }

void pmm_print_info(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("[PMM] Total: ");
    vga_put_dec(total_pages * 4);
    vga_puts(" KB  Livre: ");
    vga_put_dec(free_pages * 4);
    vga_puts(" KB  Usado: ");
    vga_put_dec((total_pages - free_pages) * 4);
    vga_puts(" KB\n");
}
