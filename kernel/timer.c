

#include <kernel/timer.h>
#include <kernel/idt.h>
#include <types.h>
#include <io.h>


extern void schedule(void);


#define PIT_CHANNEL0  0x40   
#define PIT_CMD       0x43   


#define PIT_CMD_RATE  0x36


static volatile uint32_t ticks    = 0;
static uint32_t          rtc_base = 0;


static void timer_handler(registers_t *regs) {
    (void)regs;
    ticks++;
    pic_send_eoi(32);
    schedule();   
}

void timer_init(void) {
    uint16_t divisor = PIT_BASE_FREQ / TIMER_HZ;

    
    outb(PIT_CMD, PIT_CMD_RATE);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)(divisor >> 8));

    
    idt_register_handler(IRQ_TIMER, timer_handler);

    
    pic_unmask_irq(0);
}

uint32_t timer_get_ticks(void) {
    return ticks;
}

uint32_t timer_get_seconds(void) {
    return rtc_base + ticks / TIMER_HZ;
}

void timer_set_rtc_base(uint32_t h, uint32_t m, uint32_t s) {
    rtc_base = h * 3600 + m * 60 + s;
}

void timer_sleep_ms(uint32_t ms) {
    uint32_t target = ticks + ms;
    while (ticks < target) {
        __asm__ volatile ("hlt");
    }
}
