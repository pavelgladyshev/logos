/*
 * pipe_test - Thorough pipe edge-case tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests pipe buffer boundaries, partial I/O, EPIPE, multiple pipes,
 * dup refcounting, and zero-length operations.
 *
 * PIPE_BUF_SIZE = 256 (kernel), MAX_PIPES = 8, MAX_FD = 16
 */

#include "libc.h"
#include "test_helpers.h"

/*
 * Test 1: Partial reads
 * Write 10 bytes, read 5, then read remaining 5.
 */
static void test_partial_reads(void)
{
    int pfd[2];
    char buf[32];
    int n;

    printf("\n--- Test 1: partial reads ---\n");
    if (pipe(pfd) < 0) { fail("partial reads", "pipe() failed"); return; }

    write(pfd[1], "ABCDEFGHIJ", 10);

    /* Read first 5 */
    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, 5);
    if (n != 5 || memcmp(buf, "ABCDE", 5) != 0) {
        fail("partial reads", "first read wrong");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    /* Read remaining 5 */
    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, 5);
    if (n != 5 || memcmp(buf, "FGHIJ", 5) != 0) {
        fail("partial reads", "second read wrong");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    close(pfd[0]);
    close(pfd[1]);
    pass("partial reads");
}

/*
 * Test 2: Buffer capacity
 * Fill the pipe to its 256-byte capacity, read it all back.
 */
static void test_buffer_capacity(void)
{
    int pfd[2];
    char wbuf[256];
    char rbuf[256];
    int i, n;

    printf("\n--- Test 2: buffer capacity (256 bytes) ---\n");
    if (pipe(pfd) < 0) { fail("buffer capacity", "pipe() failed"); return; }

    /* Fill write buffer with pattern */
    for (i = 0; i < 256; i++) {
        wbuf[i] = (char)(i & 0x7F);
    }

    /* Write 256 bytes (full buffer) */
    n = write(pfd[1], wbuf, 256);
    if (n != 256) {
        printf("  wrote %d bytes (expected 256)\n", n);
        fail("buffer capacity", "write did not fill buffer");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    /* Read all back */
    memset(rbuf, 0, sizeof(rbuf));
    n = read(pfd[0], rbuf, 256);
    if (n != 256) {
        printf("  read %d bytes (expected 256)\n", n);
        fail("buffer capacity", "read did not get all bytes");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    /* Verify data integrity */
    if (memcmp(wbuf, rbuf, 256) != 0) {
        fail("buffer capacity", "data mismatch");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    close(pfd[0]);
    close(pfd[1]);
    pass("buffer capacity");
}

/*
 * Test 3: EPIPE detection
 * Close read end, then write should return -1.
 */
static void test_epipe(void)
{
    int pfd[2];
    int n;

    printf("\n--- Test 3: EPIPE (write after reader closes) ---\n");
    if (pipe(pfd) < 0) { fail("EPIPE", "pipe() failed"); return; }

    /* Close read end */
    close(pfd[0]);

    /* Write should fail with EPIPE (-1) */
    n = write(pfd[1], "data", 4);
    if (n == -1) {
        pass("EPIPE");
    } else {
        printf("  write returned %d (expected -1)\n", n);
        fail("EPIPE", "write did not return -1");
    }

    close(pfd[1]);
}

/*
 * Test 4: Multiple pipes simultaneously
 * Create 2 pipes, write/read independently.
 */
static void test_multiple_pipes(void)
{
    int pfd1[2], pfd2[2];
    char buf[32];
    int n;

    printf("\n--- Test 4: multiple pipes ---\n");
    if (pipe(pfd1) < 0) { fail("multiple pipes", "pipe1 failed"); return; }
    if (pipe(pfd2) < 0) {
        fail("multiple pipes", "pipe2 failed");
        close(pfd1[0]); close(pfd1[1]);
        return;
    }

    write(pfd1[1], "pipe1", 5);
    write(pfd2[1], "pipe2", 5);

    memset(buf, 0, sizeof(buf));
    n = read(pfd2[0], buf, 31);
    if (n != 5 || strcmp(buf, "pipe2") != 0) {
        fail("multiple pipes", "pipe2 read wrong");
        close(pfd1[0]); close(pfd1[1]);
        close(pfd2[0]); close(pfd2[1]);
        return;
    }

    memset(buf, 0, sizeof(buf));
    n = read(pfd1[0], buf, 31);
    if (n != 5 || strcmp(buf, "pipe1") != 0) {
        fail("multiple pipes", "pipe1 read wrong");
        close(pfd1[0]); close(pfd1[1]);
        close(pfd2[0]); close(pfd2[1]);
        return;
    }

    close(pfd1[0]); close(pfd1[1]);
    close(pfd2[0]); close(pfd2[1]);
    pass("multiple pipes");
}

/*
 * Test 5: dup refcounting
 * dup() the write end, close original, write through dup'd fd.
 */
static void test_dup_refcount(void)
{
    int pfd[2];
    int dup_fd;
    char buf[32];
    int n;

    printf("\n--- Test 5: dup refcounting ---\n");
    if (pipe(pfd) < 0) { fail("dup refcount", "pipe() failed"); return; }

    /* dup the write end */
    dup_fd = dup(pfd[1]);
    if (dup_fd < 0) {
        fail("dup refcount", "dup() failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    /* Close original write fd */
    close(pfd[1]);

    /* Write through dup'd fd should still work (write_refs > 0) */
    n = write(dup_fd, "dup_ok", 6);
    if (n != 6) {
        printf("  write returned %d (expected 6)\n", n);
        fail("dup refcount", "write through dup failed");
        close(pfd[0]); close(dup_fd);
        return;
    }

    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, 31);
    if (n == 6 && strcmp(buf, "dup_ok") == 0) {
        pass("dup refcount");
    } else {
        fail("dup refcount", "read data mismatch");
    }

    close(pfd[0]);
    close(dup_fd);
}

/*
 * Test 6: EOF after all writers close (via dup refcounting)
 * dup write end, close both, reader should get EOF.
 */
static void test_eof_refcount(void)
{
    int pfd[2];
    int dup_fd;
    int pid, n;
    char buf[32];

    printf("\n--- Test 6: EOF after all writers close ---\n");
    if (pipe(pfd) < 0) { fail("EOF refcount", "pipe() failed"); return; }

    dup_fd = dup(pfd[1]);
    if (dup_fd < 0) {
        fail("EOF refcount", "dup() failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    pid = fork();
    if (pid < 0) {
        fail("EOF refcount", "fork() failed");
        close(pfd[0]); close(pfd[1]); close(dup_fd);
        return;
    }

    if (pid == 0) {
        /* Child: close read end, write once, then close both write fds */
        close(pfd[0]);
        write(pfd[1], "msg", 3);
        close(pfd[1]);
        close(dup_fd);
        exit(0);
    }

    /* Parent: close both write ends */
    close(pfd[1]);
    close(dup_fd);

    /* First read: get the data */
    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, 31);
    if (n != 3 || strcmp(buf, "msg") != 0) {
        printf("  first read: %d bytes '%s'\n", n, buf);
        fail("EOF refcount", "first read wrong");
        close(pfd[0]);
        wait();
        return;
    }

    /* Second read: should get EOF (0) since all writers are closed */
    n = read(pfd[0], buf, 31);
    if (n == 0) {
        pass("EOF refcount");
    } else {
        printf("  second read returned %d (expected 0 = EOF)\n", n);
        fail("EOF refcount", "did not get EOF");
    }

    close(pfd[0]);
    wait();
}

/*
 * Test 7: Write then close write end, reader gets data then EOF
 * Single-process: write, close write, read data, read EOF.
 */
static void test_write_close_eof(void)
{
    int pfd[2];
    char buf[32];
    int n;

    printf("\n--- Test 7: write, close write, read data + EOF ---\n");
    if (pipe(pfd) < 0) { fail("write-close-EOF", "pipe() failed"); return; }

    write(pfd[1], "XYZ", 3);
    close(pfd[1]);

    /* Should read the data */
    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, 31);
    if (n != 3 || strcmp(buf, "XYZ") != 0) {
        fail("write-close-EOF", "data read wrong");
        close(pfd[0]);
        return;
    }

    /* Should get EOF */
    n = read(pfd[0], buf, 31);
    if (n == 0) {
        pass("write-close-EOF");
    } else {
        printf("  read returned %d (expected 0 = EOF)\n", n);
        fail("write-close-EOF", "no EOF after data");
    }

    close(pfd[0]);
}

/*
 * Test 8: Multiple small writes, one large read
 */
static void test_coalesce_reads(void)
{
    int pfd[2];
    char buf[64];
    int n;

    printf("\n--- Test 8: multiple writes, one read ---\n");
    if (pipe(pfd) < 0) { fail("coalesce reads", "pipe() failed"); return; }

    write(pfd[1], "AA", 2);
    write(pfd[1], "BB", 2);
    write(pfd[1], "CC", 2);

    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, 63);
    if (n == 6 && strcmp(buf, "AABBCC") == 0) {
        pass("coalesce reads");
    } else {
        printf("  read %d bytes: '%s'\n", n, buf);
        fail("coalesce reads", "data mismatch");
    }

    close(pfd[0]);
    close(pfd[1]);
}

/*
 * Test 9: Read more than available
 * Write 3 bytes, read with larger buffer — should only return 3.
 */
static void test_read_more_than_available(void)
{
    int pfd[2];
    char buf[64];
    int n;

    printf("\n--- Test 9: read more than available ---\n");
    if (pipe(pfd) < 0) { fail("read>avail", "pipe() failed"); return; }

    write(pfd[1], "Hi!", 3);

    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, 64);
    if (n == 3 && strcmp(buf, "Hi!") == 0) {
        pass("read>avail");
    } else {
        printf("  read returned %d (expected 3)\n", n);
        fail("read>avail", "wrong byte count");
    }

    close(pfd[0]);
    close(pfd[1]);
}

/*
 * Test 10: dup2 with pipe — redirect stdout to pipe
 */
static void test_dup2_redirect(void)
{
    int pfd[2];
    int saved_stdout;
    char buf[64];
    int n;

    printf("\n--- Test 10: dup2 redirect stdout to pipe ---\n");
    if (pipe(pfd) < 0) { fail("dup2 redirect", "pipe() failed"); return; }

    /* Save original stdout */
    saved_stdout = dup(1);
    if (saved_stdout < 0) {
        fail("dup2 redirect", "dup(1) failed");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    /* Redirect stdout to pipe write end */
    dup2(pfd[1], 1);
    close(pfd[1]);  /* Close original write fd */

    /* Write to stdout (which is now the pipe) */
    write(1, "redirected", 10);

    /* Restore stdout */
    dup2(saved_stdout, 1);
    close(saved_stdout);

    /* Read from pipe */
    memset(buf, 0, sizeof(buf));
    n = read(pfd[0], buf, 63);
    if (n == 10 && strcmp(buf, "redirected") == 0) {
        pass("dup2 redirect");
    } else {
        printf("  read %d bytes: '%s'\n", n, buf);
        fail("dup2 redirect", "data mismatch");
    }

    close(pfd[0]);
}

int main(void)
{
    printf("pipe_test: comprehensive pipe tests\n");

    test_partial_reads();
    test_buffer_capacity();
    test_epipe();
    test_multiple_pipes();
    test_dup_refcount();
    test_eof_refcount();
    test_write_close_eof();
    test_coalesce_reads();
    test_read_more_than_available();
    test_dup2_redirect();

    print_test_results();

    return tests_failed;
}
