/*
 * ls - list directory contents
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Usage: ls [path]
 * Lists the contents of the specified directory (default: current directory).
 */

#include "libc.h"

#define MAX_ENTRIES 32

int main(int argc, char *argv[])
{
    struct dirent entries[MAX_ENTRIES];  /* On stack to avoid PIE relocation issue */
    const char *path;
    int count;

    /* Use argument if provided, otherwise list current directory */
    if (argc > 1) {
        path = argv[1];
    } else {
        path = getenv("CWD");
        if (path == (char *)0) path = "/";
    }

    count = readdir(path, entries, MAX_ENTRIES);
    if (count < 0) {
        printf("ls: cannot list '%s': error %d\n", path, count);
        return 1;
    }

    int i;
    for (i = 0; i < count; i++) {
        puts(entries[i].name);
    }

    return 0;
}
