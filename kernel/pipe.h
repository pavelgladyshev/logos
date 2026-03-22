/*
 * Pipe implementation for inter-process communication
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Pipes provide a unidirectional data channel between processes.
 * Data written to the write end can be read from the read end.
 * Blocking is handled via the SLEEP_IO mechanism with ecall-retry.
 */

#ifndef PIPE_H
#define PIPE_H

#include "types.h"

#define MAX_PIPES       8
#define PIPE_BUF_SIZE   256

/* Pipe states */
#define PIPE_FREE       0
#define PIPE_ACTIVE     1

/*
 * Pipe structure — kernel-side circular buffer with reference counts.
 *
 * The fd_entry encodes pipe information as:
 *   type  = FT_PIPE
 *   inode = pipe index (0..MAX_PIPES-1)
 *   minor = 0 for read end, 1 for write end
 */
struct pipe {
    int      state;                     /* PIPE_FREE or PIPE_ACTIVE */
    uint8_t  buf[PIPE_BUF_SIZE];        /* Circular buffer */
    uint32_t read_pos;                  /* Next byte to read */
    uint32_t write_pos;                 /* Next byte to write */
    uint32_t count;                     /* Number of bytes in buffer */
    int      read_refs;                 /* Number of open read-end fds */
    int      write_refs;                /* Number of open write-end fds */
};

/* Global pipe table */
extern struct pipe pipe_table[MAX_PIPES];

/*
 * Allocate a new pipe.
 * Initializes the pipe with read_refs=1, write_refs=1.
 * Returns pipe index (>= 0) on success, -1 if no free slot.
 */
int pipe_alloc(void);

/*
 * Free a pipe slot.
 */
void pipe_free(int pipe_idx);

/*
 * Read from a pipe buffer.
 * Returns:
 *   > 0: number of bytes read
 *     0: EOF (buffer empty AND no writers remain)
 *    -2: would block (buffer empty, writers exist) — caller should sleep
 */
int pipe_read(int pipe_idx, void *buf, uint32_t len);

/*
 * Write to a pipe buffer.
 * Returns:
 *   > 0: number of bytes written (may be partial if buffer nearly full)
 *    -1: EPIPE (no readers remain — broken pipe)
 *    -2: would block (buffer full, readers exist) — caller should sleep
 */
int pipe_write(int pipe_idx, const void *buf, uint32_t len);

/*
 * Handle fd close for a pipe end.
 * pipe_end: 0 = read end, 1 = write end.
 * Decrements the appropriate refcount, wakes sleepers, frees pipe if done.
 */
void pipe_close_fd(int pipe_idx, int pipe_end);

/*
 * Handle fd duplication for a pipe end.
 * Increments the appropriate refcount.
 * pipe_end: 0 = read end, 1 = write end.
 */
void pipe_dup_fd(int pipe_idx, int pipe_end);

/*
 * Wake all processes sleeping on a given pipe.
 * Sets matching SLEEP_IO processes to PROC_READY.
 */
void pipe_wakeup(int pipe_idx);

#endif /* PIPE_H */
