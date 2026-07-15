#include "console.h"



#include "esp_rom_serial_output.h"
#include "esp_rom_sys.h"

/* Output through the ROM console channel selected by early startup. */
void logos_putchar(char ch)
{
    esp_rom_output_putc(ch);
}

int logos_getchar(void)
{
    uint8_t ch;

    /* Polling input is acceptable for the current single-threaded shell. */
    while (esp_rom_output_rx_one_char(&ch) != 0) {
        ;
    }

    return ch;
}

int logos_write(const void *buf, int len)
{
    const unsigned char *s = buf;

    if (buf == NULL || len < 0) {
        return -1;
    }

    /* Keep write simple and synchronous until the kernel has scheduling. */
    for (int i = 0; i < len; ++i) {
        logos_putchar((char)s[i]);
    }

    return len;
}

int logos_printf(const char *fmt, ...)
{
    va_list args;
    int written;

    /* ROM formatting avoids newlib stdio and its ESP-IDF VFS backend. */
    va_start(args, fmt);
    written = esp_rom_vprintf(fmt, args);
    va_end(args);

    return written;
}
