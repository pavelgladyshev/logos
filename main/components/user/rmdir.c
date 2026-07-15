/* rmdir - remove an empty directory */
#include "libc.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        puts("usage: rmdir <path>\n");
        return 1;
    }
    if (rmdir(argv[1]) < 0) {
        printf("rmdir: cannot remove '%s'\n", argv[1]);
        return 1;
    }
    return 0;
}
