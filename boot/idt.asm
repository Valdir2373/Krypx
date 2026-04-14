; boot/idt.asm — Carrega a IDT via lidt
; Chamado por idt_init() em kernel/idt.c

bits 32

section .text

; idt_flush(uint32_t idt_ptr_addr)
global idt_flush
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret
