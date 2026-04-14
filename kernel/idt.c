/*
 * kernel/idt.c — Configuração da IDT e handlers de interrupção
 * Inclui remapeamento do PIC 8259 e dispatch para handlers C registrados.
 */

#include <kernel/idt.h>
#include <kernel/gdt.h>
#include <types.h>
#include <io.h>
#include <drivers/vga.h>
#include <system.h>

/* Portas do PIC 8259 */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1
#define PIC_EOI     0x20   /* End Of Interrupt */

/* Comandos de inicialização do PIC (ICW1) */
#define ICW1_INIT   0x10
#define ICW1_ICW4   0x01

/* ICW4 */
#define ICW4_8086   0x01   /* Modo 8086/88 */

/* A IDT com 256 entradas */
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

/* Tabela de handlers C registrados */
static interrupt_handler_t handlers[IDT_ENTRIES];

/* Declarações dos stubs ASM gerados por boot/isr.asm */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);
extern void isr128(void);

/* Carrega a IDT via lidt */
extern void idt_flush(uint32_t idt_ptr_addr);

/* Preenche uma entrada da IDT */
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = (base & 0xFFFF);
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector    = sel;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

void pic_remap(void) {
    uint8_t mask1, mask2;

    /* Salva máscaras atuais */
    mask1 = inb(PIC1_DATA);
    mask2 = inb(PIC2_DATA);

    /* ICW1: Início de inicialização em cascata */
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: Vetores base (PIC1=32, PIC2=40) */
    outb(PIC1_DATA, 0x20);  /* IRQ 0-7 → vetores 32-39 */
    io_wait();
    outb(PIC2_DATA, 0x28);  /* IRQ 8-15 → vetores 40-47 */
    io_wait();

    /* ICW3: Cascata — PIC1 tem PIC2 na linha IRQ2 */
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    /* ICW4: Modo 8086 */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restaura máscaras */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 40) {
        /* IRQ do PIC secundário — envia EOI para ambos */
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t val;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) | (1 << irq);
    outb(port, val);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t val;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) & ~(1 << irq);
    outb(port, val);
}

void idt_register_handler(uint8_t vector, interrupt_handler_t handler) {
    handlers[vector] = handler;
}

/* Nomes das exceções para kernel panic */
static const char *exception_names[] = {
    "Division by Zero",          /* 0  */
    "Debug",                     /* 1  */
    "Non-Maskable Interrupt",    /* 2  */
    "Breakpoint",                /* 3  */
    "Overflow",                  /* 4  */
    "Bound Range Exceeded",      /* 5  */
    "Invalid Opcode",            /* 6  */
    "Device Not Available",      /* 7  */
    "Double Fault",              /* 8  */
    "Coprocessor Segment",       /* 9  */
    "Invalid TSS",               /* 10 */
    "Segment Not Present",       /* 11 */
    "Stack-Segment Fault",       /* 12 */
    "General Protection Fault",  /* 13 */
    "Page Fault",                /* 14 */
    "Reserved",                  /* 15 */
    "x87 FPU Error",             /* 16 */
    "Alignment Check",           /* 17 */
    "Machine Check",             /* 18 */
    "SIMD FP Exception",         /* 19 */
};

/* Handler central — chamado por isr_common em boot/isr.asm */
void interrupt_handler(registers_t *regs) {
    uint32_t vec = regs->int_no;

    /* Dispatcha para handler C registrado (se houver) */
    if (handlers[vec]) {
        handlers[vec](regs);
        return;
    }

    /* IRQs sem handler registrado: só envia EOI e ignora */
    if (vec >= 32 && vec < 48) {
        pic_send_eoi(vec);
        return;
    }

    /* Exceções sem handler: kernel panic */
    if (vec < 32) {
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
        vga_puts("\n\n *** KERNEL PANIC — CPU EXCEPTION ***\n");
        vga_puts(" Excecao: ");
        if (vec < 20) {
            vga_puts(exception_names[vec]);
        } else {
            vga_puts("Reservada");
        }
        vga_puts(" (vetor ");
        vga_put_dec(vec);
        vga_puts(")\n");
        vga_puts(" Codigo de erro: ");
        vga_put_hex(regs->err_code);
        vga_puts("\n EIP: ");
        vga_put_hex(regs->eip);
        vga_puts("  CS: ");
        vga_put_hex(regs->cs);

        if (vec == 14) {
            /* Page Fault: CR2 = endereço que causou a falha */
            vga_puts("\n CR2 (addr): ");
            vga_put_hex(read_cr2());
        }

        vga_puts("\n Sistema travado.\n");
        cli();
        for (;;) __asm__ volatile ("hlt");
    }
}

void idt_init(void) {
    uint32_t i;

    /* Zera handlers */
    for (i = 0; i < IDT_ENTRIES; i++) handlers[i] = 0;

    /* Configura ponteiro */
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base  = (uint32_t)&idt;

    /* ============================================================
     * Gates para exceções CPU (0-31) — Ring 0, Interrupt Gate
     * flags: 0x8E = P=1, DPL=0, tipo=interrupt gate (0b10001110)
     * ============================================================ */
    idt_set_gate( 0, (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate( 1, (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate( 2, (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate( 3, (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate( 4, (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate( 5, (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate( 6, (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate( 7, (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate( 8, (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate( 9, (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    /* ============================================================
     * IRQs (32-47)
     * ============================================================ */
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    /* Syscall (int 0x80) — DPL=3 para que userspace possa chamar */
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);

    /* Remapeia o PIC antes de carregar a IDT */
    pic_remap();

    /* Carrega a IDT */
    idt_flush((uint32_t)&idt_ptr);
}
