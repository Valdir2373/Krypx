
#ifndef _COMPAT_DETECT_H
#define _COMPAT_DETECT_H

#include <types.h>

typedef enum {
    BINARY_UNKNOWN    = 0,
    BINARY_KRYPX_ELF  = 1,   
    BINARY_LINUX_ELF  = 2,   
    BINARY_WINDOWS_PE = 3,   
} binary_type_t;


binary_type_t detect_binary_type(const uint8_t *data, size_t size);


const char *binary_type_name(binary_type_t type);

#endif 
