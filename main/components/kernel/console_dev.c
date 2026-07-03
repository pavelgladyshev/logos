/*
 * Console character device driver
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Major number: 1
 * Minor 0: /dev/console (read/write terminal)
 */

#include "device.h"
#include "console.h"
#include "console_dev.h"

static int console_read(uint8_t minor, void *buf, uint32_t len) {
    uint8_t *dst = (uint8_t *)buf;
    uint32_t i = 0;

    while (i < len) {
        /* Poll for data available (bit 0 of RCR) */
        while ((CONSOLE_RCR & 1) == 0) {
            /* busy wait for keyboard input */
        }
        dst[i++] = (uint8_t)CONSOLE_RDR;
    }

    return i;
}

static int console_write(uint8_t minor, const void *buf, uint32_t len) {
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t i;

    for (i = 0; i < len; i++) {
        CONSOLE_DATA = src[i];
    }

    return len;
}

/* Console device operations - read/write terminal */
static struct device_ops console_ops = {
    .open  = 0,
    .close = 0,
    .read  = console_read,
    .write = console_write
};

/*
 * Initialize the console device driver
 * Call this at system startup to register the driver
 */
int console_dev_init(void) {
    return device_register(CONSOLE_MAJOR, &console_ops);
}
