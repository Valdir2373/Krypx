
#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

#include <types.h>


#define MULTIBOOT_INFO_MEMORY       0x00000001  
#define MULTIBOOT_INFO_BOOTDEV      0x00000002  
#define MULTIBOOT_INFO_CMDLINE      0x00000004  
#define MULTIBOOT_INFO_MODS         0x00000008  
#define MULTIBOOT_INFO_MEM_MAP      0x00000040  
#define MULTIBOOT_INFO_FRAMEBUFFER  0x00001000  


#define MULTIBOOT_MEMORY_AVAILABLE  1
#define MULTIBOOT_MEMORY_RESERVED   2


#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002

typedef struct {
    uint32_t flags;

    
    uint32_t mem_lower;     
    uint32_t mem_upper;     

    
    uint32_t boot_device;

    
    uint32_t cmdline;

    
    uint32_t mods_count;
    uint32_t mods_addr;

    
    uint32_t syms[4];

    
    uint32_t mmap_length;
    uint32_t mmap_addr;

    uint32_t drives_length;
    uint32_t drives_addr;

    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;

    
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  color_info[6];
} __attribute__((packed)) multiboot_info_t;


typedef struct {
    uint32_t size;
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;


typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t reserved;
} __attribute__((packed)) multiboot_module_t;

#endif 
