/*
 * types.h — Definições de tipos primitivos do Krypx
 * Sem dependência de libc. Tudo bare-metal.
 */
#ifndef _TYPES_H
#define _TYPES_H

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

typedef uint32_t            size_t;
typedef int32_t             ssize_t;
typedef uint32_t            uintptr_t;

typedef enum { false = 0, true = 1 } bool;

#define NULL ((void*)0)

#define UINT8_MAX   0xFF
#define UINT16_MAX  0xFFFF
#define UINT32_MAX  0xFFFFFFFFU

#endif /* _TYPES_H */
