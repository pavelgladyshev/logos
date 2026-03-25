/*
 * Shared memory implementation for inter-process communication
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Provides 8 fixed-size (4KB) shared memory segments at physical
 * addresses 0x150000-0x157FFF. No MMU — shmat returns the physical address.
 * Key-based API (System V style) with refcounting.
 */

#ifndef SHM_H
#define SHM_H

#include "types.h"

#define MAX_SHM         8
#define SHM_SEG_SIZE    4096
#define SHM_BASE_ADDR   0x00150000

/* Flags for shmget / semget */
#define IPC_CREAT       0x100
#define IPC_EXCL        0x200

/* Segment states */
#define SHM_FREE        0
#define SHM_ACTIVE      1

struct shm_seg {
    int      state;         /* SHM_FREE or SHM_ACTIVE */
    int      key;           /* User-supplied key */
    int      refcount;      /* Number of processes attached */
    uint32_t addr;          /* Physical address of this segment */
};

extern struct shm_seg shm_table[MAX_SHM];

/*
 * Look up or create a shared memory segment by key.
 * flags: IPC_CREAT to create if not found, IPC_EXCL to fail if exists.
 * Returns segment index (0..MAX_SHM-1) on success, -1 on error.
 */
int shm_get(int key, int flags);

/*
 * Attach to a shared memory segment.
 * Increments refcount.
 * Returns physical address of the segment, or 0 on error.
 */
uint32_t shm_attach(int shm_idx);

/*
 * Detach from a shared memory segment.
 * Decrements refcount. Frees segment if refcount reaches 0.
 */
void shm_detach(int shm_idx);

/*
 * Increment refcount for a segment (called by fork).
 */
void shm_dup(int shm_idx);

#endif /* SHM_H */
