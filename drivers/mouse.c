

#include <drivers/mouse.h>
#include <drivers/framebuffer.h>
#include <kernel/idt.h>
#include <io.h>
#include <types.h>

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define IRQ_MOUSE   44   


static int     mouse_x    = 512;
static int     mouse_y    = 384;
static uint8_t mouse_btns = 0;
static uint8_t mouse_prev = 0;


static uint8_t pkt[3];
static int     pkt_idx = 0;



static void ps2_wait_write(void) {
    uint32_t i = 100000;
    while ((inb(PS2_STATUS) & 0x02) && --i);
}

static void ps2_wait_read(void) {
    uint32_t i = 100000;
    while (!(inb(PS2_STATUS) & 0x01) && --i);
}


static void mouse_send(uint8_t cmd) {
    ps2_wait_write();
    outb(PS2_STATUS, 0xD4);    
    ps2_wait_write();
    outb(PS2_DATA, cmd);
    ps2_wait_read();
    inb(PS2_DATA);             
}



static void mouse_irq_handler(registers_t *regs) {
    (void)regs;
    uint8_t byte = inb(PS2_DATA);

    
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

        
        if (flags & 0xC0) {
            pic_send_eoi(IRQ_MOUSE);
            return;
        }

        
        mouse_x += dx;
        mouse_y -= dy;

        
        int max_x = fb.width  ? (int)fb.width  - 1 : 1023;
        int max_y = fb.height ? (int)fb.height - 1 : 767;
        if (mouse_x < 0)     mouse_x = 0;
        if (mouse_y < 0)     mouse_y = 0;
        if (mouse_x > max_x) mouse_x = max_x;
        if (mouse_y > max_y) mouse_y = max_y;

        mouse_prev  = mouse_btns;
        mouse_btns  = flags & 0x07;   
    }

    pic_send_eoi(IRQ_MOUSE);
}



void mouse_init(void) {
    pkt_idx    = 0;
    mouse_x    = 512;
    mouse_y    = 384;
    mouse_btns = 0;
    mouse_prev = 0;

    
    ps2_wait_write();
    outb(PS2_STATUS, 0xA8);

    
    ps2_wait_write();
    outb(PS2_STATUS, 0x20);    
    ps2_wait_read();
    uint8_t cfg = (uint8_t)(inb(PS2_DATA) | 0x02); 
    ps2_wait_write();
    outb(PS2_STATUS, 0x60);    
    ps2_wait_write();
    outb(PS2_DATA, cfg);

    
    mouse_send(0xF6);   
    mouse_send(0xF4);   

    idt_register_handler(IRQ_MOUSE, mouse_irq_handler);
    pic_unmask_irq(12);
}

int     mouse_get_x(void)            { return mouse_x;    }
int     mouse_get_y(void)            { return mouse_y;    }
uint8_t mouse_get_buttons(void)      { return mouse_btns; }
uint8_t mouse_get_prev_buttons(void) { return mouse_prev; }
