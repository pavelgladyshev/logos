/*
 * spawn_demo - Demonstrates the spawn() syscall
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Launches /bin/hello with arguments via spawn().
 * spawn() returns the child's exit code on success, or negative on failure.
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    int result;

    puts("=== spawn_demo starting ===");
    printf("main() is at address 0x%x\n", (unsigned int)main);

    /* Build argv at runtime to avoid PIE relocation issues */
    char *hello_argv[5];
    hello_argv[0] = "hello";
    hello_argv[1] = "arg1";
    hello_argv[2] = "arg2";
    hello_argv[3] = "test argument 3";
    hello_argv[4] = (char *)0;

    puts("About to spawn /bin/hello with arguments...");

    result = spawn("/bin/hello", hello_argv);

    if (result < 0) {
        printf("spawn failed with code %d\n", result);
        return 1;
    }

    printf("=== spawn_demo: child exited with code %d ===\n", result);
    return 0;
}
