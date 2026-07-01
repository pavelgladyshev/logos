/*
 * User-space C library
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Provides standard library functions for user programs.
 * All I/O goes through system calls to the kernel.
 */

#ifndef USER_LIBC_H
#define USER_LIBC_H

/* Basic types */
typedef unsigned int size_t;
typedef int ssize_t;

/* Standard file descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Open flags (simplified) */
/* Open flags */
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   0x100
#define O_TRUNC   0x200
#define O_APPEND  0x400

/*
 * System call wrappers
 */

/* Terminate the program with given exit status */
void exit(int status) __attribute__((noreturn));

/* Read from file descriptor */
ssize_t read(int fd, void *buf, size_t count);

/* Write to file descriptor */
ssize_t write(int fd, const void *buf, size_t count);

/* Open a file */
int open(const char *path, int flags);

/* Close a file descriptor */
int close(int fd);

/* Spawn a child program in a new process slot (with environment)
 * path: path to executable
 * argv: NULL-terminated array of argument strings
 * envp: NULL-terminated array of "KEY=value" strings (or NULL to inherit)
 * Returns child's exit code (>= 0) on success, negative on failure
 */
int spawnve(const char *path, char *const argv[], char *const envp[]);

/* Spawn a child program (inherits parent's environment)
 * Convenience wrapper: calls spawnve(path, argv, NULL)
 */
int spawn(const char *path, char *const argv[]);

/* Change the current working directory.
 * Returns 0 on success, negative error on failure.
 */
int chdir(const char *path);

/*
 * Filesystem operations
 */

/* Maximum filename length (must match kernel's MAX_FILENAME) */
#define MAX_FILENAME 28

/* Directory entry structure (matches kernel's struct dirent) */
struct dirent {
    unsigned int inode;          /* Inode number (0 = entry is free) */
    char         name[MAX_FILENAME]; /* Filename (null-terminated) */
};

/* List directory entries.
 * path: absolute path to directory
 * entries: buffer to receive directory entries
 * max_entries: maximum number of entries to return
 * Returns: number of entries read, or negative error
 */
int readdir(const char *path, struct dirent *entries, int max_entries);

/* Create a directory */
int mkdir(const char *path);

/* Remove a directory (must be empty) */
int rmdir(const char *path);

/* Create a device node */
int mknod(const char *path, int major, int minor);

/* Delete a file */
int unlink(const char *path);

/* Create a hard link (new name for existing file) */
int link(const char *target, const char *linkpath);

/* Rename/move a file or directory */
int rename(const char *oldpath, const char *newpath);

/* File metadata (matches kernel's struct stat_info) */
struct stat_info {
    unsigned int ino;            /* Inode number */
    unsigned int size;           /* File size in bytes */
    unsigned char type;          /* File type */
    unsigned char link_count;    /* Number of hard links */
    unsigned char major;         /* Major device number */
    unsigned char minor;         /* Minor device number */
};

/* Get file metadata */
int stat(const char *path, struct stat_info *si);

/*
 * Environment variables
 *
 * Each process has its own environment, inherited from its parent on
 * spawn(). Variables are "KEY=value" strings. Maximum 16 variables,
 * 64 chars each. Changes in a child do not affect the parent.
 */

/* Set a variable. Returns 0 on success, -1 if full. */
int setenv(const char *name, const char *value);

/* Remove a variable. Returns 0 on success, -1 if not found. */
int unsetenv(const char *name);

/* Return number of environment variables currently set. */
int env_count(void);

/* Get value by name. Returns pointer to static buffer (valid until next
 * getenv call), or NULL if not found. */
char *getenv(const char *name);

/* Get value by name into caller's buffer (reentrant).
 * Returns length of value on success, -1 if not found. */
int getenv_r(const char *name, char *buf, int buflen);

/* Get Nth env entry as "KEY=value" into caller's buffer.
 * Returns length on success, -1 if index out of range. */
int getenv_entry(int index, char *buf, int buflen);

/* Build a NULL-terminated envp array from current environment.
 * Caller provides the array (must hold env_count()+1 entries).
 * Each pointer points to a static buffer (valid until next env_to_envp call).
 * Returns number of entries. */
int env_to_envp(char *envp[]);

/*
 * Standard I/O functions (built on syscalls)
 */

/* Write a single character to stdout */
int putchar(int c);

/* Write a string followed by newline to stdout (C standard) */
int puts(const char *s);

/* Formatted output to stdout */
int printf(const char *fmt, ...);

/* Read a single character from stdin */
int getchar(void);

/* Read a line from stdin (safe version with size limit)
 * Reads until newline or size-1 characters, whichever comes first.
 * Echoes characters as typed and handles backspace.
 * Returns buf on success, or buf with partial content on EOF.
 */
char *gets(char *buf, int size);

/*
 * String functions (pure user-space, no syscalls)
 */

/* Return length of string */
size_t strlen(const char *s);

/* Compare two strings */
int strcmp(const char *s1, const char *s2);

/* Copy string */
char *strcpy(char *dest, const char *src);

/* Copy n characters */
char *strncpy(char *dest, const char *src, size_t n);

/* Compare up to n characters */
int strncmp(const char *s1, const char *s2, size_t n);

/* Find first occurrence of character in string */
char *strchr(const char *s, int c);

/*
 * Memory functions (pure user-space, no syscalls)
 */

/* Fill memory with a byte value */
void *memset(void *s, int c, size_t n);

/* Copy memory */
void *memcpy(void *dest, const void *src, size_t n);

/* Compare memory */
int memcmp(const void *s1, const void *s2, size_t n);

#endif /* USER_LIBC_H */
