; isr.asm — 64-bit interrupt service routine stubs
; Saves all general-purpose registers, calls interrupt_handler(registers_t*),
; then restores and returns with IRETQ.
;
; registers_t layout (matches struct in kernel/idt.h):
;   r15, r14, r13, r12, r11, r10, r9, r8,
;   rbp, rdi, rsi, rdx, rcx, rbx, rax,
;   int_no, err_code,
;   rip, cs, rflags, rsp, ss   (pushed by CPU)

bits 64

extern interrupt_handler

; ── Exception: no error code pushed by CPU ───────────────────────────────────
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push    qword 0        ; fake error code
    push    qword %1       ; interrupt number
    jmp     isr_common
%endmacro

; ── Exception: CPU pushes error code before we get control ───────────────────
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push    qword %1       ; interrupt number (error code already on stack)
    jmp     isr_common
%endmacro

; ── Hardware IRQ ──────────────────────────────────────────────────────────────
%macro IRQ 2
global irq%1
irq%1:
    push    qword 0
    push    qword %2
    jmp     isr_common
%endmacro

; ── CPU exceptions ───────────────────────────────────────────────────────────
ISR_NOERRCODE  0    ; #DE  Divide Error
ISR_NOERRCODE  1    ; #DB  Debug
ISR_NOERRCODE  2    ;      NMI
ISR_NOERRCODE  3    ; #BP  Breakpoint
ISR_NOERRCODE  4    ; #OF  Overflow
ISR_NOERRCODE  5    ; #BR  Bound Range Exceeded
ISR_NOERRCODE  6    ; #UD  Invalid Opcode
ISR_NOERRCODE  7    ; #NM  Device Not Available
ISR_ERRCODE    8    ; #DF  Double Fault
ISR_NOERRCODE  9    ;      Coprocessor Segment Overrun
ISR_ERRCODE   10    ; #TS  Invalid TSS
ISR_ERRCODE   11    ; #NP  Segment Not Present
ISR_ERRCODE   12    ; #SS  Stack-Segment Fault
ISR_ERRCODE   13    ; #GP  General Protection Fault
ISR_ERRCODE   14    ; #PF  Page Fault
ISR_NOERRCODE 15
ISR_NOERRCODE 16    ; #MF  x87 Floating-Point
ISR_ERRCODE   17    ; #AC  Alignment Check
ISR_NOERRCODE 18    ; #MC  Machine Check
ISR_NOERRCODE 19    ; #XM  SIMD Floating-Point
ISR_NOERRCODE 20    ; #VE  Virtualization
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30    ; #SX  Security Exception
ISR_NOERRCODE 31

; ── Hardware IRQs (remapped to vectors 32–47) ─────────────────────────────────
IRQ  0, 32    ; PIT timer
IRQ  1, 33    ; PS/2 keyboard
IRQ  2, 34
IRQ  3, 35
IRQ  4, 36
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40
IRQ  9, 41
IRQ 10, 42
IRQ 11, 43    ; network (e1000)
IRQ 12, 44    ; PS/2 mouse
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ── int $0x80 — 32-bit Linux compat syscall ───────────────────────────────────
global isr128
isr128:
    push    qword 0
    push    qword 128
    jmp     isr_common

; ── Common handler ────────────────────────────────────────────────────────────
; Stack layout when we arrive here (low address first / rsp = lowest):
;   [rsp+ 0]  int_no     (pushed by stub above)
;   [rsp+ 8]  err_code   (pushed by stub or CPU)
;   [rsp+16]  rip        (pushed by CPU)
;   [rsp+24]  cs         (pushed by CPU)
;   [rsp+32]  rflags     (pushed by CPU)
;   [rsp+40]  rsp_user   (pushed by CPU only on ring change)
;   [rsp+48]  ss         (pushed by CPU only on ring change)

isr_common:
    ; Save all general-purpose registers (manual, since PUSHA is invalid in 64-bit)
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    ; rsp now points to registers_t (r15 at lowest address)
    ; Pass pointer as first argument (SysV AMD64: rdi)
    mov     rdi, rsp
    call    interrupt_handler

    ; Restore general-purpose registers
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    ; Discard int_no and err_code
    add     rsp, 16

    ; Return from interrupt (64-bit IRET)
    iretq
