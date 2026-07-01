/*
 * rm - remove files
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: rm <path> [path2 ...]
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    int i, ret = 0;

    if (argc < 2) {
        puts("usage: rm <path> [path2 ...]");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (unlink(argv[i]) < 0) {
            printf("rm: cannot remove '%s'\n", argv[i]);
            ret = 1;
        }
    }

    return ret;
}
