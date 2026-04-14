; boot/gdt.asm — Carrega a GDT e recarrega os registradores de segmento
; Chamado por gdt_init() em kernel/gdt.c

bits 32

section .text

; gdt_flush(uint32_t gdt_ptr_addr)
; Carrega a GDT via lgdt e faz um far jump para recarregar CS (kernel code 0x08)
global gdt_flush
gdt_flush:
    mov eax, [esp+4]    ; Endereço do gdt_ptr_t
    lgdt [eax]          ; Carrega a GDT

    ; Recarrega segmentos de dados com o seletor de dados do kernel (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump para recarregar CS com seletor de código do kernel (0x08)
    jmp 0x08:.flush
.flush:
    ret

; tss_flush()
; Carrega o TSS via ltr (seletor 0x28 = índice 5 * 8, RPL=0)
global tss_flush
tss_flush:
    mov ax, 0x2B        ; 0x28 | 3 (RPL=3 para o TSS, necessário em alguns casos)
    ltr ax
    ret
