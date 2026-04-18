
#ifndef _TIMER_H
#define _TIMER_H

#include <types.h>


#define PIT_BASE_FREQ  1193182


#define TIMER_HZ       1000


void timer_init(void);


uint32_t timer_get_ticks(void);


uint32_t timer_get_seconds(void);


void timer_sleep_ms(uint32_t ms);
void timer_set_rtc_base(uint32_t h, uint32_t m, uint32_t s);

#endif 
