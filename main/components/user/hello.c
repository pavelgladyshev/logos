/*
 * Hello World - Example user program using system calls
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "libc.h"

int main(int argc, char *argv[])
{
    int i;

    puts("Hello from user program!\n");
    printf("main() is at address 0x%x\n", (unsigned int)main);
    printf("argc = %d\n", argc);

    for (i = 0; i < argc; i++) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }

    puts("Calling exit syscall...\n");
    return 42;
}
