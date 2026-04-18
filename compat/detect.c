

#include "detect.h"


#define EI_OSABI        7

#define ELFOSABI_SYSV   0x00   
#define ELFOSABI_LINUX  0x03   
#define ELFOSABI_KRYPX  0xFF   

binary_type_t detect_binary_type(const uint8_t *data, size_t size) {
    if (!data || size < 4) return BINARY_UNKNOWN;

    
    if (data[0] == 0x7F && data[1] == 'E' &&
        data[2] == 'L'  && data[3] == 'F') {

        if (size < 8) return BINARY_UNKNOWN;

        uint8_t osabi = data[EI_OSABI];
        if (osabi == ELFOSABI_KRYPX) return BINARY_KRYPX_ELF;

        
        return BINARY_LINUX_ELF;
    }

    
    if (data[0] == 'M' && data[1] == 'Z') return BINARY_WINDOWS_PE;

    return BINARY_UNKNOWN;
}

const char *binary_type_name(binary_type_t type) {
    switch (type) {
        case BINARY_KRYPX_ELF:  return "Krypx ELF (nativo)";
        case BINARY_LINUX_ELF:  return "Linux ELF i386";
        case BINARY_WINDOWS_PE: return "Windows PE/EXE";
        default:                return "Desconhecido";
    }
}
