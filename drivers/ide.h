
#ifndef _IDE_H
#define _IDE_H

#include <types.h>

#define IDE_SECTOR_SIZE  512


void ide_init(void);


bool ide_disk_present(void);


bool ide_read_sectors(uint32_t lba, uint8_t count, void *buffer);


bool ide_write_sectors(uint32_t lba, uint8_t count, const void *buffer);


uint32_t ide_get_sector_count(void);

#endif 
