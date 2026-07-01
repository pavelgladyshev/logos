/*
 * mv - move/rename files and directories
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: mv <src> <dst>
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        puts("usage: mv <src> <dst>");
        return 1;
    }

    if (rename(argv[1], argv[2]) < 0) {
        printf("mv: cannot move '%s' to '%s'\n", argv[1], argv[2]);
        return 1;
    }

    return 0;
}
