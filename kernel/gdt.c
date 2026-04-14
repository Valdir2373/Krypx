/*
 * kernel/gdt.c — Configuração da GDT (Global Descriptor Table)
 * 6 entradas: null, kernel code, kernel data, user code, user data, TSS.
 * Segmentação flat: base=0, limit=4GB para todos.
 */

#include <kernel/gdt.h>
#include <types.h>

/* A GDT e o TSS são estáticos — alocados no segmento .data */
static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;
static tss_entry_t tss;

/* Declarado em boot/gdt.asm — carrega a GDT e recarrega os segmentos */
extern void gdt_flush(uint32_t gdt_ptr_addr);
extern void tss_flush(void);

/* Monta uma entrada da GDT */
static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[idx].base_low    = (base & 0xFFFF);
    gdt[idx].base_mid    = (base >> 16) & 0xFF;
    gdt[idx].base_high   = (base >> 24) & 0xFF;
    gdt[idx].limit_low   = (limit & 0xFFFF);
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].access      = access;
}

/* Inicializa o TSS */
static void tss_init(uint32_t kernel_stack) {
    uint32_t base  = (uint32_t)&tss;
    uint32_t limit = base + sizeof(tss_entry_t);

    /* Registra o TSS como entrada 5 da GDT */
    gdt_set_entry(5, base, limit, 0xE9, 0x00);

    /* Zera o TSS */
    uint8_t *p = (uint8_t *)&tss;
    uint32_t i;
    for (i = 0; i < sizeof(tss_entry_t); i++) p[i] = 0;

    tss.ss0  = GDT_KERNEL_DATA;   /* Stack segment do kernel */
    tss.esp0 = kernel_stack;       /* Stack do kernel para Ring 0 */
    tss.cs   = GDT_KERNEL_CODE | 0x3;
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = GDT_KERNEL_DATA | 0x3;
    tss.iomap_base = sizeof(tss_entry_t);
}

void gdt_init(void) {
    /* Configura o ponteiro */
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    /*
     * Tabela de entradas:
     *  0 — Null (obrigatório)
     *  1 — Kernel Code: base=0, limit=4GB, Ring 0, executável+leitura
     *  2 — Kernel Data: base=0, limit=4GB, Ring 0, leitura+escrita
     *  3 — User Code:   base=0, limit=4GB, Ring 3, executável+leitura
     *  4 — User Data:   base=0, limit=4GB, Ring 3, leitura+escrita
     *  5 — TSS (preenchido por tss_init)
     *
     * access byte: P DPL S type
     *   P=1: presente
     *   DPL: 0 = kernel, 3 = user
     *   S=1: segmento de código/dados (não sistema)
     *   type: 0xA = execute/read, 0x2 = read/write
     *
     * gran byte: G D/B 0 AVL | limit[19:16]
     *   G=1:  granularidade em páginas (4KB)
     *   D/B=1: modo de 32 bits
     */
    gdt_set_entry(0, 0, 0x00000000, 0x00, 0x00); /* Null */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* Kernel Code */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* Kernel Data */
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); /* User Code */
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); /* User Data */

    /* Inicializa o TSS com stack temporária (será atualizada no scheduler) */
    tss_init(0x90000);

    /* Carrega a GDT via lgdt e recarrega os registradores de segmento */
    gdt_flush((uint32_t)&gdt_ptr);

    /* Carrega o TSS via ltr */
    tss_flush();
}

void tss_set_kernel_stack(uint32_t stack) {
    tss.esp0 = stack;
}
