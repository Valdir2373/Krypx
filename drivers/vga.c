

#include <drivers/vga.h>
#include <types.h>
#include <io.h>


#define VGA_BUFFER_ADDR  0xB8000


#define VGA_CTRL_PORT    0x3D4
#define VGA_DATA_PORT    0x3D5
#define VGA_CURSOR_HI    0x0E
#define VGA_CURSOR_LO    0x0F


static uint16_t *vga_buf = (uint16_t *)VGA_BUFFER_ADDR;
static uint8_t  vga_col  = 0;
static uint8_t  vga_row  = 0;
static uint8_t  vga_attr = 0;  


static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}


static inline uint8_t vga_make_attr(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)((bg << 4) | (fg & 0x0F));
}


static void vga_update_hw_cursor(void) {
    uint16_t pos = vga_row * VGA_WIDTH + vga_col;
    outb(VGA_CTRL_PORT, VGA_CURSOR_HI);
    outb(VGA_DATA_PORT, (uint8_t)(pos >> 8));
    outb(VGA_CTRL_PORT, VGA_CURSOR_LO);
    outb(VGA_DATA_PORT, (uint8_t)(pos & 0xFF));
}


static void vga_scroll(void) {
    uint32_t i;
    
    for (i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga_buf[i] = vga_buf[i + VGA_WIDTH];
    }
    
    for (i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        vga_buf[i] = vga_entry(' ', vga_attr);
    }
}

void vga_init(void) {
    vga_attr = vga_make_attr(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_col  = 0;
    vga_row  = 0;
    vga_clear();
}

void vga_clear(void) {
    uint32_t i;
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buf[i] = vga_entry(' ', vga_attr);
    }
    vga_col = 0;
    vga_row = 0;
    vga_update_hw_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_attr = vga_make_attr(fg, bg);
}

void vga_newline(void) {
    vga_col = 0;
    vga_row++;
    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_newline();
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        
        vga_col = (vga_col + 4) & ~3;
        if (vga_col >= VGA_WIDTH) vga_newline();
    } else if (c == '\b') {
        
        if (vga_col > 0) {
            vga_col--;
            vga_buf[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_attr);
        }
    } else {
        vga_buf[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_attr);
        vga_col++;
        if (vga_col >= VGA_WIDTH) vga_newline();
    }
    vga_update_hw_cursor();
}

void vga_puts(const char *str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

void vga_puts_colored(const char *str, vga_color_t fg, vga_color_t bg) {
    uint8_t saved = vga_attr;
    vga_set_color(fg, bg);
    vga_puts(str);
    vga_attr = saved;
}

void vga_set_cursor(uint8_t col, uint8_t row) {
    vga_col = col;
    vga_row = row;
    vga_update_hw_cursor();
}

void vga_get_cursor(uint8_t *col, uint8_t *row) {
    if (col) *col = vga_col;
    if (row) *row = vga_row;
}

void vga_put_hex(uint32_t val) {
    const char hex[] = "0123456789ABCDEF";
    int i;
    vga_puts("0x");
    for (i = 28; i >= 0; i -= 4) {
        vga_putchar(hex[(val >> i) & 0xF]);
    }
}

void vga_put_dec(uint32_t val) {
    char buf[12];
    int i = 0;
    if (val == 0) { vga_putchar('0'); return; }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (--i >= 0) vga_putchar(buf[i]);
}
