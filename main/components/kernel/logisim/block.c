/*
 * Block device operations and block allocation
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "fs.h"
#include "fs_globals.h"


/*
 * Block device operations
 */

int block_read(uint32_t block_num, void *buf) {
    BLOCK_MEM_ADDR = (uint32_t)buf;
    BLOCK_NUM = block_num;
    BLOCK_CMD = CMD_READ;

    /* Wait for operation to complete */
    while (BLOCK_STATE == STATE_BUSY);

    if (BLOCK_STATE == STATE_ERROR) {
        return FS_ERR_IO;
    }
    return FS_OK;
}

int block_write(uint32_t block_num, const void *buf) {
    BLOCK_MEM_ADDR = (uint32_t)buf;
    BLOCK_NUM = block_num;
    BLOCK_CMD = CMD_WRITE;

    /* Wait for operation to complete */
    while (BLOCK_STATE == STATE_BUSY);

    if (BLOCK_STATE == STATE_ERROR) {
        return FS_ERR_IO;
    }
    return FS_OK;
}


/*
 * Bitmap operations for block allocation
 */

int bitmap_get(uint32_t block) {
    uint32_t bitmap_block = sb.bitmap_start + (block / (BLOCK_SIZE * 8));
    uint32_t byte_offset = (block % (BLOCK_SIZE * 8)) / 8;
    uint32_t bit_offset = block % 8;

    if (block_read(bitmap_block, block_buf) != FS_OK) {
        return -1;
    }

    return (block_buf[byte_offset] >> bit_offset) & 1;
}

int bitmap_set(uint32_t block, int value) {
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


/*
 * Block allocation
 */

int block_alloc(void) {
    if (sb.free_blocks == 0) {
        return FS_ERR_NO_SPACE;
    }

    /* Search for a free block starting from data area */
    for (uint32_t b = sb.data_start; b < sb.total_blocks; b++) {
        if (bitmap_get(b) == 0) {
            /* Found free block, mark it as used */
            if (bitmap_set(b, 1) != FS_OK) {
                return FS_ERR_IO;
            }
            sb.free_blocks--;
            /* Write updated superblock */
            if (block_write(0, &sb) != FS_OK) {
                return FS_ERR_IO;
            }
            /* Clear the newly allocated block */
            memset(block_buf, 0, BLOCK_SIZE);
            if (block_write(b, block_buf) != FS_OK) {
                return FS_ERR_IO;
            }
            return b;
        }
    }

    return FS_ERR_NO_SPACE;
}

int block_free(uint32_t block) {
    if (block < sb.data_start || block >= sb.total_blocks) {
        return FS_ERR_INVALID;
    }

    if (bitmap_set(block, 0) != FS_OK) {
        return FS_ERR_IO;
    }

    sb.free_blocks++;
    return block_write(0, &sb);
}
