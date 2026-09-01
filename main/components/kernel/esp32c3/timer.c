#include <stdint.h>
#include "esp_cpu.h"
#include "esp32c3/rom/ets_sys.h"
#include "soc/interrupts.h"
#include "soc/systimer_struct.h"
#include "hal/systimer_ll.h"
#include "timer.h"
#include "trap.h"

#define TIMER_COUNTER 1
#define TIMER_ALARM   0
#define TIMER_CPU_INTERRUPT 3
#define SYSTIMER_TICKS_PER_SECOND 16000000ULL

static uint64_t timer_interval_ticks;

uint64_t timer_now(void)
{
    uint32_t hi, lo, lo_check;

    systimer_ll_counter_snapshot(&SYSTIMER, TIMER_COUNTER);

    while (!systimer_ll_is_counter_value_valid(
        &SYSTIMER, TIMER_COUNTER)) {
    }

    /* LO-HI-LO avoids returning a torn value across a low-word rollover. */
    do {
        lo = systimer_ll_get_counter_value_low(&SYSTIMER, TIMER_COUNTER);
        hi = systimer_ll_get_counter_value_high(&SYSTIMER, TIMER_COUNTER);
        lo_check = systimer_ll_get_counter_value_low(&SYSTIMER, TIMER_COUNTER);
    } while (lo != lo_check);

    return ((uint64_t)hi << 32) | lo;
}

static void timer_set_deadline(uint64_t deadline)
{
    systimer_ll_set_alarm_target(
        &SYSTIMER, TIMER_ALARM, deadline);

    systimer_ll_apply_alarm_value(
        &SYSTIMER, TIMER_ALARM);

    systimer_ll_enable_alarm(
        &SYSTIMER, TIMER_ALARM, true);
}

void timer_schedule_next(void)
{
    timer_set_deadline(timer_now() + timer_interval_ticks);
}

void timer_acknowledge(void)
{
    systimer_ll_clear_alarm_int(&SYSTIMER, TIMER_ALARM);
}

int timer_is_interrupt(uint32_t mcause)
{
    return mcause == (0x80000000U | TIMER_CPU_INTERRUPT);
}

void timer_init(uint32_t tick_hz)
{
    if (tick_hz == 0) {
        tick_hz = 100;
    }
    timer_interval_ticks = SYSTIMER_TICKS_PER_SECOND / tick_hz;

    systimer_ll_enable_clock(&SYSTIMER, true);

    systimer_ll_enable_counter(
        &SYSTIMER, TIMER_COUNTER, true);

    systimer_ll_connect_alarm_counter(
        &SYSTIMER, TIMER_ALARM, TIMER_COUNTER);

    systimer_ll_enable_alarm_oneshot(
        &SYSTIMER, TIMER_ALARM);

    systimer_ll_clear_alarm_int(
        &SYSTIMER, TIMER_ALARM);

    systimer_ll_enable_alarm_int(
        &SYSTIMER, TIMER_ALARM, true);

    /* Route SYSTIMER target 0 to a free CPU interrupt input.  The routing
     * helper is implemented in mask ROM, so it does not require IDF runtime. */
    intr_matrix_set(0, ETS_SYSTIMER_TARGET0_INTR_SOURCE, TIMER_CPU_INTERRUPT);
    esp_cpu_intr_set_type(TIMER_CPU_INTERRUPT, ESP_CPU_INTR_TYPE_LEVEL);
    esp_cpu_intr_set_priority(TIMER_CPU_INTERRUPT, 1);

    set_mie(get_mie() | (1U << TIMER_CPU_INTERRUPT));
    timer_schedule_next();
    set_mstatus_bit(0x80); /* MPIE: enable interrupts after the next mret. */
}
