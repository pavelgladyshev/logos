#ifndef BLOCK_H
#define BLOCK_H

#include "fs_types.h"

int block_init(void);
int block_read(uint32_t block_num, void *buf);
int block_write(uint32_t block_num, const void *buf);
int block_count(void);

#endif /* BLOCK_H */
