/*
 * fork_demo - Demonstrates fork(), exec(), wait(), and getpid() syscalls
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "libc.h"

int main(void)
{
    int pid, child_exit;

    printf("fork_demo: my PID is %d\n", getpid());

    /* Test 1: fork + exec + wait */
    printf("\n--- Test 1: fork + exec + wait ---\n");
    pid = fork();
    if (pid < 0) {
        printf("fork failed!\n");
        return 1;
    }
    if (pid == 0) {
        /* Child process */
        char *argv[2];
        argv[0] = "hello";
        argv[1] = (char *)0;
        printf("Child (PID %d): about to exec /bin/hello\n", getpid());
        exec("/bin/hello", argv);
        /* exec failed if we get here */
        printf("Child: exec failed!\n");
        exit(1);
    }
    /* Parent process */
    printf("Parent (PID %d): forked child PID %d, waiting...\n", getpid(), pid);
    child_exit = wait();
    printf("Parent: child exited with code %d\n", child_exit);

    /* Test 2: fork without exec (child exits immediately) */
    printf("\n--- Test 2: fork + exit ---\n");
    pid = fork();
    if (pid < 0) {
        printf("fork failed!\n");
        return 1;
    }
    if (pid == 0) {
        /* Child exits with code 42 */
        printf("Child (PID %d): exiting with code 42\n", getpid());
        exit(42);
    }
    printf("Parent (PID %d): forked child PID %d, waiting...\n", getpid(), pid);
    child_exit = wait();
    printf("Parent: child exited with code %d\n", child_exit);

    /* Test 3: wait with no children */
    printf("\n--- Test 3: wait with no children ---\n");
    child_exit = wait();
    printf("wait() returned %d (expected -1)\n", child_exit);

    printf("\nfork_demo: all tests done!\n");
    return 0;
}
