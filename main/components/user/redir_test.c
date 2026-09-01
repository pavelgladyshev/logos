/*
 * redir_test - Tests pipe redirection with fork+dup2+exec
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Exercises the shell pipeline pattern:
 *   parent creates pipe, forks, child redirects stdout/stdin via dup2,
 *   child execs a program, parent reads/writes through pipe.
 *
 * Test 1: Capture child's stdout (child execs hello, parent reads output)
 * Test 2: Feed child's stdin (parent writes, child reads from redirected stdin)
 * Test 3: Two-stage pipeline (writer -> pipe -> reader)
 */

#include "libc.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void pass(const char *name) {
    printf("  PASS: %s\n", name);
    tests_passed++;
}

static void fail(const char *name, const char *detail) {
    printf("  FAIL: %s - %s\n", name, detail);
    tests_failed++;
}

/*
 * Test 1: Capture child stdout via pipe
 * Child redirects stdout to pipe, writes a known string, exits.
 * Parent reads from pipe and verifies.
 */
static void test_capture_stdout(void)
{
    int pfd[2];
    int pid, n;
    char buf[128];

    printf("\n--- Test 1: capture child stdout ---\n");
    if (pipe(pfd) < 0) { fail("capture stdout", "pipe() failed"); return; }

    pid = fork();
    if (pid < 0) {
        fail("capture stdout", "fork() failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    if (pid == 0) {
        /* Child: redirect stdout to pipe write end */
        close(pfd[0]);           /* Close read end in child */
        dup2(pfd[1], 1);         /* stdout -> pipe write end */
        close(pfd[1]);           /* Close original write fd */

        /* Write to stdout (goes through pipe) */
        write(1, "child says hello", 16);
        exit(0);
    }

    /* Parent: close write end, read from pipe */
    close(pfd[1]);

    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, sizeof(buf) - 1);
    if (n == 16 && strcmp(buf, "child says hello") == 0) {
        pass("capture stdout");
    } else {
        printf("  read %d bytes: '%s'\n", n, buf);
        fail("capture stdout", "wrong output");
    }

    close(pfd[0]);
    wait();
}

/*
 * Test 2: Feed child stdin via pipe
 * Parent writes data to pipe, child reads from stdin (redirected from pipe).
 */
static void test_feed_stdin(void)
{
    int pfd[2];
    int pid;

    printf("\n--- Test 2: feed child stdin ---\n");
    if (pipe(pfd) < 0) { fail("feed stdin", "pipe() failed"); return; }

    pid = fork();
    if (pid < 0) {
        fail("feed stdin", "fork() failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    if (pid == 0) {
        /* Child: redirect stdin from pipe read end */
        close(pfd[1]);           /* Close write end in child */
        dup2(pfd[0], 0);         /* stdin -> pipe read end */
        close(pfd[0]);           /* Close original read fd */

        /* Read from stdin (which is now the pipe) */
        {
            char buf[64];
            int n;
            memset(buf, 0, sizeof(buf));
            n = read(0, buf, sizeof(buf) - 1);
            if (n == 11 && strcmp(buf, "from parent") == 0) {
                /* Signal success via exit code */
                exit(0);
            } else {
                exit(1);
            }
        }
    }

    /* Parent: close read end, write to pipe */
    close(pfd[0]);
    write(pfd[1], "from parent", 11);
    close(pfd[1]);  /* Close write end so child gets data then EOF */

    {
        int status;
        status = wait();
        if (status == 0) {
            pass("feed stdin");
        } else {
            printf("  child exited with %d (expected 0)\n", status);
            fail("feed stdin", "child read wrong data");
        }
    }
}

/*
 * Test 3: Two-process pipeline (writer -> pipe -> reader)
 * Simulates: writer | reader
 * Writer child writes to stdout (pipe), reader child reads from stdin (pipe).
 */
static void test_pipeline(void)
{
    int pfd[2];
    int writer_pid, reader_pid;
    int status;

    printf("\n--- Test 3: two-process pipeline ---\n");
    if (pipe(pfd) < 0) { fail("pipeline", "pipe() failed"); return; }

    /* Fork writer child */
    writer_pid = fork();
    if (writer_pid < 0) {
        fail("pipeline", "fork writer failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    if (writer_pid == 0) {
        /* Writer child: stdout -> pipe write end */
        close(pfd[0]);
        dup2(pfd[1], 1);
        close(pfd[1]);
        write(1, "pipeline_data", 13);
        exit(0);
    }

    /* Fork reader child */
    reader_pid = fork();
    if (reader_pid < 0) {
        fail("pipeline", "fork reader failed");
        close(pfd[0]); close(pfd[1]);
        wait();  /* Reap writer */
        return;
    }

    if (reader_pid == 0) {
        /* Reader child: stdin -> pipe read end */
        close(pfd[1]);
        dup2(pfd[0], 0);
        close(pfd[0]);

        {
            char buf[64];
            int n;
            memset(buf, 0, sizeof(buf));
            n = read(0, buf, sizeof(buf) - 1);
            if (n == 13 && strcmp(buf, "pipeline_data") == 0) {
                exit(0);
            } else {
                exit(1);
            }
        }
    }

    /* Parent: close both pipe ends (children have their own copies) */
    close(pfd[0]);
    close(pfd[1]);

    /* Wait for both children */
    status = wait();
    {
        int status2;
        status2 = wait();
        /* Either child could finish first; check both exit codes */
        if (status == 0 && status2 == 0) {
            pass("pipeline");
        } else {
            printf("  child exit codes: %d, %d\n", status, status2);
            fail("pipeline", "child(ren) failed");
        }
    }
}

/*
 * Test 4: Fork preserves pipe fds
 * Parent creates pipe, writes, forks. Child reads what parent wrote.
 */
static void test_fork_preserves_pipe(void)
{
    int pfd[2];
    int pid;

    printf("\n--- Test 4: fork preserves pipe fds ---\n");
    if (pipe(pfd) < 0) { fail("fork preserves pipe", "pipe() failed"); return; }

    /* Parent writes before fork */
    write(pfd[1], "pre-fork", 8);

    pid = fork();
    if (pid < 0) {
        fail("fork preserves pipe", "fork() failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    if (pid == 0) {
        /* Child: close write end, read what parent wrote before fork */
        close(pfd[1]);
        {
            char buf[32];
            int n;
            memset(buf, 0, sizeof(buf));
            n = read(pfd[0], buf, 31);
            if (n == 8 && strcmp(buf, "pre-fork") == 0) {
                exit(0);
            } else {
                exit(1);
            }
        }
    }

    /* Parent: close both ends, wait for child */
    close(pfd[0]);
    close(pfd[1]);

    {
        int status;
        status = wait();
        if (status == 0) {
            pass("fork preserves pipe");
        } else {
            printf("  child exited with %d\n", status);
            fail("fork preserves pipe", "child read wrong data");
        }
    }
}

/*
 * Test 5: exec preserves pipe fds
 * Parent creates pipe, forks. Child dup2's stdout to pipe, execs hello.
 * Parent reads hello's output from pipe.
 *
 * Note: hello writes to stdout, which child has redirected to the pipe.
 * Since exec preserves fds, the pipe redirection survives.
 */
static void test_exec_preserves_pipe(void)
{
    int pfd[2];
    int pid, n;
    char buf[256];

    printf("\n--- Test 5: exec preserves pipe fds ---\n");
    if (pipe(pfd) < 0) { fail("exec preserves pipe", "pipe() failed"); return; }

    pid = fork();
    if (pid < 0) {
        fail("exec preserves pipe", "fork() failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    if (pid == 0) {
        /* Child: redirect stdout to pipe, exec hello */
        char *argv[2];
        close(pfd[0]);
        dup2(pfd[1], 1);
        close(pfd[1]);

        argv[0] = "hello";
        argv[1] = (char *)0;
        exec("/bin/hello", argv);
        /* If exec fails, exit with error */
        exit(127);
    }

    /* Parent: close write end, read from pipe */
    close(pfd[1]);

    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, sizeof(buf) - 1);
    close(pfd[0]);

    wait();

    if (n > 0) {
        printf("  captured %d bytes from hello\n", n);
        pass("exec preserves pipe");
    } else {
        printf("  read returned %d\n", n);
        fail("exec preserves pipe", "no output captured");
    }
}

int main(void)
{
    printf("redir_test: pipe redirection tests\n");

    test_capture_stdout();
    test_feed_stdin();
    test_pipeline();
    test_fork_preserves_pipe();
    test_exec_preserves_pipe();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed;
}
