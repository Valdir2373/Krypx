/*
 * drivers/e1000.c — Intel e1000 Gigabit Ethernet (MMIO)
 * Suporta o modelo 82540EM emulado pelo QEMU (-device e1000,netdev=net0).
 *
 * Implementação:
 *   - Detecta via PCI (8086:100E)
 *   - Usa MMIO (BAR0)
 *   - Ring de transmissão: TX_RING_SIZE descritores de 16 bytes
 *   - Ring de recepção: RX_RING_SIZE descritores de 16 bytes
 *   - Polling no recv (sem DMA interrupt por padrão para simplificar)
 */

#include <drivers/e1000.h>
#include <drivers/pci.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <lib/string.h>
#include <io.h>
#include <types.h>

/* ============================================================
 * Registradores MMIO do e1000 (offsets a partir de BAR0)
 * ============================================================ */
#define E1000_CTRL    0x0000   /* Device Control */
#define E1000_STATUS  0x0008   /* Device Status */
#define E1000_EECD    0x0010   /* EEPROM Control */
#define E1000_EERD    0x0014   /* EEPROM Read */
#define E1000_ICR     0x00C0   /* Interrupt Cause Read */
#define E1000_IMS     0x00D0   /* Interrupt Mask Set */
#define E1000_IMC     0x00D8   /* Interrupt Mask Clear */
#define E1000_RCTL    0x0100   /* Receive Control */
#define E1000_TCTL    0x0400   /* Transmit Control */
#define E1000_TIPG    0x0410   /* Transmit IPG */
#define E1000_RDBAL   0x2800   /* RX Desc Base Low */
#define E1000_RDBAH   0x2804   /* RX Desc Base High */
#define E1000_RDLEN   0x2808   /* RX Desc Ring Length */
#define E1000_RDH     0x2810   /* RX Desc Head */
#define E1000_RDT     0x2818   /* RX Desc Tail */
#define E1000_TDBAL   0x3800   /* TX Desc Base Low */
#define E1000_TDBAH   0x3804   /* TX Desc Base High */
#define E1000_TDLEN   0x3808   /* TX Desc Ring Length */
#define E1000_TDH     0x3810   /* TX Desc Head */
#define E1000_TDT     0x3818   /* TX Desc Tail */
#define E1000_MTA     0x5200   /* Multicast Table Array (128 × 4 bytes) */
#define E1000_RAL     0x5400   /* Receive Address Low (MAC bytes 0-3) */
#define E1000_RAH     0x5404   /* Receive Address High (MAC bytes 4-5 + AV bit) */

/* CTRL bits */
#define E1000_CTRL_LRST   (1 << 3)
#define E1000_CTRL_ASDE   (1 << 5)
#define E1000_CTRL_SLU    (1 << 6)
#define E1000_CTRL_RST    (1 << 26)

/* RCTL bits */
#define E1000_RCTL_EN     (1 << 1)
#define E1000_RCTL_SBP    (1 << 2)
#define E1000_RCTL_UPE    (1 << 3)
#define E1000_RCTL_MPE    (1 << 4)
#define E1000_RCTL_BAM    (1 << 15)  /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE  0x00000000 /* Buffer size 2048 bytes */
#define E1000_RCTL_SECRC  (1 << 26)  /* Strip Ethernet CRC */

/* TCTL bits */
#define E1000_TCTL_EN     (1 << 1)
#define E1000_TCTL_PSP    (1 << 3)
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12

/* TX Descriptor status bits */
#define E1000_TXD_STAT_DD  (1 << 0)  /* Descriptor Done */
#define E1000_TXD_CMD_EOP  (1 << 0)  /* End of Packet */
#define E1000_TXD_CMD_IFCS (1 << 1)  /* Insert FCS */
#define E1000_TXD_CMD_RS   (1 << 3)  /* Report Status */

/* RX Descriptor status bits */
#define E1000_RXD_STAT_DD  (1 << 0)  /* Descriptor Done */
#define E1000_RXD_STAT_EOP (1 << 1)  /* End of Packet */

/* Ring sizes (deve ser potência de 2) */
#define TX_RING_SIZE  16
#define RX_RING_SIZE  16
#define RX_BUF_SIZE   2048

/* Estrutura de TX descriptor (16 bytes) */
typedef struct {
    uint64_t addr;      /* Buffer físico */
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

/* Estrutura de RX descriptor (16 bytes) */
typedef struct {
    uint64_t addr;      /* Buffer físico */
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

/* ============================================================
 * Estado do driver
 * ============================================================ */
static bool      g_ready = false;
static uint32_t  g_mmio  = 0;      /* Base MMIO (BAR0, identity mapped) */
static uint8_t   g_mac[6];

/* Rings de descritores (alinhados a 16 bytes) */
static e1000_tx_desc_t tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static e1000_rx_desc_t rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));

/* Buffers dos pacotes TX e RX */
static uint8_t tx_buf[TX_RING_SIZE][RX_BUF_SIZE];
static uint8_t rx_buf[RX_RING_SIZE][RX_BUF_SIZE];

static uint32_t tx_tail = 0;  /* Próximo descriptor TX a usar */
static uint32_t rx_tail = 0;  /* Próximo descriptor RX a verificar */

static e1000_recv_cb_t g_recv_cb = 0;

/* ============================================================
 * Acesso MMIO
 * ============================================================ */
static inline uint32_t e1000_read(uint32_t reg) {
    return *(volatile uint32_t *)(g_mmio + reg);
}
static inline void e1000_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(g_mmio + reg) = val;
}

/* ============================================================
 * Leitura do MAC da EEPROM
 * ============================================================ */
static uint16_t eeprom_read(uint8_t addr) {
    e1000_write(E1000_EERD, 1 | ((uint32_t)addr << 8));
    uint32_t val;
    do { val = e1000_read(E1000_EERD); } while (!(val & (1 << 4)));
    return (uint16_t)(val >> 16);
}

static void read_mac(void) {
    uint16_t w0 = eeprom_read(0);
    uint16_t w1 = eeprom_read(1);
    uint16_t w2 = eeprom_read(2);
    g_mac[0] = w0 & 0xFF; g_mac[1] = (w0 >> 8) & 0xFF;
    g_mac[2] = w1 & 0xFF; g_mac[3] = (w1 >> 8) & 0xFF;
    g_mac[4] = w2 & 0xFF; g_mac[5] = (w2 >> 8) & 0xFF;
}

/* ============================================================
 * Inicialização
 * ============================================================ */
bool e1000_init(void) {
    pci_device_t pdev;
    if (!pci_find_device(E1000_VENDOR, E1000_DEVICE, &pdev)) return false;

    pci_enable_bus_master(&pdev);

    /* BAR0: MMIO. Os bits [1:0] indicam tipo: 0x0 = Memory */
    uint32_t bar0 = pdev.bar[0] & ~0xFu;
    g_mmio = bar0;

    /* Mapeia o range MMIO no espaço virtual (identity) */
    extern void vmm_map_range(uint32_t *, uint32_t, uint32_t, uint32_t, uint32_t);
    extern uint32_t *vmm_get_current_dir(void);
    vmm_map_range(vmm_get_current_dir(), bar0, bar0, 0x20000, 0x03);

    /* Reset do dispositivo */
    e1000_write(E1000_CTRL, E1000_CTRL_RST);
    {uint32_t i; for(i=0;i<10000;i++) __asm__ volatile("nop");}

    /* Auto-negotiate + Set Link Up */
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_ASDE | E1000_CTRL_SLU);
    /* Desabilita interrupções */
    e1000_write(E1000_IMC, 0xFFFFFFFF);

    /* Lê MAC */
    read_mac();

    /* Programa o MAC no filtro de recepção */
    e1000_write(E1000_RAL,
        (uint32_t)g_mac[0]
      | ((uint32_t)g_mac[1] << 8)
      | ((uint32_t)g_mac[2] << 16)
      | ((uint32_t)g_mac[3] << 24));
    e1000_write(E1000_RAH,
        (uint32_t)g_mac[4]
      | ((uint32_t)g_mac[5] << 8)
      | (1u << 31));  /* Address Valid */

    /* Zera a Multicast Table Array */
    {uint32_t i; for(i=0;i<128;i++) e1000_write(E1000_MTA + i*4, 0);}

    /* Configura ring de recepção */
    {
        uint32_t i;
        for (i = 0; i < RX_RING_SIZE; i++) {
            rx_ring[i].addr   = (uint64_t)(uint32_t)rx_buf[i];
            rx_ring[i].status = 0;
        }
    }
    e1000_write(E1000_RDBAL, (uint32_t)rx_ring);
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, RX_RING_SIZE * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_RDH, 0);
    rx_tail = RX_RING_SIZE - 1;
    e1000_write(E1000_RDT, rx_tail);
    e1000_write(E1000_RCTL,
        E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_UPE | E1000_RCTL_MPE |
        E1000_RCTL_BAM | E1000_RCTL_BSIZE | E1000_RCTL_SECRC);

    /* Configura ring de transmissão */
    memset(tx_ring, 0, sizeof(tx_ring));
    e1000_write(E1000_TDBAL, (uint32_t)tx_ring);
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, TX_RING_SIZE * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_TDH, 0);
    tx_tail = 0;
    e1000_write(E1000_TDT, 0);
    e1000_write(E1000_TCTL,
        E1000_TCTL_EN | E1000_TCTL_PSP |
        (0x10 << E1000_TCTL_CT_SHIFT) |
        (0x40 << E1000_TCTL_COLD_SHIFT));
    e1000_write(E1000_TIPG, 0x0060200A);

    g_ready = true;
    return true;
}

bool e1000_ready(void)          { return g_ready; }
const uint8_t *e1000_get_mac(void) { return g_mac; }

void e1000_set_recv_callback(e1000_recv_cb_t cb) { g_recv_cb = cb; }

/* ============================================================
 * Transmissão
 * ============================================================ */
bool e1000_send(const void *data, uint16_t len) {
    if (!g_ready || len == 0 || len > RX_BUF_SIZE) return false;

    uint32_t idx = tx_tail % TX_RING_SIZE;

    memcpy(tx_buf[idx], data, len);
    tx_ring[idx].addr   = (uint64_t)(uint32_t)tx_buf[idx];
    tx_ring[idx].length = len;
    tx_ring[idx].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    tx_ring[idx].status = 0;

    tx_tail = (tx_tail + 1) % TX_RING_SIZE;
    e1000_write(E1000_TDT, tx_tail);

    /* Aguarda conclusão (polling) */
    uint32_t timeout = 10000;
    while (!(tx_ring[idx].status & E1000_TXD_STAT_DD) && timeout--) {
        __asm__ volatile("nop");
    }
    return !!(tx_ring[idx].status & E1000_TXD_STAT_DD);
}

/* ============================================================
 * Recepção (polling)
 * ============================================================ */
uint16_t e1000_recv(void *buf, uint16_t max_len) {
    if (!g_ready) return 0;

    uint32_t next = (rx_tail + 1) % RX_RING_SIZE;
    if (!(rx_ring[next].status & E1000_RXD_STAT_DD)) return 0;

    uint16_t len = rx_ring[next].length;
    if (len > max_len) len = max_len;
    memcpy(buf, rx_buf[next], len);

    rx_ring[next].status = 0;
    rx_tail = next;
    e1000_write(E1000_RDT, rx_tail);
    return len;
}

void e1000_irq_handler(void) {
    if (!g_ready) return;
    uint32_t icr = e1000_read(E1000_ICR);  /* ACK interrupções */
    (void)icr;

    if (!g_recv_cb) return;
    static uint8_t tmp[RX_BUF_SIZE];
    uint16_t len;
    while ((len = e1000_recv(tmp, RX_BUF_SIZE)) > 0) {
        g_recv_cb(tmp, len);
    }
}
