/*
 * mknod - create a device node
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: mknod <path> <major> <minor>
 * Creates a device node at the specified path with given major/minor numbers.
 */

#include "libc.h"

/*
 * Simple string-to-integer conversion.
 * Returns the parsed non-negative integer, or -1 on error.
 */
static int atoi(const char *s) {
    int n = 0;

    if (*s == '\0') return -1;

    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }

    /* If we stopped on a non-NUL character, the string wasn't all digits */
    if (*s != '\0') return -1;

    return n;
}

int main(int argc, char *argv[]) {
    int major, minor, result;

    if (argc < 4) {
        puts("usage: mknod <path> <major> <minor>\n");
        return 1;
    }

    major = atoi(argv[2]);
    minor = atoi(argv[3]);

    if (major < 0 || minor < 0) {
        puts("mknod: major and minor must be non-negative integers\n");
        return 1;
    }

    result = mknod(argv[1], major, minor);
    if (result < 0) {
        printf("mknod: cannot create '%s': error %d\n", argv[1], result);
        return 1;
    }

    return 0;
}
