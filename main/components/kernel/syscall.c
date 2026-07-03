/*
 * System call implementation
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Handles system calls from user programs via ecall instruction.
 * Uses per-process file descriptors from the process table.
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
#include "process.h"

/* Console minor device number */
#define CONSOLE_MINOR  0

/*
 * Get a file descriptor entry for the current process.
 * Returns pointer to fd_entry, or NULL if fd is invalid.
 */
static struct fd_entry *get_fd(int fd) {
    if (fd < 0 || fd >= MAX_FD)
        return (struct fd_entry *)0;
    if (!proc_table[current_proc].fds[fd].in_use)
        return (struct fd_entry *)0;
    return &proc_table[current_proc].fds[fd];
}

/*
 * Allocate a new file descriptor for the current process.
 * Returns fd number (>= 0) on success, -1 if no fd available.
 */
static int fd_alloc(void) {
    int i;
    struct fd_entry *fds = proc_table[current_proc].fds;
    for (i = 3; i < MAX_FD; i++) {  /* Start at 3 (skip stdin/stdout/stderr) */
        if (!fds[i].in_use) {
            fds[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

/*
 * sys_exit - Terminate the current process
 * a0 = exit status
 *
 * If the process has a parent (was launched by spawn):
 *   - Stores exit code, resumes parent with exit code as spawn return value
 *   - Calls trap_ret() directly (never returns)
 * If no parent (shell, launched by kernel):
 *   - Stores exit code, returns 1 to signal kernel to handle it
 */
static int sys_exit(trap_frame_t *tf) {
    struct process *cur = &proc_table[current_proc];
    int exit_status = (int)tf->a0;

    cur->exit_code = exit_status;

    if (cur->parent >= 0) {
        /* Child process — return to parent */
        int parent_slot = cur->parent;
        struct process *parent = &proc_table[parent_slot];

        /* Set parent's spawn() return value to child's exit code */
        parent->tf.a0 = (uint32_t)exit_status;
        /* Advance parent's mepc past the ecall instruction */
        parent->tf.mepc += 4;

        /* Update process states */
        parent->state = PROC_RUNNING;
        proc_free(current_proc);
        current_proc = parent_slot;

        /* Store exit code in parent's "?" env var */
        proc_set_env_int(parent_slot, "?", exit_status);

        /* Resume parent — never returns */
        trap_ret(&parent->tf);
    }

    /* Top-level process (shell) — signal kernel */
    return 1;
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

    fde = get_fd(fd);
    if (fde == (struct fd_entry *)0) {
        return -1;  /* EBADF */
    }

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

    fde = get_fd(fd);
    if (fde == (struct fd_entry *)0) {
        return -1;  /* EBADF */
    }

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
        /* Relative path — prepend CWD from current process's environment */
        int cwd_idx = proc_env_find(current_proc, "CWD");
        const char *cwd = "/";
        if (cwd_idx >= 0) {
            char *eq = strchr(proc_table[current_proc].env[cwd_idx], '=');
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

    fde = &proc_table[current_proc].fds[fd];
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
    if (fd < 3 || fd >= MAX_FD || !proc_table[current_proc].fds[fd].in_use) {
        return -1;  /* EBADF */
    }

    proc_table[current_proc].fds[fd].in_use = 0;
    return 0;
}

/*
 * Temporary buffers for spawn arguments.
 * Must copy BEFORE loading new program, since the caller's
 * memory contains the string pointers/data.
 */
static char spawn_path_buf[MAX_ARG_LEN];
static char spawn_argv_buf[MAX_ARGC][MAX_ARG_LEN];
static int spawn_argc;

/*
 * sys_spawn - Launch a child program in a new process slot
 * a0 = path to executable
 * a1 = argv (NULL-terminated array of string pointers)
 * a2 = envp (NULL-terminated array of "KEY=value" strings, or NULL)
 *
 * On success: switches to child process via trap_ret (never returns).
 *             When child exits, parent resumes with child's exit code in a0.
 * On failure: returns negative error code to caller.
 */
static int32_t sys_spawn(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char **argv = (char **)tf->a1;
    char **envp = (char **)tf->a2;
    struct program_info info;
    int result;
    uint32_t sp;
    uint32_t *argv_ptrs;
    char *argv_strings;
    int child_slot;
    struct process *child;
    int i, len;

    /* Copy path to kernel buffer (caller's memory stays intact, but
     * we need a kernel copy for resolve_path to work with) */
    len = strlen(path);
    if (len >= MAX_ARG_LEN) {
        len = MAX_ARG_LEN - 1;
    }
    memcpy(spawn_path_buf, path, len);
    spawn_path_buf[len] = '\0';

    /* Resolve relative path to absolute */
    {
        char resolved[MAX_ARG_LEN];
        if (resolve_path(spawn_path_buf, resolved, MAX_ARG_LEN) < 0) {
            return FS_ERR_INVALID;
        }
        memcpy(spawn_path_buf, resolved, strlen(resolved) + 1);
    }

    /* Copy arguments to kernel buffer */
    spawn_argc = 0;
    if (argv != (char **)0) {
        while (argv[spawn_argc] != (char *)0 && spawn_argc < MAX_ARGC) {
            len = strlen(argv[spawn_argc]);
            if (len >= MAX_ARG_LEN) {
                len = MAX_ARG_LEN - 1;
            }
            memcpy(spawn_argv_buf[spawn_argc], argv[spawn_argc], len);
            spawn_argv_buf[spawn_argc][len] = '\0';
            spawn_argc++;
        }
    }

    /* Allocate a new process slot for the child */
    child_slot = proc_alloc();
    if (child_slot < 0) {
        return -1;  /* No free process slot */
    }
    child = &proc_table[child_slot];

    /* Inherit parent's environment, then override if envp provided */
    proc_env_copy(child_slot, current_proc);
    if (envp != (char **)0) {
        int envc = 0;
        child->env_count = 0;
        while (envp[envc] != (char *)0 && envc < MAX_ENVC) {
            len = strlen(envp[envc]);
            if (len >= MAX_ENV_LEN) {
                len = MAX_ENV_LEN - 1;
            }
            memcpy(child->env[envc], envp[envc], len);
            child->env[envc][len] = '\0';
            envc++;
        }
        child->env_count = envc;
    }

    /* Load the program into the child's memory slot */
    result = elf_load_at(spawn_path_buf, child->mem_base, PROC_SLOT_SIZE, &info);
    if (result != LOAD_OK) {
        proc_free(child_slot);
        return result;  /* Return error to caller */
    }

    /* Set up the child's stack with arguments.
     *
     * Stack layout (high to low):
     *   argv string data   (spawn_argc * MAX_ARG_LEN bytes)
     *   [16-byte align]
     *   argv[0..argc-1], NULL   (pointer array)
     *   [16-byte align]
     *   <- sp
     */
    sp = child->stack_top;

    /* Reserve space for argv strings at the top */
    argv_strings = (char *)(sp - (spawn_argc * MAX_ARG_LEN));
    sp = (uint32_t)argv_strings;

    /* Copy argv strings from kernel buffer */
    for (i = 0; i < spawn_argc; i++) {
        len = strlen(spawn_argv_buf[i]);
        memcpy(argv_strings + (i * MAX_ARG_LEN), spawn_argv_buf[i], len + 1);
    }

    /* Align sp to 16 bytes */
    sp = sp & ~0xF;

    /* Reserve space for argv pointer array (including NULL terminator) */
    sp -= (spawn_argc + 1) * sizeof(uint32_t);
    argv_ptrs = (uint32_t *)sp;

    /* Fill in argv pointers */
    for (i = 0; i < spawn_argc; i++) {
        argv_ptrs[i] = (uint32_t)(argv_strings + (i * MAX_ARG_LEN));
    }
    argv_ptrs[spawn_argc] = 0;  /* NULL terminator */

    /* Align sp to 16 bytes again */
    sp = sp & ~0xF;

    /* Set up child's trap frame */
    child->tf.c_trap_sp = proc_table[current_proc].tf.c_trap_sp;
    child->tf.c_trap = proc_table[current_proc].tf.c_trap;
    child->tf.mepc = info.entry_point;
    child->tf.sp = sp;
    child->tf.ra = 0;  /* No return address */
    child->tf.a0 = (uint32_t)spawn_argc;
    child->tf.a1 = (uint32_t)argv_ptrs;

    /* Initialize child's file descriptors (stdin/stdout/stderr) */
    proc_fd_init(child_slot);

    /* Set up parent-child relationship */
    child->parent = current_proc;
    child->state = PROC_RUNNING;
    proc_table[current_proc].state = PROC_READY;

    /* Switch to child process */
    current_proc = child_slot;

    /* Start child — never returns.
     * When child calls exit(), sys_exit will resume the parent
     * with the child's exit code in a0 (spawn's return value). */
    trap_ret(&child->tf);

    /* Not reached */
    return 0;
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
        return FS_ERR_INVALID;
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
        return FS_ERR_INVALID;
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
        return FS_ERR_INVALID;
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
        return FS_ERR_INVALID;
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
        return FS_ERR_INVALID;
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
        return FS_ERR_NOT_DIR;
    }

    /* Update CWD in current process's environment */
    proc_set_env(current_proc, "CWD", resolved);

    return 0;
}

/*
 * sys_setenv - Set an environment variable in the current process
 * a0 = name, a1 = value
 * Returns: 0 on success, -1 on error
 */
static int32_t sys_setenv(trap_frame_t *tf) {
    const char *name = (const char *)tf->a0;
    const char *value = (const char *)tf->a1;
    struct process *p = &proc_table[current_proc];
    int nlen = strlen(name);
    int vlen = strlen(value);
    int idx;
    char *s;

    if (nlen + 1 + vlen >= MAX_ENV_LEN) {
        vlen = MAX_ENV_LEN - nlen - 2;
        if (vlen < 0) return -1;
    }

    idx = proc_env_find(current_proc, name);
    if (idx < 0) {
        /* New entry */
        if (p->env_count >= MAX_ENVC) return -1;
        idx = p->env_count++;
    }

    /* Build "NAME=value" */
    s = p->env[idx];
    memcpy(s, name, nlen);
    s[nlen] = '=';
    memcpy(s + nlen + 1, value, vlen);
    s[nlen + 1 + vlen] = '\0';
    return 0;
}

/*
 * sys_getenv - Get an environment variable from the current process
 * a0 = name, a1 = buf, a2 = buflen
 * Copies the value (not "KEY=value", just the value) into buf.
 * Returns: length of value on success, -1 if not found
 */
static int32_t sys_getenv(trap_frame_t *tf) {
    const char *name = (const char *)tf->a0;
    char *buf = (char *)tf->a1;
    uint32_t buflen = tf->a2;
    struct process *p = &proc_table[current_proc];
    int idx;
    char *eq;
    char *value;
    int vlen;

    idx = proc_env_find(current_proc, name);
    if (idx < 0) return -1;

    eq = strchr(p->env[idx], '=');
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
 * sys_unsetenv - Remove an environment variable from the current process
 * a0 = name
 * Returns: 0 on success, -1 if not found
 */
static int32_t sys_unsetenv(trap_frame_t *tf) {
    const char *name = (const char *)tf->a0;
    struct process *p = &proc_table[current_proc];
    int idx = proc_env_find(current_proc, name);
    if (idx < 0) return -1;

    /* Compact: move last entry into this slot */
    p->env_count--;
    if (idx < p->env_count) {
        strcpy(p->env[idx], p->env[p->env_count]);
    }
    return 0;
}

/*
 * sys_getenv_count - Return number of environment variables in the current process
 * Returns: count >= 0
 */
static int32_t sys_getenv_count(void) {
    return proc_table[current_proc].env_count;
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
    struct process *p = &proc_table[current_proc];
    int len;

    if (index < 0 || index >= p->env_count) return -1;

    len = strlen(p->env[index]);
    if (buf != (char *)0 && buflen > 0) {
        int copy = len;
        if (copy >= (int)buflen) copy = (int)buflen - 1;
        memcpy(buf, p->env[index], copy);
        buf[copy] = '\0';
    }
    return len;
}

/*
 * Dispatch a system call.
 * Returns 1 if the top-level program should exit (shell with no parent),
 * 0 otherwise.
 *
 * Note: sys_spawn and sys_exit may call trap_ret() directly and never return.
 */
/*
 * sys_unlink - Delete a file
 * a0 = path
 * Returns: 0 on success, negative error
 */
static int32_t sys_unlink(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char resolved[MAX_ARG_LEN];
    uint32_t parent_ino;
    const char *name;

    if (resolve_path(path, resolved, MAX_ARG_LEN) < 0) {
        return FS_ERR_INVALID;
    }

    int result = resolve_parent(resolved, &parent_ino, &name);
    if (result != FS_OK) {
        return result;
    }

    return file_delete(parent_ino, name);
}

/*
 * sys_link - Create a hard link
 * a0 = target path (existing file), a1 = link path (new name)
 * Returns: 0 on success, negative error
 */
static int32_t sys_link(trap_frame_t *tf) {
    const char *target = (const char *)tf->a0;
    const char *linkpath = (const char *)tf->a1;
    char resolved_target[MAX_ARG_LEN];
    char resolved_link[MAX_ARG_LEN];
    uint32_t target_ino;
    uint32_t link_parent_ino;
    const char *link_name;
    struct inode in;

    if (resolve_path(target, resolved_target, MAX_ARG_LEN) < 0) {
        return FS_ERR_INVALID;
    }
    int result = fs_open(resolved_target, &target_ino);
    if (result != FS_OK) {
        return result;
    }

    if (inode_read(target_ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }
    if (in.type != FT_FILE) {
        return FS_ERR_INVALID;
    }

    if (resolve_path(linkpath, resolved_link, MAX_ARG_LEN) < 0) {
        return FS_ERR_INVALID;
    }
    result = resolve_parent(resolved_link, &link_parent_ino, &link_name);
    if (result != FS_OK) {
        return result;
    }

    result = dir_add(link_parent_ino, link_name, target_ino);
    if (result != FS_OK) {
        return result;
    }

    in.link_count++;
    return inode_write(target_ino, &in);
}

/*
 * sys_rename - Rename/move a file or directory
 * a0 = old path, a1 = new path
 * Returns: 0 on success, negative error
 */
static int32_t sys_rename(trap_frame_t *tf) {
    const char *oldpath = (const char *)tf->a0;
    const char *newpath = (const char *)tf->a1;
    char resolved_old[MAX_ARG_LEN];
    char resolved_new[MAX_ARG_LEN];
    uint32_t old_parent_ino, new_parent_ino;
    const char *old_name, *new_name;
    uint32_t file_ino;

    if (resolve_path(oldpath, resolved_old, MAX_ARG_LEN) < 0) {
        return FS_ERR_INVALID;
    }
    int result = resolve_parent(resolved_old, &old_parent_ino, &old_name);
    if (result != FS_OK) {
        return result;
    }

    result = dir_lookup(old_parent_ino, old_name, &file_ino);
    if (result != FS_OK) {
        return result;
    }

    if (resolve_path(newpath, resolved_new, MAX_ARG_LEN) < 0) {
        return FS_ERR_INVALID;
    }
    result = resolve_parent(resolved_new, &new_parent_ino, &new_name);
    if (result != FS_OK) {
        return result;
    }

    result = dir_add(new_parent_ino, new_name, file_ino);
    if (result != FS_OK) {
        return result;
    }

    result = dir_remove(old_parent_ino, old_name);
    if (result != FS_OK) {
        return result;
    }

    /* If directory, update ".." to point to new parent */
    struct inode in;
    if (inode_read(file_ino, &in) == FS_OK && in.type == FT_DIR) {
        dir_remove(file_ino, "..");
        dir_add(file_ino, "..", new_parent_ino);
    }

    return FS_OK;
}

/*
 * sys_stat - Get file metadata
 * a0 = path, a1 = pointer to struct stat_info
 * Returns: 0 on success, negative error
 */
static int32_t sys_stat(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    struct stat_info *si = (struct stat_info *)tf->a1;
    char resolved[MAX_ARG_LEN];
    uint32_t ino;
    struct inode in;

    if (resolve_path(path, resolved, MAX_ARG_LEN) < 0) {
        return FS_ERR_INVALID;
    }

    int result = fs_open(resolved, &ino);
    if (result != FS_OK) {
        return result;
    }

    if (inode_read(ino, &in) != FS_OK) {
        return FS_ERR_IO;
    }

    si->ino = ino;
    si->size = in.size;
    si->type = in.type;
    si->link_count = in.link_count;
    si->major = in.major;
    si->minor = in.minor;
    return FS_OK;
}

int syscall_dispatch(trap_frame_t *tf) {
    uint32_t syscall_num = tf->a7;
    int32_t result = 0;

    switch (syscall_num) {
        case SYS_exit:
            if (sys_exit(tf)) {
                /* Top-level process exit — signal kernel */
                return 1;
            }
            /* sys_exit for a child calls trap_ret() and never reaches here */
            return 0;

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

        case SYS_spawn:
            result = sys_spawn(tf);
            /* If we get here, spawn failed — return error to caller */
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

        case SYS_unlink:
            result = sys_unlink(tf);
            break;

        case SYS_link:
            result = sys_link(tf);
            break;

        case SYS_rename:
            result = sys_rename(tf);
            break;

        case SYS_stat:
            result = sys_stat(tf);
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
