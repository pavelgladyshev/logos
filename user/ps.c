/*
 * ps - list active processes
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: ps
 */

#include "libc.h"

#define MAX_PROCS 8

int main(void)
{
    struct proc_info procs[MAX_PROCS];
    int count, i;

    count = ps(procs, MAX_PROCS);
    if (count < 0) {
        puts("ps: error");
        return 1;
    }

    puts("PID  STATE    PARENT");
    for (i = 0; i < count; i++) {
        char *state;
        int s = procs[i].state;
        if (s == 1) state = "ready";
        else if (s == 2) state = "running";
        else if (s == 3) state = "sleeping";
        else if (s == 4) state = "zombie";
        else state = "?";

        printf("%-4d %-8s %d\n", procs[i].pid, state, procs[i].parent);
    }

    return 0;
}
