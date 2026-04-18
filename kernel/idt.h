
#ifndef _IDT_H
#define _IDT_H

#include <types.h>

#define IDT_ENTRIES 256

/* IRQ vectors after PIC remapping to 0x20-0x2F */
#define IRQ_BASE      32
#define IRQ_TIMER     32
#define IRQ_KEYBOARD  33
#define IRQ_CASCADE   34
#define IRQ_COM2      35
#define IRQ_COM1      36
#define IRQ_MOUSE     44
#define IRQ_NETWORK   43
#define IRQ_SYSCALL   0x80

/* 64-bit IDT gate descriptor (16 bytes) */
typedef struct {
    uint16_t offset_low;    /* handler address bits [15:0]  */
    uint16_t selector;      /* code segment selector (0x08) */
    uint8_t  ist;           /* IST index (0 = use RSP0 from TSS) */
    uint8_t  type_attr;     /* 0x8E = kernel interrupt gate */
    uint16_t offset_mid;    /* handler address bits [31:16] */
    uint32_t offset_high;   /* handler address bits [63:32] */
    uint32_t reserved;      /* must be 0 */
} __attribute__((packed)) idt_entry_t;

/* 64-bit IDTR (10 bytes: 2-byte limit + 8-byte base) */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

/* Register frame saved by isr_common on the kernel stack.
 * Layout (low address first = first struct member):
 *   r15, r14, r13, r12, r11, r10, r9, r8,
 *   rbp, rdi, rsi, rdx, rcx, rbx, rax,
 *   int_no, err_code,
 *   rip, cs, rflags, rsp, ss   (pushed by CPU)
 */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

typedef void (*interrupt_handler_t)(registers_t *regs);

void idt_init(void);
void idt_register_handler(uint8_t vector, interrupt_handler_t handler);
void pic_remap(void);
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);
void interrupt_handler(registers_t *regs);

#endif

