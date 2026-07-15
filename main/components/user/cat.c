/* cat - print a file or standard input */
#include "libc.h"

int main(int argc, char *argv[])
{
    int fd = STDIN_FILENO;
    char buf[64];
    int n;

    if (argc > 1) {
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            printf("cat: %s: No such file\n", argv[1]);
            return 1;
        }
    }

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, n);
    }

    if (fd != STDIN_FILENO) close(fd);
    return 0;
}
