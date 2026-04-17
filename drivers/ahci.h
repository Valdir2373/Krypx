
#ifndef _AHCI_H
#define _AHCI_H

#include <types.h>

bool     ahci_init(void);
bool     ahci_present(void);
uint32_t ahci_sector_count(void);
int      ahci_read_sectors(uint32_t lba, uint8_t count, void *buf);
int      ahci_write_sectors(uint32_t lba, uint8_t count, const void *buf);

#endif
