/*
 * kill - terminate a process
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: kill <pid>
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    int pid;

    if (argc != 2) {
        puts("usage: kill <pid>");
        return 1;
    }

    pid = atoi(argv[1]);
    if (pid < 0) {
        puts("kill: invalid pid");
        return 1;
    }

    if (kill(pid) < 0) {
        printf("kill: cannot kill pid %d\n", pid);
        return 1;
    }

    return 0;
}
