; boot/isr.asm — Wrappers ASM para todos os handlers de interrupção
; Cada ISR salva o estado completo da CPU, chama o handler C e restaura.
;
; Para exceções SEM código de erro: empurramos 0 como erro fictício.
; Para exceções COM código de erro: a CPU já empurra o código.
; Em seguida empurramos o número da interrupção e chamamos interrupt_handler().

bits 32

extern interrupt_handler    ; Handler C em kernel/idt.c

; ============================================================
; Macro para ISR SEM código de erro (push 0 + número)
; ============================================================
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0        ; Código de erro fictício
    push dword %1       ; Número da interrupção
    jmp isr_common
%endmacro

; ============================================================
; Macro para ISR COM código de erro (CPU já empurrou o código)
; ============================================================
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1       ; Número da interrupção
    jmp isr_common
%endmacro

; ============================================================
; Macro para IRQ (remapeados para vetores 32+)
; ============================================================
%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0        ; Sem código de erro
    push dword %2       ; Número do vetor (32 + IRQ)
    jmp isr_common
%endmacro

; ============================================================
; Exceções da CPU (0-31)
; ============================================================
ISR_NOERRCODE  0    ; Division by Zero
ISR_NOERRCODE  1    ; Debug
ISR_NOERRCODE  2    ; Non-Maskable Interrupt
ISR_NOERRCODE  3    ; Breakpoint
ISR_NOERRCODE  4    ; Overflow
ISR_NOERRCODE  5    ; Bound Range Exceeded
ISR_NOERRCODE  6    ; Invalid Opcode
ISR_NOERRCODE  7    ; Device Not Available (FPU)
ISR_ERRCODE    8    ; Double Fault (tem código de erro = 0)
ISR_NOERRCODE  9    ; Coprocessor Segment Overrun
ISR_ERRCODE   10    ; Invalid TSS
ISR_ERRCODE   11    ; Segment Not Present
ISR_ERRCODE   12    ; Stack-Segment Fault
ISR_ERRCODE   13    ; General Protection Fault
ISR_ERRCODE   14    ; Page Fault
ISR_NOERRCODE 15    ; Reserved
ISR_NOERRCODE 16    ; x87 FPU Error
ISR_ERRCODE   17    ; Alignment Check
ISR_NOERRCODE 18    ; Machine Check
ISR_NOERRCODE 19    ; SIMD FP Exception
ISR_NOERRCODE 20    ; Virtualization Exception
ISR_NOERRCODE 21    ; Reserved
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30    ; Security Exception
ISR_NOERRCODE 31

; ============================================================
; IRQs (0-15, mapeados para vetores 32-47)
; ============================================================
IRQ  0, 32    ; PIT Timer
IRQ  1, 33    ; Keyboard
IRQ  2, 34    ; Cascade (PIC secundário)
IRQ  3, 35    ; COM2
IRQ  4, 36    ; COM1
IRQ  5, 37    ; LPT2
IRQ  6, 38    ; Floppy
IRQ  7, 39    ; LPT1
IRQ  8, 40    ; RTC
IRQ  9, 41    ; Free
IRQ 10, 42    ; Free
IRQ 11, 43    ; Network (e1000)
IRQ 12, 44    ; Mouse PS/2
IRQ 13, 45    ; FPU
IRQ 14, 46    ; IDE Primary
IRQ 15, 47    ; IDE Secondary

; ============================================================
; Syscall (int 0x80)
; ============================================================
global isr128
isr128:
    push dword 0
    push dword 128
    jmp isr_common

; ============================================================
; Handler comum: salva estado, chama C, restaura
; ============================================================
isr_common:
    ; Salva registradores gerais (edi, esi, ebp, esp, ebx, edx, ecx, eax)
    pusha

    ; Salva DS e carrega o seletor de dados do kernel
    mov ax, ds
    push eax
    mov ax, 0x10        ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Chama o handler C: void interrupt_handler(registers_t *regs)
    push esp            ; Ponteiro para os registradores salvos na stack
    call interrupt_handler
    add esp, 4          ; Limpa o argumento

    ; Restaura DS
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Restaura registradores gerais
    popa

    ; Remove int_no e err_code que empurramos
    add esp, 8

    ; Retorna da interrupção (restaura EIP, CS, EFLAGS)
    iret
