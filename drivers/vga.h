/*
 * drivers/vga.h — Driver VGA modo texto 80x25
 * Acesso direto ao buffer de vídeo em 0xB8000. Sem libc, sem POSIX.
 */
#ifndef _VGA_H
#define _VGA_H

#include <types.h>

/* Dimensões do modo texto padrão */
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

/* Cores VGA (4 bits: foreground e background) */
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

/* Inicializa o terminal VGA (limpa tela, posiciona cursor) */
void vga_init(void);

/* Limpa a tela com a cor de fundo atual */
void vga_clear(void);

/* Define a cor padrão (foreground + background) */
void vga_set_color(vga_color_t fg, vga_color_t bg);

/* Escreve um caractere na posição atual do cursor */
void vga_putchar(char c);

/* Escreve uma string na posição atual do cursor */
void vga_puts(const char *str);

/* Escreve uma string com cor específica (restaura cor anterior) */
void vga_puts_colored(const char *str, vga_color_t fg, vga_color_t bg);

/* Move o cursor para posição (col, row) */
void vga_set_cursor(uint8_t col, uint8_t row);

/* Obtém posição atual do cursor */
void vga_get_cursor(uint8_t *col, uint8_t *row);

/* Escreve um número hexadecimal (debug) */
void vga_put_hex(uint32_t val);

/* Escreve um número decimal (debug) */
void vga_put_dec(uint32_t val);

/* Avança uma linha (scroll se necessário) */
void vga_newline(void);

#endif /* _VGA_H */
