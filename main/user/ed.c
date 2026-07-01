/*
 * ed - minimal line editor
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Commands:
 *   o <file>  - Open file, read into buffer
 *   p         - Print all lines with line numbers
 *   <n>p      - Print line n
 *   <n>i      - Insert before line n (enter lines, end with ".")
 *   <n>a      - Append after line n (enter lines, end with ".")
 *   <n>d      - Delete line n
 *   w <file>  - Write buffer to file
 *   q         - Quit
 */

#include "libc.h"

#define MAX_LINES  48
#define LINE_LEN   80
#define CMD_LEN    80

static char lines[MAX_LINES][LINE_LEN];
static int nlines = 0;

static int parse_num(const char *s, int *num)
{
    int n = 0;
    int found = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
        found = 1;
    }
    *num = n;
    return found;
}

static void cmd_open(const char *filename)
{
    int fd;
    char buf[LINE_LEN];
    int col;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("? cannot open '%s'\n", filename);
        return;
    }

    nlines = 0;
    col = 0;
    while (nlines < MAX_LINES) {
        char c;
        int n = read(fd, &c, 1);
        if (n <= 0) {
            /* EOF — flush any partial line */
            if (col > 0) {
                buf[col] = '\0';
                strncpy(lines[nlines], buf, LINE_LEN - 1);
                lines[nlines][LINE_LEN - 1] = '\0';
                nlines++;
            }
            break;
        }
        if (c == '\n') {
            buf[col] = '\0';
            strncpy(lines[nlines], buf, LINE_LEN - 1);
            lines[nlines][LINE_LEN - 1] = '\0';
            nlines++;
            col = 0;
        } else if (col < LINE_LEN - 1) {
            buf[col++] = c;
        }
    }

    close(fd);
    printf("%d lines\n", nlines);
}

static void cmd_write(const char *filename)
{
    int fd, i;

    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        printf("? cannot write '%s'\n", filename);
        return;
    }

    for (i = 0; i < nlines; i++) {
        int len = strlen(lines[i]);
        write(fd, lines[i], len);
        write(fd, "\n", 1);
    }

    close(fd);
    printf("%d lines written\n", nlines);
}

static void cmd_print(int from, int to)
{
    int i;
    for (i = from; i <= to && i < nlines; i++) {
        printf("%3d: %s\n", i + 1, lines[i]);
    }
}

static void cmd_insert(int pos)
{
    char buf[LINE_LEN];

    if (pos < 0) pos = 0;
    if (pos > nlines) pos = nlines;

    while (nlines < MAX_LINES) {
        gets(buf, LINE_LEN);
        if (buf[0] == '.' && buf[1] == '\0') break;
        /* Shift lines down */
        int i;
        for (i = nlines; i > pos; i--) {
            strcpy(lines[i], lines[i - 1]);
        }
        strncpy(lines[pos], buf, LINE_LEN - 1);
        lines[pos][LINE_LEN - 1] = '\0';
        nlines++;
        pos++;
    }
}

static void cmd_delete(int linenum)
{
    int i;
    if (linenum < 0 || linenum >= nlines) {
        puts("? line out of range");
        return;
    }
    for (i = linenum; i < nlines - 1; i++) {
        strcpy(lines[i], lines[i + 1]);
    }
    nlines--;
}

int main(void)
{
    char cmd[CMD_LEN];

    puts("ed: type 'h' for help");

    for (;;) {
        write(STDOUT_FILENO, ": ", 2);
        gets(cmd, CMD_LEN);

        if (cmd[0] == '\0') continue;

        if (cmd[0] == 'q') {
            break;
        } else if (cmd[0] == 'h') {
            puts("o <file> - open file");
            puts("p        - print all lines");
            puts("<n>p     - print line n");
            puts("<n>i     - insert before line n");
            puts("<n>a     - append after line n");
            puts("<n>d     - delete line n");
            puts("w <file> - write to file");
            puts("q        - quit");
        } else if (cmd[0] == 'o' && cmd[1] == ' ') {
            cmd_open(cmd + 2);
        } else if (cmd[0] == 'w' && cmd[1] == ' ') {
            cmd_write(cmd + 2);
        } else if (cmd[0] == 'p') {
            cmd_print(0, nlines - 1);
        } else {
            /* Parse optional line number */
            int num = 0;
            int has_num = parse_num(cmd, &num);
            const char *c = cmd;
            while (*c >= '0' && *c <= '9') c++;

            if (*c == 'p' && has_num) {
                cmd_print(num - 1, num - 1);
            } else if (*c == 'i' && has_num) {
                cmd_insert(num - 1);
            } else if (*c == 'a' && has_num) {
                cmd_insert(num);
            } else if (*c == 'd' && has_num) {
                cmd_delete(num - 1);
            } else {
                puts("? unknown command");
            }
        }
    }

    return 0;
}
