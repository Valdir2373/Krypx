
#ifndef _FAT32_H
#define _FAT32_H

#include <types.h>
#include <fs/vfs.h>


typedef struct {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;     
    uint16_t total_sectors_16;     
    uint8_t  media_type;
    uint16_t fat_size_16;          
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];           
} __attribute__((packed)) fat32_bpb_t;


typedef struct {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  create_time_ms;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_hi;   
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_lo;   
    uint32_t file_size;
} __attribute__((packed)) fat32_dirent_t;


#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F   


#define FAT32_EOC   0x0FFFFFF8   
#define FAT32_FREE  0x00000000
#define FAT32_BAD   0x0FFFFFF7


bool fat32_init(uint32_t lba_start);
vfs_node_t *fat32_get_root(void);

#endif 
