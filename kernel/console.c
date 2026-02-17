/*
 * Console output implementation for RISC-V
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "console.h"

/* Variadic argument handling using GCC builtins (works for bare-metal) */
typedef __builtin_va_list va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

void putchar(int c) {
    CONSOLE_DATA = (uint32_t)c;
}

void puts(const char *s) {
    while (*s) {
        putchar(*s++);
    }
}

void put_uint(uint32_t n) {
    char buf[12];
    int i = 0;

    if (n == 0) {
        putchar('0');
        return;
    }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    while (i > 0) {
        putchar(buf[--i]);
    }
}

void put_int(int n) {
    if (n < 0) {
        putchar('-');
        n = -n;
    }
    put_uint((uint32_t)n);
}

void put_hex(uint32_t n) {
    int i;
    int started = 0;

    for (i = 28; i >= 0; i -= 4) {
        int digit = (n >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            if (digit < 10) {
                putchar('0' + digit);
            } else {
                putchar('a' + digit - 10);
            }
            started = 1;
        }
    }
}

/* Simple printf implementation supporting %s, %d, %u, %x, %c, %% */
void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's':
                    puts(va_arg(ap, const char*));
                    break;
                case 'd':
                    put_int(va_arg(ap, int));
                    break;
                case 'u':
                    put_uint(va_arg(ap, uint32_t));
                    break;
                case 'x':
                    put_hex(va_arg(ap, uint32_t));
                    break;
                case 'c':
                    putchar(va_arg(ap, int));
                    break;
                case '%':
                    putchar('%');
                    break;
                case '\0':
                    va_end(ap);
                    return;
                default:
                    putchar('%');
                    putchar(*fmt);
                    break;
            }
        } else {
            putchar(*fmt);
        }
        fmt++;
    }
    va_end(ap);
}
