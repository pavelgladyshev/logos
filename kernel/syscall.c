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
#include "pipe.h"
#include "shm.h"
#include "sem.h"

/* Console minor device number */
#define CONSOLE_MINOR  0

/* Open flags (must match user/libc.h) */
#define OPEN_WRONLY  1
#define OPEN_RDWR    2
#define OPEN_CREAT   0x100
#define OPEN_TRUNC   0x200
#define OPEN_APPEND  0x400

/* Forward declaration — defined later in this file */
static int resolve_parent(const char *path, uint32_t *parent_ino, const char **name);

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
 * Handles orphan cleanup, wakes sleeping parent (if applicable),
 * or zombifies if parent is alive but not waiting.
 * If no parent (shell, launched by kernel): returns 1 to signal kernel.
 */
static int sys_exit(trap_frame_t *tf) {
    struct process *cur = &proc_table[current_proc];
    int exit_status = (int)tf->a0;
    int i;

    cur->exit_code = exit_status;

    /* Orphan cleanup: reparent or free children of the exiting process */
    for (i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].parent == current_proc) {
            if (proc_table[i].state == PROC_ZOMBIE) {
                proc_free(i);
            } else {
                proc_table[i].parent = -1;  /* orphan */
            }
        }
    }

    if (cur->parent >= 0) {
        int parent_slot = cur->parent;
        struct process *parent = &proc_table[parent_slot];

        if (parent->state == PROC_SLEEPING) {
            /* Check if parent is waiting for us specifically (spawn) or any child (wait) */
            if ((parent->sleep_reason == SLEEP_CHILD && parent->sleep_chan == current_proc) ||
                parent->sleep_reason == SLEEP_WAIT) {
                /* Wake parent with exit code as return value */
                parent->tf.a0 = (uint32_t)exit_status;
                parent->tf.mepc += 4;
                parent->state = PROC_READY;
                parent->sleep_reason = SLEEP_NONE;
                parent->sleep_chan = -1;

                /* Store exit code in parent's "?" env var */
                proc_set_env_int(parent_slot, "?", exit_status);

                /* Free child and reschedule */
                proc_free(current_proc);
                schedule();  /* never returns */
            }
        }

        /* Parent exists but is not waiting for us — become zombie */
        cur->state = PROC_ZOMBIE;
        schedule();  /* never returns */
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
    } else if (fde->type == FT_PIPE) {
        /* Read from pipe */
        int pipe_idx = (int)fde->inode;
        result = pipe_read(pipe_idx, buf, len);
        if (result == -2) {
            /* Would block: sleep and retry (ecall will re-execute) */
            proc_table[current_proc].state = PROC_SLEEPING;
            proc_table[current_proc].sleep_reason = SLEEP_IO;
            proc_table[current_proc].sleep_chan = pipe_idx;
            schedule();  /* never returns; woken process re-executes ecall */
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
    } else if (fde->type == FT_PIPE) {
        /* Write to pipe */
        int pipe_idx = (int)fde->inode;
        result = pipe_write(pipe_idx, buf, len);
        if (result == -2) {
            /* Would block: sleep and retry (ecall will re-execute) */
            proc_table[current_proc].state = PROC_SLEEPING;
            proc_table[current_proc].sleep_reason = SLEEP_IO;
            proc_table[current_proc].sleep_chan = pipe_idx;
            schedule();  /* never returns; woken process re-executes ecall */
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
 * a0 = path, a1 = flags
 * Flags: O_RDONLY(0), O_WRONLY(1), O_RDWR(2), O_CREAT(0x100),
 *        O_TRUNC(0x200), O_APPEND(0x400)
 * Returns: file descriptor, or negative error
 */
static int32_t sys_open(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    uint32_t flags = tf->a1;
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
        /* File not found — create if O_CREAT is set */
        if (flags & OPEN_CREAT) {
            uint32_t parent_ino;
            const char *name;
            result = resolve_parent(resolved, &parent_ino, &name);
            if (result != FS_OK) {
                return -1;
            }
            result = file_create(parent_ino, name);
            if (result < 0) {
                return -1;
            }
            ino = (uint32_t)result;
        } else {
            return -1;  /* ENOENT */
        }
    }

    /* Get inode type */
    result = inode_get_type(ino, &type);
    if (result != FS_OK) {
        return -1;
    }

    /* Handle O_TRUNC: truncate existing regular file to zero length */
    if ((flags & OPEN_TRUNC) && type == FT_FILE) {
        file_truncate(ino, 0);
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

    /* Handle O_APPEND: set offset to end of file */
    if ((flags & OPEN_APPEND) && type == FT_FILE) {
        uint32_t size;
        if (inode_get_size(ino, &size) == FS_OK) {
            fde->offset = size;
        }
    }

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
    struct fd_entry *fde;

    if (fd < 0 || fd >= MAX_FD) {
        return -1;  /* EBADF */
    }

    fde = &proc_table[current_proc].fds[fd];
    if (!fde->in_use) {
        return -1;  /* EBADF */
    }

    /* If this is a pipe fd, update refcounts */
    if (fde->type == FT_PIPE) {
        pipe_close_fd((int)fde->inode, (int)fde->minor);
    }

    fde->in_use = 0;
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
 * setup_user_stack - Set up a process stack with argc/argv.
 * Copies spawn_argv_buf[0..spawn_argc-1] strings to the stack area,
 * builds pointer array.
 *
 * stack_top: top of the process's stack
 * out_sp: receives the new stack pointer value
 * out_argv: receives the address of the argv pointer array
 */
static void setup_user_stack(uint32_t stack_top, uint32_t *out_sp, uint32_t *out_argv) {
    uint32_t sp = stack_top;
    uint32_t *argv_ptrs;
    char *argv_strings;
    int i, len;

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

    *out_sp = sp;
    *out_argv = (uint32_t)argv_ptrs;
}

/* Set process name from path (uses basename) */
static void proc_set_name(struct process *p, const char *path) {
    const char *name = path;
    const char *s = path;
    while (*s) { if (*s == '/') name = s + 1; s++; }
    int i;
    for (i = 0; i < 31 && name[i]; i++) p->name[i] = name[i];
    p->name[i] = '\0';
}

/*
 * sys_spawn - Launch a child program in a new process slot
 * a0 = path to executable
 * a1 = argv (NULL-terminated array of string pointers)
 * a2 = envp (NULL-terminated array of "KEY=value" strings, or NULL)
 *
 * On success: puts parent to sleep, child becomes READY, schedule() runs.
 *             When child exits, parent resumes with child's exit code in a0.
 * On failure: returns negative error code to caller.
 */
static int32_t sys_spawn(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char **argv = (char **)tf->a1;
    char **envp = (char **)tf->a2;
    struct program_info info;
    int result;
    uint32_t sp, argv_addr;
    int child_slot;
    struct process *child;
    int len;

    /* Copy path to kernel buffer */
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
    proc_set_name(child, spawn_path_buf);

    /* Set up the child's stack with arguments */
    setup_user_stack(child->stack_top, &sp, &argv_addr);

    /* Set up child's trap frame */
    child->tf.c_trap_sp = proc_table[current_proc].tf.c_trap_sp;
    child->tf.c_trap = proc_table[current_proc].tf.c_trap;
    child->tf.mepc = info.entry_point;
    child->tf.sp = sp;
    child->tf.ra = 0;  /* No return address */
    child->tf.a0 = (uint32_t)spawn_argc;
    child->tf.a1 = argv_addr;

    /* Initialize child's file descriptors (stdin/stdout/stderr) */
    proc_fd_init(child_slot);

    /* Set up parent-child relationship and sleep parent */
    child->parent = current_proc;
    child->state = PROC_READY;
    proc_table[current_proc].state = PROC_SLEEPING;
    proc_table[current_proc].sleep_reason = SLEEP_CHILD;
    proc_table[current_proc].sleep_chan = child_slot;

    /* Reschedule — scheduler will pick the child */
    schedule();  /* never returns */

    /* Not reached */
    return 0;
}

/*
 * sys_fork - Create a copy of the current process
 *
 * Copies the full 32KB memory slot, trap frame, env, and fds.
 * Adjusts child's registers (sp, mepc, ra, s0) by the memory delta.
 *
 * Returns: child PID to parent, 0 to child, -1 on error
 */
static int32_t sys_fork(trap_frame_t *tf) {
    struct process *parent = &proc_table[current_proc];
    int child_slot;
    struct process *child;
    int32_t delta;
    uint32_t parent_base, parent_end;

    child_slot = proc_alloc();
    if (child_slot < 0) {
        return -1;  /* No free slot */
    }
    child = &proc_table[child_slot];

    /* Copy the entire 32KB memory slot */
    memcpy((void *)child->mem_base, (void *)parent->mem_base, PROC_SLOT_SIZE);

    /* Copy trap frame */
    child->tf = parent->tf;

    /* Compute address delta for register adjustment */
    delta = (int32_t)(child->mem_base - parent->mem_base);
    parent_base = parent->mem_base;
    parent_end = parent_base + PROC_SLOT_SIZE;

    /* Always adjust sp and mepc — they point into the process's slot */
    child->tf.sp += delta;
    child->tf.mepc += delta;

    /* Adjust ra and s0 if they point within parent's slot */
    if (child->tf.ra >= parent_base && child->tf.ra < parent_end) {
        child->tf.ra += delta;
    }
    if (child->tf.s0 >= parent_base && child->tf.s0 < parent_end) {
        child->tf.s0 += delta;
    }

    /* Advance child's mepc past the ecall instruction */
    child->tf.mepc += 4;

    /* Child's fork() returns 0 */
    child->tf.a0 = 0;

    /* Copy environment and file descriptors */
    proc_env_copy(child_slot, current_proc);
    memcpy(child->fds, parent->fds, sizeof(parent->fds));

    /* Increment pipe refcounts for all pipe fds inherited by child */
    {
        int fd_i;
        for (fd_i = 0; fd_i < MAX_FD; fd_i++) {
            if (child->fds[fd_i].in_use && child->fds[fd_i].type == FT_PIPE) {
                pipe_dup_fd((int)child->fds[fd_i].inode,
                            (int)child->fds[fd_i].minor);
            }
        }
    }

    /* Increment SHM refcounts for all segments attached by parent */
    child->shm_attached = parent->shm_attached;
    {
        int shm_i;
        uint8_t mask = child->shm_attached;
        for (shm_i = 0; shm_i < MAX_SHM; shm_i++) {
            if (mask & (1 << shm_i)) {
                shm_dup(shm_i);
            }
        }
    }

    /* Increment semaphore refcounts for all semaphores held by parent */
    child->sem_attached = parent->sem_attached;
    {
        int sem_i;
        uint8_t mask = child->sem_attached;
        for (sem_i = 0; sem_i < MAX_SEMS; sem_i++) {
            if (mask & (1 << sem_i)) {
                sem_dup(sem_i);
            }
        }
    }

    /* Set up parent-child relationship */
    child->parent = current_proc;
    child->state = PROC_READY;

    /* Parent's fork() returns child's PID */
    return child->pid;
}

/*
 * sys_exec - Replace current process image with a new program
 * a0 = path to executable
 * a1 = argv (NULL-terminated array of string pointers)
 * a2 = envp (NULL-terminated array of "KEY=value" strings, or NULL)
 *
 * On success: never returns (jumps to new program)
 * On failure: returns -1
 */
static int32_t sys_exec(trap_frame_t *tf) {
    const char *path = (const char *)tf->a0;
    char **argv = (char **)tf->a1;
    char **envp = (char **)tf->a2;
    struct process *cur = &proc_table[current_proc];
    struct program_info info;
    int result;
    uint32_t sp, argv_addr;
    int len;

    /* Copy path to kernel buffer BEFORE overwriting process memory */
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
            return -1;
        }
        memcpy(spawn_path_buf, resolved, strlen(resolved) + 1);
    }

    /* Copy arguments to kernel buffer BEFORE overwriting */
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

    /* Copy envp to process env if provided */
    if (envp != (char **)0) {
        int envc = 0;
        cur->env_count = 0;
        while (envp[envc] != (char *)0 && envc < MAX_ENVC) {
            len = strlen(envp[envc]);
            if (len >= MAX_ENV_LEN) {
                len = MAX_ENV_LEN - 1;
            }
            memcpy(cur->env[envc], envp[envc], len);
            cur->env[envc][len] = '\0';
            envc++;
        }
        cur->env_count = envc;
    }

    /* Load the new program — overwrites current process memory */
    result = elf_load_at(spawn_path_buf, cur->mem_base, PROC_SLOT_SIZE, &info);
    if (result != LOAD_OK) {
        return -1;  /* Load failed — original memory is corrupted, but return to caller */
    }
    proc_set_name(cur, spawn_path_buf);

    /* Set up the new stack with arguments */
    setup_user_stack(cur->stack_top, &sp, &argv_addr);

    /* Reset trap frame for new program (keep c_trap_sp, c_trap) */
    cur->tf.mepc = info.entry_point;
    cur->tf.sp = sp;
    cur->tf.ra = 0;
    cur->tf.a0 = (uint32_t)spawn_argc;
    cur->tf.a1 = argv_addr;

    /* File descriptors are preserved across exec (Unix semantics).
     * This allows pipe redirections set up via dup2() before exec
     * to survive into the new program. */

    /* Jump to new program — never returns */
    trap_ret(&cur->tf);

    /* Not reached */
    return 0;
}

/*
 * sys_wait - Wait for any child process to exit
 *
 * Returns: child's exit code if a zombie child exists,
 *          puts parent to sleep if living children exist,
 *          -1 if no children at all
 */
static int32_t sys_wait(trap_frame_t *tf) {
    int i;
    int has_children = 0;
    (void)tf;

    /* Single pass: collect zombie or check for living children */
    for (i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].parent != current_proc)
            continue;
        if (proc_table[i].state == PROC_ZOMBIE) {
            int exit_code = proc_table[i].exit_code;
            proc_free(i);
            return exit_code;
        }
        if (proc_table[i].state != PROC_FREE) {
            has_children = 1;
        }
    }

    if (!has_children) {
        return -1;  /* No children at all */
    }

    /* Living children but no zombies — sleep until one exits */
    proc_table[current_proc].state = PROC_SLEEPING;
    proc_table[current_proc].sleep_reason = SLEEP_WAIT;
    proc_table[current_proc].sleep_chan = -1;
    schedule();  /* never returns; sys_exit will wake us */

    /* Not reached */
    return 0;
}

/*
 * sys_getpid - Return the PID of the current process
 */
static int32_t sys_getpid(void) {
    return proc_table[current_proc].pid;
}

/*
 * sys_dup - Duplicate a file descriptor
 * a0 = oldfd
 * Returns: new fd (lowest available), or -1 on error
 */
static int32_t sys_dup(trap_frame_t *tf) {
    int oldfd = (int)tf->a0;
    struct fd_entry *fds = proc_table[current_proc].fds;
    int i;

    if (get_fd(oldfd) == (struct fd_entry *)0) {
        return -1;  /* EBADF */
    }

    /* Find lowest available fd (starting from 0) */
    for (i = 0; i < MAX_FD; i++) {
        if (!fds[i].in_use) {
            fds[i] = fds[oldfd];
            /* Increment pipe refcount for the new fd */
            if (fds[i].type == FT_PIPE) {
                pipe_dup_fd((int)fds[i].inode, (int)fds[i].minor);
            }
            return i;
        }
    }

    return -1;  /* EMFILE — no free fd */
}

/*
 * sys_dup2 - Duplicate a file descriptor to a specific fd number
 * a0 = oldfd, a1 = newfd
 * Returns: newfd on success, or -1 on error
 */
static int32_t sys_dup2(trap_frame_t *tf) {
    int oldfd = (int)tf->a0;
    int newfd = (int)tf->a1;
    struct fd_entry *fds = proc_table[current_proc].fds;

    if (get_fd(oldfd) == (struct fd_entry *)0) {
        return -1;  /* EBADF */
    }

    if (newfd < 0 || newfd >= MAX_FD) {
        return -1;  /* EBADF */
    }

    if (oldfd == newfd) {
        return newfd;  /* No-op per POSIX */
    }

    /* Close newfd if open — handle pipe refcount decrement */
    if (fds[newfd].in_use) {
        if (fds[newfd].type == FT_PIPE) {
            pipe_close_fd((int)fds[newfd].inode, (int)fds[newfd].minor);
        }
        fds[newfd].in_use = 0;
    }

    /* Copy fd_entry */
    fds[newfd] = fds[oldfd];

    /* Increment pipe refcount for the new fd */
    if (fds[newfd].type == FT_PIPE) {
        pipe_dup_fd((int)fds[newfd].inode, (int)fds[newfd].minor);
    }

    return newfd;
}

/*
 * sys_pipe - Create a pipe
 * a0 = pointer to int[2] array (user space)
 *       pipefd[0] receives the read end
 *       pipefd[1] receives the write end
 * Returns: 0 on success, -1 on error
 */
static int32_t sys_pipe(trap_frame_t *tf) {
    int *pipefd = (int *)tf->a0;
    int pipe_idx;
    int read_fd, write_fd;
    struct fd_entry *fds = proc_table[current_proc].fds;
    int i;

    if (pipefd == (int *)0) {
        return -1;
    }

    /* Find two free fds */
    read_fd = -1;
    write_fd = -1;
    for (i = 0; i < MAX_FD; i++) {
        if (!fds[i].in_use) {
            if (read_fd < 0) {
                read_fd = i;
            } else {
                write_fd = i;
                break;
            }
        }
    }
    if (read_fd < 0 || write_fd < 0) {
        return -1;  /* EMFILE - not enough free fds */
    }

    /* Allocate pipe */
    pipe_idx = pipe_alloc();
    if (pipe_idx < 0) {
        return -1;  /* ENFILE - no free pipe slots */
    }

    /* Set up read end fd */
    fds[read_fd].in_use = 1;
    fds[read_fd].type = FT_PIPE;
    fds[read_fd].inode = (uint32_t)pipe_idx;
    fds[read_fd].offset = 0;
    fds[read_fd].major = 0;
    fds[read_fd].minor = 0;  /* 0 = read end */

    /* Set up write end fd */
    fds[write_fd].in_use = 1;
    fds[write_fd].type = FT_PIPE;
    fds[write_fd].inode = (uint32_t)pipe_idx;
    fds[write_fd].offset = 0;
    fds[write_fd].major = 0;
    fds[write_fd].minor = 1;  /* 1 = write end */

    /* Write fd numbers to user-space array */
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;

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

/*
 * sys_kill - Terminate a process by PID
 * a0 = pid
 * Returns: 0 on success, -1 if process not found
 */
static int32_t sys_kill(trap_frame_t *tf) {
    int target_pid = (int)tf->a0;
    int i;

    for (i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state != PROC_FREE && proc_table[i].pid == target_pid) {
            if (i == current_proc) return -1;
            if (proc_table[i].parent == -1) return -1;

            proc_table[i].state = PROC_ZOMBIE;
            proc_table[i].exit_code = -1;

            /* Wake parent if waiting */
            int parent = proc_table[i].parent;
            if (parent >= 0 && proc_table[parent].state == PROC_SLEEPING &&
                (proc_table[parent].sleep_reason == SLEEP_WAIT ||
                 (proc_table[parent].sleep_reason == SLEEP_CHILD &&
                  proc_table[parent].sleep_chan == i))) {
                proc_table[parent].state = PROC_READY;
            }

            return 0;
        }
    }

    return -1;
}

/*
 * sys_ps - List active processes
 * a0 = pointer to array of struct proc_info, a1 = max entries
 * Returns: number of entries filled
 */
static int32_t sys_ps(trap_frame_t *tf) {
    struct proc_info *buf = (struct proc_info *)tf->a0;
    int max_entries = (int)tf->a1;
    int count = 0;
    int i;

    for (i = 0; i < MAX_PROCS && count < max_entries; i++) {
        if (proc_table[i].state != PROC_FREE) {
            buf[count].pid = proc_table[i].pid;
            buf[count].state = proc_table[i].state;
            /* Convert parent slot index to PID (-1 stays as -1) */
            buf[count].parent = (proc_table[i].parent >= 0)
                ? proc_table[proc_table[i].parent].pid : -1;
            /* Copy program name */
            int j;
            for (j = 0; j < 31 && proc_table[i].name[j]; j++)
                buf[count].name[j] = proc_table[i].name[j];
            buf[count].name[j] = '\0';
            count++;
        }
    }

    return count;
}

/* ================================================================
 * Shared Memory syscalls
 * ================================================================ */

/*
 * sys_shmget - Get/create shared memory segment
 * a0 = key, a1 = flags
 * Returns: segment index on success, -1 on error
 */
static int32_t sys_shmget(trap_frame_t *tf) {
    int key = (int)tf->a0;
    int flags = (int)tf->a1;
    return shm_get(key, flags);
}

/*
 * sys_shmat - Attach to shared memory segment
 * a0 = shm_id (segment index)
 * Returns: physical address of segment, 0 on error
 */
static int32_t sys_shmat(trap_frame_t *tf) {
    int shm_id = (int)tf->a0;
    uint32_t addr;

    if (shm_id < 0 || shm_id >= MAX_SHM) return 0;

    /* Already attached — just return the address */
    if (proc_table[current_proc].shm_attached & (1 << shm_id)) {
        return (int32_t)shm_table[shm_id].addr;
    }

    addr = shm_attach(shm_id);
    if (addr != 0) {
        proc_table[current_proc].shm_attached |= (uint8_t)(1 << shm_id);
    }
    return (int32_t)addr;
}

/*
 * sys_shmdt - Detach from shared memory segment
 * a0 = shm_id (segment index)
 * Returns: 0 on success, -1 on error
 */
static int32_t sys_shmdt(trap_frame_t *tf) {
    int shm_id = (int)tf->a0;

    if (shm_id < 0 || shm_id >= MAX_SHM) return -1;
    if (!(proc_table[current_proc].shm_attached & (1 << shm_id))) {
        return -1;  /* Not attached */
    }

    proc_table[current_proc].shm_attached &= (uint8_t)~(1 << shm_id);
    shm_detach(shm_id);
    return 0;
}

/* ================================================================
 * Semaphore syscalls
 * ================================================================ */

/*
 * sys_semget - Get/create semaphore
 * a0 = key, a1 = initial value, a2 = flags
 * Returns: semaphore index on success, -1 on error
 */
static int32_t sys_semget(trap_frame_t *tf) {
    int key = (int)tf->a0;
    int init_value = (int)tf->a1;
    int flags = (int)tf->a2;
    int sem_id;

    sem_id = sem_get(key, init_value, flags);
    if (sem_id >= 0) {
        /* Mark this process as holding a reference (if not already) */
        if (!(proc_table[current_proc].sem_attached & (1 << sem_id))) {
            proc_table[current_proc].sem_attached |= (uint8_t)(1 << sem_id);
            sem_table[sem_id].refcount++;
        }
    }
    return sem_id;
}

/*
 * sys_semwait - Wait (decrement) semaphore
 * a0 = sem_id
 * Returns: 0 on success, -1 on error
 *
 * Uses ecall-retry pattern: if value==0, process sleeps with SLEEP_SEM.
 * When woken, the ecall re-executes and retries.
 */
static int32_t sys_semwait(trap_frame_t *tf) {
    int sem_id = (int)tf->a0;
    int result;

    if (sem_id < 0 || sem_id >= MAX_SEMS) return -1;

    result = sem_wait(sem_id);
    if (result == -2) {
        /* Would block: sleep and retry (ecall will re-execute) */
        proc_table[current_proc].state = PROC_SLEEPING;
        proc_table[current_proc].sleep_reason = SLEEP_SEM;
        proc_table[current_proc].sleep_chan = sem_id;
        schedule();  /* never returns; woken process re-executes ecall */
    }
    return result;
}

/*
 * sys_sempost - Post (increment) semaphore
 * a0 = sem_id
 * Returns: 0 on success, -1 on error
 */
static int32_t sys_sempost(trap_frame_t *tf) {
    int sem_id = (int)tf->a0;
    return sem_post(sem_id);
}

/*
 * sys_semclose - Close semaphore handle
 * a0 = sem_id
 * Returns: 0 on success, -1 on error
 */
static int32_t sys_semclose(trap_frame_t *tf) {
    int sem_id = (int)tf->a0;

    if (sem_id < 0 || sem_id >= MAX_SEMS) return -1;
    if (!(proc_table[current_proc].sem_attached & (1 << sem_id))) {
        return -1;  /* Not attached */
    }

    proc_table[current_proc].sem_attached &= (uint8_t)~(1 << sem_id);
    sem_close(sem_id);
    return 0;
}

/*
 * Dispatch a system call.
 * Returns 1 if the top-level program should exit (shell with no parent),
 * 0 otherwise.
 *
 * Note: sys_spawn and sys_exit may call trap_ret() directly and never return.
 */
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

        case SYS_fork:
            result = sys_fork(tf);
            break;

        case SYS_exec:
            result = sys_exec(tf);
            /* If we get here, exec failed — return error to caller */
            break;

        case SYS_wait:
            result = sys_wait(tf);
            /* If we get here, wait returned immediately (zombie or no children) */
            break;

        case SYS_getpid:
            result = sys_getpid();
            break;

        case SYS_dup:
            result = sys_dup(tf);
            break;

        case SYS_dup2:
            result = sys_dup2(tf);
            break;

        case SYS_pipe:
            result = sys_pipe(tf);
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

        case SYS_kill:
            result = sys_kill(tf);
            break;

        case SYS_ps:
            result = sys_ps(tf);
            break;

        case SYS_shmget:
            result = sys_shmget(tf);
            break;

        case SYS_shmat:
            result = sys_shmat(tf);
            break;

        case SYS_shmdt:
            result = sys_shmdt(tf);
            break;

        case SYS_semget:
            result = sys_semget(tf);
            break;

        case SYS_semwait:
            result = sys_semwait(tf);
            break;

        case SYS_sempost:
            result = sys_sempost(tf);
            break;

        case SYS_semclose:
            result = sys_semclose(tf);
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
