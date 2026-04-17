
#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <types.h>
#include <kernel/idt.h>


#define SYS_EXIT    1
#define SYS_FORK    2
#define SYS_READ    3
#define SYS_WRITE   4
#define SYS_OPEN    5
#define SYS_CLOSE   6
#define SYS_WAITPID 7
#define SYS_GETPID  9
#define SYS_SBRK   10
#define SYS_YIELD  11
#define SYS_SEEK   19
#define SYS_STAT   20
#define SYS_MKDIR  39
#define SYS_UNLINK 10
#define SYS_GETTIME 13


void syscall_init(void);


void syscall_handler(registers_t *regs);

#endif 
