/*
 * Inode operations
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef INODE_H
#define INODE_H

#include "fs_types.h"

int inode_read(uint32_t ino, struct inode *ip);
int inode_write(uint32_t ino, const struct inode *ip);
int inode_alloc(void);
int inode_free(uint32_t ino);
int inode_get_type(uint32_t ino, uint8_t *type);
int inode_get_size(uint32_t ino, uint32_t *size);
int inode_get_device(uint32_t ino, uint8_t *major, uint8_t *minor);

#endif /* INODE_H */
