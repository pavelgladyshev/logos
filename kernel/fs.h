/*
 * Simple Unix-like File System for RISC-V
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * This implements a minimal file system with:
 * - Superblock
 * - Inode table
 * - Block allocation bitmap
 * - Simple linear directories
 * - Inodes with file size and direct block indices only
 */

#ifndef FS_H
#define FS_H

/* Include filesystem type definitions */
#include "fs_types.h"

/* String functions */
#ifdef NATIVE_BUILD
    #include <string.h>
#else
    #include "string.h"
#endif

/* Include module headers */
#include "block.h"
#include "inode.h"
#include "dir.h"
#include "file.h"

/*
 * Filesystem operations (fs.c)
 */
int fs_format(uint32_t total_blocks);
int fs_mount(void);
int fs_open(const char *path, uint32_t *ino);
uint32_t fs_root_inode(void);
int fs_mknod(uint32_t dir_ino, const char *name, uint8_t major, uint8_t minor);
int fs_open_parent(const char *path, uint32_t *parent_ino, const char **name);
int fs_link(uint32_t target_ino, uint32_t dir_ino, const char *name);
int fs_rename(uint32_t old_dir_ino, const char *old_name, uint32_t new_dir_ino, const char *new_name);

/* Longest path (with its NUL) fs_open_parent() takes: a system call's string argument */
#define FS_MAX_PATH  64


#endif /* FS_H */
