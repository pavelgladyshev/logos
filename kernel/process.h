/*
 * Process management for LOGOS kernel
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Each process gets a fixed 64KB memory slot and its own trap frame,
 * file descriptors, environment, and PID.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"
#include "trap.h"
#include "syscall.h"  /* for struct fd_entry, MAX_FD */

/* Process states */
#define PROC_FREE    0   /* Slot is available */
#define PROC_RUNNING 1   /* Currently executing on CPU */
#define PROC_READY   2   /* Suspended (parent waiting for child to exit) */

/* Process table limits */
#define MAX_PROCS       8

/* Memory layout: 64KB fixed slots starting after kernel BSS */
#define PROC_SLOT_SIZE  0x10000     /* 64KB per process */
#define PROC_MEM_START  0x00110000  /* First slot base address */

/* Compute addresses for slot n */
#define PROC_SLOT_BASE(n)   (PROC_MEM_START + (n) * PROC_SLOT_SIZE)
#define PROC_SLOT_STACK(n)  (PROC_SLOT_BASE(n) + PROC_SLOT_SIZE - 0x100)

/* Process control block */
struct process {
    int              state;      /* PROC_FREE / PROC_RUNNING / PROC_READY */
    int              pid;        /* Process ID (0 = unused) */
    int              parent;     /* Slot index of parent (-1 = kernel) */
    int              exit_code;  /* Exit status */
    trap_frame_t     tf;         /* Saved CPU registers (144 bytes) */
    uint32_t         mem_base;   /* Start of memory slot */
    uint32_t         stack_top;  /* Top of stack within slot */
    struct fd_entry  fds[MAX_FD]; /* Per-process file descriptors */
    char             env[MAX_ENVC][MAX_ENV_LEN];  /* Per-process environment */
    int              env_count;                     /* Number of env entries */
};

/* Global process table and current process index */
extern struct process proc_table[MAX_PROCS];
extern int current_proc;

/*
 * Initialize the process subsystem.
 * Sets up slot addresses for all entries.
 */
void proc_init(void);

/*
 * Allocate a free process slot.
 * Returns slot index (>= 0) on success, -1 if no free slot.
 */
int proc_alloc(void);

/*
 * Free a process slot.
 */
void proc_free(int slot);

/*
 * Initialize file descriptors for a process.
 * Opens stdin/stdout/stderr on the console device.
 */
void proc_fd_init(int slot);

/*
 * Find a variable in a process's environment by name.
 * Returns index, or -1 if not found.
 */
int proc_env_find(int slot, const char *name);

/*
 * Initialize environment for a process slot with defaults.
 * Sets PATH=/bin, CWD=/, ?=0.
 */
void proc_env_init(int slot);

/*
 * Copy environment from one process slot to another.
 */
void proc_env_copy(int dst_slot, int src_slot);

/*
 * Set an environment variable in a specific process slot.
 */
void proc_set_env(int slot, const char *name, const char *value);

/*
 * Set an environment variable to an integer value in a specific process slot.
 */
void proc_set_env_int(int slot, const char *name, int value);

#endif /* PROCESS_H */
