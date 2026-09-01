#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include <stdint.h>

void timer_init(uint32_t tick_hz);
void timer_acknowledge(void);
void timer_schedule_next(void);
int timer_is_interrupt(uint32_t mcause);
uint64_t timer_now(void);

#endif /* KERNEL_TIMER_H */
