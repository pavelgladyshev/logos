/*
 * Bootloader I/O: block device access and console output
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "boot.h"

/* Block device MMIO registers */
#define BLOCK_DEV_BASE     0x200000
#define BLOCK_CMD_REG      (*(volatile uint32_t*)(BLOCK_DEV_BASE + 0x00))
#define BLOCK_MEM_ADDR_REG (*(volatile uint32_t*)(BLOCK_DEV_BASE + 0x04))
#define BLOCK_NUM_REG      (*(volatile uint32_t*)(BLOCK_DEV_BASE + 0x08))
#define BLOCK_STATE_REG    (*(volatile uint32_t*)(BLOCK_DEV_BASE + 0x0C))

#define CMD_READ           1
#define STATE_BUSY         1
#define STATE_ERROR        3

/* Console MMIO */
#define CONSOLE_DATA       (*(volatile uint32_t*)0xFFFF000C)

int boot_block_read(uint32_t block_num, void *buf)
{
    BLOCK_MEM_ADDR_REG = (uint32_t)buf;
    BLOCK_NUM_REG = block_num;
    BLOCK_CMD_REG = CMD_READ;

    while (BLOCK_STATE_REG == STATE_BUSY)
        ;

    if (BLOCK_STATE_REG == STATE_ERROR)
        return -1;

    return 0;
}

void boot_putchar(int c)
{
    CONSOLE_DATA = (uint32_t)c;
}

void boot_puts(const char *s)
{
    while (*s)
        boot_putchar(*s++);
}

void boot_halt(void)
{
    boot_puts("BOOT: System halted.\n");
    while (1) {}
}
