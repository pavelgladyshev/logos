/*
 * demo_env - Demonstrate environment variable API
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Shows getenv(), setenv(), unsetenv(), and env listing.
 * Run from the shell to see environment propagation:
 *   $ set FOO=hello
 *   $ demo_env
 */

#include "libc.h"

int main(void)
{
    char *envp[17];
    int count, i;

    puts("=== Environment Variable Demo ===\n\n");

    /* Show inherited environment */
    puts("Inherited environment:\n");
    count = env_to_envp(envp);
    for (i = 0; i < count; i++) {
        printf("  %s\n", envp[i]);
    }
    if (count == 0) {
        puts("  (none)\n");
    }

    /* Look up a specific variable */
    char *path = getenv("PATH");
    printf("\ngetenv(\"PATH\") = %s\n", path ? path : "(null)");

    /* Set a new variable */
    puts("\nCalling setenv(\"GREETING\", \"hello world\")...\n");
    setenv("GREETING", "hello world");

    char *greeting = getenv("GREETING");
    printf("getenv(\"GREETING\") = %s\n", greeting ? greeting : "(null)");

    /* List updated environment */
    puts("\nUpdated environment:\n");
    count = env_to_envp(envp);
    for (i = 0; i < count; i++) {
        printf("  %s\n", envp[i]);
    }

    /* Remove the variable */
    puts("\nCalling unsetenv(\"GREETING\")...\n");
    unsetenv("GREETING");

    puts("Final environment:\n");
    count = env_to_envp(envp);
    for (i = 0; i < count; i++) {
        printf("  %s\n", envp[i]);
    }

    return 0;
}
