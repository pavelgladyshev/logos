/*
 * User-space system call stubs
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Provides the low-level interface to invoke system calls via ecall.
 * These have the standard C calling convention and are called directly
 * from user code (declared in libc.h).
 */

    .text

/* void exit(int status) - SYS_exit = 0 */
    .globl exit
    .type exit, @function
exit:
    li   a7, 0      /* SYS_exit */
    ecall
    /* Should not return, but just in case: */
1:  j    1b

    .size exit, .-exit

/* ssize_t read(int fd, void *buf, size_t count) - SYS_read = 1 */
    .globl read
    .type read, @function
read:
    li   a7, 1      /* SYS_read */
    ecall
    ret

    .size read, .-read

/* ssize_t write(int fd, const void *buf, size_t count) - SYS_write = 2 */
    .globl write
    .type write, @function
write:
    li   a7, 2      /* SYS_write */
    ecall
    ret

    .size write, .-write

/* int open(const char *path, int flags) - SYS_open = 3 */
    .globl open
    .type open, @function
open:
    li   a7, 3      /* SYS_open */
    ecall
    ret

    .size open, .-open

/* int close(int fd) - SYS_close = 4 */
    .globl close
    .type close, @function
close:
    li   a7, 4      /* SYS_close */
    ecall
    ret

    .size close, .-close

/* int execve(const char *path, char *const argv[], char *const envp[]) - SYS_exec = 5 */
    .globl execve
    .type execve, @function
execve:
    li   a7, 5      /* SYS_exec */
    ecall
    ret             /* Only returns on error */

    .size execve, .-execve

/* int readdir(const char *path, struct dirent *entries, int max_entries) - SYS_readdir = 6 */
    .globl readdir
    .type readdir, @function
readdir:
    li   a7, 6      /* SYS_readdir */
    ecall
    ret

    .size readdir, .-readdir

/* int mkdir(const char *path) - SYS_mkdir = 7 */
    .globl mkdir
    .type mkdir, @function
mkdir:
    li   a7, 7      /* SYS_mkdir */
    ecall
    ret

    .size mkdir, .-mkdir

/* int rmdir(const char *path) - SYS_rmdir = 8 */
    .globl rmdir
    .type rmdir, @function
rmdir:
    li   a7, 8      /* SYS_rmdir */
    ecall
    ret

    .size rmdir, .-rmdir

/* int mknod(const char *path, int major, int minor) - SYS_mknod = 9 */
    .globl mknod
    .type mknod, @function
mknod:
    li   a7, 9      /* SYS_mknod */
    ecall
    ret

    .size mknod, .-mknod

/* int setenv(const char *name, const char *value) - SYS_setenv = 10 */
    .globl setenv
    .type setenv, @function
setenv:
    li   a7, 10     /* SYS_setenv */
    ecall
    ret

    .size setenv, .-setenv

/* int getenv_r(const char *name, char *buf, int buflen) - SYS_getenv = 11 */
    .globl getenv_r
    .type getenv_r, @function
getenv_r:
    li   a7, 11     /* SYS_getenv */
    ecall
    ret

    .size getenv_r, .-getenv_r

/* int unsetenv(const char *name) - SYS_unsetenv = 12 */
    .globl unsetenv
    .type unsetenv, @function
unsetenv:
    li   a7, 12     /* SYS_unsetenv */
    ecall
    ret

    .size unsetenv, .-unsetenv

/* int env_count(void) - SYS_getenv_count = 13 */
    .globl env_count
    .type env_count, @function
env_count:
    li   a7, 13     /* SYS_getenv_count */
    ecall
    ret

    .size env_count, .-env_count

/* int getenv_entry(int index, char *buf, int buflen) - SYS_getenv_entry = 14 */
    .globl getenv_entry
    .type getenv_entry, @function
getenv_entry:
    li   a7, 14     /* SYS_getenv_entry */
    ecall
    ret

    .size getenv_entry, .-getenv_entry

/* int chdir(const char *path) - SYS_chdir = 15 */
    .globl chdir
    .type chdir, @function
chdir:
    li   a7, 15     /* SYS_chdir */
    ecall
    ret

    .size chdir, .-chdir
