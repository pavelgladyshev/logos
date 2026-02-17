/*
 * mkdir - create directories
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: mkdir <path>
 * Creates a new directory at the specified path.
 */

#include "libc.h"

int main(int argc, char *argv[]) {
    int result;

    if (argc < 2) {
        puts("usage: mkdir <path>\n");
        return 1;
    }

    result = mkdir(argv[1]);
    if (result < 0) {
        printf("mkdir: cannot create '%s': error %d\n", argv[1], result);
        return 1;
    }

    return 0;
}
