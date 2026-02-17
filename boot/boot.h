/*
 * Bootloader header
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef BOOT_H
#define BOOT_H

#include "types.h"
#include "fs_types.h"

/* Kernel load address in RAM */
#define KERNEL_LOAD_ADDR   0x00100000

/* Kernel file path in the filesystem */
#define KERNEL_PATH        "/boot/kernel"

/* Boot I/O functions */
int  boot_block_read(uint32_t block_num, void *buf);
void boot_putchar(int c);
void boot_puts(const char *s);
void boot_put_hex(uint32_t n);
void boot_put_uint(uint32_t n);
void boot_halt(void);

/* Boot filesystem functions */
int boot_resolve_path(const char *path, struct superblock *sb,
                      uint8_t *buf, uint32_t *result_ino);
int boot_file_read(uint32_t ino, struct superblock *sb, uint8_t *buf,
                   uint32_t offset, void *dst, uint32_t len);

/* Boot string/memory utilities */
void boot_memcpy(void *dst, const void *src, uint32_t n);
void boot_memset(void *dst, int c, uint32_t n);
int  boot_strcmp(const char *s1, const char *s2);

#endif /* BOOT_H */
