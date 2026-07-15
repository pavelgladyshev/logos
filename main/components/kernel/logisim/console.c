/*
 * Console I/O implementation for the Logisim RISC-V target
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "console.h"
#include "console_constants.h"

typedef __builtin_va_list va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

void logos_putchar(char ch)
{
    CONSOLE_DATA = (uint32_t)(uint8_t)ch;
}

int logos_getchar(void)
{
    while ((CONSOLE_RCR & 1U) == 0U) {
        ;
    }

    return (int)(CONSOLE_RDR & 0xffU);
}

int logos_write(const void *buf, int len)
{
    const uint8_t *src = (const uint8_t *)buf;

    if (buf == 0 || len < 0) {
        return -1;
    }

    for (int i = 0; i < len; ++i) {
        logos_putchar((char)src[i]);
    }

    return len;
}

static int write_string(const char *s)
{
    int written = 0;

    while (*s != '\0') {
        logos_putchar(*s++);
        ++written;
    }

    return written;
}

static int write_uint(uint32_t value)
{
    char buf[10];
    int length = 0;
    int written = 0;

    if (value == 0) {
        logos_putchar('0');
        return 1;
    }

    while (value != 0) {
        buf[length++] = (char)('0' + value % 10U);
        value /= 10U;
    }

    while (length != 0) {
        logos_putchar(buf[--length]);
        ++written;
    }

    return written;
}

static int write_int(int value)
{
    uint32_t magnitude;
    int written = 0;

    if (value < 0) {
        logos_putchar('-');
        written = 1;
        magnitude = 0U - (uint32_t)value;
    } else {
        magnitude = (uint32_t)value;
    }

    return written + write_uint(magnitude);
}

static int write_hex(uint32_t value)
{
    int written = 0;
    int started = 0;

    for (int shift = 28; shift >= 0; shift -= 4) {
        int digit = (int)((value >> shift) & 0xfU);

        if (digit != 0 || started || shift == 0) {
            logos_putchar((char)(digit < 10 ? '0' + digit : 'a' + digit - 10));
            started = 1;
            ++written;
        }
    }

    return written;
}

/* Simple formatter supporting %s, %d, %u, %x, %c, and %%. */
int logos_printf(const char *fmt, ...)
{
    va_list ap;
    int written = 0;

    va_start(ap, fmt);
    while (*fmt != '\0') {
        if (*fmt++ != '%') {
            logos_putchar(fmt[-1]);
            ++written;
            continue;
        }

        switch (*fmt) {
        case 's':
            written += write_string(va_arg(ap, const char *));
            break;
        case 'd':
            written += write_int(va_arg(ap, int));
            break;
        case 'u':
            written += write_uint(va_arg(ap, uint32_t));
            break;
        case 'x':
            written += write_hex(va_arg(ap, uint32_t));
            break;
        case 'c':
            logos_putchar((char)va_arg(ap, int));
            ++written;
            break;
        case '%':
            logos_putchar('%');
            ++written;
            break;
        case '\0':
            va_end(ap);
            return written;
        default:
            logos_putchar('%');
            logos_putchar(*fmt);
            written += 2;
            break;
        }
        ++fmt;
    }
    va_end(ap);

    return written;
}
