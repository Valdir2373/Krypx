
#include <kernel/syscall.h>
#include <kernel/idt.h>
#include <kernel/timer.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <fs/vfs.h>
#include <compat/linux_compat.h>
#include <lib/string.h>
#include <types.h>

static int32_t sys_exit(int32_t code) {
    process_t *p = process_current();
    if (p) { p->state = PROC_ZOMBIE; p->exit_code = code; }
    schedule();
    return 0;
}

static int32_t sys_open(const char *path, int32_t flags) {
    if (!path) return -1;
    process_t *p = process_current();
    if (!p) return -1;
    int fd = -1;
    int i;
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
        if (node->impl && node->inode)
            vfs_write(node, 0, 0, 0);  /* update size to 0 */
    }

    p->fds[fd]        = node;
    p->fd_offsets[fd] = (flags & O_APPEND) ? node->size : 0;
    vfs_open(node, (uint32_t)flags);
    return fd;
}

static int32_t sys_close(int32_t fd) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    process_t *p = process_current();
    if (!p || !p->fds[fd]) return -1;
    vfs_close(p->fds[fd]);
    p->fds[fd]        = 0;
    p->fd_offsets[fd] = 0;
    return 0;
}

static int32_t sys_read(uint32_t fd, char *buf, uint32_t count) {
    if (fd == 0) {
        uint32_t i;
        for (i = 0; i < count; i++) {
            char c = keyboard_read();
            buf[i] = c;
            if (c == '\n') { i++; break; }
        }
        return (int32_t)i;
    }
    if (fd == 1 || fd == 2) return -1;
    process_t *p = process_current();
    if (!p || fd >= (uint32_t)MAX_FDS || !p->fds[fd]) return -1;
    uint32_t n = vfs_read(p->fds[fd], p->fd_offsets[fd], count, (uint8_t*)buf);
    p->fd_offsets[fd] += n;
    return (int32_t)n;
}

static int32_t sys_write(uint32_t fd, const char *buf, uint32_t count) {
    if (fd == 1 || fd == 2) {
        uint32_t i;
        for (i = 0; i < count; i++) vga_putchar(buf[i]);
        return (int32_t)count;
    }
    process_t *p = process_current();
    if (!p || fd >= (uint32_t)MAX_FDS || !p->fds[fd]) return -1;
    uint32_t n = vfs_write(p->fds[fd], p->fd_offsets[fd], count, (const uint8_t*)buf);
    p->fd_offsets[fd] += n;
    return (int32_t)n;
}

static int32_t sys_seek(uint32_t fd, int32_t offset, uint32_t whence) {
    process_t *p = process_current();
    if (!p || fd >= (uint32_t)MAX_FDS || !p->fds[fd]) return -1;
    vfs_node_t *node = p->fds[fd];
    uint32_t pos = p->fd_offsets[fd];
    if (whence == 0) pos = (uint32_t)offset;
    else if (whence == 1) pos = (uint32_t)((int32_t)pos + offset);
    else if (whence == 2) pos = (uint32_t)((int32_t)node->size + offset);
    p->fd_offsets[fd] = pos;
    return (int32_t)pos;
}

static int32_t sys_getpid(void) {
    process_t *p = process_current();
    return p ? (int32_t)p->pid : 0;
}

static int32_t sys_yield(void) { schedule(); return 0; }

static int32_t sys_sbrk(int32_t inc) {
    process_t *p = process_current();
    if (!p) return -1;
    uint32_t old = p->heap_end;
    p->heap_end += (uint32_t)inc;
    return (int32_t)old;
}

static int32_t sys_gettime(void) {
    return (int32_t)timer_get_ticks();
}

static int32_t sys_mkdir(const char *path, uint32_t mode) {
    (void)mode;
    if (!path) return -1;
    char name[256];
    vfs_node_t *dir = vfs_resolve_parent(path, name);
    if (!dir || !name[0]) return -1;
    return vfs_mkdir(dir, name, 0755);
}

static int32_t sys_unlink(const char *path) {
    if (!path) return -1;
    char name[256];
    vfs_node_t *dir = vfs_resolve_parent(path, name);
    if (!dir || !name[0]) return -1;
    return vfs_unlink(dir, name);
}

void syscall_handler(registers_t *regs) {
    int32_t ret = -1;
    process_t *cur = process_current();
    if (cur && cur->compat_mode == COMPAT_LINUX) {
        linux_syscall_handler(regs);
        return;
    }
    switch (regs->eax) {
        case SYS_EXIT:    ret = sys_exit((int32_t)regs->ebx); break;
        case SYS_READ:    ret = sys_read(regs->ebx, (char*)regs->ecx, regs->edx); break;
        case SYS_WRITE:   ret = sys_write(regs->ebx, (const char*)regs->ecx, regs->edx); break;
        case SYS_OPEN:    ret = sys_open((const char*)regs->ebx, (int32_t)regs->ecx); break;
        case SYS_CLOSE:   ret = sys_close((int32_t)regs->ebx); break;
        case SYS_GETPID:  ret = sys_getpid(); break;
        case SYS_SBRK:    ret = sys_sbrk((int32_t)regs->ebx); break;
        case SYS_YIELD:   ret = sys_yield(); break;
        case SYS_SEEK:    ret = sys_seek(regs->ebx, (int32_t)regs->ecx, regs->edx); break;
        case SYS_GETTIME: ret = sys_gettime(); break;
        case SYS_MKDIR:   ret = sys_mkdir((const char*)regs->ebx, regs->ecx); break;
        /* case 10 handled by SYS_SBRK above */
        default:          ret = -1; break;
    }
    regs->eax = (uint32_t)ret;
}

void syscall_init(void) {
    idt_register_handler(IRQ_SYSCALL, syscall_handler);
}
