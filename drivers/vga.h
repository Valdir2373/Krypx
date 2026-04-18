
#ifndef _VGA_H
#define _VGA_H

#include <types.h>


#define VGA_WIDTH   80
#define VGA_HEIGHT  25


typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW        = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;


void vga_init(void);


void vga_clear(void);


void vga_set_color(vga_color_t fg, vga_color_t bg);


void vga_putchar(char c);


void vga_puts(const char *str);


void vga_puts_colored(const char *str, vga_color_t fg, vga_color_t bg);


void vga_set_cursor(uint8_t col, uint8_t row);


void vga_get_cursor(uint8_t *col, uint8_t *row);


void vga_put_hex(uint32_t val);


void vga_put_dec(uint32_t val);


void vga_newline(void);

#endif 
