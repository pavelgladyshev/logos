/* ls - list directory contents */
#include "libc.h"

#define MAX_ENTRIES 32

int main(int argc, char *argv[])
{
    struct dirent entries[MAX_ENTRIES];
    const char *path = argc > 1 ? argv[1] : getenv("CWD");
    int count, i;

    if (path == (char *)0) path = "/";
    count = readdir(path, entries, MAX_ENTRIES);
    if (count < 0) {
        printf("ls: cannot list '%s': error %d\n", path, count);
        return 1;
    }
    for (i = 0; i < count; i++) {
        puts(entries[i].name);
    }
    return 0;
}
