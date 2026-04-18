
bits 64

section .text

; gdt_flush(gdt_ptr_t *ptr)  — rdi = pointer to { uint16_t limit, uint64_t base }
global gdt_flush
gdt_flush:
    lgdt    [rdi]

    mov     ax, 0x10    ; kernel data64 selector
    mov     ss, ax
    mov     ds, ax
    mov     es, ax
    xor     ax, ax
    mov     fs, ax
    mov     gs, ax

    ; Far return to reload CS = 0x08 (kernel code64)
    pop     rax
    push    qword 0x08
    push    rax
    retfq

; tss_flush() — loads TSS selector into TR
global tss_flush
tss_flush:
    mov     ax, 0x30    ; TSS selector (entry 6 × 8 = 0x30)
    ltr     ax
    ret
