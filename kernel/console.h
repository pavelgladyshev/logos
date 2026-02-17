/*
 * Console I/O functions and MMIO definitions
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include "types.h"

/* Console memory-mapped I/O addresses (Logisim TTY component) */
#define CONSOLE_DATA  (*(volatile uint32_t *)0xFFFF000C)  /* Output data */
#define CONSOLE_RCR   (*(volatile uint32_t *)0xFFFF0004)  /* Receiver control (bit 0 = data ready) */
#define CONSOLE_RDR   (*(volatile uint32_t *)0xFFFF0008)  /* Receiver data */

/* Kernel console output functions */
void putchar(int c);
void puts(const char *s);
void put_int(int n);
void put_uint(uint32_t n);
void put_hex(uint32_t n);
void printf(const char *fmt, ...);

#endif /* CONSOLE_H */
