/*
 * Process management for LOGOS kernel
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Each process gets a fixed 32KB memory slot and its own trap frame,
 * file descriptors, environment, and PID.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"
#include "trap.h"
#include "syscall.h"  /* for struct fd_entry, MAX_FD */

/* Process states — classic 5-state model */
#define PROC_FREE     0   /* Slot is available */
#define PROC_READY    1   /* Runnable, waiting for CPU */
#define PROC_RUNNING  2   /* Currently executing on CPU (exactly one) */
#define PROC_SLEEPING 3   /* Waiting for an event (not schedulable) */
#define PROC_ZOMBIE   4   /* Exited, waiting for parent to collect */

/* Sleep reasons (why is the process sleeping?) */
#define SLEEP_NONE    0   /* Not sleeping */
#define SLEEP_CHILD   1   /* spawn(): waiting for specific child to exit */
#define SLEEP_WAIT    2   /* wait(): waiting for any child to exit */
#define SLEEP_TIMER   3   /* future: sleep(n) — waiting for timer */
#define SLEEP_IO      4   /* future: blocking read — waiting for input */

/* Process table limits */
#define MAX_PROCS       8

/* Memory layout: 32KB fixed slots starting after kernel BSS */
#define PROC_SLOT_SIZE  0x8000      /* 32KB per process */
#define PROC_MEM_START  0x00110000  /* First slot base address */

/* Compute addresses for slot n */
#define PROC_SLOT_BASE(n)   (PROC_MEM_START + (n) * PROC_SLOT_SIZE)
#define PROC_SLOT_STACK(n)  (PROC_SLOT_BASE(n) + PROC_SLOT_SIZE - 0x100)

/* Process control block */
struct process {
    int              state;        /* PROC_FREE / READY / RUNNING / SLEEPING / ZOMBIE */
    int              pid;          /* Process ID (0 = unused) */
    int              parent;       /* Slot index of parent (-1 = kernel) */
    int              exit_code;    /* Exit status */
    int              sleep_reason; /* Why sleeping (SLEEP_CHILD, SLEEP_WAIT, etc.) */
    int              sleep_chan;    /* Sleep context: child slot, wake time, device, etc. */
    trap_frame_t     tf;           /* Saved CPU registers (144 bytes) */
    uint32_t         mem_base;     /* Start of memory slot */
    uint32_t         stack_top;    /* Top of stack within slot */
    char             name[32];     /* Program name (from exec/spawn path) */
    struct fd_entry  fds[MAX_FD];  /* Per-process file descriptors */
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

/*
 * Initialize timer interrupts for preemptive scheduling.
 * Enables timer and external interrupt sources, sets first alarm.
 */
void timer_init(void);

/*
 * Round-robin scheduler — select next PROC_READY process and switch to it.
 * Sets selected process to PROC_RUNNING. Never returns (calls trap_ret).
 * Caller must set outgoing process state before calling.
 */
void schedule(void);

#endif /* PROCESS_H */
