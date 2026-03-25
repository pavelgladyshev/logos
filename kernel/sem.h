/*
 * Semaphore implementation for inter-process synchronization
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Provides 8 kernel-managed counting semaphores with blocking via
 * the ecall-retry pattern (same as pipes). Key-based API.
 */

#ifndef SEM_H
#define SEM_H

#include "types.h"

#define MAX_SEMS    8

/* Semaphore states */
#define SEM_FREE    0
#define SEM_ACTIVE  1

struct sem {
    int      state;         /* SEM_FREE or SEM_ACTIVE */
    int      key;           /* User-supplied key */
    int      value;         /* Current count */
    int      refcount;      /* Number of processes with handle open */
};

extern struct sem sem_table[MAX_SEMS];

/*
 * Look up or create a semaphore by key.
 * init_value: initial count (only used when creating).
 * flags: IPC_CREAT to create if not found, IPC_EXCL to fail if exists.
 * Returns semaphore index (0..MAX_SEMS-1) on success, -1 on error.
 */
int sem_get(int key, int init_value, int flags);

/*
 * Wait (decrement) a semaphore.
 * Returns 0 if decremented successfully.
 * Returns -2 if would block (value <= 0) — caller should sleep with SLEEP_SEM.
 */
int sem_wait(int sem_idx);

/*
 * Post (increment) a semaphore and wake blocked processes.
 * Returns 0 on success, -1 on error.
 */
int sem_post(int sem_idx);

/*
 * Close a semaphore handle.
 * Decrements refcount. Frees semaphore if refcount reaches 0.
 */
void sem_close(int sem_idx);

/*
 * Increment refcount for a semaphore (called by fork).
 */
void sem_dup(int sem_idx);

/*
 * Wake all processes sleeping on a given semaphore.
 * Sets matching SLEEP_SEM processes to PROC_READY.
 */
void sem_wakeup(int sem_idx);

#endif /* SEM_H */
