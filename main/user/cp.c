/*
 * cp - copy files
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: cp <src> <dst>
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    int src_fd, dst_fd;
    char buf[256];
    int n;

    if (argc != 3) {
        puts("usage: cp <src> <dst>");
        return 1;
    }

    src_fd = open(argv[1], O_RDONLY);
    if (src_fd < 0) {
        printf("cp: cannot open '%s'\n", argv[1]);
        return 1;
    }

    dst_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC);
    if (dst_fd < 0) {
        printf("cp: cannot create '%s'\n", argv[2]);
        close(src_fd);
        return 1;
    }

    while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
        if (write(dst_fd, buf, n) != n) {
            printf("cp: write error\n");
            close(src_fd);
            close(dst_fd);
            return 1;
        }
    }

    close(src_fd);
    close(dst_fd);
    return 0;
}
