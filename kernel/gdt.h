/*
 * kernel/gdt.h — Global Descriptor Table (GDT) do Krypx
 * Segmentação flat: base=0, limit=4GB para todos os segmentos.
 */
#ifndef _GDT_H
#define _GDT_H

#include <types.h>

/* Seletores de segmento (índice * 8) */
#define GDT_NULL         0x00    /* Segmento nulo obrigatório */
#define GDT_KERNEL_CODE  0x08    /* Código do kernel (Ring 0) */
#define GDT_KERNEL_DATA  0x10    /* Dados do kernel (Ring 0) */
#define GDT_USER_CODE    0x18    /* Código do userspace (Ring 3) */
#define GDT_USER_DATA    0x20    /* Dados do userspace (Ring 3) */
#define GDT_TSS          0x28    /* Task State Segment */

/* Número total de entradas */
#define GDT_ENTRIES 6

/* Uma entrada da GDT (8 bytes) */
typedef struct {
    uint16_t limit_low;     /* Bits [15:0] do limite */
    uint16_t base_low;      /* Bits [15:0] da base */
    uint8_t  base_mid;      /* Bits [23:16] da base */
    uint8_t  access;        /* Flags de acesso: P DPL S type */
    uint8_t  granularity;   /* Bits [19:16] do limite + flags G D/B 0 AVL */
    uint8_t  base_high;     /* Bits [31:24] da base */
} __attribute__((packed)) gdt_entry_t;

/* Ponteiro para a GDT (carregado via lgdt) */
typedef struct {
    uint16_t limit;   /* Tamanho da GDT - 1 */
    uint32_t base;    /* Endereço linear da GDT */
} __attribute__((packed)) gdt_ptr_t;

/* TSS (Task State Segment) — necessário para troca Ring 3 → Ring 0 */
typedef struct {
    uint32_t prev_tss;
    uint32_t esp0;    /* Stack pointer do kernel (Ring 0) */
    uint32_t ss0;     /* Stack segment do kernel */
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

/* Inicializa e carrega a GDT */
void gdt_init(void);

/* Atualiza o ESP0 do TSS (chamado ao fazer context switch) */
void tss_set_kernel_stack(uint32_t stack);

#endif /* _GDT_H */
