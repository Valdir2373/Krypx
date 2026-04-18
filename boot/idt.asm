
bits 64

section .text

; idt_flush(idt_ptr_t *ptr)  — rdi = pointer to { uint16_t limit, uint64_t base }
global idt_flush
idt_flush:
    lidt    [rdi]
    ret
