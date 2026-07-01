/*
 * Native filesystem utility for managing files in block storage
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage:
 *   fstool format <blockfile> <num_blocks>   - Format with new filesystem
 *   fstool ls <blockfile> <path>             - List directory contents
 *   fstool add <blockfile> <path> <hostfile> - Copy file into filesystem
 */

#include <stdio.h>
#include <stdlib.h>
#include "fs.h"
#include "native_block.h"

/*
 * Get parent directory inode and filename from a path
 * Returns FS_OK on success
 */
static int path_parent(const char *path, uint32_t *parent_ino, char *filename) {
    if (path == NULL || path[0] != '/' || parent_ino == NULL || filename == NULL) {
        return FS_ERR_INVALID;
    }

    /* Find the last '/' in the path */
    const char *last_slash = path;
    const char *p = path;
    while (*p) {
        if (*p == '/') {
            last_slash = p;
        }
        p++;
    }

    /* Extract filename (part after last slash) */
    const char *fname = last_slash + 1;
    if (*fname == '\0') {
        return FS_ERR_INVALID;  /* Path ends with '/' */
    }

    /* Copy filename */
    int i = 0;
    while (*fname && i < MAX_FILENAME - 1) {
        filename[i++] = *fname++;
    }
    filename[i] = '\0';

    /* Get parent directory path */
    if (last_slash == path) {
        /* Parent is root */
        *parent_ino = fs_root_inode();
        return FS_OK;
    }

    /* Create parent path */
    size_t parent_len = last_slash - path;
    char *parent_path = malloc(parent_len + 1);
    if (parent_path == NULL) {
        return FS_ERR_IO;
    }

    memcpy(parent_path, path, parent_len);
    parent_path[parent_len] = '\0';

    /* Look up parent */
    int result = fs_open(parent_path, parent_ino);
    free(parent_path);

    if (result != FS_OK) {
        return result;
    }

    /* Verify parent is a directory */
    uint8_t type;
    if (inode_get_type(*parent_ino, &type) != FS_OK) {
        return FS_ERR_IO;
    }
    if (type != FT_DIR) {
        return FS_ERR_NOT_DIR;
    }

    return FS_OK;
}

/*
 * Print usage information
 */
static void print_usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s format <blockfile> <num_blocks>   - Format with new filesystem\n", prog);
    fprintf(stderr, "  %s ls <blockfile> <path>             - List directory contents\n", prog);
    fprintf(stderr, "  %s mkdir <blockfile> <path>          - Create directory\n", prog);
    fprintf(stderr, "  %s add <blockfile> <path> <hostfile> - Copy file into filesystem\n", prog);
}

/*
 * Format command
 */
static int cmd_format(const char *blockfile, const char *num_blocks_str) {
    uint32_t num_blocks = (uint32_t)atoi(num_blocks_str);

    if (num_blocks < 16 || num_blocks > MAX_BLOCKS) {
        fprintf(stderr, "Error: num_blocks must be between 16 and %d\n", MAX_BLOCKS);
        return 1;
    }

    printf("Creating block file '%s' with %u blocks...\n", blockfile, num_blocks);

    if (native_backing_store_create(blockfile, num_blocks) != 0) {
        fprintf(stderr, "Error: Failed to create block file\n");
        return 1;
    }

    printf("Formatting filesystem...\n");
    int result = fs_format(num_blocks);
    if (result != FS_OK) {
        fprintf(stderr, "Error: fs_format failed with code %d\n", result);
        native_backing_store_close();
        return 1;
    }

    printf("Filesystem formatted successfully.\n");

    native_backing_store_close();
    return 0;
}

/*
 * List directory command
 */
static int cmd_ls(const char *blockfile, const char *path) {
    if (native_backing_store_open(blockfile) != 0) {
        fprintf(stderr, "Error: Failed to open block file '%s'\n", blockfile);
        return 1;
    }

    if (fs_mount() != FS_OK) {
        fprintf(stderr, "Error: Failed to mount filesystem (invalid or corrupted)\n");
        native_backing_store_close();
        return 1;
    }

    uint32_t dir_ino;
    int result = fs_open(path, &dir_ino);
    if (result != FS_OK) {
        fprintf(stderr, "Error: Path '%s' not found (code %d)\n", path, result);
        native_backing_store_close();
        return 1;
    }

    /* Get directory entries using dir_list */
    struct dirent entries[MAX_INODES];
    uint32_t count;

    result = dir_list(dir_ino, entries, MAX_INODES, &count);
    if (result == FS_ERR_NOT_DIR) {
        fprintf(stderr, "Error: '%s' is not a directory\n", path);
        native_backing_store_close();
        return 1;
    }
    if (result != FS_OK) {
        fprintf(stderr, "Error: Failed to list directory (code %d)\n", result);
        native_backing_store_close();
        return 1;
    }

    printf("Contents of '%s':\n", path);

    for (uint32_t i = 0; i < count; i++) {
        uint8_t type;
        uint32_t size;
        if (inode_get_type(entries[i].inode, &type) == FS_OK &&
            inode_get_size(entries[i].inode, &size) == FS_OK) {
            const char *type_str = (type == FT_DIR) ? "DIR " : "FILE";
            printf("  %s  %6u  %s\n", type_str, size, entries[i].name);
        }
    }

    native_backing_store_close();
    return 0;
}

/*
 * Make directory command
 */
static int cmd_mkdir(const char *blockfile, const char *path) {
    if (native_backing_store_open(blockfile) != 0) {
        fprintf(stderr, "Error: Failed to open block file '%s'\n", blockfile);
        return 1;
    }

    if (fs_mount() != FS_OK) {
        fprintf(stderr, "Error: Failed to mount filesystem\n");
        native_backing_store_close();
        return 1;
    }

    /* Get parent directory and directory name */
    uint32_t parent_ino;
    char dirname[MAX_FILENAME];

    int result = path_parent(path, &parent_ino, dirname);
    if (result != FS_OK) {
        fprintf(stderr, "Error: Invalid path '%s' (code %d)\n", path, result);
        native_backing_store_close();
        return 1;
    }

    /* Create directory */
    int dir_ino = fs_mkdir(parent_ino, dirname);
    if (dir_ino < 0) {
        fprintf(stderr, "Error: Failed to create directory (code %d)\n", dir_ino);
        native_backing_store_close();
        return 1;
    }

    printf("Created directory '%s'\n", path);

    native_backing_store_close();
    return 0;
}

/*
 * Add file command - copy host file into filesystem
 */
static int cmd_add(const char *blockfile, const char *path, const char *hostfile) {
    /* Open and read host file first */
    FILE *hf = fopen(hostfile, "rb");
    if (hf == NULL) {
        fprintf(stderr, "Error: Cannot open host file '%s'\n", hostfile);
        return 1;
    }

    /* Get file size */
    fseek(hf, 0, SEEK_END);
    long file_size = ftell(hf);
    fseek(hf, 0, SEEK_SET);

    if (file_size > DIRECT_BLOCKS * BLOCK_SIZE) {
        fprintf(stderr, "Error: File too large (max %d bytes)\n", DIRECT_BLOCKS * BLOCK_SIZE);
        fclose(hf);
        return 1;
    }

    /* Read file content */
    uint8_t *content = malloc(file_size > 0 ? file_size : 1);
    if (content == NULL) {
        fprintf(stderr, "Error: Out of memory\n");
        fclose(hf);
        return 1;
    }

    if (file_size > 0 && fread(content, file_size, 1, hf) != 1) {
        fprintf(stderr, "Error: Failed to read host file\n");
        free(content);
        fclose(hf);
        return 1;
    }
    fclose(hf);

    /* Open block file */
    if (native_backing_store_open(blockfile) != 0) {
        fprintf(stderr, "Error: Failed to open block file '%s'\n", blockfile);
        free(content);
        return 1;
    }

    if (fs_mount() != FS_OK) {
        fprintf(stderr, "Error: Failed to mount filesystem\n");
        free(content);
        native_backing_store_close();
        return 1;
    }

    /* Get parent directory and filename */
    uint32_t parent_ino;
    char filename[MAX_FILENAME];

    int result = path_parent(path, &parent_ino, filename);
    if (result != FS_OK) {
        fprintf(stderr, "Error: Invalid path '%s' (code %d)\n", path, result);
        free(content);
        native_backing_store_close();
        return 1;
    }

    /* Create file */
    int file_ino = file_create(parent_ino, filename);
    if (file_ino < 0) {
        fprintf(stderr, "Error: Failed to create file (code %d)\n", file_ino);
        free(content);
        native_backing_store_close();
        return 1;
    }

    /* Write content */
    if (file_size > 0) {
        result = file_write(file_ino, 0, content, file_size);
        if (result < 0) {
            fprintf(stderr, "Error: Failed to write file (code %d)\n", result);
            free(content);
            native_backing_store_close();
            return 1;
        }
    }

    printf("Added '%s' (%ld bytes) to '%s'\n", hostfile, file_size, path);

    free(content);
    native_backing_store_close();
    return 0;
}

/*
 * Main entry point
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "format") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s format <blockfile> <num_blocks>\n", argv[0]);
            return 1;
        }
        return cmd_format(argv[2], argv[3]);
    }
    else if (strcmp(cmd, "ls") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s ls <blockfile> <path>\n", argv[0]);
            return 1;
        }
        return cmd_ls(argv[2], argv[3]);
    }
    else if (strcmp(cmd, "mkdir") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s mkdir <blockfile> <path>\n", argv[0]);
            return 1;
        }
        return cmd_mkdir(argv[2], argv[3]);
    }
    else if (strcmp(cmd, "add") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Usage: %s add <blockfile> <path> <hostfile>\n", argv[0]);
            return 1;
        }
        return cmd_add(argv[2], argv[3], argv[4]);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}
