/*
 * ps - list active processes
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: ps
 */

#include "libc.h"

#define MAX_PROCS 8

/* Process states (must match kernel/process.h) */
#define PROC_READY    1
#define PROC_RUNNING  2
#define PROC_SLEEPING 3
#define PROC_ZOMBIE   4

int main(void)
{
    struct proc_info procs[MAX_PROCS];
    int count, i;

    count = ps(procs, MAX_PROCS);
    if (count < 0) {
        puts("ps: error");
        return 1;
    }

    puts("PID  STATE    PARENT  NAME");
    for (i = 0; i < count; i++) {
        char *state;
        int s = procs[i].state;
        if (s == PROC_RUNNING) state = "running";
        else if (s == PROC_READY) state = "ready";
        else if (s == PROC_SLEEPING) state = "sleeping";
        else if (s == PROC_ZOMBIE) state = "zombie";
        else state = "?";

        printf("%-4d %-8s %-7d %s\n", procs[i].pid, state, procs[i].parent, procs[i].name);
    }

    return 0;
}
