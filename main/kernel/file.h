/*
 * File operations
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef FILE_H
#define FILE_H

#include "fs_types.h"

int file_create(uint32_t dir_ino, const char *name);
int file_read(uint32_t ino, uint32_t offset, void *buf, uint32_t len);
int file_load_direct(uint32_t ino, uint32_t offset, void *buf, uint32_t len);
int file_write(uint32_t ino, uint32_t offset, const void *buf, uint32_t len);
int file_truncate(uint32_t ino, uint32_t new_size);
int file_delete(uint32_t dir_ino, const char *name);

#endif /* FILE_H */
