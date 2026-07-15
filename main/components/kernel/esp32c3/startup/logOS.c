#include "logos_start.h"
#include "console.h"
/*
 * Entry point used by the custom cpu_start.c path.
 * ESP-IDF's SYS_STARTUP_FN() is bypassed, so this is where logOS takes over.
 */
void kernel_start(void)
{
    /* Install the kernel trap vector before any code can issue ecall. */
    main();
    logos_printf("Starting trap handler\n");
    trap_install();

    /* Bring up the filesystem/demo state, then enter the built-in shell. */
    logos_start();
    logos_shell_run();

    /* The shell currently never returns; keep a halt loop for completeness. */
    for (;;) {
        asm volatile ("wfi");
    }
}

/*
 * Kept for normal ESP-IDF app builds. The custom kernel path does not enter
 * here because cpu_start.c calls kernel_start() directly.
 */
void app_main(void)
{
    logos_start();
    logos_shell_run();
}
