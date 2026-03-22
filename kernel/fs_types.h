/*
 * Core filesystem types and constants
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef FS_TYPES_H
#define FS_TYPES_H

#ifdef NATIVE_BUILD
    #include <stdint.h>
#else
    #include "types.h"
#endif

/* File system constants */
#define BLOCK_SIZE         512       /* Size of each block in bytes */
#define MAX_FILENAME       28        /* Maximum filename length */
#define DIRECT_BLOCKS      60        /* Number of direct block pointers in inode */
#define MAX_INODES         64        /* Maximum number of inodes */
#define MAX_BLOCKS         1024      /* Maximum number of blocks */
#define ROOT_INODE         0         /* Inode number of root directory */

/* Magic number to identify our filesystem */
#define FS_MAGIC           0x53465346  /* "FSFS" */

/* File types */
#define FT_FREE            0         /* Inode is free */
#define FT_FILE            1         /* Regular file */
#define FT_DIR             2         /* Directory */
#define FT_CHARDEV         3         /* Character device */
#define FT_PIPE            4         /* Pipe (IPC) */

/* Error codes */
#define FS_OK              0
#define FS_ERR_IO          (-1)
#define FS_ERR_NOT_FOUND   (-2)
#define FS_ERR_EXISTS      (-3)
#define FS_ERR_NO_SPACE    (-4)
#define FS_ERR_NOT_DIR     (-5)
#define FS_ERR_IS_DIR      (-6)
#define FS_ERR_NOT_EMPTY   (-7)
#define FS_ERR_INVALID     (-8)

/*
 * Superblock structure (stored in block 0)
 * Contains metadata about the filesystem
 */
struct superblock {
    uint32_t magic;            /* Magic number to identify filesystem */
    uint32_t total_blocks;     /* Total number of blocks in filesystem */
    uint32_t total_inodes;     /* Total number of inodes */
    uint32_t free_blocks;      /* Number of free blocks */
    uint32_t free_inodes;      /* Number of free inodes */
    uint32_t bitmap_start;     /* Starting block of block bitmap */
    uint32_t bitmap_blocks;    /* Number of blocks used by bitmap */
    uint32_t inode_start;      /* Starting block of inode table */
    uint32_t inode_blocks;     /* Number of blocks used by inode table */
    uint32_t data_start;       /* Starting block of data area */
    uint8_t  reserved[BLOCK_SIZE - 40];  /* Padding to fill block */
};

/*
 * Inode structure
 * Contains metadata about a file or directory
 * For device files: major/minor store device numbers, blocks[] unused
 */
struct inode {
    uint32_t size;             /* File size in bytes */
    uint8_t  type;             /* File type (FT_FREE, FT_FILE, FT_DIR, FT_CHARDEV) */
    uint8_t  major;            /* Major device number (for FT_CHARDEV) */
    uint8_t  minor;            /* Minor device number (for FT_CHARDEV) */
    uint8_t  link_count;       /* Number of hard links to this inode */
    uint16_t blocks[DIRECT_BLOCKS];  /* Direct block pointers (16-bit indices) */
};

/* Size: 4 + 1 + 1 + 1 + 1 + 60*2 = 128 bytes per inode */
#define INODE_SIZE         sizeof(struct inode)
#define INODES_PER_BLOCK   (BLOCK_SIZE / INODE_SIZE)

/*
 * Directory entry structure
 * Maps filename to inode number
 */
struct dirent {
    uint32_t inode;            /* Inode number (DIRENT_FREE = entry is free) */
    char     name[MAX_FILENAME]; /* Filename (null-terminated) */
};

/* Sentinel value for free directory entries (never a valid inode number) */
#define DIRENT_FREE        0xFFFFFFFF

/* Size: 4 + 28 = 32 bytes per directory entry */
#define DIRENT_SIZE        sizeof(struct dirent)
#define DIRENTS_PER_BLOCK  (BLOCK_SIZE / DIRENT_SIZE)

/*
 * Stat information returned by sys_stat
 */
struct stat_info {
    uint32_t ino;              /* Inode number */
    uint32_t size;             /* File size in bytes */
    uint8_t  type;             /* File type (FT_FILE, FT_DIR, FT_CHARDEV) */
    uint8_t  link_count;       /* Number of hard links */
    uint8_t  major;            /* Major device number */
    uint8_t  minor;            /* Minor device number */
};

/*
 * Process information returned by sys_ps
 */
struct proc_info {
    int pid;                   /* Process ID (0 = unused slot) */
    int state;                 /* PROC_FREE/READY/RUNNING/SLEEPING/ZOMBIE */
    int parent;                /* Parent slot index (-1 = kernel) */
    char name[32];             /* Program name (basename of ELF path) */
};

#endif /* FS_TYPES_H */
