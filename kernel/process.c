/*
 * Process management implementation
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "process.h"
#include "console_dev.h"  /* for CONSOLE_MAJOR */
#include "fs_types.h"     /* for FT_CHARDEV, FT_PIPE */
#include "pipe.h"         /* for pipe_close_fd */
#include "string.h"
#include "trap.h"

/* Timer MMIO registers */
#define TIMER_MTIME    ((volatile uint32_t *)0x200bff8)
#define TIMER_MTIMECMP ((volatile uint32_t *)0x2004000)
#define TIME_SLICE     800

/* Global process table */
struct process proc_table[MAX_PROCS];
int current_proc = -1;

/* Monotonically increasing PID counter */
static int next_pid = 1;

void proc_init(void) {
    int i;
    for (i = 0; i < MAX_PROCS; i++) {
        proc_table[i].state = PROC_FREE;
        proc_table[i].pid = 0;
        proc_table[i].parent = -1;
        proc_table[i].exit_code = 0;
        proc_table[i].sleep_reason = SLEEP_NONE;
        proc_table[i].sleep_chan = -1;
        proc_table[i].mem_base = PROC_SLOT_BASE(i);
        proc_table[i].stack_top = PROC_SLOT_STACK(i);
    }
}

int proc_alloc(void) {
    int i;
    for (i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_FREE) {
            proc_table[i].pid = next_pid++;
            proc_table[i].state = PROC_READY;
            proc_table[i].parent = -1;
            proc_table[i].exit_code = 0;
            proc_table[i].sleep_reason = SLEEP_NONE;
            proc_table[i].sleep_chan = -1;
            proc_table[i].name[0] = '\0';
            return i;
        }
    }
    return -1;
}

/* Close all pipe fds in a slot, decrementing refcounts */
static void proc_close_pipes(int slot) {
    int i;
    for (i = 0; i < MAX_FD; i++) {
        if (proc_table[slot].fds[i].in_use &&
            proc_table[slot].fds[i].type == FT_PIPE) {
            pipe_close_fd((int)proc_table[slot].fds[i].inode,
                          (int)proc_table[slot].fds[i].minor);
            proc_table[slot].fds[i].in_use = 0;
        }
    }
}

void proc_free(int slot) {
    proc_close_pipes(slot);

    proc_table[slot].state = PROC_FREE;
    proc_table[slot].pid = 0;
    proc_table[slot].parent = -1;
    proc_table[slot].sleep_reason = SLEEP_NONE;
    proc_table[slot].sleep_chan = -1;
}

void proc_fd_init(int slot) {
    struct process *p = &proc_table[slot];
    int i;

    proc_close_pipes(slot);

    /* fd 0 = stdin, fd 1 = stdout, fd 2 = stderr -> console device */
    for (i = 0; i < 3; i++) {
        p->fds[i].in_use = 1;
        p->fds[i].inode = 0;
        p->fds[i].offset = 0;
        p->fds[i].type = FT_CHARDEV;
        p->fds[i].major = CONSOLE_MAJOR;
        p->fds[i].minor = 0;
    }
}

int proc_env_find(int slot, const char *name) {
    struct process *p = &proc_table[slot];
    int nlen = strlen(name);
    int i;
    for (i = 0; i < p->env_count; i++) {
        if (strncmp(p->env[i], name, nlen) == 0 && p->env[i][nlen] == '=') {
            return i;
        }
    }
    return -1;
}

void proc_env_init(int slot) {
    struct process *p = &proc_table[slot];
    p->env_count = 0;
    strcpy(p->env[0], "PATH=/bin");
    strcpy(p->env[1], "CWD=/");
    strcpy(p->env[2], "?=0");
    p->env_count = 3;
}

void proc_env_copy(int dst_slot, int src_slot) {
    struct process *dst = &proc_table[dst_slot];
    struct process *src = &proc_table[src_slot];
    int i;
    for (i = 0; i < src->env_count; i++) {
        strcpy(dst->env[i], src->env[i]);
    }
    dst->env_count = src->env_count;
}

void proc_set_env(int slot, const char *name, const char *value) {
    struct process *p = &proc_table[slot];
    int nlen = strlen(name);
    int vlen = strlen(value);
    int idx;
    char *s;

    if (nlen + 1 + vlen >= MAX_ENV_LEN) return;

    idx = proc_env_find(slot, name);
    if (idx < 0) {
        if (p->env_count >= MAX_ENVC) return;
        idx = p->env_count++;
    }

    s = p->env[idx];
    memcpy(s, name, nlen);
    s[nlen] = '=';
    memcpy(s + nlen + 1, value, vlen);
    s[nlen + 1 + vlen] = '\0';
}

void proc_set_env_int(int slot, const char *name, int value) {
    char buf[12];
    int val = value;
    int neg = 0;
    int pos = 0;
    int i;

    if (val < 0) { neg = 1; val = -val; }
    do {
        buf[pos++] = '0' + (val % 10);
        val /= 10;
    } while (val > 0);
    if (neg) buf[pos++] = '-';
    /* Reverse in place */
    for (i = 0; i < pos / 2; i++) {
        char t = buf[i];
        buf[i] = buf[pos - 1 - i];
        buf[pos - 1 - i] = t;
    }
    buf[pos] = '\0';
    proc_set_env(slot, name, buf);
}

void timer_init(void) {
    /* Enable timer (bit 7) and external (bit 11) interrupt sources */
    set_mie(0x880);
    /* Set first timer alarm */
    *TIMER_MTIMECMP = *TIMER_MTIME + TIME_SLICE;
    /* Set MPIE so interrupts become enabled on mret */
    set_mstatus_bit(0x80);
}

void schedule(void) {
    int i, next;
    for (i = 0; i < MAX_PROCS; i++) {
        next = (current_proc + 1 + i) % MAX_PROCS;
        if (proc_table[next].state == PROC_READY) {
            proc_table[next].state = PROC_RUNNING;
            current_proc = next;
            set_trap_handler(trap_handler, &proc_table[next].tf);
            trap_ret(&proc_table[next].tf);  /* never returns */
        }
    }
    /* No runnable process found — kernel main loop handles shell restart */
}
