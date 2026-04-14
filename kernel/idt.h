/*
 * kernel/idt.h — Interrupt Descriptor Table (IDT) do Krypx
 * 256 entradas cobrindo exceções (0-31), IRQs (32-47) e syscall (0x80)
 */
#ifndef _IDT_H
#define _IDT_H

#include <types.h>

/* Número de entradas na IDT */
#define IDT_ENTRIES 256

/* Vetores de interrupção */
#define IRQ_BASE      32    /* IRQs do PIC começam no vetor 32 */
#define IRQ_TIMER     32    /* IRQ0 — PIT timer */
#define IRQ_KEYBOARD  33    /* IRQ1 — Teclado PS/2 */
#define IRQ_CASCADE   34    /* IRQ2 — Cascata PIC secundário */
#define IRQ_COM2      35    /* IRQ3 — Serial COM2 */
#define IRQ_COM1      36    /* IRQ4 — Serial COM1 */
#define IRQ_MOUSE     44    /* IRQ12 — Mouse PS/2 */
#define IRQ_NETWORK   43    /* IRQ11 — Rede e1000 */
#define IRQ_SYSCALL   0x80  /* Syscall (int 0x80) */

/* Uma entrada da IDT (8 bytes) */
typedef struct {
    uint16_t offset_low;   /* Bits [15:0] do endereço do handler */
    uint16_t selector;     /* Seletor de código (GDT_KERNEL_CODE = 0x08) */
    uint8_t  zero;         /* Sempre 0 */
    uint8_t  type_attr;    /* Tipo e atributos: P DPL 0 tipo */
    uint16_t offset_high;  /* Bits [31:16] do endereço do handler */
} __attribute__((packed)) idt_entry_t;

/* Ponteiro para a IDT (carregado via lidt) */
typedef struct {
    uint16_t limit;   /* Tamanho da IDT - 1 */
    uint32_t base;    /* Endereço linear da IDT */
} __attribute__((packed)) idt_ptr_t;

/* Registradores salvos pelo wrapper ASM ao entrar no handler */
typedef struct {
    /* Segmentos */
    uint32_t ds;
    /* Registradores gerais (pushal) */
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    /* Número da interrupção e código de erro */
    uint32_t int_no, err_code;
    /* Empurrados automaticamente pelo CPU */
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

/* Tipo de handler de interrupção em C */
typedef void (*interrupt_handler_t)(registers_t *regs);

/* Inicializa e carrega a IDT */
void idt_init(void);

/* Registra um handler C para um vetor de interrupção */
void idt_register_handler(uint8_t vector, interrupt_handler_t handler);

/* Remapeia o PIC 8259 (IRQ 0-15 → vetores 32-47) */
void pic_remap(void);

/* Envia EOI (End of Interrupt) ao PIC */
void pic_send_eoi(uint8_t irq);

/* Mascara/desmascara um IRQ no PIC */
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

/* Handler principal de interrupções (chamado pelo ASM wrapper) */
void interrupt_handler(registers_t *regs);

#endif /* _IDT_H */
