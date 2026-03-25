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
#define SYS_fork          16
#define SYS_exec          17
#define SYS_wait          18
#define SYS_getpid        19
#define SYS_dup           20
#define SYS_dup2          21
#define SYS_pipe          22
#define SYS_unlink        23
#define SYS_link          24
#define SYS_rename        25
#define SYS_stat          26
#define SYS_kill          27
#define SYS_ps            28
#define SYS_shmget        29
#define SYS_shmat         30
#define SYS_shmdt         31
#define SYS_semget        32
#define SYS_semwait       33
#define SYS_sempost       34
#define SYS_semclose      35

#endif /* SYSCALL_NR_H */
