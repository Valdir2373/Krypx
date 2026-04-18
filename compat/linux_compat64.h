#ifndef _LINUX_COMPAT64_H
#define _LINUX_COMPAT64_H

#include <types.h>

/* x86_64 syscall frame — must match syscall_entry.asm push order */
typedef struct {
    uint64_t rax;   /* +0  syscall number (in) / return value (out) */
    uint64_t rdi;   /* +8  arg1 */
    uint64_t rsi;   /* +16 arg2 */
    uint64_t rdx;   /* +24 arg3 */
    uint64_t r10;   /* +32 arg4 */
    uint64_t r8;    /* +40 arg5 */
    uint64_t r9;    /* +48 arg6 */
    uint64_t rcx;   /* +56 saved user RIP */
    uint64_t r11;   /* +64 saved user RFLAGS */
} syscall64_frame_t;

void linux_syscall64_init(void);
void linux_syscall64_handler(syscall64_frame_t *f);

/* Kernel service socket API (used by X11 server and other kernel services) */
int  lx64_register_kernel_service(const char *path);
int  lx64_ksvc_read(int svc_idx, void *buf, uint32_t max);
int  lx64_ksvc_write(int svc_idx, const void *buf, uint32_t len);
bool lx64_ksvc_has_client(int svc_idx);

#endif
