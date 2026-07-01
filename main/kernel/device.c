/*
 * Character device driver implementation
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "device.h"

/* Device driver table - indexed by major number */
static struct device_ops *device_table[MAX_DEVICES];

int device_register(uint8_t major, struct device_ops *ops) {
    if (major >= MAX_DEVICES) {
        return FS_ERR_INVALID;
    }
    if (device_table[major] != 0) {
        return FS_ERR_EXISTS;  /* Already registered */
    }
    device_table[major] = ops;
    return FS_OK;
}

int device_unregister(uint8_t major) {
    if (major >= MAX_DEVICES) {
        return FS_ERR_INVALID;
    }
    device_table[major] = 0;
    return FS_OK;
}

int device_open(uint8_t major, uint8_t minor) {
    if (major >= MAX_DEVICES || device_table[major] == 0) {
        return FS_ERR_NOT_FOUND;
    }
    if (device_table[major]->open) {
        return device_table[major]->open(minor);
    }
    return FS_OK;  /* No open handler is OK */
}

int device_close(uint8_t major, uint8_t minor) {
    if (major >= MAX_DEVICES || device_table[major] == 0) {
        return FS_ERR_NOT_FOUND;
    }
    if (device_table[major]->close) {
        return device_table[major]->close(minor);
    }
    return FS_OK;  /* No close handler is OK */
}

int device_read(uint8_t major, uint8_t minor, void *buf, uint32_t len) {
    if (major >= MAX_DEVICES || device_table[major] == 0) {
        return FS_ERR_NOT_FOUND;
    }
    if (device_table[major]->read == 0) {
        return FS_ERR_INVALID;  /* Device doesn't support reading */
    }
    return device_table[major]->read(minor, buf, len);
}

int device_write(uint8_t major, uint8_t minor, const void *buf, uint32_t len) {
    if (major >= MAX_DEVICES || device_table[major] == 0) {
        return FS_ERR_NOT_FOUND;
    }
    if (device_table[major]->write == 0) {
        return FS_ERR_INVALID;  /* Device doesn't support writing */
    }
    return device_table[major]->write(minor, buf, len);
}
