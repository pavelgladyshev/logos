/*
 * Semaphore implementation
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "sem.h"
#include "shm.h"      /* for IPC_CREAT, IPC_EXCL flag values */
#include "process.h"

struct sem sem_table[MAX_SEMS];

int sem_get(int key, int init_value, int flags) {
    int i;
    int free_slot = -1;

    for (i = 0; i < MAX_SEMS; i++) {
        if (sem_table[i].state == SEM_ACTIVE && sem_table[i].key == key) {
            if (flags & IPC_EXCL) {
                return -1;
            }
            return i;
        }
        if (sem_table[i].state == SEM_FREE && free_slot < 0) {
            free_slot = i;
        }
    }

    if (!(flags & IPC_CREAT)) {
        return -1;
    }
    if (free_slot < 0) {
        return -1;
    }

    sem_table[free_slot].state = SEM_ACTIVE;
    sem_table[free_slot].key = key;
    sem_table[free_slot].value = init_value;
    sem_table[free_slot].refcount = 0;

    return free_slot;
}

int sem_wait(int sem_idx) {
    if (sem_idx < 0 || sem_idx >= MAX_SEMS) return -1;
    if (sem_table[sem_idx].state != SEM_ACTIVE) return -1;

    if (sem_table[sem_idx].value <= 0) {
        return -2;  /* Would block */
    }

    sem_table[sem_idx].value--;
    return 0;
}

int sem_post(int sem_idx) {
    if (sem_idx < 0 || sem_idx >= MAX_SEMS) return -1;
    if (sem_table[sem_idx].state != SEM_ACTIVE) return -1;

    sem_table[sem_idx].value++;
    sem_wakeup(sem_idx);
    return 0;
}

void sem_close(int sem_idx) {
    if (sem_idx < 0 || sem_idx >= MAX_SEMS) return;
    if (sem_table[sem_idx].state != SEM_ACTIVE) return;

    sem_table[sem_idx].refcount--;

    /* Wake sleepers so they can see the semaphore state change */
    sem_wakeup(sem_idx);

    if (sem_table[sem_idx].refcount <= 0) {
        sem_table[sem_idx].state = SEM_FREE;
    }
}

void sem_dup(int sem_idx) {
    if (sem_idx >= 0 && sem_idx < MAX_SEMS &&
        sem_table[sem_idx].state == SEM_ACTIVE) {
        sem_table[sem_idx].refcount++;
    }
}

void sem_wakeup(int sem_idx) {
    int i;
    for (i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_SLEEPING &&
            proc_table[i].sleep_reason == SLEEP_SEM &&
            proc_table[i].sleep_chan == sem_idx) {
            proc_table[i].state = PROC_READY;
            proc_table[i].sleep_reason = SLEEP_NONE;
            proc_table[i].sleep_chan = -1;
        }
    }
}
