/*
 * drivers/e1000.h — Driver Intel e1000 (82540EM) Gigabit Ethernet
 * PCI Vendor=0x8086, Device=0x100E (emulado pelo QEMU com -device e1000)
 */
#ifndef _E1000_H
#define _E1000_H

#include <types.h>

#define E1000_VENDOR  0x8086
#define E1000_DEVICE  0x100E

/* Inicializa o driver e1000 (detecta via PCI, configura DMA rings) */
bool e1000_init(void);

/* Retorna true se o driver foi iniciado com sucesso */
bool e1000_ready(void);

/* Retorna o MAC address da NIC (6 bytes) */
const uint8_t *e1000_get_mac(void);

/* Envia um frame Ethernet (dados + tamanho em bytes) */
bool e1000_send(const void *data, uint16_t len);

/* Recebe um frame (chamado pelo handler IRQ ou polling) */
/* Retorna tamanho do frame lido em buf, ou 0 se não há frame */
uint16_t e1000_recv(void *buf, uint16_t max_len);

/* Handler de interrupção (registrado no IDT para o IRQ do dispositivo) */
void e1000_irq_handler(void);

/* Callback chamado quando um frame chega (configurado pela camada Ethernet) */
typedef void (*e1000_recv_cb_t)(const void *frame, uint16_t len);
void e1000_set_recv_callback(e1000_recv_cb_t cb);

#endif /* _E1000_H */
