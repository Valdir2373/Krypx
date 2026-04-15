/*
 * drivers/mouse.c — Driver PS/2 Mouse
 * Protocolo padrão PS/2: pacotes de 3 bytes via IRQ12.
 * Byte0=flags (botões+sinais), Byte1=dX, Byte2=dY (Y invertido).
 * Acumula deltas no handler IRQ e expõe posição absoluta.
 */

#include <drivers/mouse.h>
#include <drivers/framebuffer.h>
#include <kernel/idt.h>
#include <io.h>
#include <types.h>

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define IRQ_MOUSE   44   /* IRQ12 → vetor 44 após remapeamento do PIC */

/* Posição e estado atuais */
static int     mouse_x    = 512;
static int     mouse_y    = 384;
static uint8_t mouse_btns = 0;
static uint8_t mouse_prev = 0;

/* Buffer do pacote de 3 bytes */
static uint8_t pkt[3];
static int     pkt_idx = 0;

/* ---- Helpers PS/2 ---- */

static void ps2_wait_write(void) {
    uint32_t i = 100000;
    while ((inb(PS2_STATUS) & 0x02) && --i);
}

static void ps2_wait_read(void) {
    uint32_t i = 100000;
    while (!(inb(PS2_STATUS) & 0x01) && --i);
}

/* Envia um byte para o mouse (via controlador PS/2) */
static void mouse_send(uint8_t cmd) {
    ps2_wait_write();
    outb(PS2_STATUS, 0xD4);    /* Rota o próximo byte para o mouse */
    ps2_wait_write();
    outb(PS2_DATA, cmd);
    ps2_wait_read();
    inb(PS2_DATA);             /* Descarta ACK */
}

/* ---- Handler IRQ12 ---- */

static void mouse_irq_handler(registers_t *regs) {
    (void)regs;
    uint8_t byte = inb(PS2_DATA);

    /* Byte 0: bit 3 deve ser sempre 1 (sincronização do pacote) */
    if (pkt_idx == 0 && !(byte & 0x08)) {
        pic_send_eoi(IRQ_MOUSE);
        return;
    }

    pkt[pkt_idx++] = byte;

    if (pkt_idx == 3) {
        pkt_idx = 0;

        uint8_t flags = pkt[0];
        int     dx    = (int)(int8_t)pkt[1];
        int     dy    = (int)(int8_t)pkt[2];

        /* Ignorar pacotes com overflow */
        if (flags & 0xC0) {
            pic_send_eoi(IRQ_MOUSE);
            return;
        }

        /* Atualizar posição absoluta.
         * No PS/2 o eixo Y positivo = mover para cima → inverter. */
        mouse_x += dx;
        mouse_y -= dy;

        /* Limitar à tela */
        int max_x = fb.width  ? (int)fb.width  - 1 : 1023;
        int max_y = fb.height ? (int)fb.height - 1 : 767;
        if (mouse_x < 0)     mouse_x = 0;
        if (mouse_y < 0)     mouse_y = 0;
        if (mouse_x > max_x) mouse_x = max_x;
        if (mouse_y > max_y) mouse_y = max_y;

        mouse_prev  = mouse_btns;
        mouse_btns  = flags & 0x07;   /* bits 0-2: esq, dir, meio */
    }

    pic_send_eoi(IRQ_MOUSE);
}

/* ---- API pública ---- */

void mouse_init(void) {
    pkt_idx    = 0;
    mouse_x    = 512;
    mouse_y    = 384;
    mouse_btns = 0;
    mouse_prev = 0;

    /* 1. Habilitar porta auxiliar (mouse) */
    ps2_wait_write();
    outb(PS2_STATUS, 0xA8);

    /* 2. Habilitar IRQ12 no Compaq Status Byte */
    ps2_wait_write();
    outb(PS2_STATUS, 0x20);    /* Lê config do controlador */
    ps2_wait_read();
    uint8_t cfg = (uint8_t)(inb(PS2_DATA) | 0x02); /* Bit 1 = ativa IRQ12 */
    ps2_wait_write();
    outb(PS2_STATUS, 0x60);    /* Escreve config */
    ps2_wait_write();
    outb(PS2_DATA, cfg);

    /* 3. Defaults + habilitar envio de dados */
    mouse_send(0xF6);   /* Set Defaults */
    mouse_send(0xF4);   /* Enable Data Reporting */

    idt_register_handler(IRQ_MOUSE, mouse_irq_handler);
    pic_unmask_irq(12);
}

int     mouse_get_x(void)            { return mouse_x;    }
int     mouse_get_y(void)            { return mouse_y;    }
uint8_t mouse_get_buttons(void)      { return mouse_btns; }
uint8_t mouse_get_prev_buttons(void) { return mouse_prev; }
