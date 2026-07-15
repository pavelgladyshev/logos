/* rm - remove files */
#include "libc.h"

int main(int argc, char *argv[])
{
    int i, result = 0;
    if (argc < 2) {
        puts("usage: rm <path> [path2 ...]\n");
        return 1;
    }
    for (i = 1; i < argc; i++) {
        if (rmfile(argv[i]) < 0) {
            printf("rm: cannot remove '%s'\n", argv[i]);
            result = 1;
        }
    }
    return result;
}
