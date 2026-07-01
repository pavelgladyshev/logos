/*
 * Bootloader filesystem: minimal read-only filesystem traversal
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "boot.h"

/* Read an inode from the inode table */
static int boot_inode_read(uint32_t ino, struct superblock *sb,
                           uint8_t *buf, struct inode *out)
{
    uint32_t block = sb->inode_start + (ino / INODES_PER_BLOCK);
    uint32_t offset = (ino % INODES_PER_BLOCK) * INODE_SIZE;

    if (boot_block_read(block, buf) != 0)
        return -1;

    boot_memcpy(out, buf + offset, INODE_SIZE);
    return 0;
}

/* Look up a name in a directory inode's data blocks */
static int boot_dir_lookup(struct inode *dir, struct superblock *sb,
                           uint8_t *buf, const char *name, uint32_t *found_ino)
{
    uint32_t entries_total;
    uint32_t entry_idx;
    int b, i;

    if (dir->type != FT_DIR)
        return -1;

    entries_total = dir->size / DIRENT_SIZE;
    entry_idx = 0;

    for (b = 0; b < DIRECT_BLOCKS && entry_idx < entries_total; b++) {
        struct dirent *de;

        if (dir->blocks[b] == 0)
            continue;

        if (boot_block_read(dir->blocks[b], buf) != 0)
            return -1;

        de = (struct dirent *)buf;
        for (i = 0; i < (int)DIRENTS_PER_BLOCK && entry_idx < entries_total; i++, entry_idx++) {
            if (de[i].inode != DIRENT_FREE && boot_strcmp(de[i].name, name) == 0) {
                *found_ino = de[i].inode;
                return 0;
            }
        }
    }

    return -1;
}

/* Resolve an absolute path to an inode number */
int boot_resolve_path(const char *path, struct superblock *sb,
                      uint8_t *buf, uint32_t *result_ino)
{
    uint32_t current_ino;
    const char *p;

    if (path == 0 || path[0] != '/')
        return -1;

    current_ino = ROOT_INODE;
    p = path + 1;

    /* Root directory "/" */
    if (*p == '\0') {
        *result_ino = ROOT_INODE;
        return 0;
    }

    while (*p != '\0') {
        char name[MAX_FILENAME];
        int i = 0;
        struct inode dir;
        uint32_t found_ino;

        /* Extract next path component */
        while (*p != '\0' && *p != '/' && i < MAX_FILENAME - 1)
            name[i++] = *p++;
        name[i] = '\0';

        /* Skip trailing slashes */
        while (*p == '/') p++;

        /* Read current directory inode */
        if (boot_inode_read(current_ino, sb, buf, &dir) != 0)
            return -1;

        /* Lookup component in directory */
        if (boot_dir_lookup(&dir, sb, buf, name, &found_ino) != 0)
            return -1;

        current_ino = found_ino;
    }

    *result_ino = current_ino;
    return 0;
}

/*
 * Read file data from an inode into a destination buffer.
 * Returns bytes read, or -1 on error.
 */
int boot_file_read(uint32_t ino, struct superblock *sb, uint8_t *buf,
                   uint32_t offset, void *dst, uint32_t len)
{
    struct inode in;
    uint8_t *d;
    uint32_t bytes_read;

    if (boot_inode_read(ino, sb, buf, &in) != 0)
        return -1;

    if (offset >= in.size)
        return 0;
    if (offset + len > in.size)
        len = in.size - offset;

    d = (uint8_t *)dst;
    bytes_read = 0;

    while (len > 0) {
        uint32_t block_idx = offset / BLOCK_SIZE;
        uint32_t block_off = offset % BLOCK_SIZE;
        uint32_t chunk = BLOCK_SIZE - block_off;
        if (chunk > len) chunk = len;

        if (block_idx >= DIRECT_BLOCKS || in.blocks[block_idx] == 0)
            break;

        if (boot_block_read(in.blocks[block_idx], buf) != 0)
            return -1;

        boot_memcpy(d, buf + block_off, chunk);

        d += chunk;
        offset += chunk;
        len -= chunk;
        bytes_read += chunk;
    }

    return bytes_read;
}
