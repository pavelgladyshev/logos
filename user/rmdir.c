/*
 * rmdir - remove empty directories
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: rmdir <path>
 * Removes the directory at the specified path (must be empty).
 */

#include "libc.h"

int main(int argc, char *argv[]) {
    int result;

    if (argc < 2) {
        puts("usage: rmdir <path>");
        return 1;
    }

    result = rmdir(argv[1]);
    if (result < 0) {
        printf("rmdir: cannot remove '%s': error %d\n", argv[1], result);
        return 1;
    }

    return 0;
}
