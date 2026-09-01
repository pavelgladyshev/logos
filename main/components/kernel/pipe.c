/*
 * Pipe implementation for inter-process communication
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Provides kernel-buffered circular pipes for IPC between processes.
 * Each pipe has separate read and write reference counts to detect
 * EOF (all writers closed) and broken pipe (all readers closed).
 *
 * Blocking uses the ecall-retry pattern: when a process cannot complete
 * a pipe read/write, it sleeps with SLEEP_IO. When woken, the ecall
 * re-executes and the operation is retried.
 */

#include "pipe.h"
#include "process.h"
#include "string.h"

/* Global pipe table */
struct pipe pipe_table[MAX_PIPES];

int pipe_alloc(void) {
    int i;
    for (i = 0; i < MAX_PIPES; i++) {
        if (pipe_table[i].state == PIPE_FREE) {
            memset(&pipe_table[i], 0, sizeof(struct pipe));
            pipe_table[i].state = PIPE_ACTIVE;
            pipe_table[i].read_refs = 1;
            pipe_table[i].write_refs = 1;
            return i;
        }
    }
    return -1;  /* No free pipe slots */
}

void pipe_free(int pipe_idx) {
    pipe_table[pipe_idx].state = PIPE_FREE;
}

int pipe_read(int pipe_idx, void *buf, uint32_t len) {
    struct pipe *p = &pipe_table[pipe_idx];
    uint8_t *dst = (uint8_t *)buf;
    uint32_t to_read;
    uint32_t i;

    if (p->count == 0) {
        if (p->write_refs == 0) {
            return 0;   /* EOF: no more writers */
        }
        return -2;      /* Would block: writers exist but no data yet */
    }

    /* Read up to len bytes or whatever is available */
    to_read = (len < p->count) ? len : p->count;
    for (i = 0; i < to_read; i++) {
        dst[i] = p->buf[p->read_pos];
        p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
    }
    p->count -= to_read;

    /* Wake writers who may have been blocked on a full buffer */
    pipe_wakeup(pipe_idx);

    return (int)to_read;
}

int pipe_write(int pipe_idx, const void *buf, uint32_t len) {
    struct pipe *p = &pipe_table[pipe_idx];
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t space;
    uint32_t to_write;
    uint32_t i;

    if (p->read_refs == 0) {
        return -1;      /* EPIPE: no readers — broken pipe */
    }

    space = PIPE_BUF_SIZE - p->count;
    if (space == 0) {
        return -2;      /* Would block: buffer full, readers exist */
    }

    /* Write up to available space (partial write is OK) */
    to_write = (len < space) ? len : space;
    for (i = 0; i < to_write; i++) {
        p->buf[p->write_pos] = src[i];
        p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
    }
    p->count += to_write;

    /* Wake readers who may have been blocked on an empty buffer */
    pipe_wakeup(pipe_idx);

    return (int)to_write;
}

void pipe_close_fd(int pipe_idx, int pipe_end) {
    struct pipe *p = &pipe_table[pipe_idx];

    if (pipe_end == 0) {
        p->read_refs--;
    } else {
        p->write_refs--;
    }

    /* Wake sleepers so they can re-check conditions:
     * - Readers blocked on empty will see write_refs==0 => EOF
     * - Writers blocked on full will see read_refs==0 => EPIPE
     */
    pipe_wakeup(pipe_idx);

    /* Free pipe if both ends are fully closed */
    if (p->read_refs <= 0 && p->write_refs <= 0) {
        pipe_free(pipe_idx);
    }
}

void pipe_dup_fd(int pipe_idx, int pipe_end) {
    if (pipe_end == 0) {
        pipe_table[pipe_idx].read_refs++;
    } else {
        pipe_table[pipe_idx].write_refs++;
    }
}

void pipe_wakeup(int pipe_idx) {
    int i;
    for (i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_SLEEPING &&
            proc_table[i].sleep_reason == SLEEP_IO &&
            proc_table[i].sleep_chan == pipe_idx) {
            proc_table[i].state = PROC_READY;
            proc_table[i].sleep_reason = SLEEP_NONE;
            proc_table[i].sleep_chan = -1;
        }
    }
}
