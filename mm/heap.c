/*
 * mm/heap.c — Kernel Heap com first-fit e coalescing
 *
 * Layout de um bloco:
 *   [heap_block_t header][dados...]
 *
 * Coalescing: ao liberar, une com bloco seguinte e/ou anterior se livres.
 */

#include <mm/heap.h>
#include <mm/pmm.h>
#include <drivers/vga.h>
#include <system.h>
#include <types.h>

#define HEAP_MAGIC  0xDEADBEEF
#define MIN_SPLIT   32   /* Tamanho mínimo para dividir um bloco */

/* Header de cada bloco no heap */
typedef struct heap_block {
    uint32_t         magic;   /* Sempre HEAP_MAGIC — detecta corrupção */
    size_t           size;    /* Tamanho dos dados (sem o header) */
    bool             free;    /* true = livre */
    struct heap_block *next;  /* Próximo bloco (ou NULL) */
    struct heap_block *prev;  /* Bloco anterior (ou NULL) */
} heap_block_t;

static heap_block_t *heap_start = 0;
static uint32_t     heap_base   = 0;
static uint32_t     heap_end    = 0;
static uint32_t     heap_max    = 0;

/* ---- Helpers ---- */

/* Expande o heap alocando mais páginas do PMM */
static bool heap_expand(size_t needed) {
    uint32_t pages = (needed + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t i;
    for (i = 0; i < pages; i++) {
        uint32_t phys = pmm_alloc_page();
        if (!phys) return false;
        /* Identity mapping: virt == phys no kernel */
        heap_end += PAGE_SIZE;
    }
    return true;
}

void heap_init(uint32_t start, uint32_t size) {
    heap_base = start;
    heap_end  = start + size;
    heap_max  = start + (64 * 1024 * 1024);  /* Máximo 64 MB de heap */

    /* Primeiro bloco ocupa todo o espaço disponível */
    heap_start = (heap_block_t *)start;
    heap_start->magic = HEAP_MAGIC;
    heap_start->size  = size - sizeof(heap_block_t);
    heap_start->free  = true;
    heap_start->next  = 0;
    heap_start->prev  = 0;
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;

    /* Alinha em 8 bytes */
    size = (size + 7) & ~7;

    heap_block_t *blk = heap_start;

    /* First-fit */
    while (blk) {
        if (blk->magic != HEAP_MAGIC) {
            /* Heap corrompido */
            return 0;
        }

        if (blk->free && blk->size >= size) {
            /* Divide o bloco se sobrar espaço suficiente */
            if (blk->size >= size + sizeof(heap_block_t) + MIN_SPLIT) {
                heap_block_t *new_blk = (heap_block_t *)((uint8_t *)blk
                                        + sizeof(heap_block_t) + size);
                new_blk->magic = HEAP_MAGIC;
                new_blk->size  = blk->size - size - sizeof(heap_block_t);
                new_blk->free  = true;
                new_blk->next  = blk->next;
                new_blk->prev  = blk;

                if (blk->next) blk->next->prev = new_blk;
                blk->next = new_blk;
                blk->size = size;
            }

            blk->free = false;
            return (void *)((uint8_t *)blk + sizeof(heap_block_t));
        }

        /* Último bloco e ainda livre: tenta expandir */
        if (!blk->next && blk->free) {
            size_t need = size - blk->size;
            if (heap_end + need <= heap_max) {
                if (heap_expand(need)) {
                    blk->size += need;
                    continue;
                }
            }
        }

        blk = blk->next;
    }

    /* Nenhum bloco livre: expande o heap com novo bloco */
    size_t total = sizeof(heap_block_t) + size;
    if (heap_end + total <= heap_max && heap_expand(total)) {
        heap_block_t *new_blk = (heap_block_t *)(heap_end - total);
        new_blk->magic = HEAP_MAGIC;
        new_blk->size  = size;
        new_blk->free  = false;
        new_blk->next  = 0;

        /* Liga ao último bloco existente */
        heap_block_t *last = heap_start;
        while (last->next) last = last->next;
        last->next     = new_blk;
        new_blk->prev  = last;

        return (void *)((uint8_t *)new_blk + sizeof(heap_block_t));
    }

    return 0;  /* Out of memory */
}

void *kmalloc_aligned(size_t size) {
    /* Aloca size + PAGE_SIZE e alinha a 4 KB */
    void *ptr = kmalloc(size + PAGE_SIZE);
    if (!ptr) return 0;
    uint32_t addr = (uint32_t)ptr;
    uint32_t aligned = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    return (void *)aligned;
}

void kfree(void *ptr) {
    if (!ptr) return;

    heap_block_t *blk = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));

    if (blk->magic != HEAP_MAGIC) return;  /* Double-free ou corrupção */
    if (blk->free) return;

    blk->free = true;

    /* Coalescing com o bloco seguinte */
    if (blk->next && blk->next->free) {
        blk->size += sizeof(heap_block_t) + blk->next->size;
        blk->next  = blk->next->next;
        if (blk->next) blk->next->prev = blk;
    }

    /* Coalescing com o bloco anterior */
    if (blk->prev && blk->prev->free) {
        blk->prev->size += sizeof(heap_block_t) + blk->size;
        blk->prev->next  = blk->next;
        if (blk->next) blk->next->prev = blk->prev;
    }
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (!size) { kfree(ptr); return 0; }

    heap_block_t *blk = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
    if (blk->size >= size) return ptr;

    void *new_ptr = kmalloc(size);
    if (!new_ptr) return 0;

    /* Copia dados antigos */
    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)new_ptr;
    size_t copy  = blk->size < size ? blk->size : size;
    size_t i;
    for (i = 0; i < copy; i++) dst[i] = src[i];

    kfree(ptr);
    return new_ptr;
}

void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr) {
        uint8_t *p = (uint8_t *)ptr;
        size_t i;
        for (i = 0; i < size; i++) p[i] = 0;
    }
    return ptr;
}

void heap_print_info(void) {
    uint32_t total = 0, used = 0, free_bytes = 0, blocks = 0;
    heap_block_t *blk = heap_start;
    while (blk) {
        blocks++;
        total += blk->size;
        if (blk->free) free_bytes += blk->size;
        else           used       += blk->size;
        blk = blk->next;
    }
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("[HEAP] Blocos: ");
    vga_put_dec(blocks);
    vga_puts("  Total: ");
    vga_put_dec(total / 1024);
    vga_puts(" KB  Usado: ");
    vga_put_dec(used / 1024);
    vga_puts(" KB  Livre: ");
    vga_put_dec(free_bytes / 1024);
    vga_puts(" KB\n");
}
