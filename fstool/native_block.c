/*
 * Native backing store operations using standard C file I/O
 * For use with the fstool native utility
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include <stdio.h>
#include <string.h>
#include "fs_types.h"
#include "fs_globals.h"

/* Global file handle for the block storage file */
static FILE *block_file = NULL;
static char block_filename[256] = "";

/*
 * Open the block storage file
 * Returns 0 on success, -1 on failure
 */
int native_backing_store_open(const char *filename) {
    if (block_file != NULL) {
        fclose(block_file);
    }

    block_file = fopen(filename, "r+b");
    if (block_file == NULL) {
        /* Try to create the file if it doesn't exist */
        block_file = fopen(filename, "w+b");
        if (block_file == NULL) {
            return -1;
        }
    }

    strncpy(block_filename, filename, sizeof(block_filename) - 1);
    block_filename[sizeof(block_filename) - 1] = '\0';
    return 0;
}

/*
 * Create a new block storage file with the specified number of blocks
 * Returns 0 on success, -1 on failure
 */
int native_backing_store_create(const char *filename, uint32_t num_blocks) {
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        return -1;
    }

    /* Write zeros for all blocks */
    uint8_t zero_block[BLOCK_SIZE];
    memset(zero_block, 0, BLOCK_SIZE);

    for (uint32_t i = 0; i < num_blocks; i++) {
        if (fwrite(zero_block, BLOCK_SIZE, 1, f) != 1) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);

    /* Now open it for read/write */
    return native_backing_store_open(filename);
}

/*
 * Close the block storage file
 */
void native_backing_store_close(void) {
    if (block_file != NULL) {
        fclose(block_file);
        block_file = NULL;
    }
    block_filename[0] = '\0';
}

/*
 * Read a block from the storage file
 */
int block_read(uint32_t block_num, void *buf) {
    if (block_file == NULL) {
        return FS_ERR_IO;
    }

    if (fseek(block_file, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0) {
        return FS_ERR_IO;
    }

    if (fread(buf, BLOCK_SIZE, 1, block_file) != 1) {
        /* Check if we're reading beyond end of file - return zeros */
        if (feof(block_file)) {
            memset(buf, 0, BLOCK_SIZE);
            return FS_OK;
        }
        return FS_ERR_IO;
    }

    return FS_OK;
}

/*
 * Write a block to the storage file
 */
int block_write(uint32_t block_num, const void *buf) {
    if (block_file == NULL) {
        return FS_ERR_IO;
    }

    if (fseek(block_file, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0) {
        return FS_ERR_IO;
    }

    if (fwrite(buf, BLOCK_SIZE, 1, block_file) != 1) {
        return FS_ERR_IO;
    }

    /* Flush to ensure data is written */
    fflush(block_file);

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
