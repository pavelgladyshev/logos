/*
 * Shared kernel console API
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

void logos_putchar(char ch);
int logos_getchar(void);
int logos_write(const void *buf, int len);
int logos_printf(const char *fmt, ...);
int console_dev_init(void);

#endif /* KERNEL_CONSOLE_H */
