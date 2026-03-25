/*
 * Shared memory implementation
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "shm.h"
#include "string.h"

struct shm_seg shm_table[MAX_SHM];

int shm_get(int key, int flags) {
    int i;
    int free_slot = -1;

    /* Search for existing segment with matching key */
    for (i = 0; i < MAX_SHM; i++) {
        if (shm_table[i].state == SHM_ACTIVE && shm_table[i].key == key) {
            if (flags & IPC_EXCL) {
                return -1;  /* Exists and caller wanted exclusive */
            }
            return i;
        }
        if (shm_table[i].state == SHM_FREE && free_slot < 0) {
            free_slot = i;
        }
    }

    /* Not found — create if IPC_CREAT */
    if (!(flags & IPC_CREAT)) {
        return -1;  /* Not found and no create flag */
    }
    if (free_slot < 0) {
        return -1;  /* No free slots */
    }

    /* Initialize segment */
    shm_table[free_slot].state = SHM_ACTIVE;
    shm_table[free_slot].key = key;
    shm_table[free_slot].refcount = 0;
    shm_table[free_slot].addr = SHM_BASE_ADDR + (free_slot * SHM_SEG_SIZE);

    /* Zero the memory */
    memset((void *)shm_table[free_slot].addr, 0, SHM_SEG_SIZE);

    return free_slot;
}

uint32_t shm_attach(int shm_idx) {
    if (shm_idx < 0 || shm_idx >= MAX_SHM) return 0;
    if (shm_table[shm_idx].state != SHM_ACTIVE) return 0;

    shm_table[shm_idx].refcount++;
    return shm_table[shm_idx].addr;
}

void shm_detach(int shm_idx) {
    if (shm_idx < 0 || shm_idx >= MAX_SHM) return;
    if (shm_table[shm_idx].state != SHM_ACTIVE) return;

    shm_table[shm_idx].refcount--;
    if (shm_table[shm_idx].refcount <= 0) {
        shm_table[shm_idx].state = SHM_FREE;
    }
}

void shm_dup(int shm_idx) {
    if (shm_idx >= 0 && shm_idx < MAX_SHM &&
        shm_table[shm_idx].state == SHM_ACTIVE) {
        shm_table[shm_idx].refcount++;
    }
}
