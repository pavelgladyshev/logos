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
        struct stat_info st;
        char fullpath[128];
        int plen;

        /* Build full path: dir + "/" + name */
        plen = 0;
        {
            const char *s = path;
            while (*s && plen < 126) fullpath[plen++] = *s++;
        }
        /* Add separator if path doesn't end with / */
        if (plen > 0 && fullpath[plen - 1] != '/' && plen < 126)
            fullpath[plen++] = '/';
        {
            const char *s = entries[i].name;
            while (*s && plen < 127) fullpath[plen++] = *s++;
        }
        fullpath[plen] = '\0';

        if (stat(fullpath, &st) == 0) {
            char *type;
            if (st.type == 2)      type = "d";
            else if (st.type == 3) type = "c";
            else                   type = "-";
            printf("%s %6u %s\n", type, st.size, entries[i].name);
        } else {
            printf("? %6s %s\n", "?", entries[i].name);
        }
    }

    return 0;
}
