
/* AC'97 audio driver — Intel ICH AC97 (PCI 0x8086:0x2415/0x2425) */

#include <drivers/ac97.h>
#include <drivers/pci.h>
#include <io.h>
#include <types.h>

/* AC'97 NAM (Native Audio Mixer) register offsets */
#define NAM_RESET       0x00
#define NAM_MASTER_VOL  0x02   /* Master Output Volume */
#define NAM_PCM_VOL     0x18   /* PCM Output Volume */
#define NAM_MIC_VOL     0x0E   /* MIC Volume */
#define NAM_RECORD_SEL  0x1A

/* Volume register bits: [13:8]=left, [5:0]=right, bit15=mute
   0=max (0dB), 31=min (-46.5dB) */

static uint16_t ac97_nam  = 0;   /* NAM I/O base port */
static uint16_t ac97_nabm = 0;   /* NABM I/O base port */
static bool     ac97_ok   = false;
static int      ac97_vol  = 80;  /* current volume 0-100 */

/* Known Intel ICH AC'97 controller PCI IDs */
static const uint16_t AC97_DEVS[] = {
    0x2415, /* ICH (82801AA) */
    0x2425, /* ICH0 (82801AB) */
    0x2445, /* ICH2 */
    0x2485, /* ICH3 */
    0x24C5, /* ICH4 */
    0x24D5, /* ICH5 */
    0x266E, /* ICH6 */
    0x27DE, /* ICH7 */
    0
};

bool ac97_available(void) { return ac97_ok; }
int  ac97_get_volume(void) { return ac97_vol; }

void ac97_set_volume(int pct) {
    if (!ac97_ok) return;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    ac97_vol = pct;

    if (pct == 0) {
        outw(ac97_nam + NAM_MASTER_VOL, 0x8000); /* mute */
        outw(ac97_nam + NAM_PCM_VOL,    0x8000);
        return;
    }
    /* Attenuation: 0=max, 31=min; pct=100 → att=0, pct=1 → att=30 */
    uint8_t att = (uint8_t)((100 - pct) * 31 / 100);
    uint16_t vol = (uint16_t)(att | ((uint16_t)att << 8));
    outw(ac97_nam + NAM_MASTER_VOL, vol);
    outw(ac97_nam + NAM_PCM_VOL,    vol);
}

void ac97_init(void) {
    /* Scan PCI bus for AC'97 controller */
    uint8_t bus, slot;
    for (bus = 0; bus < 8; bus++) {
        for (slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_read16(bus, slot, 0, PCI_VENDOR_ID);
            if (vendor != 0x8086) continue;
            uint16_t device = pci_read16(bus, slot, 0, PCI_DEVICE_ID);
            int i;
            for (i = 0; AC97_DEVS[i]; i++) {
                if (device == AC97_DEVS[i]) goto found;
            }
        }
    }
    return; /* not found */

found:
    /* BAR0 = NAM I/O space, BAR1 = NABM I/O space */
    ac97_nam  = (uint16_t)(pci_read32(bus, slot, 0, PCI_BAR0) & 0xFFFE);
    ac97_nabm = (uint16_t)(pci_read32(bus, slot, 0, PCI_BAR1) & 0xFFFE);

    /* Enable PCI I/O space + Bus Master */
    uint16_t cmd = pci_read16(bus, slot, 0, PCI_COMMAND);
    pci_write16(bus, slot, 0, PCI_COMMAND,
                (uint16_t)(cmd | PCI_CMD_IO_SPACE | PCI_CMD_BUS_MASTER));

    /* Cold reset */
    outw(ac97_nam + NAM_RESET, 0);

    /* Small delay */
    volatile int d = 100000;
    while (d-- > 0) __asm__ volatile ("nop");

    ac97_ok = true;

    /* Set default volume */
    ac97_set_volume(ac97_vol);
}
