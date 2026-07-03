#pragma once

/*
 * Kernel console API. These calls deliberately use ROM console functions
 * instead of stdio/VFS/UART-driver paths, which are not fully initialized in
 * the custom kernel startup path.
 */
void kernel_console_putchar(char ch);
int kernel_console_getchar(void);
int kernel_console_write(const void *buf, int len);
int kernel_console_printf(const char *fmt, ...);
