#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "shell.h"


static void read_line(char *buf, int size)
{
    int i = 0;

    while (i < size - 1) {
        int c = getchar();

        if (c == '\r' || c == '\n') {
            putchar('\n');
            break;
        }

        if ((c == '\b' || c == 127) && i > 0) {
            i--;
            printf("\b \b");
            fflush(stdout);
            continue;
        }

        if (c >= 32 && c <= 126) {
            buf[i++] = (char)c;
            putchar(c);
            fflush(stdout);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    buf[i] = '\0';
}

void logos_shell_run(void)
{
    char line[128];

    printf("\nlogOS shell\n");

    while (1) {
        printf("logOS> ");
        fflush(stdout);

        read_line(line, sizeof(line));

        if (strcmp(line, "help") == 0) {
            printf("help ls cat clear\n");
        } else if (strcmp(line, "clear") == 0) {
            printf("\033[2J\033[H");
        } else if (strcmp(line, "") == 0) {
            continue;
        } else {
            printf("unknown command: %s\n", line);
        }
    }
}
