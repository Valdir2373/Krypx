
#ifndef _GDT_H
#define _GDT_H

#include <types.h>

/* Segment selectors */
#define GDT_NULL         0x00
#define GDT_KERNEL_CODE  0x08   /* 64-bit kernel code,  DPL=0 */
#define GDT_KERNEL_DATA  0x10   /* 64-bit kernel data,  DPL=0 */
#define GDT_USER_CODE32  0x18   /* 32-bit user code (compat), DPL=3 */
#define GDT_USER_DATA    0x20   /* user data,           DPL=3 */
#define GDT_USER_CODE64  0x28   /* 64-bit user code,    DPL=3 */
#define GDT_TSS          0x30   /* TSS64 (16-byte, uses entries 6+7) */

#define GDT_ENTRIES 8           /* entries 0-5 + 2 for TSS64 = 8 slots */

/* Standard 8-byte GDT descriptor */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

/* 10-byte GDTR (64-bit: 2-byte limit + 8-byte base) */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

/* 64-bit TSS (Task State Segment) */
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;          /* kernel stack for ring-0 entry */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];        /* interrupt stack table */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss64_t;

/* 16-byte TSS64 GDT descriptor (spans two 8-byte GDT slots) */
typedef struct {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;        /* 0x89: Present, 64-bit TSS Available */
    uint8_t  flags2;        /* 0x00 */
    uint8_t  base_high;
    uint32_t base_upper;    /* base bits [63:32] */
    uint32_t reserved;
} __attribute__((packed)) tss64_desc_t;

void gdt_init(void);
void tss_set_kernel_stack(uint64_t rsp0);

/* arch_prctl(ARCH_SET_FS) — sets FS.base MSR for TLS */
void set_fs_base(uint64_t base);
void set_gs_base(uint64_t base);

#endif

