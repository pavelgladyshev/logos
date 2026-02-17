/*
 * exec_demo - Demonstrates the exec() syscall
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Launches /bin/hello with arguments via exec().
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    int result;

    puts("=== exec_demo starting ===\n");
    printf("main() is at address 0x%x\n", (unsigned int)main);

    /* Build argv at runtime to avoid PIE relocation issues */
    char *hello_argv[5];
    hello_argv[0] = "hello";
    hello_argv[1] = "arg1";
    hello_argv[2] = "arg2";
    hello_argv[3] = "test argument 3";
    hello_argv[4] = (char *)0;

    puts("About to exec /bin/hello with arguments...\n");

    result = exec("/bin/hello", hello_argv);

    /* If we get here, exec failed */
    printf("exec failed with code %d\n", result);
    return 1;
}
