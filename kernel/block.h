/*
 * Block device operations and block allocation
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef BLOCK_H
#define BLOCK_H

#include "fs_types.h"

/* Block device memory-mapped I/O registers */
#define BLOCK_DEV_BASE     0x200000
#define BLOCK_CMD          (*(volatile uint32_t*)(BLOCK_DEV_BASE + 0x00))
#define BLOCK_MEM_ADDR     (*(volatile uint32_t*)(BLOCK_DEV_BASE + 0x04))
#define BLOCK_NUM          (*(volatile uint32_t*)(BLOCK_DEV_BASE + 0x08))
#define BLOCK_STATE        (*(volatile uint32_t*)(BLOCK_DEV_BASE + 0x0C))
#define BLOCK_ERROR        (*(volatile uint32_t*)(BLOCK_DEV_BASE + 0x10))

/* Block device commands */
#define CMD_READ           1
#define CMD_WRITE          2

/* Block device states */
#define STATE_IDLE         0
#define STATE_BUSY         1
#define STATE_DONE         2
#define STATE_ERROR        3

/* Block device operations */
int block_read(uint32_t block_num, void *buf);
int block_write(uint32_t block_num, const void *buf);

/* Bitmap operations */
int bitmap_get(uint32_t block);
int bitmap_set(uint32_t block, int value);

/* Block allocation */
int block_alloc(void);
int block_free(uint32_t block);

#endif /* BLOCK_H */
