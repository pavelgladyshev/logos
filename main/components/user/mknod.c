/* mknod - create a device node */
#include "libc.h"

static int parse_uint(const char *s)
{
    int n = 0;
    if (*s == '\0') return -1;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return *s == '\0' ? n : -1;
}

int main(int argc, char *argv[])
{
    int major, minor;
    if (argc < 4) {
        puts("usage: mknod <path> <major> <minor>\n");
        return 1;
    }
    major = parse_uint(argv[2]);
    minor = parse_uint(argv[3]);
    if (major < 0 || minor < 0 || mknod(argv[1], major, minor) < 0) {
        printf("mknod: cannot create '%s'\n", argv[1]);
        return 1;
    }
    return 0;
}
