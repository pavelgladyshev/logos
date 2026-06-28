#ifndef FS_ALLOC_H
#define FS_ALLOC_H

#include "fs_types.h"

int bitmap_get(uint32_t block);
int bitmap_set(uint32_t block, int value);
int block_alloc(void);
int block_free(uint32_t block);

#endif /* FS_ALLOC_H */
