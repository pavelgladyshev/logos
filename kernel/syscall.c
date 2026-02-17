/*
 * System call implementation
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Handles system calls from user programs via ecall instruction.
 */

#include "syscall.h"
#include "fs.h"
#include "file.h"
#include "dir.h"
#include "inode.h"
#include "device.h"
#include "console_dev.h"
#include "loader.h"
#include "string.h"

/* Console minor device number */
#define CONSOLE_MINOR  0

/* File descriptor table */
static struct fd_entry fd_table[MAX_FD];

/* Exit state */
static int program_exited = 0;
static int exit_code = 0;

/*
 * Persistent kernel environment.
 * Survives across program restarts (not cleared by syscall_init).
 * Initialized once at boot by syscall_env_init().
 */
static char kern_env[MAX_ENVC][MAX_ENV_LEN];
static int kern_env_count = 0;

/*
 * Initialize the syscall subsystem.
 * Pre-opens stdin (fd 0), stdout (fd 1), and stderr (fd 2) to console device.
 */
void syscall_init(void) {
    int i;

    /* Clear all file descriptors */
    for (i = 0; i < MAX_FD; i++) {
        fd_table[i].in_use = 0;
    }

    /* fd 0 = stdin -> console device */
    fd_table[0].in_use = 1;
    fd_table[0].inode = 0;  /* No inode, direct device access */
    fd_table[0].offset = 0;
    fd_table[0].type = FT_CHARDEV;
    fd_table[0].major = CONSOLE_MAJOR;
    fd_table[0].minor = CONSOLE_MINOR;

    /* fd 1 = stdout -> console device */
    fd_table[1].in_use = 1;
    fd_table[1].inode = 0;  /* No inode, direct device access */
    fd_table[1].offset = 0;
    fd_table[1].type = FT_CHARDEV;
    fd_table[1].major = CONSOLE_MAJOR;
    fd_table[1].minor = CONSOLE_MINOR;

    /* fd 2 = stderr -> console device */
    fd_table[2].in_use = 1;
    fd_table[2].inode = 0;
    fd_table[2].offset = 0;
    fd_table[2].type = FT_CHARDEV;
    fd_table[2].major = CONSOLE_MAJOR;
    fd_table[2].minor = CONSOLE_MINOR;

    /* Reset exit state */
    program_exited = 0;
    exit_code = 0;
}

/*
 * Allocate a new file descriptor.
 * Returns fd number (>= 0) on success, -1 if no fd available.
 */
static int fd_alloc(void) {
    int i;
    for (i = 3; i < MAX_FD; i++) {  /* Start at 3 (skip stdin/stdout/stderr) */
        if (!fd_table[i].in_use) {
            fd_table[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

/*
 * sys_exit - Terminate the program
 * a0 = exit status
 */
static void sys_exit(trap_frame_t *tf) {
    exit_code = (int)tf->a0;
    program_exited = 1;
}

/*
 * sys_read - Read from a file descriptor
 * a0 = fd, a1 = buf, a2 = len
 * Returns: number of bytes read, or negative error
 */
static int32_t sys_read(trap_frame_t *tf) {
    int fd = (int)tf->a0;
    void *buf = (void *)tf->a1;
    uint32_t len = tf->a2;
    struct fd_entry *fde;
    int result;

    /* Validate fd */
    if (fd < 0 || fd >= MAX_FD || !fd_table[fd].in_use) {
        return -1;  /* EBADF */
    }

    fde = &fd_table[fd];

    if (fde->type == FT_CHARDEV) {
        /* Read from character device */
        result = device_read(fde->major, fde->minor, buf, len);
        return result;
    } else if (fde->type == FT_FILE) {
        /* Read from regular file */
        result = file_read(fde->inode, fde->offset, buf, len);
        if (result > 0) {
            fde->offset += result;
        }
        return result;
    }

    return -1;  /* Unsupported file type */
}

/*
 * sys_write - Write to a file descriptor
 * a0 = fd, a1 = buf, a2 = len
 * Returns: number of bytes written, or negative error
 */
static int32_t sys_write(trap_frame_t *tf) {
    int fd = (int)tf->a0;
    const void *buf = (const void *)tf->a1;
    uint32_t len = tf->a2;
    struct fd_entry *fde;
    int result;

    /* Validate fd */
    if (fd < 0 || fd >= MAX_FD || !fd_table[fd].in_use) {
        return -1;  /* EBADF */
    }

    fde = &fd_table[fd];

    if (fde->type == FT_CHARDEV) {
        /* Write to character device */
        result = device_write(fde->major, fde->minor, buf, len);
        return result;
    } else if (fde->type == FT_FILE) {
        /* Write to regular file */
        result = file_write(fde->inode, fde->offset, buf, len);
        if (result > 0) {
            fde->offset += result;
        }
        return result;
    }

    return -1;  /* Unsupported file type */
}

/*
 * Find a variable in the kernel environment by name.
 * Returns index, or -1 if not found.
 */
static int kern_env_find(const char *name) {
    int nlen = strlen(name);
    int i;
    for (i = 0; i < kern_env_count; i++) {
        if (strncmp(kern_env[i], name, nlen) == 0 && kern_env[i][nlen] == '=') {
            return i;
        }
    }
    return -1;
}

/*
 * canonicalize_path - Remove . and .. components from an absolute path in-place.
 * Input must start with '/'. Modifies the buffer directly.
 * Examples: "/bin/.." -> "/", "/bin/./ls" -> "/bin/ls", "/a/b/../c" -> "/a/c"
 */
static void canonicalize_path(char *path) {
    static char tmp[MAX_ARG_LEN];
    int depth = 0;
    char *src = path + 1;  /* Skip leading '/' */
    char *dst = tmp;

    /* Build canonical path in tmp buffer */
    while (*src != '\0') {
        /* Extract next component */
        char *start = src;
        while (*src != '\0' && *src != '/') src++;
        int len = src - start;

        /* Skip slashes */
        while (*src == '/') src++;

        if (len == 1 && start[0] == '.') {
            /* "." — skip */
        } else if (len == 2 && start[0] == '.' && start[1] == '.') {
            /* ".." — pop one level by scanning back to previous '/' */
            if (depth > 0) {
                depth--;
                /* Back up over the last component */
                if (dst > tmp) dst--;  /* back over '/' */
                while (dst > tmp && *(dst - 1) != '/') dst--;
            }
        } else {
            /* Normal component */
            if (dst > tmp) *dst++ = '/';
            memcpy(dst, start, len);
            dst += len;
            depth++;
        }
    }

    /* Copy result back: always starts with '/' */
    path[0] = '/';
    memcpy(path + 1, tmp, dst - tmp);
    path[1 + (dst - tmp)] = '\0';
}

/*
 * resolve_path - Convert a possibly-relative path to an absolute path.
 * If 'path' starts with '/', it is copied as-is.
 * Otherwise, CWD is prepended: "CWD/path" (or "/path" if CWD is "/").
 * The result is canonicalized (. and .. resolved).
 * Returns 0 on success, -1 if the result would exceed abs_size.
 */
static int resolve_path(const char *path, char *abs_path, int abs_size) {
    if (path == (const char *)0) return -1;

    if (path[0] == '/') {
        /* Already absolute — copy directly */
        int len = strlen(path);
        if (len >= abs_size) return -1;
        memcpy(abs_path, path, len + 1);
    } else {
        /* Relative path — prepend CWD */
        int cwd_idx = kern_env_find("CWD");
        const char *cwd = "/";
        if (cwd_idx >= 0) {
            char *eq = strchr(kern_env[cwd_idx], '=');
            if (eq != (char *)0) cwd = eq + 1;
        }

        int cwd_len = strlen(cwd);
        int path_len = strlen(path);

        /* Check if CWD ends with '/' (root case) */
        int need_slash = (cwd_len > 0 && cwd[cwd_len - 1] != '/') ? 1 : 0;
        int total = cwd_len + need_slash + path_len;
        if (total >= abs_size) return -1;

        memcpy(abs_path, cwd, cwd_len);
        if (need_slash) {
            abs_path[cwd_len] = '/';
        }
        memcpy(abs_path + cwd_len + need_slash, path, path_len + 1);
    }

    canonicalize_path(abs_path);
    return 0;
}

/*
 * sys_open - Open a file
 * a0 = path, a1 = flags (ignored for now, read-only)
 * Returns: file descriptor, or negative error
 */
static int32_t sys_open(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char resolved[MAX_ARG_LEN];
    uint32_t ino;
    uint8_t type;
    int fd;
    int result;
    struct fd_entry *fde;

    /* Resolve relative path to absolute */
    if (resolve_path(path, resolved, MAX_ARG_LEN) < 0) {
        return -1;
    }

    /* Look up the path */
    result = fs_open(resolved, &ino);
    if (result != FS_OK) {
        return -1;  /* ENOENT or other error */
    }

    /* Get inode type */
    result = inode_get_type(ino, &type);
    if (result != FS_OK) {
        return -1;
    }

    /* Allocate a file descriptor */
    fd = fd_alloc();
    if (fd < 0) {
        return -1;  /* EMFILE - too many open files */
    }

    fde = &fd_table[fd];
    fde->inode = ino;
    fde->offset = 0;
    fde->type = type;

    /* For character devices, get major/minor from inode */
    if (type == FT_CHARDEV) {
        uint8_t major, minor;
        inode_get_device(ino, &major, &minor);
        fde->major = major;
        fde->minor = minor;
    }

    return fd;
}

/*
 * sys_close - Close a file descriptor
 * a0 = fd
 * Returns: 0 on success, negative error
 */
static int32_t sys_close(trap_frame_t *tf) {
    int fd = (int)tf->a0;

    /* Validate fd (can't close stdin/stdout/stderr) */
    if (fd < 3 || fd >= MAX_FD || !fd_table[fd].in_use) {
        return -1;  /* EBADF */
    }

    fd_table[fd].in_use = 0;
    return 0;
}

/*
 * Temporary buffers for exec arguments.
 * Must copy BEFORE loading new program, since load overwrites
 * the old program's memory where the strings live.
 */
static char exec_path_buf[MAX_ARG_LEN];
static char exec_argv_buf[MAX_ARGC][MAX_ARG_LEN];
static int exec_argc;

/*
 * sys_exec - Replace current program with a new one
 * a0 = path to executable
 * a1 = argv (NULL-terminated array of string pointers)
 * a2 = envp (NULL-terminated array of "KEY=value" strings, or NULL)
 *
 * On success: does not return, starts new program
 * On failure: returns negative error code
 */
static int32_t sys_exec(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char **argv = (char **)tf->a1;
    char **envp = (char **)tf->a2;
    struct program_info info;
    int result;
    uint32_t sp;
    uint32_t *argv_ptrs;
    char *argv_strings;
    int i, len;

    /* Copy path to kernel buffer BEFORE loading (old memory will be overwritten) */
    len = strlen(path);
    if (len >= MAX_ARG_LEN) {
        len = MAX_ARG_LEN - 1;
    }
    memcpy(exec_path_buf, path, len);
    exec_path_buf[len] = '\0';

    /* Resolve relative path to absolute */
    {
        char resolved[MAX_ARG_LEN];
        if (resolve_path(exec_path_buf, resolved, MAX_ARG_LEN) < 0) {
            return -FS_ERR_INVALID;
        }
        memcpy(exec_path_buf, resolved, strlen(resolved) + 1);
    }

    /* Copy arguments to kernel buffer BEFORE loading */
    exec_argc = 0;
    if (argv != (char **)0) {
        while (argv[exec_argc] != (char *)0 && exec_argc < MAX_ARGC) {
            len = strlen(argv[exec_argc]);
            if (len >= MAX_ARG_LEN) {
                len = MAX_ARG_LEN - 1;
            }
            memcpy(exec_argv_buf[exec_argc], argv[exec_argc], len);
            exec_argv_buf[exec_argc][len] = '\0';
            exec_argc++;
        }
    }

    /* Update persistent kernel environment from envp if provided */
    if (envp != (char **)0) {
        int envc = 0;
        while (envp[envc] != (char *)0 && envc < MAX_ENVC) {
            len = strlen(envp[envc]);
            if (len >= MAX_ENV_LEN) {
                len = MAX_ENV_LEN - 1;
            }
            memcpy(kern_env[envc], envp[envc], len);
            kern_env[envc][len] = '\0';
            envc++;
        }
        kern_env_count = envc;
    }

    /* Now load the new program - this overwrites old program's memory */
    result = elf_load(exec_path_buf, &info);
    if (result != LOAD_OK) {
        return result;  /* Return error to caller */
    }

    /* Set up the new stack with arguments from kernel buffers.
     *
     * Stack layout (high to low):
     *   argv string data   (exec_argc * MAX_ARG_LEN bytes)
     *   [16-byte align]
     *   argv[0..argc-1], NULL   (pointer array)
     *   [16-byte align]
     *   <- sp
     */
    sp = PROGRAM_STACK_TOP;

    /* Reserve space for argv strings at the top */
    argv_strings = (char *)(sp - (exec_argc * MAX_ARG_LEN));
    sp = (uint32_t)argv_strings;

    /* Copy argv strings from kernel buffer */
    for (i = 0; i < exec_argc; i++) {
        len = strlen(exec_argv_buf[i]);
        memcpy(argv_strings + (i * MAX_ARG_LEN), exec_argv_buf[i], len + 1);
    }

    /* Align sp to 16 bytes */
    sp = sp & ~0xF;

    /* Reserve space for argv pointer array (including NULL terminator) */
    sp -= (exec_argc + 1) * sizeof(uint32_t);
    argv_ptrs = (uint32_t *)sp;

    /* Fill in argv pointers */
    for (i = 0; i < exec_argc; i++) {
        argv_ptrs[i] = (uint32_t)(argv_strings + (i * MAX_ARG_LEN));
    }
    argv_ptrs[exec_argc] = 0;  /* NULL terminator */

    /* Align sp to 16 bytes again */
    sp = sp & ~0xF;

    /* Reset file descriptors (keep stdin/stdout/stderr) */
    for (i = 3; i < MAX_FD; i++) {
        fd_table[i].in_use = 0;
    }

    /* Set up trap frame for new program */
    tf->mepc = info.entry_point;
    tf->sp = sp;
    tf->ra = 0;  /* No return address */
    tf->a0 = (uint32_t)exec_argc;
    tf->a1 = (uint32_t)argv_ptrs;

    /* Return 2 to indicate exec succeeded - don't advance mepc */
    return 2;
}

/*
 * resolve_parent - Split a path into parent directory inode and final name.
 * E.g. "/foo/bar" -> parent_ino = inode of "/foo", *name = "bar"
 *      "/bar"     -> parent_ino = root inode,      *name = "bar"
 * Returns FS_OK on success, negative error on failure.
 */
static int resolve_parent(const char *path, uint32_t *parent_ino, const char **name) {
    const char *last_slash;
    const char *p;

    if (path == (const char *)0 || path[0] != '/') {
        return FS_ERR_INVALID;
    }

    /* Find the last '/' */
    last_slash = path;
    for (p = path; *p != '\0'; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }

    *name = last_slash + 1;

    /* Name must not be empty */
    if (**name == '\0') {
        return FS_ERR_INVALID;
    }

    if (last_slash == path) {
        /* Path is "/something" - parent is root */
        *parent_ino = fs_root_inode();
        return FS_OK;
    }

    /* Temporarily null-terminate the parent portion to resolve it.
     * We need a buffer since path may be in user read-only memory. */
    uint32_t parent_len = last_slash - path;
    if (parent_len >= MAX_ARG_LEN) {
        return FS_ERR_INVALID;
    }

    char parent_path[MAX_ARG_LEN];
    memcpy(parent_path, path, parent_len);
    parent_path[parent_len] = '\0';

    return fs_open(parent_path, parent_ino);
}

/*
 * sys_readdir - List directory entries
 * a0 = path, a1 = dirent buffer, a2 = max_entries
 * Returns: number of entries read, or negative error
 */
static int32_t sys_readdir(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char resolved[MAX_ARG_LEN];
    struct dirent *entries = (struct dirent *)tf->a1;
    uint32_t max_entries = tf->a2;
    uint32_t ino;
    uint32_t count;

    /* Resolve relative path to absolute */
    if (resolve_path(path, resolved, MAX_ARG_LEN) < 0) {
        return -FS_ERR_INVALID;
    }

    /* Resolve path to inode */
    int result = fs_open(resolved, &ino);
    if (result != FS_OK) {
        return result;
    }

    /* List directory entries */
    result = dir_list(ino, entries, max_entries, &count);
    if (result != FS_OK) {
        return result;
    }

    return (int32_t)count;
}

/*
 * sys_mkdir - Create a directory
 * a0 = path
 * Returns: 0 on success, negative error
 */
static int32_t sys_mkdir(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char resolved[MAX_ARG_LEN];
    uint32_t parent_ino;
    const char *name;

    /* Resolve relative path to absolute */
    if (resolve_path(path, resolved, MAX_ARG_LEN) < 0) {
        return -FS_ERR_INVALID;
    }

    int result = resolve_parent(resolved, &parent_ino, &name);
    if (result != FS_OK) {
        return result;
    }

    return fs_mkdir(parent_ino, name);
}

/*
 * sys_rmdir - Remove a directory
 * a0 = path
 * Returns: 0 on success, negative error
 */
static int32_t sys_rmdir(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char resolved[MAX_ARG_LEN];
    uint32_t parent_ino;
    const char *name;

    /* Resolve relative path to absolute */
    if (resolve_path(path, resolved, MAX_ARG_LEN) < 0) {
        return -FS_ERR_INVALID;
    }

    int result = resolve_parent(resolved, &parent_ino, &name);
    if (result != FS_OK) {
        return result;
    }

    return fs_rmdir(parent_ino, name);
}

/*
 * sys_mknod - Create a device node
 * a0 = path, a1 = major, a2 = minor
 * Returns: 0 on success, negative error
 */
static int32_t sys_mknod(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char resolved[MAX_ARG_LEN];
    uint8_t major = (uint8_t)tf->a1;
    uint8_t minor = (uint8_t)tf->a2;
    uint32_t parent_ino;
    const char *name;

    /* Resolve relative path to absolute */
    if (resolve_path(path, resolved, MAX_ARG_LEN) < 0) {
        return -FS_ERR_INVALID;
    }

    int result = resolve_parent(resolved, &parent_ino, &name);
    if (result != FS_OK) {
        return result;
    }

    return fs_mknod(parent_ino, name, major, minor);
}

/*
 * sys_chdir - Change the current working directory
 * a0 = path (absolute or relative)
 * Validates path is an existing directory, then updates CWD env var.
 * Returns: 0 on success, negative error
 */
static int32_t sys_chdir(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char resolved[MAX_ARG_LEN];
    uint32_t ino;
    uint8_t type;
    int result;

    /* Resolve to absolute path */
    if (resolve_path(path, resolved, MAX_ARG_LEN) < 0) {
        return -FS_ERR_INVALID;
    }

    /* Check path exists */
    result = fs_open(resolved, &ino);
    if (result != FS_OK) {
        return result;
    }

    /* Check it's a directory */
    result = inode_get_type(ino, &type);
    if (result != FS_OK) {
        return result;
    }
    if (type != FT_DIR) {
        return -FS_ERR_NOT_DIR;
    }

    /* Update CWD in kernel environment */
    int idx = kern_env_find("CWD");
    int rlen = strlen(resolved);
    if (idx >= 0) {
        /* Update existing CWD entry */
        char *slot = kern_env[idx];
        memcpy(slot, "CWD=", 4);
        memcpy(slot + 4, resolved, rlen);
        slot[4 + rlen] = '\0';
    } else {
        /* Add new CWD entry */
        if (kern_env_count >= MAX_ENVC) return -1;
        idx = kern_env_count++;
        char *slot = kern_env[idx];
        memcpy(slot, "CWD=", 4);
        memcpy(slot + 4, resolved, rlen);
        slot[4 + rlen] = '\0';
    }

    return 0;
}

/*
 * sys_setenv - Set a kernel environment variable
 * a0 = name, a1 = value
 * Returns: 0 on success, -1 on error
 */
static int32_t sys_setenv(trap_frame_t *tf) {
    const char *name = (const char *)tf->a0;
    const char *value = (const char *)tf->a1;
    int nlen = strlen(name);
    int vlen = strlen(value);
    int idx;
    char *slot;

    if (nlen + 1 + vlen >= MAX_ENV_LEN) {
        vlen = MAX_ENV_LEN - nlen - 2;
        if (vlen < 0) return -1;
    }

    idx = kern_env_find(name);
    if (idx < 0) {
        /* New entry */
        if (kern_env_count >= MAX_ENVC) return -1;
        idx = kern_env_count++;
    }

    /* Build "NAME=value" */
    slot = kern_env[idx];
    memcpy(slot, name, nlen);
    slot[nlen] = '=';
    memcpy(slot + nlen + 1, value, vlen);
    slot[nlen + 1 + vlen] = '\0';
    return 0;
}

/*
 * sys_getenv - Get a kernel environment variable
 * a0 = name, a1 = buf, a2 = buflen
 * Copies the value (not "KEY=value", just the value) into buf.
 * Returns: length of value on success, -1 if not found
 */
static int32_t sys_getenv(trap_frame_t *tf) {
    const char *name = (const char *)tf->a0;
    char *buf = (char *)tf->a1;
    uint32_t buflen = tf->a2;
    int idx;
    char *eq;
    char *value;
    int vlen;

    idx = kern_env_find(name);
    if (idx < 0) return -1;

    eq = strchr(kern_env[idx], '=');
    if (eq == (char *)0) return -1;
    value = eq + 1;
    vlen = strlen(value);

    if (buf != (char *)0 && buflen > 0) {
        int copy = vlen;
        if (copy >= (int)buflen) copy = (int)buflen - 1;
        memcpy(buf, value, copy);
        buf[copy] = '\0';
    }

    return vlen;
}

/*
 * sys_unsetenv - Remove a kernel environment variable
 * a0 = name
 * Returns: 0 on success, -1 if not found
 */
static int32_t sys_unsetenv(trap_frame_t *tf) {
    const char *name = (const char *)tf->a0;
    int idx = kern_env_find(name);
    if (idx < 0) return -1;

    /* Compact: move last entry into this slot */
    kern_env_count--;
    if (idx < kern_env_count) {
        strcpy(kern_env[idx], kern_env[kern_env_count]);
    }
    return 0;
}

/*
 * sys_getenv_count - Return number of kernel environment variables
 * Returns: count >= 0
 */
static int32_t sys_getenv_count(void) {
    return kern_env_count;
}

/*
 * sys_getenv_entry - Get the Nth environment entry as "KEY=value"
 * a0 = index, a1 = buf, a2 = buflen
 * Returns: length of entry on success, -1 if index out of range
 */
static int32_t sys_getenv_entry(trap_frame_t *tf) {
    int index = (int)tf->a0;
    char *buf = (char *)tf->a1;
    uint32_t buflen = tf->a2;
    int len;

    if (index < 0 || index >= kern_env_count) return -1;

    len = strlen(kern_env[index]);
    if (buf != (char *)0 && buflen > 0) {
        int copy = len;
        if (copy >= (int)buflen) copy = (int)buflen - 1;
        memcpy(buf, kern_env[index], copy);
        buf[copy] = '\0';
    }
    return len;
}

/*
 * Initialize kernel environment with defaults. Called once at boot.
 */
void syscall_env_init(void) {
    kern_env_count = 0;
    /* Set default PATH, CWD, and exit status */
    strcpy(kern_env[0], "PATH=/bin");
    strcpy(kern_env[1], "CWD=/");
    strcpy(kern_env[2], "?=0");
    kern_env_count = 3;
}

/*
 * Dispatch a system call.
 * Returns 1 if program should exit, 0 otherwise.
 */
int syscall_dispatch(trap_frame_t *tf) {
    uint32_t syscall_num = tf->a7;
    int32_t result = 0;

    switch (syscall_num) {
        case SYS_exit:
            sys_exit(tf);
            return 1;  /* Signal to exit */

        case SYS_read:
            result = sys_read(tf);
            break;

        case SYS_write:
            result = sys_write(tf);
            break;

        case SYS_open:
            result = sys_open(tf);
            break;

        case SYS_close:
            result = sys_close(tf);
            break;

        case SYS_exec:
            result = sys_exec(tf);
            if (result == 2) {
                /* exec succeeded - don't modify a0 or mepc,
                 * trap_ret will start the new program */
                return 0;
            }
            /* exec failed - return error to caller */
            break;

        case SYS_readdir:
            result = sys_readdir(tf);
            break;

        case SYS_mkdir:
            result = sys_mkdir(tf);
            break;

        case SYS_rmdir:
            result = sys_rmdir(tf);
            break;

        case SYS_mknod:
            result = sys_mknod(tf);
            break;

        case SYS_setenv:
            result = sys_setenv(tf);
            break;

        case SYS_getenv:
            result = sys_getenv(tf);
            break;

        case SYS_unsetenv:
            result = sys_unsetenv(tf);
            break;

        case SYS_getenv_count:
            result = sys_getenv_count();
            break;

        case SYS_getenv_entry:
            result = sys_getenv_entry(tf);
            break;

        case SYS_chdir:
            result = sys_chdir(tf);
            break;

        default:
            /* Unknown syscall */
            result = -1;
            break;
    }

    /* Store return value in a0 */
    tf->a0 = (uint32_t)result;

    /* Advance mepc past the ecall instruction (4 bytes) */
    tf->mepc += 4;

    return 0;  /* Continue execution */
}

/*
 * Get the exit code from sys_exit.
 */
int syscall_get_exit_code(void) {
    return exit_code;
}

/*
 * Set a kernel environment variable from within the kernel.
 * Used by main.c to store exit code in "?" before reloading shell.
 */
void syscall_set_env(const char *name, const char *value) {
    int nlen = strlen(name);
    int vlen = strlen(value);
    int idx;
    char *slot;

    if (nlen + 1 + vlen >= MAX_ENV_LEN) return;

    idx = kern_env_find(name);
    if (idx < 0) {
        if (kern_env_count >= MAX_ENVC) return;
        idx = kern_env_count++;
    }

    slot = kern_env[idx];
    memcpy(slot, name, nlen);
    slot[nlen] = '=';
    memcpy(slot + nlen + 1, value, vlen);
    slot[nlen + 1 + vlen] = '\0';
}
