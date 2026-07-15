/*
 * Console character device driver
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Major number: 1
 * Minor 0: /dev/console (read/write terminal)
 */

#include "device.h"
#include "console.h"
#include "console_constants.h"

static int console_read(uint8_t minor, void *buf, uint32_t len) {
    (void)minor;
    uint8_t *dst = (uint8_t *)buf;
    uint32_t i = 0;

    while (i < len) {
        dst[i++] = (uint8_t)logos_getchar();
    }

    return i;
}

static int console_write(uint8_t minor, const void *buf, uint32_t len) {
    (void)minor;
    return logos_write(buf, (int)len);
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
