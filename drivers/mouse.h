/*
 * drivers/mouse.h — Driver PS/2 Mouse (IRQ12)
 * Recebe pacotes de 3 bytes e converte para posição absoluta.
 */
#ifndef _MOUSE_H
#define _MOUSE_H

#include <types.h>

/* Inicializa o mouse PS/2 (habilita aux port, registra IRQ12) */
void mouse_init(void);

/* Posição absoluta (coordenadas de tela, em pixels) */
int     mouse_get_x(void);
int     mouse_get_y(void);

/* Estado dos botões: bit0=esquerdo, bit1=direito, bit2=meio */
uint8_t mouse_get_buttons(void);
uint8_t mouse_get_prev_buttons(void);

#endif /* _MOUSE_H */
