bits 64

; x86_64 syscall ABI:
;   RAX = syscall number / return value
;   RDI, RSI, RDX, R10, R8, R9 = args 1-6
;   RCX = saved user RIP (by syscall hardware)
;   R11 = saved user RFLAGS (by syscall hardware)
;
; syscall64_frame_t layout (must match compat/linux_compat64.h):
;   +0  rax   +8  rdi  +16 rsi  +24 rdx
;   +32 r10  +40 r8   +48 r9   +56 rcx  +64 r11

global syscall_entry
global g_syscall_kernel_rsp
global g_syscall_user_rsp
extern linux_syscall64_handler

section .bss
align 16
syscall_kstack_space: resb 65536
syscall_kstack_top:

section .data
align 8
g_syscall_kernel_rsp: dq 0
g_syscall_user_rsp:   dq 0

section .text
syscall_entry:
    ; Save user RSP, switch to kernel stack
    mov [rel g_syscall_user_rsp], rsp
    mov rsp, [rel g_syscall_kernel_rsp]

    ; Build syscall64_frame_t — push in reverse order (r11 first = highest addr)
    push r11        ; +64: saved RFLAGS
    push rcx        ; +56: saved RIP
    push r9         ; +48: arg6
    push r8         ; +40: arg5
    push r10        ; +32: arg4
    push rdx        ; +24: arg3
    push rsi        ; +16: arg2
    push rdi        ; +8:  arg1
    push rax        ; +0:  syscall number → return value

    ; Save callee-saved registers (not in frame, but needed across the call)
    push r15
    push r14
    push r13
    push r12
    push rbp
    push rbx

    ; Pass pointer to frame to the handler (frame is 48 bytes above callee regs)
    lea rdi, [rsp + 48]
    call linux_syscall64_handler

    ; Restore callee-saved
    pop rbx
    pop rbp
    pop r12
    pop r13
    pop r14
    pop r15

    ; Restore frame registers
    pop rax         ; return value (handler wrote to frame->rax)
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9
    pop rcx         ; saved user RIP → rcx for sysretq
    pop r11         ; saved RFLAGS  → r11 for sysretq

    ; Restore user stack
    mov rsp, [rel g_syscall_user_rsp]

    ; Return to userspace
    sysretq
