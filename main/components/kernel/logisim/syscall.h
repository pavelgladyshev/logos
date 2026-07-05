/*
 * System call definitions and kernel interface
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "trap.h"
#include "syscall_nr.h"  /* SYS_exit, SYS_read, ... */

/* Maximum number of arguments for spawn syscall */
#define MAX_ARGC    8
#define MAX_ARG_LEN 64

/* Maximum number of environment variables for spawn syscall */
#define MAX_ENVC    16
#define MAX_ENV_LEN 64

/* Maximum number of file descriptors per process */
#define MAX_FD      8

/* File descriptor entry */
struct fd_entry {
    int in_use;          /* 1 if this fd is open */
    uint32_t inode;      /* Inode number for files */
    uint32_t offset;     /* Current position for regular files */
    uint8_t type;        /* FT_FILE, FT_CHARDEV, etc. */
    uint8_t major;       /* Major device number (for char devices) */
    uint8_t minor;       /* Minor device number (for char devices) */
};

/*
 * Dispatch a system call based on trap frame contents.
 * Called from c_trap_handler when mcause indicates ecall.
 * Returns 1 if top-level program should exit, 0 otherwise.
 *
 * Note: sys_spawn and sys_exit may call trap_ret() directly
 * and never return to this function.
 */
int syscall_dispatch(trap_frame_t *tf);

#endif /* SYSCALL_H */
