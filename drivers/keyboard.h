/*
 * drivers/keyboard.h — Driver teclado PS/2 (IRQ1)
 * Converte scancodes Set 1 para ASCII. Suporta Shift, Caps Lock.
 */
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <types.h>

/* Portas PS/2 */
#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64

/* Inicializa o driver de teclado e registra o handler no IRQ1 */
void keyboard_init(void);

/* Retorna o último caractere ASCII lido (0 se nenhum) — não bloqueante */
char keyboard_getchar(void);

/* Aguarda um caractere ASCII e retorna — bloqueante */
char keyboard_read(void);

/* Retorna true se há um caractere disponível no buffer */
bool keyboard_available(void);

#endif /* _KEYBOARD_H */
