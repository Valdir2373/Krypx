
#include <kernel/syscall.h>
#include <kernel/idt.h>
#include <kernel/timer.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <fs/vfs.h>
#include <compat/linux_compat.h>
#include <compat/linux_compat64.h>
#include <lib/string.h>
#include <types.h>

/* syscall_entry is defined in boot/syscall_entry.asm */
extern void syscall_entry(void);
extern uint64_t g_syscall_kernel_rsp;

static inline void wrmsr64(uint32_t msr, uint64_t val) {
    __asm__ volatile ("wrmsr" : :
        "c"(msr),
        "a"((uint32_t)(val & 0xFFFFFFFFULL)),
        "d"((uint32_t)(val >> 32))
        : "memory");
}
static inline uint64_t rdmsr64(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static int64_t sys_exit(int64_t code) {
    process_t *p = process_current();
    if (p) { p->state = PROC_ZOMBIE; p->exit_code = (int32_t)code; }
    schedule();
    return 0;
}

static int64_t sys_open(const char *path, int64_t flags) {
    if (!path) return -1;
    process_t *p = process_current();
    if (!p) return -1;
    int fd = -1, i;
    for (i = 3; i < MAX_FDS; i++) {
        if (!p->fds[i]) { fd = i; break; }
    }
    if (fd < 0) return -1;

    vfs_node_t *node = vfs_resolve(path);
    if (!node) {
        if (flags & O_CREAT) {
            char name[256];
            vfs_node_t *dir = vfs_resolve_parent(path, name);
            if (!dir || !name[0]) return -1;
            if (vfs_create(dir, name, 0644) != 0) return -1;
            node = vfs_resolve(path);
        }
        if (!node) return -1;
    }

    if ((flags & O_TRUNC) && node->write) {
        node->size = 0;
        vfs_write(node, 0, 0, 0);
    }

    p->fds[fd]        = node;
    p->fd_offsets[fd] = (flags & O_APPEND) ? node->size : 0;
    vfs_open(node, (uint32_t)flags);
    return fd;
}

static int64_t sys_close(int64_t fd) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    process_t *p = process_current();
    if (!p || !p->fds[fd]) return -1;
    vfs_close(p->fds[fd]);
    p->fds[fd]        = 0;
    p->fd_offsets[fd] = 0;
    return 0;
}

static int64_t sys_read(uint64_t fd, char *buf, uint64_t count) {
    if (fd == 0) {
        uint64_t i;
        for (i = 0; i < count; i++) {
            char c = keyboard_read();
            buf[i] = c;
            if (c == '\n') { i++; break; }
        }
        return (int64_t)i;
    }
    if (fd == 1 || fd == 2) return -1;
    process_t *p = process_current();
    if (!p || fd >= (uint64_t)MAX_FDS || !p->fds[fd]) return -1;
    uint32_t n = vfs_read(p->fds[fd], (uint32_t)p->fd_offsets[fd], (uint32_t)count, (uint8_t*)buf);
    p->fd_offsets[fd] += n;
    return (int64_t)n;
}

static int64_t sys_write(uint64_t fd, const char *buf, uint64_t count) {
    if (fd == 1 || fd == 2) {
        uint64_t i;
        for (i = 0; i < count; i++) vga_putchar(buf[i]);
        return (int64_t)count;
    }
    process_t *p = process_current();
    if (!p || fd >= (uint64_t)MAX_FDS || !p->fds[fd]) return -1;
    uint32_t n = vfs_write(p->fds[fd], (uint32_t)p->fd_offsets[fd], (uint32_t)count, (const uint8_t*)buf);
    p->fd_offsets[fd] += n;
    return (int64_t)n;
}

static int64_t sys_seek(uint64_t fd, int64_t offset, uint64_t whence) {
    process_t *p = process_current();
    if (!p || fd >= (uint64_t)MAX_FDS || !p->fds[fd]) return -1;
    vfs_node_t *node = p->fds[fd];
    uint64_t pos = p->fd_offsets[fd];
    if (whence == 0)      pos = (uint64_t)offset;
    else if (whence == 1) pos = (uint64_t)((int64_t)pos + offset);
    else if (whence == 2) pos = (uint64_t)((int64_t)node->size + offset);
    p->fd_offsets[fd] = pos;
    return (int64_t)pos;
}

static int64_t sys_getpid(void) {
    process_t *p = process_current();
    return p ? (int64_t)p->pid : 0;
}

static int64_t sys_yield(void) { schedule(); return 0; }

static int64_t sys_sbrk(int64_t inc) {
    process_t *p = process_current();
    if (!p) return -1;
    uint64_t old = p->heap_end;
    p->heap_end += (uint64_t)inc;
    return (int64_t)old;
}

static int64_t sys_gettime(void) { return (int64_t)timer_get_ticks(); }

static int64_t sys_mkdir(const char *path, uint64_t mode) {
    (void)mode;
    if (!path) return -1;
    char name[256];
    vfs_node_t *dir = vfs_resolve_parent(path, name);
    if (!dir || !name[0]) return -1;
    return vfs_mkdir(dir, name, 0755);
}

static int64_t sys_unlink(const char *path) {
    if (!path) return -1;
    char name[256];
    vfs_node_t *dir = vfs_resolve_parent(path, name);
    if (!dir || !name[0]) return -1;
    return vfs_unlink(dir, name);
}

/* int 0x80 handler — Krypx native ABI (64-bit):
 * rax = syscall number, rdi = arg1, rsi = arg2, rdx = arg3 */
void syscall_handler(registers_t *regs) {
    process_t *cur = process_current();
    if (cur && cur->compat_mode == COMPAT_LINUX) {
        linux_syscall_handler(regs);
        return;
    }
    int64_t ret = -1;
    switch (regs->rax) {
        case SYS_EXIT:    ret = sys_exit((int64_t)regs->rdi); break;
        case SYS_READ:    ret = sys_read(regs->rdi, (char*)regs->rsi, regs->rdx); break;
        case SYS_WRITE:   ret = sys_write(regs->rdi, (const char*)regs->rsi, regs->rdx); break;
        case SYS_OPEN:    ret = sys_open((const char*)regs->rdi, (int64_t)regs->rsi); break;
        case SYS_CLOSE:   ret = sys_close((int64_t)regs->rdi); break;
        case SYS_GETPID:  ret = sys_getpid(); break;
        case SYS_SBRK:    ret = sys_sbrk((int64_t)regs->rdi); break;
        case SYS_YIELD:   ret = sys_yield(); break;
        case SYS_SEEK:    ret = sys_seek(regs->rdi, (int64_t)regs->rsi, regs->rdx); break;
        case SYS_GETTIME: ret = sys_gettime(); break;
        case SYS_MKDIR:   ret = sys_mkdir((const char*)regs->rdi, regs->rsi); break;
        default:          ret = -1; break;
    }
    regs->rax = (uint64_t)ret;
}

void syscall_init(void) {
    /* int 0x80 — Krypx native ABI */
    idt_register_handler(IRQ_SYSCALL, syscall_handler);

    /* ── Set up SYSCALL/SYSRETQ instruction path (x86_64 Linux ABI) ──────── */

    /* 1. Enable SCE (SysCall Enable) bit in EFER MSR */
    uint64_t efer = rdmsr64(0xC0000080UL);
    efer |= 1ULL;   /* bit 0 = SCE */
    wrmsr64(0xC0000080UL, efer);

    /* 2. STAR MSR — segment selectors for SYSCALL and SYSRETQ
     *   STAR[47:32] = 0x0008 → on SYSCALL: CS=0x08 (kernel code), SS=0x10 (kernel data)
     *   STAR[63:48] = 0x0018 → on SYSRETQ: CS=0x18+16=0x28|RPL3=0x2B (user code64)
     *                                       SS=0x18+8 =0x20|RPL3=0x23 (user data)
     * (GDT layout: 0x08=kernel code64, 0x10=kernel data,
     *              0x18=user code32, 0x20=user data, 0x28=user code64)
     */
    uint64_t star = (0x0018ULL << 48) | (0x0008ULL << 32);
    wrmsr64(0xC0000081UL, star);

    /* 3. LSTAR MSR — handler address for SYSCALL */
    wrmsr64(0xC0000082UL, (uint64_t)(uintptr_t)syscall_entry);

    /* 4. FMASK MSR — RFLAGS bits to clear on SYSCALL (clear IF=bit9 to disable IRQs) */
    wrmsr64(0xC0000084UL, 0x200ULL);

    /* 5. Init kernel-service data structures for 64-bit Linux compat */
    linux_syscall64_init();

    /* 6. Seed g_syscall_kernel_rsp with a valid stack for the first syscall */
    /* It will be updated to each process's kernel_stack on every context switch */
    {
        extern uint64_t g_syscall_kernel_rsp;
        /* Temporarily point to a static emergency stack until first schedule() */
        static uint8_t _emergency_kstack[4096] __attribute__((aligned(16)));
        g_syscall_kernel_rsp = (uint64_t)(uintptr_t)(_emergency_kstack + 4096);
    }
}
