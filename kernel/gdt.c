
#include <kernel/gdt.h>
#include <include/io.h>
#include <types.h>

/* GDT: 6 standard entries + 2-slot TSS64 descriptor = 8 slots total */
static gdt_entry_t  gdt[GDT_ENTRIES];
static gdt_ptr_t    gdt_ptr;
static tss64_t      tss64 __attribute__((aligned(16)));

extern void gdt_flush(gdt_ptr_t *ptr);
extern void tss_flush(void);

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);
    gdt[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[idx].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[idx].access      = access;
}

static void tss64_install(void) {
    uint64_t base = (uint64_t)&tss64;
    uint32_t limit = (uint32_t)sizeof(tss64_t) - 1;

    /* TSS64 descriptor is 16 bytes — stored in GDT slots 6 and 7 */
    tss64_desc_t *desc = (tss64_desc_t *)&gdt[6];
    desc->length     = (uint16_t)limit;
    desc->base_low   = (uint16_t)(base & 0xFFFF);
    desc->base_mid   = (uint8_t)((base >> 16) & 0xFF);
    desc->flags1     = 0x89;   /* Present, 64-bit TSS Available */
    desc->flags2     = 0x00;
    desc->base_high  = (uint8_t)((base >> 24) & 0xFF);
    desc->base_upper = (uint32_t)(base >> 32);
    desc->reserved   = 0;

    /* Zero TSS, set no IOPB (iomap_base = sizeof TSS = no I/O port access from user) */
    uint8_t *p = (uint8_t *)&tss64;
    uint32_t i;
    for (i = 0; i < sizeof(tss64_t); i++) p[i] = 0;
    tss64.iomap_base = (uint16_t)sizeof(tss64_t);
}

void gdt_init(void) {
    /* GDT_ENTRIES = 8, but entries 6+7 form the TSS64 (16 bytes) */
    gdt_ptr.limit = (uint16_t)(sizeof(gdt_entry_t) * GDT_ENTRIES - 1);
    gdt_ptr.base  = (uint64_t)&gdt;

    /* Standard descriptors (base/limit ignored in 64-bit for code/data) */
    gdt_set_entry(0, 0, 0x00000, 0x00, 0x00);   /* null */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xAF);   /* kernel code64: L=1, G=1 */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);   /* kernel data */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);   /* user code32 compat */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);   /* user data */
    gdt_set_entry(5, 0, 0xFFFFF, 0xFA, 0xAF);   /* user code64: L=1, G=1, DPL=3 */

    tss64_install();

    gdt_flush(&gdt_ptr);
    tss_flush();
}

void tss_set_kernel_stack(uint64_t rsp0) {
    tss64.rsp0 = rsp0;
}

static inline void wrmsr_inline(uint32_t msr, uint64_t val) {
    __asm__ volatile ("wrmsr" : :
        "c"(msr),
        "a"((uint32_t)(val & 0xFFFFFFFFU)),
        "d"((uint32_t)(val >> 32))
        : "memory");
}

void set_fs_base(uint64_t base) {
    wrmsr_inline(0xC0000100, base);   /* IA32_FS_BASE */
}

void set_gs_base(uint64_t base) {
    wrmsr_inline(0xC0000101, base);   /* IA32_GS_BASE */
}
