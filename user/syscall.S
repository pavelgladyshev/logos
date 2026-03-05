/*
 * User-space system call stubs
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Provides the low-level interface to invoke system calls via ecall.
 * These have the standard C calling convention and are called directly
 * from user code (declared in libc.h).
 *
 * Syscall numbers are defined in kernel/syscall_nr.h (shared with the kernel).
 */

#include "syscall_nr.h"

    .text

/* void exit(int status) */
    .globl exit
    .type exit, @function
exit:
    li   a7, SYS_exit
    ecall
    /* Should not return, but just in case: */
1:  j    1b

    .size exit, .-exit

/* ssize_t read(int fd, void *buf, size_t count) */
    .globl read
    .type read, @function
read:
    li   a7, SYS_read
    ecall
    ret

    .size read, .-read

/* ssize_t write(int fd, const void *buf, size_t count) */
    .globl write
    .type write, @function
write:
    li   a7, SYS_write
    ecall
    ret

    .size write, .-write

/* int open(const char *path, int flags) */
    .globl open
    .type open, @function
open:
    li   a7, SYS_open
    ecall
    ret

    .size open, .-open

/* int close(int fd) */
    .globl close
    .type close, @function
close:
    li   a7, SYS_close
    ecall
    ret

    .size close, .-close

/* int spawnve(const char *path, char *const argv[], char *const envp[]) */
    .globl spawnve
    .type spawnve, @function
spawnve:
    li   a7, SYS_spawn
    ecall
    ret             /* Returns child exit code or negative error */

    .size spawnve, .-spawnve

/* int readdir(const char *path, struct dirent *entries, int max_entries) */
    .globl readdir
    .type readdir, @function
readdir:
    li   a7, SYS_readdir
    ecall
    ret

    .size readdir, .-readdir

/* int mkdir(const char *path) */
    .globl mkdir
    .type mkdir, @function
mkdir:
    li   a7, SYS_mkdir
    ecall
    ret

    .size mkdir, .-mkdir

/* int rmdir(const char *path) */
    .globl rmdir
    .type rmdir, @function
rmdir:
    li   a7, SYS_rmdir
    ecall
    ret

    .size rmdir, .-rmdir

/* int mknod(const char *path, int major, int minor) */
    .globl mknod
    .type mknod, @function
mknod:
    li   a7, SYS_mknod
    ecall
    ret

    .size mknod, .-mknod

/* int setenv(const char *name, const char *value) */
    .globl setenv
    .type setenv, @function
setenv:
    li   a7, SYS_setenv
    ecall
    ret

    .size setenv, .-setenv

/* int getenv_r(const char *name, char *buf, int buflen) */
    .globl getenv_r
    .type getenv_r, @function
getenv_r:
    li   a7, SYS_getenv
    ecall
    ret

    .size getenv_r, .-getenv_r

/* int unsetenv(const char *name) */
    .globl unsetenv
    .type unsetenv, @function
unsetenv:
    li   a7, SYS_unsetenv
    ecall
    ret

    .size unsetenv, .-unsetenv

/* int env_count(void) */
    .globl env_count
    .type env_count, @function
env_count:
    li   a7, SYS_getenv_count
    ecall
    ret

    .size env_count, .-env_count

/* int getenv_entry(int index, char *buf, int buflen) */
    .globl getenv_entry
    .type getenv_entry, @function
getenv_entry:
    li   a7, SYS_getenv_entry
    ecall
    ret

    .size getenv_entry, .-getenv_entry

/* int chdir(const char *path) */
    .globl chdir
    .type chdir, @function
chdir:
    li   a7, SYS_chdir
    ecall
    ret

    .size chdir, .-chdir
