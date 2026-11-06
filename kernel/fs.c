/*
 * Simple Unix-like File System - Core module
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Contains filesystem format/mount operations.
 */

#include "fs.h"

/* Global filesystem state */
struct superblock sb;
uint8_t block_buf[BLOCK_SIZE] __attribute__((aligned(4)));   /* word-aligned for memcpy */

/*
 * Filesystem format
 */
int fs_format(uint32_t total_blocks) {
    if (total_blocks > MAX_BLOCKS || total_blocks < 16) {
        return FS_ERR_INVALID;
    }

    /* Calculate layout */
    uint32_t bitmap_blocks = (total_blocks + BLOCK_SIZE * 8 - 1) / (BLOCK_SIZE * 8);
    uint32_t inode_blocks = (MAX_INODES * INODE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t data_start = 1 + bitmap_blocks + inode_blocks;

    /* Initialize superblock */
    memset(&sb, 0, sizeof(sb));
    sb.magic = FS_MAGIC;
    sb.total_blocks = total_blocks;
    sb.total_inodes = MAX_INODES;
    sb.free_blocks = total_blocks - data_start;
    sb.free_inodes = MAX_INODES - 1;  /* Root inode is used */
    sb.bitmap_start = 1;
    sb.bitmap_blocks = bitmap_blocks;
    sb.inode_start = 1 + bitmap_blocks;
    sb.inode_blocks = inode_blocks;
    sb.data_start = data_start;

    /* Write superblock */
    if (block_write(0, &sb) != FS_OK) {
        return FS_ERR_IO;
    }

    /* Initialize bitmap - mark metadata blocks as used */
    memset(block_buf, 0, BLOCK_SIZE);
    for (uint32_t i = 0; i < data_start; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        block_buf[byte] |= (1 << bit);
    }

    /* Write bitmap block(s) */
    if (block_write(sb.bitmap_start, block_buf) != FS_OK) {
        return FS_ERR_IO;
    }

    /* Clear remaining bitmap blocks if any */
    if (bitmap_blocks > 1) {
        memset(block_buf, 0, BLOCK_SIZE);
        for (uint32_t i = 1; i < bitmap_blocks; i++) {
            if (block_write(sb.bitmap_start + i, block_buf) != FS_OK) {
                return FS_ERR_IO;
            }
        }
    }

    /* Initialize inode table - all inodes free */
    memset(block_buf, 0, BLOCK_SIZE);
    for (uint32_t i = 0; i < inode_blocks; i++) {
        if (block_write(sb.inode_start + i, block_buf) != FS_OK) {
            return FS_ERR_IO;
        }
    }

    /* Create root directory (inode 0) */
    struct inode root;
    memset(&root, 0, INODE_SIZE);
    root.type = FT_DIR;
    root.size = 0;

    if (inode_write(ROOT_INODE, &root) != FS_OK) {
        return FS_ERR_IO;
    }

    /* Add . and .. to root (both point to root itself) */
    if (dir_add(ROOT_INODE, ".", ROOT_INODE) != FS_OK) {
        return FS_ERR_IO;
    }
    if (dir_add(ROOT_INODE, "..", ROOT_INODE) != FS_OK) {
        return FS_ERR_IO;
    }

    return FS_OK;
}

/*
 * Filesystem mount
 */
int fs_mount(void) {
    /* Read superblock */
    if (block_read(0, &sb) != FS_OK) {
        return FS_ERR_IO;
    }

    /* Verify magic number */
    if (sb.magic != FS_MAGIC) {
        return FS_ERR_INVALID;
    }

    return FS_OK;
}

/*
 * fs_open - Open a file/directory by absolute path
 * Returns FS_OK and sets *ino on success
 * Returns FS_ERR_NOT_FOUND if path doesn't exist
 * Returns FS_ERR_NOT_DIR if intermediate component is not a directory
 */
int fs_open(const char *path, uint32_t *ino) {
    /* Path must start with '/' */
    if (path == NULL || path[0] != '/') {
        return FS_ERR_INVALID;
    }

    /* Start at root */
    uint32_t current_ino = ROOT_INODE;

    /* Skip leading '/' */
    const char *p = path + 1;

    /* If path is just "/", return root inode */
    if (*p == '\0') {
        *ino = ROOT_INODE;
        return FS_OK;
    }

    /* Parse path components */
    while (*p != '\0') {
        /* Extract the next component name */
        char name[MAX_FILENAME];
        int i = 0;

        while (*p != '\0' && *p != '/' && i < MAX_FILENAME - 1) {
            name[i++] = *p++;
        }
        name[i] = '\0';

        /* Skip trailing '/' */
        while (*p == '/') {
            p++;
        }

        /* Look up this component in current directory */
        uint32_t found_ino;
        int result = dir_lookup(current_ino, name, &found_ino);

        if (result == FS_ERR_NOT_DIR) {
            /* Current node is not a directory but we need to traverse further */
            return FS_ERR_NOT_DIR;
        }

        if (result != FS_OK) {
            return result;  /* FS_ERR_NOT_FOUND or other error */
        }

        /* If there are more components, verify this is a directory */
        if (*p != '\0') {
            struct inode in;
            if (inode_read(found_ino, &in) != FS_OK) {
                return FS_ERR_IO;
            }
            if (in.type != FT_DIR) {
                return FS_ERR_NOT_DIR;
            }
        }

        current_ino = found_ino;
    }

    *ino = current_ino;
    return FS_OK;
}

/*
 * fs_root_inode - Get the root directory inode number
 */
uint32_t fs_root_inode(void) {
    return ROOT_INODE;
}

/*
 * fs_mknod - Create a character device file
 * dir_ino: parent directory inode
 * name: device file name
 * major: major device number
 * minor: minor device number
 * Returns inode number on success, negative error code on failure
 */
int fs_mknod(uint32_t dir_ino, const char *name, uint8_t major, uint8_t minor) {
    /* Reject . and .. as explicit names */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return FS_ERR_INVALID;
    }

    /* Allocate a new inode */
    int ino = inode_alloc();
    if (ino < 0) {
        return ino;
    }

    /* Initialize the inode as a character device */
    struct inode in;
    memset(&in, 0, INODE_SIZE);
    in.type = FT_CHARDEV;
    in.link_count = 1;
    in.size = 0;
    in.major = major;
    in.minor = minor;

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

/*
 * fs_open_parent - Split a path into parent directory inode and final name.
 * E.g. "/foo/bar" -> parent_ino = inode of "/foo", *name = "bar"
 *      "/bar"     -> parent_ino = root inode,      *name = "bar"
 * Returns FS_OK on success, negative error on failure.
 */
int fs_open_parent(const char *path, uint32_t *parent_ino, const char **name) {
    const char *last_slash;
    const char *p;

    if (path == (const char *)0 || path[0] != '/') {
        return FS_ERR_INVALID;
    }

    /* Find the last '/' */
    last_slash = path;
    for (p = path; *p != '\0'; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }

    *name = last_slash + 1;

    /* Name must not be empty */
    if (**name == '\0') {
        return FS_ERR_INVALID;
    }

    if (last_slash == path) {
        /* Path is "/something" - parent is root */
        *parent_ino = fs_root_inode();
        return FS_OK;
    }

    /* Temporarily null-terminate the parent portion to resolve it.
     * We need a buffer since path may be in user read-only memory. */
    uint32_t parent_len = last_slash - path;
    if (parent_len >= FS_MAX_PATH) {
        return FS_ERR_INVALID;
    }

    char parent_path[FS_MAX_PATH];
    memcpy(parent_path, path, parent_len);
    parent_path[parent_len] = '\0';

    return fs_open(parent_path, parent_ino);
}

/*
 * fs_link - Create a hard link: name in directory dir_ino for the file target_ino
 * Returns FS_OK on success, negative error on failure.
 */
int fs_link(uint32_t target_ino, uint32_t dir_ino, const char *name) {
    struct inode in;

    if (inode_read(target_ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }
    if (in.type != FT_FILE) {
        return FS_ERR_INVALID;
    }

    int result = dir_add(dir_ino, name, target_ino);
    if (result != FS_OK) {
        return result;
    }

    in.link_count++;
    return inode_write(target_ino, &in);
}

/*
 * fs_rename - Rename/move the entry old_name of directory old_dir_ino to new_name
 * in directory new_dir_ino (a file or a directory).
 * Returns FS_OK on success, negative error on failure.
 */
int fs_rename(uint32_t old_dir_ino, const char *old_name, uint32_t new_dir_ino, const char *new_name) {
    uint32_t file_ino;

    int result = dir_lookup(old_dir_ino, old_name, &file_ino);
    if (result != FS_OK) {
        return result;
    }

    result = dir_add(new_dir_ino, new_name, file_ino);
    if (result != FS_OK) {
        return result;
    }

    result = dir_remove(old_dir_ino, old_name);
    if (result != FS_OK) {
        return result;
    }

    /* If directory, update ".." to point to new parent */
    struct inode in;
    if (inode_read(file_ino, &in) == FS_OK && in.type == FT_DIR) {
        dir_remove(file_ino, "..");
        dir_add(file_ino, "..", new_dir_ino);
    }

    return FS_OK;
}
