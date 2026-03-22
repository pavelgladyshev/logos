/*
 * cat - Concatenate and print files
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage:
 *   cat              Read from stdin until EOF
 *   cat <file>       Print contents of file
 *
 * Useful as a pipeline component: echo hello | cat, cat < file.txt
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    int fd;
    char buf[64];
    int n;

    if (argc > 1) {
        /* Open the specified file */
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            printf("cat: %s: No such file\n", argv[1]);
            return 1;
        }
    } else {
        /* No arguments — read from stdin */
        fd = STDIN_FILENO;
    }

    /* Read and write loop */
    while (1) {
        n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;  /* EOF or error */
        write(STDOUT_FILENO, buf, n);
    }

    if (fd != STDIN_FILENO) {
        close(fd);
    }

    return 0;
}
