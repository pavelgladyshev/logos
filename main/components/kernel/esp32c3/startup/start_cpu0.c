#include "console.h"
#include "hal/wdt_hal.h"
#include "soc/soc_caps.h"

int main();

extern void kernel_start(void) __attribute__((noreturn));

/*
 * ESP-IDF's default call_start_cpu0() performs the low-level CPU, memory,
 * cache, flash, clock, and UART setup before dispatching through its weak
 * start_cpu0() hook. This strong definition takes control at that boundary,
 * before ESP-IDF starts FreeRTOS.
 */
void start_cpu0(void)
{
    logos_printf("Start cpu0\n");
#if SOC_RTC_WDT_SUPPORTED
    /* Normal ESP-IDF startup disables this later; logOS does not reach it. */
    wdt_hal_context_t rtc_wdt = RWDT_HAL_CONTEXT_DEFAULT();

    wdt_hal_write_protect_disable(&rtc_wdt);
    wdt_hal_disable(&rtc_wdt);
    wdt_hal_write_protect_enable(&rtc_wdt);
    logos_printf("Watchdogs disabled\n");
#endif


    main();
    __builtin_unreachable();
}
