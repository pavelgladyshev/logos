/*
 * pipe_demo - Demonstrates pipe(), fork(), dup2(), and close() for IPC
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Test 1: Basic pipe read/write within one process
 * Test 2: Fork + pipe (parent writes, child reads, EOF detection)
 * Test 3: Fork + pipe + dup2 (child redirects stdout to pipe)
 */

#include "libc.h"

int main(void)
{
    int pipefd[2];
    int pid;

    printf("pipe_demo: testing pipe IPC\n\n");

    /* ---- Test 1: Single-process pipe ---- */
    printf("--- Test 1: single-process pipe ---\n");
    if (pipe(pipefd) < 0) {
        printf("FAIL: pipe() returned error\n");
        return 1;
    }
    printf("pipe created: read_fd=%d, write_fd=%d\n", pipefd[0], pipefd[1]);

    /* Write a message through the pipe */
    write(pipefd[1], "Hello!", 6);
    printf("wrote 6 bytes to pipe\n");

    /* Read it back */
    {
        char buf[32];
        int n;
        memset(buf, 0, sizeof(buf));
        n = read(pipefd[0], buf, sizeof(buf) - 1);
        printf("read %d bytes from pipe: '%s'\n", n, buf);
        if (n == 6 && strcmp(buf, "Hello!") == 0) {
            printf("PASS: single-process pipe works\n");
        } else {
            printf("FAIL: expected 6 bytes 'Hello!', got %d bytes '%s'\n", n, buf);
        }
    }

    /* Close both ends */
    close(pipefd[0]);
    close(pipefd[1]);

    /* ---- Test 2: Fork + pipe ---- */
    printf("\n--- Test 2: fork + pipe (parent writes, child reads) ---\n");
    if (pipe(pipefd) < 0) {
        printf("FAIL: pipe() returned error\n");
        return 1;
    }

    pid = fork();
    if (pid < 0) {
        printf("FAIL: fork() returned error\n");
        return 1;
    }

    if (pid == 0) {
        /* Child: close write end, read from pipe */
        close(pipefd[1]);
        {
            char buf[64];
            int n;
            memset(buf, 0, sizeof(buf));
            n = read(pipefd[0], buf, sizeof(buf) - 1);
            printf("child read %d bytes: '%s'\n", n, buf);

            /* Read again — should get EOF since parent will close write end */
            n = read(pipefd[0], buf, sizeof(buf) - 1);
            printf("child second read: %d (expected 0 = EOF)\n", n);
            if (n == 0) {
                printf("PASS: EOF detected\n");
            } else {
                printf("FAIL: expected EOF (0), got %d\n", n);
            }
        }
        close(pipefd[0]);
        exit(0);
    }

    /* Parent: close read end, write to pipe */
    close(pipefd[0]);
    write(pipefd[1], "Message from parent", 19);
    printf("parent wrote 19 bytes\n");

    /* Close write end — child will see EOF */
    close(pipefd[1]);

    /* Wait for child */
    {
        int status;
        status = wait();
        printf("parent: child exited with code %d\n", status);
    }

    /* ---- Test 3: Fork + pipe + dup2 ---- */
    printf("\n--- Test 3: pipe + dup2 (child redirects stdout) ---\n");
    if (pipe(pipefd) < 0) {
        printf("FAIL: pipe() returned error\n");
        return 1;
    }

    pid = fork();
    if (pid < 0) {
        printf("FAIL: fork() returned error\n");
        return 1;
    }

    if (pid == 0) {
        /* Child: redirect stdout to pipe write end */
        close(pipefd[0]);          /* Close read end in child */
        dup2(pipefd[1], 1);        /* stdout -> pipe write end */
        close(pipefd[1]);          /* Close original write fd */

        /* Write to stdout (which is now the pipe) */
        write(1, "piped output", 12);
        exit(0);
    }

    /* Parent: read from pipe read end */
    close(pipefd[1]);  /* Close write end in parent */
    {
        char buf[64];
        int n;
        memset(buf, 0, sizeof(buf));
        n = read(pipefd[0], buf, sizeof(buf) - 1);
        printf("parent read %d bytes from child's stdout: '%s'\n", n, buf);
        if (n == 12 && strcmp(buf, "piped output") == 0) {
            printf("PASS: dup2 pipe redirection works\n");
        } else {
            printf("FAIL: expected 12 bytes 'piped output', got %d '%s'\n", n, buf);
        }
    }
    close(pipefd[0]);
    wait();

    printf("\npipe_demo: all tests done!\n");
    return 0;
}
