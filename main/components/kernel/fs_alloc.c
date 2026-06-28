#include <string.h>

#include "block.h"
#include "fs_alloc.h"
#include "fs_globals.h"

int bitmap_get(uint32_t block)
{
    uint32_t bitmap_block = sb.bitmap_start + (block / (BLOCK_SIZE * 8));
    uint32_t byte_offset = (block % (BLOCK_SIZE * 8)) / 8;
    uint32_t bit_offset = block % 8;

    if (block_read(bitmap_block, block_buf) != FS_OK) {
        return FS_ERR_IO;
    }

    return (block_buf[byte_offset] >> bit_offset) & 1;
}

int bitmap_set(uint32_t block, int value)
{
    uint32_t bitmap_block = sb.bitmap_start + (block / (BLOCK_SIZE * 8));
    uint32_t byte_offset = (block % (BLOCK_SIZE * 8)) / 8;
    uint32_t bit_offset = block % 8;

    if (block_read(bitmap_block, block_buf) != FS_OK) {
        return FS_ERR_IO;
    }

    if (value) {
        block_buf[byte_offset] |= (1 << bit_offset);
    } else {
        block_buf[byte_offset] &= ~(1 << bit_offset);
    }

    return block_write(bitmap_block, block_buf);
}

int block_alloc(void)
{
    if (sb.free_blocks == 0) {
        return FS_ERR_NO_SPACE;
    }

    for (uint32_t block = sb.data_start; block < sb.total_blocks; block++) {
        if (bitmap_get(block) == 0) {
            if (bitmap_set(block, 1) != FS_OK) {
                return FS_ERR_IO;
            }

            sb.free_blocks--;

            if (block_write(0, &sb) != FS_OK) {
                return FS_ERR_IO;
            }

            memset(block_buf, 0, BLOCK_SIZE);

            if (block_write(block, block_buf) != FS_OK) {
                return FS_ERR_IO;
            }

            return (int)block;
        }
    }

    return FS_ERR_NO_SPACE;
}

int block_free(uint32_t block)
{
    if (block < sb.data_start || block >= sb.total_blocks) {
        return FS_ERR_INVALID;
    }

    if (bitmap_set(block, 0) != FS_OK) {
        return FS_ERR_IO;
    }

    sb.free_blocks++;

    return block_write(0, &sb);
}
