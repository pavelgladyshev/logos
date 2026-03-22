/*
 * fd_test - File descriptor management edge-case tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests close/dup/dup2 edge cases, invalid fd operations,
 * fd reuse after close, and fd inheritance across fork.
 */

#include "libc.h"
#include "test_helpers.h"

/*
 * Test 1: Close and reuse fd 0
 * Close stdin (fd 0), then pipe() should allocate fd 0 as the lowest free.
 */
static void test_close_reuse_fd0(void)
{
    int pfd[2];
    int saved_stdin;

    printf("\n--- Test 1: close fd 0, pipe reuses it ---\n");

    /* Save stdin so we can restore it later */
    saved_stdin = dup(0);
    if (saved_stdin < 0) {
        fail("close reuse fd0", "dup(0) failed");
        return;
    }

    /* Close stdin */
    close(0);

    /* Create pipe — should get fd 0 as read end (lowest free) */
    if (pipe(pfd) < 0) {
        fail("close reuse fd0", "pipe() failed");
        dup2(saved_stdin, 0);
        close(saved_stdin);
        return;
    }

    if (pfd[0] == 0) {
        pass("close reuse fd0");
    } else {
        printf("  pfd[0]=%d (expected 0)\n", pfd[0]);
        fail("close reuse fd0", "fd 0 not reused");
    }

    /* Clean up: close pipe, restore stdin */
    close(pfd[0]);
    close(pfd[1]);
    dup2(saved_stdin, 0);
    close(saved_stdin);
}

/*
 * Test 2: dup returns lowest available fd
 */
static void test_dup_lowest_fd(void)
{
    int pfd[2];
    int d1, d2;

    printf("\n--- Test 2: dup returns lowest available fd ---\n");
    if (pipe(pfd) < 0) { fail("dup lowest", "pipe() failed"); return; }

    /* dup should return lowest available (probably 5 since 0-2 are console, 3-4 are pipe) */
    d1 = dup(pfd[0]);
    d2 = dup(pfd[0]);

    if (d1 >= 0 && d2 >= 0 && d2 == d1 + 1) {
        printf("  dup returned %d, %d (consecutive)\n", d1, d2);
        pass("dup lowest");
    } else {
        printf("  dup returned %d, %d\n", d1, d2);
        fail("dup lowest", "fds not consecutive");
    }

    close(d1);
    close(d2);
    close(pfd[0]);
    close(pfd[1]);
}

/*
 * Test 3: dup2 with same fd (no-op)
 */
static void test_dup2_same_fd(void)
{
    int result;

    printf("\n--- Test 3: dup2(fd, fd) is a no-op ---\n");
    result = dup2(1, 1);
    if (result == 1) {
        pass("dup2 same fd");
    } else {
        printf("  dup2(1,1) returned %d (expected 1)\n", result);
        fail("dup2 same fd", "wrong return value");
    }
}

/*
 * Test 4: Close invalid fd
 */
static void test_close_invalid_fd(void)
{
    int r1, r2, r3;

    printf("\n--- Test 4: close invalid fd ---\n");
    r1 = close(-1);
    r2 = close(99);
    r3 = close(15);  /* MAX_FD-1, likely not open */

    if (r1 == -1 && r2 == -1) {
        pass("close invalid fd");
    } else {
        printf("  close(-1)=%d, close(99)=%d, close(15)=%d\n", r1, r2, r3);
        fail("close invalid fd", "should return -1");
    }
}

/*
 * Test 5: Read/write on closed fd
 */
static void test_rw_closed_fd(void)
{
    int pfd[2];
    char buf[8];
    int r, w;

    printf("\n--- Test 5: read/write on closed fd ---\n");
    if (pipe(pfd) < 0) { fail("rw closed fd", "pipe() failed"); return; }

    /* Remember the fd numbers, then close them */
    int rfd = pfd[0];
    int wfd = pfd[1];
    close(pfd[0]);
    close(pfd[1]);

    /* Operations on closed fds should fail */
    r = read(rfd, buf, 8);
    w = write(wfd, "test", 4);

    if (r == -1 && w == -1) {
        pass("rw closed fd");
    } else {
        printf("  read=%d, write=%d (expected -1, -1)\n", r, w);
        fail("rw closed fd", "should return -1");
    }
}

/*
 * Test 6: dup2 replaces open fd (close + copy)
 * dup2(pipe_write, 1) should close original stdout, then copy pipe_write to fd 1.
 */
static void test_dup2_replace(void)
{
    int pfd[2];
    int saved_stdout;
    char buf[64];
    int n;

    printf("\n--- Test 6: dup2 replaces open fd ---\n");
    if (pipe(pfd) < 0) { fail("dup2 replace", "pipe() failed"); return; }

    saved_stdout = dup(1);
    if (saved_stdout < 0) {
        fail("dup2 replace", "dup(1) failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    /* Replace stdout with pipe write end */
    dup2(pfd[1], 1);
    close(pfd[1]);

    /* Write to fd 1 (now the pipe) */
    write(1, "replaced!", 9);

    /* Restore stdout */
    dup2(saved_stdout, 1);
    close(saved_stdout);

    /* Read from pipe to verify */
    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, 63);
    close(pfd[0]);

    if (n == 9 && strcmp(buf, "replaced!") == 0) {
        pass("dup2 replace");
    } else {
        printf("  read %d bytes: '%s'\n", n, buf);
        fail("dup2 replace", "data mismatch");
    }
}

/*
 * Test 7: dup on invalid fd
 */
static void test_dup_invalid_fd(void)
{
    int r1, r2;

    printf("\n--- Test 7: dup/dup2 on invalid fd ---\n");
    r1 = dup(-1);
    r2 = dup(15);  /* likely not open */

    if (r1 == -1 && r2 == -1) {
        pass("dup invalid fd");
    } else {
        printf("  dup(-1)=%d, dup(15)=%d\n", r1, r2);
        fail("dup invalid fd", "should return -1");
    }
}

/*
 * Test 8: fd inheritance across fork
 * Parent opens pipe, writes data, forks.
 * Child should inherit the pipe fds and read the data.
 */
static void test_fd_inherit_fork(void)
{
    int pfd[2];
    int pid;

    printf("\n--- Test 8: fd inheritance across fork ---\n");
    if (pipe(pfd) < 0) { fail("fd inherit fork", "pipe() failed"); return; }

    write(pfd[1], "inherited", 9);

    pid = fork();
    if (pid < 0) {
        fail("fd inherit fork", "fork() failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    if (pid == 0) {
        /* Child: should have same fds as parent */
        char buf[32];
        int n;
        close(pfd[1]);
        memset(buf, 0, sizeof(buf));
        n = read(pfd[0], buf, 31);
        close(pfd[0]);
        if (n == 9 && strcmp(buf, "inherited") == 0) {
            exit(0);
        }
        exit(1);
    }

    /* Parent */
    close(pfd[0]);
    close(pfd[1]);

    {
        int status;
        status = wait();
        if (status == 0) {
            pass("fd inherit fork");
        } else {
            printf("  child exited with %d\n", status);
            fail("fd inherit fork", "child read wrong data");
        }
    }
}

/*
 * Test 9: Close all stdio fds, open pipe, verify pipe gets low fds
 */
static void test_fd_allocation_order(void)
{
    int saved0, saved1, saved2;
    int pfd[2];

    printf("\n--- Test 9: fd allocation after closing stdio ---\n");

    /* Save stdio fds */
    saved0 = dup(0);
    saved1 = dup(1);
    saved2 = dup(2);

    if (saved0 < 0 || saved1 < 0 || saved2 < 0) {
        fail("fd alloc order", "could not save stdio");
        if (saved0 >= 0) close(saved0);
        if (saved1 >= 0) close(saved1);
        if (saved2 >= 0) close(saved2);
        return;
    }

    /* Close all stdio */
    close(0);
    close(1);
    close(2);

    /* Pipe should allocate fds 0 and 1 */
    if (pipe(pfd) < 0) {
        /* Can't printf since stdout is closed, just restore and report */
        dup2(saved0, 0);
        dup2(saved1, 1);
        dup2(saved2, 2);
        close(saved0); close(saved1); close(saved2);
        printf("  pipe() failed after closing stdio\n");
        fail("fd alloc order", "pipe() failed");
        return;
    }

    /* Check allocations before restoring stdio */
    int r0 = pfd[0];
    int r1 = pfd[1];

    /* Close pipe before restoring stdio */
    close(pfd[0]);
    close(pfd[1]);

    /* Restore stdio */
    dup2(saved0, 0);
    dup2(saved1, 1);
    dup2(saved2, 2);
    close(saved0);
    close(saved1);
    close(saved2);

    /* Now we can safely printf */
    if (r0 == 0 && r1 == 1) {
        pass("fd alloc order");
    } else {
        printf("  pfd[0]=%d, pfd[1]=%d (expected 0, 1)\n", r0, r1);
        fail("fd alloc order", "pipe did not get lowest fds");
    }
}

/*
 * Test 10: Open file, close it, open pipe — verify fd reuse
 */
static void test_fd_reuse_after_file(void)
{
    int fd, pfd[2];

    printf("\n--- Test 10: fd reuse after closing file ---\n");

    /* Open a file to get an fd */
    fd = open("/etc/hello.txt", 0);
    if (fd < 0) {
        fail("fd reuse", "open() failed");
        return;
    }
    printf("  opened file on fd %d\n", fd);

    /* Close it */
    close(fd);

    /* Open pipe — the read end should reuse the closed fd */
    if (pipe(pfd) < 0) {
        fail("fd reuse", "pipe() failed");
        return;
    }

    if (pfd[0] == fd) {
        printf("  pipe reused fd %d\n", fd);
        pass("fd reuse");
    } else {
        printf("  pipe got fd %d (expected %d)\n", pfd[0], fd);
        /* This isn't necessarily wrong if other fds were free too */
        if (pfd[0] <= fd) {
            pass("fd reuse");
        } else {
            fail("fd reuse", "fd not reused");
        }
    }

    close(pfd[0]);
    close(pfd[1]);
}

int main(void)
{
    printf("fd_test: file descriptor management tests\n");

    test_close_reuse_fd0();
    test_dup_lowest_fd();
    test_dup2_same_fd();
    test_close_invalid_fd();
    test_rw_closed_fd();
    test_dup2_replace();
    test_dup_invalid_fd();
    test_fd_inherit_fork();
    test_fd_allocation_order();
    test_fd_reuse_after_file();

    print_test_results();

    return tests_failed;
}
