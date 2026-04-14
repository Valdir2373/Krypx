/*
 * kernel/timer.h — PIT (Programmable Interval Timer) 8253/8254
 * Configura o PIT para ~1000 Hz (1 tick por milissegundo).
 */
#ifndef _TIMER_H
#define _TIMER_H

#include <types.h>

/* Frequência base do PIT (Hz) */
#define PIT_BASE_FREQ  1193182

/* Frequência alvo do timer do kernel */
#define TIMER_HZ       1000

/* Inicializa o PIT e registra o handler no IRQ0 */
void timer_init(void);

/* Retorna o número de ticks desde o boot */
uint32_t timer_get_ticks(void);

/* Retorna segundos desde o boot */
uint32_t timer_get_seconds(void);

/* Busy-wait por N milissegundos (usa ticks — só use antes do scheduler) */
void timer_sleep_ms(uint32_t ms);

#endif /* _TIMER_H */
