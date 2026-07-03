#include <stddef.h>
#include <string.h>

#include "kernel_time.h"
#include "shell.h"
#include "user_syscall.h"

/* Shell output goes through the user syscall shim, not directly to ROM UART. */
static void shell_write(const char *s)
{
    logos_write(1, s, strlen(s));
}

/* Blocking line editor for the current single-threaded shell. */
static void read_line(char *buf, int size)
{
    int i = 0;

    while (i < size - 1) {
        int c = logos_getchar();

        if (c == '\r' || c == '\n') {
            logos_putchar('\n');
            break;
        }

        /* Basic terminal backspace handling. */
        if ((c == '\b' || c == 127) && i > 0) {
            i--;
            shell_write("\b \b");
            continue;
        }

        /* Echo printable ASCII as it is accepted into the line buffer. */
        if (c >= 32 && c <= 126) {
            buf[i++] = (char)c;
            logos_putchar(c);
        }

        kernel_delay_ms(10);
    }

    buf[i] = '\0';
}

void logos_shell_run(void)
{
    char line[128];

    shell_write("\nlogOS shell\n");

    while (1) {
        shell_write("logOS> ");

        read_line(line, sizeof(line));

        /* Built-ins are intentionally tiny until process loading is wired in. */
        if (strcmp(line, "help") == 0) {
            shell_write("help ls cat clear\n");
        } else if (strcmp(line, "clear") == 0) {
            shell_write("\033[2J\033[H");
        } else if (strcmp(line, "") == 0) {
            continue;
        } else {
            shell_write("unknown command: ");
            shell_write(line);
            shell_write("\n");
        }
    }
}
