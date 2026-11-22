/*
 * Character device driver interface
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef DEVICE_H
#define DEVICE_H

#include "fs_types.h"

/* Maximum number of device drivers (major numbers 0-15) */
#define MAX_DEVICES        16

/*
 * Device driver operations structure
 * Each character device driver provides these callbacks
 */
struct device_ops {
    int (*read)(uint8_t minor, void *buf, uint32_t len);
    int (*write)(uint8_t minor, const void *buf, uint32_t len);
};

/*
 * Device driver registration
 */
int device_register(uint8_t major, struct device_ops *ops);

/*
 * Device operations (called by syscall.c and file.c)
 */
int device_read(uint8_t major, uint8_t minor, void *buf, uint32_t len);
int device_write(uint8_t major, uint8_t minor, const void *buf, uint32_t len);

#endif /* DEVICE_H */
