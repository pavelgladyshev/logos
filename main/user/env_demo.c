/*
 * env_demo - Demonstrate environment variable API
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Shows getenv(), setenv(), unsetenv(), and env listing.
 * Run from the shell to see environment propagation:
 *   $ set FOO=hello
 *   $ env_demo
 */

#include "libc.h"

int main(void)
{
    char *envp[17];
    int count, i;

    puts("=== Environment Variable Demo ===");
    puts("");

    /* Show inherited environment */
    puts("Inherited environment:");
    count = env_to_envp(envp);
    for (i = 0; i < count; i++) {
        printf("  %s\n", envp[i]);
    }
    if (count == 0) {
        puts("  (none)");
    }

    /* Look up a specific variable */
    char *path = getenv("PATH");
    printf("\ngetenv(\"PATH\") = %s\n", path ? path : "(null)");

    /* Set a new variable */
    printf("\n");
    puts("Calling setenv(\"GREETING\", \"hello world\")...");
    setenv("GREETING", "hello world");

    char *greeting = getenv("GREETING");
    printf("getenv(\"GREETING\") = %s\n", greeting ? greeting : "(null)");

    /* List updated environment */
    printf("\n");
    puts("Updated environment:");
    count = env_to_envp(envp);
    for (i = 0; i < count; i++) {
        printf("  %s\n", envp[i]);
    }

    /* Remove the variable */
    printf("\n");
    puts("Calling unsetenv(\"GREETING\")...");
    unsetenv("GREETING");

    puts("Final environment:");
    count = env_to_envp(envp);
    for (i = 0; i < count; i++) {
        printf("  %s\n", envp[i]);
    }

    return 0;
}
