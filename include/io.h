/*
 * io.h — Port I/O inline para acesso direto ao hardware x86
 * inb/outb/inw/outw sem libc, sem POSIX.
 */
#ifndef _IO_H
#define _IO_H

#include <types.h>

/* Lê um byte da porta de I/O */
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Escreve um byte na porta de I/O */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Lê uma word (16 bits) da porta de I/O */
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Escreve uma word (16 bits) na porta de I/O */
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* Lê um dword (32 bits) da porta de I/O */
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Escreve um dword (32 bits) na porta de I/O */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* Pequeno delay via I/O (para hardware lento) */
static inline void io_wait(void) {
    outb(0x80, 0x00);
}

#endif /* _IO_H */
