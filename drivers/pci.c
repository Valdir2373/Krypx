/*
 * drivers/pci.c — Barramento PCI: leitura/escrita no espaço de configuração
 * Varredura bruta (força bruta) de todos os slots.
 */

#include <drivers/pci.h>
#include <io.h>
#include <types.h>

/* ============================================================
 * Acesso ao espaço de configuração PCI via mecanismo 1
 * ============================================================ */
static uint32_t pci_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)(0x80000000u
                    | ((uint32_t)bus  << 16)
                    | ((uint32_t)slot << 11)
                    | ((uint32_t)func << 8)
                    | (offset & 0xFC));
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, slot, func, offset));
    return (uint16_t)(inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8));
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, slot, func, offset));
    return (uint8_t)(inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8));
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t tmp = pci_read32(bus, slot, func, offset & ~2u);
    int shift = (offset & 2) * 8;
    tmp &= ~(0xFFFFu << shift);
    tmp |= ((uint32_t)val << shift);
    pci_write32(bus, slot, func, offset & ~2u, tmp);
}

/* ============================================================
 * Varredura PCI
 * ============================================================ */
bool pci_find_device(uint16_t vendor, uint16_t device, pci_device_t *out) {
    uint8_t bus, slot, func;
    for (bus = 0; bus < 8; bus++) {
        for (slot = 0; slot < 32; slot++) {
            for (func = 0; func < 8; func++) {
                uint32_t id = pci_read32(bus, slot, func, PCI_VENDOR_ID);
                uint16_t v  = id & 0xFFFF;
                uint16_t d  = (id >> 16) & 0xFFFF;
                if (v == 0xFFFF) continue;  /* Slot vazio */

                if (v == vendor && d == device) {
                    out->bus      = bus;
                    out->slot     = slot;
                    out->func     = func;
                    out->vendor_id = v;
                    out->device_id = d;

                    uint32_t cls = pci_read32(bus, slot, func, PCI_CLASS_CODE);
                    out->revision  = cls & 0xFF;
                    out->prog_if   = (cls >> 8)  & 0xFF;
                    out->subclass  = (cls >> 16) & 0xFF;
                    out->class_code = (cls >> 24) & 0xFF;

                    out->irq = pci_read8(bus, slot, func, PCI_INTERRUPT_LINE);

                    uint8_t b;
                    for (b = 0; b < 6; b++) {
                        out->bar[b] = pci_read32(bus, slot, func,
                                                  PCI_BAR0 + b * 4);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

void pci_enable_bus_master(pci_device_t *dev) {
    uint16_t cmd = pci_read16(dev->bus, dev->slot, dev->func, PCI_COMMAND);
    cmd |= PCI_CMD_BUS_MASTER | PCI_CMD_MEM_SPACE | PCI_CMD_IO_SPACE;
    pci_write16(dev->bus, dev->slot, dev->func, PCI_COMMAND, cmd);
}

void pci_init(void) {
    /* Por enquanto, varredura é feita on-demand via pci_find_device */
}
