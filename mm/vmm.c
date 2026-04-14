/*
 * mm/vmm.c — Virtual Memory Manager
 * Identity map dos primeiros 8 MB com PSE (4 MB pages) para o kernel.
 * vmm_map_page usa page tables normais de 4 KB para mapeamentos finos.
 */

#include <mm/vmm.h>
#include <mm/pmm.h>
#include <drivers/vga.h>
#include <system.h>
#include <types.h>

/* Page Directory do kernel — alinhado a 4 KB */
static uint32_t kernel_page_dir[1024] __attribute__((aligned(4096)));

/* Page Directory ativo */
static uint32_t *current_dir = 0;

/* ============================================================
 * Obtém ponteiro para a page table do índice dir_idx,
 * criando uma nova (via PMM) se necessário.
 * ============================================================ */
static uint32_t *get_or_create_table(uint32_t *dir, uint32_t dir_idx,
                                      uint32_t flags) {
    uint32_t entry = dir[dir_idx];

    if (entry & PAGE_PRESENT) {
        /* Tabela já existe — retorna seu endereço (bits [31:12]) */
        return (uint32_t *)(entry & ~0xFFF);
    }

    /* Aloca nova page table */
    uint32_t phys = pmm_alloc_page();
    if (!phys) return 0;

    /* Zera a nova tabela */
    uint32_t *table = (uint32_t *)phys;
    uint32_t i;
    for (i = 0; i < 1024; i++) table[i] = 0;

    /* Registra no diretório */
    dir[dir_idx] = phys | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    return table;
}

void vmm_map_page(uint32_t *dir, uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;

    uint32_t *table = get_or_create_table(dir, dir_idx, flags);
    if (!table) return;

    table[table_idx] = (phys & ~0xFFF) | PAGE_PRESENT | flags;

    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap_page(uint32_t *dir, uint32_t virt) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;

    uint32_t de = dir[dir_idx];
    if (!(de & PAGE_PRESENT)) return;

    uint32_t *table = (uint32_t *)(de & ~0xFFF);
    table[table_idx] = 0;

    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uint32_t vmm_get_physical(uint32_t *dir, uint32_t virt) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;

    uint32_t de = dir[dir_idx];
    if (!(de & PAGE_PRESENT)) return 0;

    uint32_t *table = (uint32_t *)(de & ~0xFFF);
    uint32_t  te    = table[table_idx];
    if (!(te & PAGE_PRESENT)) return 0;

    return (te & ~0xFFF) | (virt & 0xFFF);
}

void vmm_map_range(uint32_t *dir, uint32_t virt, uint32_t phys,
                   uint32_t size, uint32_t flags) {
    uint32_t offset;
    for (offset = 0; offset < size; offset += PAGE_SIZE) {
        vmm_map_page(dir, virt + offset, phys + offset, flags);
    }
}

uint32_t *vmm_create_address_space(void) {
    uint32_t phys = pmm_alloc_page();
    if (!phys) return 0;

    uint32_t *dir = (uint32_t *)phys;
    uint32_t i;

    for (i = 0; i < 1024; i++) dir[i] = 0;

    /* Copia mapeamento do kernel (entradas 768+) */
    for (i = 768; i < 1024; i++) {
        dir[i] = kernel_page_dir[i];
    }
    return dir;
}

void vmm_switch_address_space(uint32_t *dir) {
    current_dir = dir;
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)dir) : "memory");
}

uint32_t *vmm_get_current_dir(void) {
    return current_dir;
}

/* ============================================================
 * vmm_init — Identity map com PSE (4 MB pages) e ativa CR0.PG
 * ============================================================ */
void vmm_init(void) {
    uint32_t i;

    for (i = 0; i < 1024; i++) kernel_page_dir[i] = 0;

    /*
     * Identity map dos primeiros 32 MB com PSE (4 MB pages):
     *   entrada 0 → 0x00000000 - 0x003FFFFF  (0   – 4 MB)
     *   entrada 1 → 0x00400000 - 0x007FFFFF  (4   – 8 MB)
     *   entrada 2 → 0x00800000 - 0x00BFFFFF  (8   – 12 MB)  ← heap
     *   entrada 3 → 0x00C00000 - 0x00FFFFFF  (12  – 16 MB)
     *   entrada 4 → 0x01000000 - 0x013FFFFF  (16  – 20 MB)
     *   entrada 5 → 0x01400000 - 0x017FFFFF  (20  – 24 MB)
     *   entrada 6 → 0x01800000 - 0x01BFFFFF  (24  – 28 MB)
     *   entrada 7 → 0x01C00000 - 0x01FFFFFF  (28  – 32 MB)
     */
    uint32_t mb4;
    for (mb4 = 0; mb4 < 8; mb4++) {
        kernel_page_dir[mb4] = (mb4 * 0x400000) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_4MB;
    }

    /* Ativa PSE (CR4 bit 4) */
    __asm__ volatile (
        "mov %%cr4, %%eax\n"
        "or  $0x10,  %%eax\n"
        "mov %%eax, %%cr4\n"
        : : : "eax"
    );

    /* Carrega CR3 */
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)kernel_page_dir) : "memory");

    /* Ativa paginação: CR0 bit 31 */
    __asm__ volatile (
        "mov %%cr0, %%eax\n"
        "or  $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        : : : "eax"
    );

    current_dir = kernel_page_dir;
}
