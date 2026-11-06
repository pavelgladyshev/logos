/*
 * Native backing store operations using standard C file I/O
 * For use with the fstool native utility and the FUSE driver logosfs
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * block_read() and block_write() here take the place of the kernel's, which
 * drive the block storage device: they read and write a disk image file.
 * Everything else (bitmap, block_alloc, block_free) is kernel/block.c's.
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
