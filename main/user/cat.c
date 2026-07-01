/*
 * cat - read a file byte by byte and print to stdout
 * Used to test EOF handling on regular files.
 */

#include "libc.h"

int main(void) {
    int fd;
    char c;
    int n;
    int total = 0;

    fd = open("/etc/hello.txt", O_RDONLY);
    if (fd < 0) {
        puts("cat: cannot open /etc/hello.txt");
        return 1;
    }

    printf("cat: opened fd=%d, reading byte by byte...\n", fd);

    while (1) {
        n = read(fd, &c, 1);
        if (n < 0) {
            printf("cat: read error %d after %d bytes\n", n, total);
            break;
        }
        if (n == 0) {
            printf("\ncat: EOF after %d bytes\n", total);
            break;
        }
        putchar(c);
        total++;
    }

    close(fd);
    return 0;
}
