
#ifndef _PCI_H
#define _PCI_H

#include <types.h>


#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC


#define PCI_VENDOR_ID     0x00
#define PCI_DEVICE_ID     0x02
#define PCI_COMMAND       0x04
#define PCI_STATUS        0x06
#define PCI_CLASS_CODE    0x08   
#define PCI_HEADER_TYPE   0x0E
#define PCI_BAR0          0x10
#define PCI_BAR1          0x14
#define PCI_BAR2          0x18
#define PCI_BAR3          0x1C
#define PCI_BAR4          0x20
#define PCI_BAR5          0x24
#define PCI_INTERRUPT_LINE 0x3C


#define PCI_CMD_IO_SPACE    (1 << 0)
#define PCI_CMD_MEM_SPACE   (1 << 1)
#define PCI_CMD_BUS_MASTER  (1 << 2)


typedef struct {
    uint8_t  bus, slot, func;
    uint16_t vendor_id, device_id;
    uint8_t  class_code, subclass, prog_if, revision;
    uint8_t  irq;
    uint32_t bar[6];
} pci_device_t;


uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read8 (uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void     pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);


bool pci_find_device(uint16_t vendor, uint16_t device, pci_device_t *out);


void pci_enable_bus_master(pci_device_t *dev);


void pci_init(void);

#endif 
