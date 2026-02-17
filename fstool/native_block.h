/*
 * Native backing store operations
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef NATIVE_BLOCK_H
#define NATIVE_BLOCK_H

#include "fs_types.h"

int native_backing_store_open(const char *filename);
int native_backing_store_create(const char *filename, uint32_t num_blocks);
void native_backing_store_close(void);

#endif /* NATIVE_BLOCK_H */
