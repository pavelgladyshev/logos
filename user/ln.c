/*
 * ln - create hard links
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: ln <target> <linkname>
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        puts("usage: ln <target> <linkname>");
        return 1;
    }

    if (link(argv[1], argv[2]) < 0) {
        printf("ln: cannot create link '%s' -> '%s'\n", argv[2], argv[1]);
        return 1;
    }

    return 0;
}
