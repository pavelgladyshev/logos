/*
 * File operations
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "fs.h"
#include "fs_globals.h"
#include "device.h"


int file_create(uint32_t dir_ino, const char *name) {
    /* Reject . and .. as explicit names */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return FS_ERR_INVALID;
    }

    /* Allocate a new inode */
    int ino = inode_alloc();
    if (ino < 0) {
        return ino;
    }

    /* Initialize the inode as a regular file */
    struct inode in;
    memset(&in, 0, INODE_SIZE);
    in.type = FT_FILE;
    in.size = 0;

    if (inode_write(ino, &in) != FS_OK) {
        inode_free(ino);
        return FS_ERR_IO;
    }

    /* Add entry to directory */
    int err = dir_add(dir_ino, name, ino);
    if (err != FS_OK) {
        inode_free(ino);
        return err;
    }

    return ino;
}

int file_read(uint32_t ino, uint32_t offset, void *buf, uint32_t len) {
    struct inode in;

    if (inode_read(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    /* Handle character device files */
    if (in.type == FT_CHARDEV) {
        return device_read(in.major, in.minor, buf, len);
    }

    if (in.type != FT_FILE) {
        return FS_ERR_IS_DIR;
    }

    /* Adjust length if reading past end of file */
    if (offset >= in.size) {
        return 0;
    }
    if (offset + len > in.size) {
        len = in.size - offset;
    }

    uint8_t *dst = (uint8_t *)buf;
    uint32_t bytes_read = 0;

    while (len > 0) {
        uint32_t block_idx = offset / BLOCK_SIZE;
        uint32_t block_offset = offset % BLOCK_SIZE;
        uint32_t chunk = BLOCK_SIZE - block_offset;
        if (chunk > len) chunk = len;

        if (block_idx >= DIRECT_BLOCKS || in.blocks[block_idx] == 0) {
            break;
        }

        if (block_read(in.blocks[block_idx], block_buf) != FS_OK) {
            return FS_ERR_IO;
        }

        memcpy(dst, block_buf + block_offset, chunk);

        dst += chunk;
        offset += chunk;
        len -= chunk;
        bytes_read += chunk;
    }

    return bytes_read;
}

/*
 * Load file data directly into a destination buffer using DMA.
 * For block-aligned full blocks where dst is 4-byte aligned,
 * DMA goes straight to dst (no memcpy).
 * Partial blocks or unaligned destinations use block_buf as intermediate.
 * Only works for regular files (not devices). Used by the ELF loader.
 */
int file_load_direct(uint32_t ino, uint32_t offset, void *buf, uint32_t len) {
    struct inode in;

    if (inode_read(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    if (in.type != FT_FILE) {
        return FS_ERR_IS_DIR;
    }

    /* Adjust length if reading past end of file */
    if (offset >= in.size) {
        return 0;
    }
    if (offset + len > in.size) {
        len = in.size - offset;
    }

    uint8_t *dst = (uint8_t *)buf;
    uint32_t bytes_read = 0;

    while (len > 0) {
        uint32_t block_idx = offset / BLOCK_SIZE;
        uint32_t block_offset = offset % BLOCK_SIZE;
        uint32_t chunk = BLOCK_SIZE - block_offset;
        if (chunk > len) chunk = len;

        if (block_idx >= DIRECT_BLOCKS || in.blocks[block_idx] == 0) {
            break;
        }

        if (block_offset == 0 && chunk == BLOCK_SIZE &&
            (((unsigned long)dst) & 3) == 0) {
            /* Full block, dst 4-byte aligned: DMA directly to destination */
            if (block_read(in.blocks[block_idx], dst) != FS_OK) {
                return FS_ERR_IO;
            }
        } else {
            /* Partial block or unaligned dst: use intermediate buffer */
            if (block_read(in.blocks[block_idx], block_buf) != FS_OK) {
                return FS_ERR_IO;
            }
            memcpy(dst, block_buf + block_offset, chunk);
        }

        dst += chunk;
        offset += chunk;
        len -= chunk;
        bytes_read += chunk;
    }

    return bytes_read;
}

int file_write(uint32_t ino, uint32_t offset, const void *buf, uint32_t len) {
    struct inode in;

    if (inode_read(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    /* Handle character device files */
    if (in.type == FT_CHARDEV) {
        return device_write(in.major, in.minor, buf, len);
    }

    if (in.type != FT_FILE) {
        return FS_ERR_IS_DIR;
    }

    const uint8_t *src = (const uint8_t *)buf;
    uint32_t bytes_written = 0;

    while (len > 0) {
        uint32_t block_idx = offset / BLOCK_SIZE;
        uint32_t block_offset = offset % BLOCK_SIZE;
        uint32_t chunk = BLOCK_SIZE - block_offset;
        if (chunk > len) chunk = len;

        if (block_idx >= DIRECT_BLOCKS) {
            break;  /* File too large for direct blocks only */
        }

        /* Allocate block if needed */
        if (in.blocks[block_idx] == 0) {
            int new_block = block_alloc();
            if (new_block < 0) {
                /* Update inode with what we've written so far */
                if (offset > in.size) in.size = offset;
                inode_write(ino, &in);
                return bytes_written > 0 ? bytes_written : new_block;
            }
            in.blocks[block_idx] = new_block;
        }

        /* Read-modify-write for partial block writes */
        if (block_offset != 0 || chunk != BLOCK_SIZE) {
            if (block_read(in.blocks[block_idx], block_buf) != FS_OK) {
                return FS_ERR_IO;
            }
        }

        memcpy(block_buf + block_offset, src, chunk);

        if (block_write(in.blocks[block_idx], block_buf) != FS_OK) {
            return FS_ERR_IO;
        }

        src += chunk;
        offset += chunk;
        len -= chunk;
        bytes_written += chunk;
    }

    /* Update file size if we extended the file */
    if (offset > in.size) {
        in.size = offset;
    }

    if (inode_write(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    return bytes_written;
}

int file_truncate(uint32_t ino, uint32_t new_size) {
    struct inode in;

    if (inode_read(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    if (in.type != FT_FILE) {
        return FS_ERR_IS_DIR;
    }

    /* Free blocks that are no longer needed */
    uint32_t first_free_block = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (uint32_t i = first_free_block; i < DIRECT_BLOCKS; i++) {
        if (in.blocks[i] != 0) {
            block_free(in.blocks[i]);
            in.blocks[i] = 0;
        }
    }

    in.size = new_size;
    return inode_write(ino, &in);
}

int file_delete(uint32_t dir_ino, const char *name) {
    uint32_t file_ino;

    /* Find the file */
    int err = dir_lookup(dir_ino, name, &file_ino);
    if (err != FS_OK) {
        return err;
    }

    /* Check that it's a file or character device */
    struct inode in;
    if (inode_read(file_ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    if (in.type != FT_FILE && in.type != FT_CHARDEV) {
        return FS_ERR_IS_DIR;
    }

    /* Remove from directory */
    err = dir_remove(dir_ino, name);
    if (err != FS_OK) {
        return err;
    }

    /* Decrement link count; only free inode when no links remain */
    if (in.link_count > 1) {
        in.link_count--;
        return inode_write(file_ino, &in);
    }

    return inode_free(file_ino);
}
