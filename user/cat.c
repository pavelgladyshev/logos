/*
 * cat - print files
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: cat <file> [file2 ...]
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    int i, fd, ret = 0;
    char c;

    if (argc < 2) {
        puts("usage: cat <file> [file2 ...]");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            printf("cat: cannot open '%s'\n", argv[i]);
            ret = 1;
            continue;
        }
        while (read(fd, &c, 1) > 0)
            putchar(c);
        close(fd);
    }

    return ret;
}
