/*
 * kill - terminate a process
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: kill <pid>
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    int pid = 0;
    char *p;

    if (argc != 2) {
        puts("usage: kill <pid>");
        return 1;
    }

    p = argv[1];
    while (*p >= '0' && *p <= '9') {
        pid = pid * 10 + (*p - '0');
        p++;
    }

    if (kill(pid) < 0) {
        printf("kill: cannot kill pid %d\n", pid);
        return 1;
    }

    return 0;
}
