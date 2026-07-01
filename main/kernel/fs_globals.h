/*
 * Filesystem global state declarations
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef FS_GLOBALS_H
#define FS_GLOBALS_H

#include "fs_types.h"

/* Global filesystem state (defined in fs.c) */
extern struct superblock sb;
extern uint8_t block_buf[BLOCK_SIZE];

#endif /* FS_GLOBALS_H */
