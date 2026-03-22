/*
 * mknod - create a device node
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: mknod <path> <major> <minor>
 * Creates a device node at the specified path with given major/minor numbers.
 */

#include "libc.h"

int main(int argc, char *argv[]) {
    int major, minor, result;

    if (argc < 4) {
        puts("usage: mknod <path> <major> <minor>");
        return 1;
    }

    major = atoi(argv[2]);
    minor = atoi(argv[3]);

    if (major < 0 || minor < 0) {
        puts("mknod: major and minor must be non-negative integers");
        return 1;
    }

    result = mknod(argv[1], major, minor);
    if (result < 0) {
        printf("mknod: cannot create '%s': error %d\n", argv[1], result);
        return 1;
    }

    return 0;
}
