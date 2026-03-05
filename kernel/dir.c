/*
 * Directory operations
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "fs.h"
#include "fs_globals.h"

int dir_lookup(uint32_t dir_ino, const char *name, uint32_t *found_ino) {
    struct inode dir;

    if (inode_read(dir_ino, &dir) != FS_OK) {
        return FS_ERR_IO;
    }

    if (dir.type != FT_DIR) {
        return FS_ERR_NOT_DIR;
    }

    /* Search through all directory blocks */
    uint32_t entries = dir.size / DIRENT_SIZE;
    uint32_t entry_idx = 0;

    for (int b = 0; b < DIRECT_BLOCKS && entry_idx < entries; b++) {
        if (dir.blocks[b] == 0) continue;

        if (block_read(dir.blocks[b], block_buf) != FS_OK) {
            return FS_ERR_IO;
        }

        struct dirent *de = (struct dirent *)block_buf;
        for (int i = 0; i < (int)DIRENTS_PER_BLOCK && entry_idx < entries; i++, entry_idx++) {
            if (de[i].inode != DIRENT_FREE && strcmp(de[i].name, name) == 0) {
                *found_ino = de[i].inode;
                return FS_OK;
            }
        }
    }

    return FS_ERR_NOT_FOUND;
}

int dir_add(uint32_t dir_ino, const char *name, uint32_t ino) {
    struct inode dir;
    uint32_t dummy;

    if (inode_read(dir_ino, &dir) != FS_OK) {
        return FS_ERR_IO;
    }

    if (dir.type != FT_DIR) {
        return FS_ERR_NOT_DIR;
    }

    /* Check if name already exists */
    if (dir_lookup(dir_ino, name, &dummy) == FS_OK) {
        return FS_ERR_EXISTS;
    }

    /* Check filename length */
    if (strlen(name) >= MAX_FILENAME) {
        return FS_ERR_INVALID;
    }

    /* Find a free slot in existing blocks */
    for (int b = 0; b < DIRECT_BLOCKS; b++) {
        if (dir.blocks[b] == 0) continue;

        if (block_read(dir.blocks[b], block_buf) != FS_OK) {
            return FS_ERR_IO;
        }

        struct dirent *de = (struct dirent *)block_buf;
        for (int i = 0; i < (int)DIRENTS_PER_BLOCK; i++) {
            if (de[i].inode == DIRENT_FREE) {
                /* Found free slot */
                de[i].inode = ino;
                strcpy(de[i].name, name);
                if (block_write(dir.blocks[b], block_buf) != FS_OK) {
                    return FS_ERR_IO;
                }
                dir.size += DIRENT_SIZE;
                return inode_write(dir_ino, &dir);
            }
        }
    }

    /* Need to allocate a new block for directory */
    for (int b = 0; b < DIRECT_BLOCKS; b++) {
        if (dir.blocks[b] == 0) {
            int new_block = block_alloc();
            if (new_block < 0) {
                return new_block;
            }
            dir.blocks[b] = new_block;

            /* Initialize the new block with free entries */
            {
                struct dirent *de = (struct dirent *)block_buf;
                int j;
                for (j = 0; j < (int)DIRENTS_PER_BLOCK; j++) {
                    de[j].inode = DIRENT_FREE;
                    de[j].name[0] = '\0';
                }
                /* Use the first slot */
                de[0].inode = ino;
                strcpy(de[0].name, name);
            }

            if (block_write(new_block, block_buf) != FS_OK) {
                return FS_ERR_IO;
            }

            dir.size += DIRENT_SIZE;
            return inode_write(dir_ino, &dir);
        }
    }

    return FS_ERR_NO_SPACE;
}

int dir_remove(uint32_t dir_ino, const char *name) {
    struct inode dir;

    if (inode_read(dir_ino, &dir) != FS_OK) {
        return FS_ERR_IO;
    }

    if (dir.type != FT_DIR) {
        return FS_ERR_NOT_DIR;
    }

    /* Search for the entry */
    for (int b = 0; b < DIRECT_BLOCKS; b++) {
        if (dir.blocks[b] == 0) continue;

        if (block_read(dir.blocks[b], block_buf) != FS_OK) {
            return FS_ERR_IO;
        }

        struct dirent *de = (struct dirent *)block_buf;
        for (int i = 0; i < (int)DIRENTS_PER_BLOCK; i++) {
            if (de[i].inode != DIRENT_FREE && strcmp(de[i].name, name) == 0) {
                /* Found it - clear the entry */
                de[i].inode = DIRENT_FREE;
                de[i].name[0] = '\0';

                if (block_write(dir.blocks[b], block_buf) != FS_OK) {
                    return FS_ERR_IO;
                }

                dir.size -= DIRENT_SIZE;
                return inode_write(dir_ino, &dir);
            }
        }
    }

    return FS_ERR_NOT_FOUND;
}

int dir_list(uint32_t dir_ino, struct dirent *entries, uint32_t max_entries, uint32_t *count) {
    struct inode dir;
    uint8_t local_buf[BLOCK_SIZE];

    if (inode_read(dir_ino, &dir) != FS_OK) {
        return FS_ERR_IO;
    }

    if (dir.type != FT_DIR) {
        return FS_ERR_NOT_DIR;
    }

    *count = 0;

    /* Iterate through all directory blocks */
    for (int b = 0; b < DIRECT_BLOCKS; b++) {
        if (dir.blocks[b] == 0) continue;

        if (block_read(dir.blocks[b], local_buf) != FS_OK) {
            return FS_ERR_IO;
        }

        struct dirent *de = (struct dirent *)local_buf;
        for (uint32_t i = 0; i < DIRENTS_PER_BLOCK; i++) {
            if (de[i].inode != DIRENT_FREE) {
                if (*count < max_entries) {
                    entries[*count] = de[i];
                }
                (*count)++;
            }
        }
    }

    return FS_OK;
}

static int dir_is_empty(uint32_t dir_ino) {
    struct inode dir;

    if (inode_read(dir_ino, &dir) != FS_OK) {
        return 0;
    }

    for (int b = 0; b < DIRECT_BLOCKS; b++) {
        if (dir.blocks[b] == 0) continue;

        if (block_read(dir.blocks[b], block_buf) != FS_OK) {
            return 0;
        }

        struct dirent *de = (struct dirent *)block_buf;
        for (int i = 0; i < (int)DIRENTS_PER_BLOCK; i++) {
            if (de[i].inode != DIRENT_FREE &&
                strcmp(de[i].name, ".") != 0 &&
                strcmp(de[i].name, "..") != 0) {
                return 0;  /* Directory has entries other than . and .. */
            }
        }
    }

    return 1;  /* Directory is empty (only . and .. or nothing) */
}

int fs_mkdir(uint32_t parent_ino, const char *name) {
    /* Reject . and .. as explicit names */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return FS_ERR_INVALID;
    }

    /* Allocate a new inode */
    int ino = inode_alloc();
    if (ino < 0) {
        return ino;
    }

    /* Initialize the inode as a directory */
    struct inode in;
    memset(&in, 0, INODE_SIZE);
    in.type = FT_DIR;
    in.size = 0;

    if (inode_write(ino, &in) != FS_OK) {
        inode_free(ino);
        return FS_ERR_IO;
    }

    /* Add . and .. entries to the new directory */
    int err = dir_add(ino, ".", ino);
    if (err != FS_OK) {
        inode_free(ino);
        return err;
    }

    err = dir_add(ino, "..", parent_ino);
    if (err != FS_OK) {
        /* Clean up: remove . entry and free inode */
        dir_remove(ino, ".");
        inode_free(ino);
        return err;
    }

    /* Add entry to parent directory */
    err = dir_add(parent_ino, name, ino);
    if (err != FS_OK) {
        dir_remove(ino, "..");
        dir_remove(ino, ".");
        inode_free(ino);
        return err;
    }

    return ino;
}

int fs_rmdir(uint32_t parent_ino, const char *name) {
    /* Reject . and .. as explicit names */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return FS_ERR_INVALID;
    }

    uint32_t dir_ino;

    /* Find the directory */
    int err = dir_lookup(parent_ino, name, &dir_ino);
    if (err != FS_OK) {
        return err;
    }

    /* Check that it's a directory */
    struct inode in;
    if (inode_read(dir_ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    if (in.type != FT_DIR) {
        return FS_ERR_NOT_DIR;
    }

    /* Check that it's empty (ignoring . and ..) */
    if (!dir_is_empty(dir_ino)) {
        return FS_ERR_NOT_EMPTY;
    }

    /* Remove . and .. from the directory being deleted */
    dir_remove(dir_ino, ".");
    dir_remove(dir_ino, "..");

    /* Remove from parent directory */
    err = dir_remove(parent_ino, name);
    if (err != FS_OK) {
        return err;
    }

    /* Free the inode */
    return inode_free(dir_ino);
}
