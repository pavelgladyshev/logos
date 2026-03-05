/*
 * System call numbers — shared between kernel and user space
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * This header contains only preprocessor defines, so it is safe to
 * include from both C code and assembly (.S) files.
 */

#ifndef SYSCALL_NR_H
#define SYSCALL_NR_H

#define SYS_exit          0
#define SYS_read          1
#define SYS_write         2
#define SYS_open          3
#define SYS_close         4
#define SYS_spawn         5
#define SYS_readdir       6
#define SYS_mkdir         7
#define SYS_rmdir         8
#define SYS_mknod         9
#define SYS_setenv        10
#define SYS_getenv        11
#define SYS_unsetenv      12
#define SYS_getenv_count  13
#define SYS_getenv_entry  14
#define SYS_chdir         15

#endif /* SYSCALL_NR_H */
