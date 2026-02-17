/*
 * System call definitions and kernel interface
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "trap.h"

/* System call numbers */
#define SYS_exit    0
#define SYS_read    1
#define SYS_write   2
#define SYS_open    3
#define SYS_close   4
#define SYS_exec    5
#define SYS_readdir 6
#define SYS_mkdir   7
#define SYS_rmdir   8
#define SYS_mknod   9
#define SYS_setenv   10
#define SYS_getenv   11
#define SYS_unsetenv      12
#define SYS_getenv_count  13
#define SYS_getenv_entry  14
#define SYS_chdir         15

/* Maximum number of arguments for exec syscall */
#define MAX_ARGC    8
#define MAX_ARG_LEN 64

/* Maximum number of environment variables for exec syscall */
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
 * Initialize the syscall subsystem.
 * Sets up fd table with stdin/stdout/stderr pointing to console.
 */
void syscall_init(void);

/*
 * Dispatch a system call based on trap frame contents.
 * Called from c_trap_handler when mcause indicates ecall.
 * Returns 1 if program should exit, 0 otherwise.
 */
int syscall_dispatch(trap_frame_t *tf);

/*
 * Get the exit code from sys_exit.
 * Valid only after syscall_dispatch returns 1.
 */
int syscall_get_exit_code(void);

/*
 * Initialize the kernel environment with default values.
 * Called once at boot.
 */
void syscall_env_init(void);

/*
 * Set a kernel environment variable from within the kernel.
 * Used to store exit code in "?" before reloading shell.
 */
void syscall_set_env(const char *name, const char *value);

#endif /* SYSCALL_H */
