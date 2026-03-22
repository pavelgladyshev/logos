/*
 * Inode operations
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "fs.h"
#include "fs_globals.h"

int inode_read(uint32_t ino, struct inode *ip) {
    if (ino >= sb.total_inodes) {
        return FS_ERR_INVALID;
    }

    uint32_t block = sb.inode_start + (ino / INODES_PER_BLOCK);
    uint32_t offset = (ino % INODES_PER_BLOCK) * INODE_SIZE;

    if (block_read(block, block_buf) != FS_OK) {
        return FS_ERR_IO;
    }

    memcpy(ip, block_buf + offset, INODE_SIZE);
    return FS_OK;
}

int inode_write(uint32_t ino, const struct inode *ip) {
    if (ino >= sb.total_inodes) {
        return FS_ERR_INVALID;
    }

    uint32_t block = sb.inode_start + (ino / INODES_PER_BLOCK);
    uint32_t offset = (ino % INODES_PER_BLOCK) * INODE_SIZE;

    if (block_read(block, block_buf) != FS_OK) {
        return FS_ERR_IO;
    }

    memcpy(block_buf + offset, ip, INODE_SIZE);
    return block_write(block, block_buf);
}

int inode_alloc(void) {
    if (sb.free_inodes == 0) {
        return FS_ERR_NO_SPACE;
    }

    struct inode in;
    for (uint32_t i = 0; i < sb.total_inodes; i++) {
        if (inode_read(i, &in) != FS_OK) {
            return FS_ERR_IO;
        }
        if (in.type == FT_FREE) {
            /* Found free inode */
            memset(&in, 0, INODE_SIZE);
            in.link_count = 1;
            if (inode_write(i, &in) != FS_OK) {
                return FS_ERR_IO;
            }
            sb.free_inodes--;
            if (block_write(0, &sb) != FS_OK) {
                return FS_ERR_IO;
            }
            return i;
        }
    }

    return FS_ERR_NO_SPACE;
}

int inode_free(uint32_t ino) {
    struct inode in;

    if (inode_read(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    /* Free all data blocks */
    for (int i = 0; i < DIRECT_BLOCKS; i++) {
        if (in.blocks[i] != 0) {
            block_free(in.blocks[i]);
        }
    }

    /* Mark inode as free */
    memset(&in, 0, INODE_SIZE);
    in.type = FT_FREE;

    if (inode_write(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    sb.free_inodes++;
    return block_write(0, &sb);
}

int inode_get_type(uint32_t ino, uint8_t *type) {
    struct inode in;

    if (inode_read(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    *type = in.type;
    return FS_OK;
}

int inode_get_size(uint32_t ino, uint32_t *size) {
    struct inode in;

    if (inode_read(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    *size = in.size;
    return FS_OK;
}

int inode_get_device(uint32_t ino, uint8_t *major, uint8_t *minor) {
    struct inode in;

    if (inode_read(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    *major = in.major;
    *minor = in.minor;
    return FS_OK;
}
