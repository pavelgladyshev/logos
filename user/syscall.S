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

/* int fork(void) */
    .globl fork
    .type fork, @function
fork:
    li   a7, SYS_fork
    ecall
    ret

    .size fork, .-fork

/* int execve(const char *path, char *const argv[], char *const envp[]) */
    .globl execve
    .type execve, @function
execve:
    li   a7, SYS_exec
    ecall
    ret             /* Only returns on failure */

    .size execve, .-execve

/* int wait(void) */
    .globl wait
    .type wait, @function
wait:
    li   a7, SYS_wait
    ecall
    ret

    .size wait, .-wait

/* int getpid(void) */
    .globl getpid
    .type getpid, @function
getpid:
    li   a7, SYS_getpid
    ecall
    ret

    .size getpid, .-getpid

/* int dup(int oldfd) */
    .globl dup
    .type dup, @function
dup:
    li   a7, SYS_dup
    ecall
    ret

    .size dup, .-dup

/* int dup2(int oldfd, int newfd) */
    .globl dup2
    .type dup2, @function
dup2:
    li   a7, SYS_dup2
    ecall
    ret

    .size dup2, .-dup2

/* int pipe(int pipefd[2]) */
    .globl pipe
    .type pipe, @function
pipe:
    li   a7, SYS_pipe
    ecall
    ret

    .size pipe, .-pipe

/* int unlink(const char *path) */
    .globl unlink
    .type unlink, @function
unlink:
    li   a7, SYS_unlink
    ecall
    ret

    .size unlink, .-unlink

/* int link(const char *target, const char *linkpath) */
    .globl link
    .type link, @function
link:
    li   a7, SYS_link
    ecall
    ret

    .size link, .-link

/* int rename(const char *oldpath, const char *newpath) */
    .globl rename
    .type rename, @function
rename:
    li   a7, SYS_rename
    ecall
    ret

    .size rename, .-rename

/* int stat(const char *path, struct stat_info *si) */
    .globl stat
    .type stat, @function
stat:
    li   a7, SYS_stat
    ecall
    ret

    .size stat, .-stat

/* int kill(int pid) */
    .globl kill
    .type kill, @function
kill:
    li   a7, SYS_kill
    ecall
    ret

    .size kill, .-kill

/* int ps(struct proc_info *buf, int max_entries) */
    .globl ps
    .type ps, @function
ps:
    li   a7, SYS_ps
    ecall
    ret

    .size ps, .-ps

/* int shmget(int key, int flags) */
    .globl shmget
    .type shmget, @function
shmget:
    li   a7, SYS_shmget
    ecall
    ret

    .size shmget, .-shmget

/* void *shmat(int shm_id) */
    .globl shmat
    .type shmat, @function
shmat:
    li   a7, SYS_shmat
    ecall
    ret

    .size shmat, .-shmat

/* int shmdt(int shm_id) */
    .globl shmdt
    .type shmdt, @function
shmdt:
    li   a7, SYS_shmdt
    ecall
    ret

    .size shmdt, .-shmdt

/* int semget(int key, int value, int flags) */
    .globl semget
    .type semget, @function
semget:
    li   a7, SYS_semget
    ecall
    ret

    .size semget, .-semget

/* int semwait(int sem_id) */
    .globl semwait
    .type semwait, @function
semwait:
    li   a7, SYS_semwait
    ecall
    ret

    .size semwait, .-semwait

/* int sempost(int sem_id) */
    .globl sempost
    .type sempost, @function
sempost:
    li   a7, SYS_sempost
    ecall
    ret

    .size sempost, .-sempost

/* int semclose(int sem_id) */
    .globl semclose
    .type semclose, @function
semclose:
    li   a7, SYS_semclose
    ecall
    ret

    .size semclose, .-semclose
