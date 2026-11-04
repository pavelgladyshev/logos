/*
 * pipe_test - Thorough pipe edge-case tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests pipe buffer boundaries, partial I/O, EPIPE, multiple pipes,
 * dup refcounting, zero-length operations, writes larger than the buffer,
 * and atomic writes of at most PIPE_BUF bytes.
 *
 * PIPE_BUF = 512 (kernel PIPE_BUF_SIZE), MAX_PIPES = 8, MAX_FD = 16
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
 * Fill the pipe to its PIPE_BUF-byte capacity, read it all back.
 */
static void test_buffer_capacity(void)
{
    int pfd[2];
    char wbuf[PIPE_BUF];
    char rbuf[PIPE_BUF];
    int i, n;

    printf("\n--- Test 2: buffer capacity (%d bytes) ---\n", PIPE_BUF);
    if (pipe(pfd) < 0) { fail("buffer capacity", "pipe() failed"); return; }

    /* Fill write buffer with pattern */
    for (i = 0; i < PIPE_BUF; i++) {
        wbuf[i] = (char)(i & 0x7F);
    }

    /* Write PIPE_BUF bytes (full buffer) */
    n = write(pfd[1], wbuf, PIPE_BUF);
    if (n != PIPE_BUF) {
        printf("  wrote %d bytes (expected %d)\n", n, PIPE_BUF);
        fail("buffer capacity", "write did not fill buffer");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    /* Read all back */
    memset(rbuf, 0, sizeof(rbuf));
    n = read(pfd[0], rbuf, PIPE_BUF);
    if (n != PIPE_BUF) {
        printf("  read %d bytes (expected %d)\n", n, PIPE_BUF);
        fail("buffer capacity", "read did not get all bytes");
        close(pfd[0]); close(pfd[1]);
        return;
    }

    /* Verify data integrity */
    if (memcmp(wbuf, rbuf, PIPE_BUF) != 0) {
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

/*
 * Test 11: A write bigger than the pipe buffer is delivered in full.
 * The parent writes 1000 bytes in one call (the buffer holds PIPE_BUF) while a
 * child reads them in small pieces; write() must return 1000 and the child
 * must receive every byte in order.
 */
static void test_large_write(void)
{
    int pfd[2];
    static char wbuf[1000];
    int i, n, pid, status;

    printf("\n--- Test 11: write larger than the buffer ---\n");
    if (pipe(pfd) < 0) { fail("large write", "pipe() failed"); return; }
    for (i = 0; i < 1000; i++) wbuf[i] = (char)('A' + i % 26);

    pid = fork();
    if (pid < 0) { fail("large write", "fork() failed"); return; }
    if (pid == 0) {
        char rbuf[40];
        int total = 0, bad = 0, r, j;
        close(pfd[1]);
        while ((r = read(pfd[0], rbuf, sizeof(rbuf))) > 0) {
            for (j = 0; j < r; j++)
                if (rbuf[j] != (char)('A' + (total + j) % 26)) bad = 1;
            total += r;
        }
        close(pfd[0]);
        exit((total == 1000 && !bad) ? 0 : 1);
    }

    close(pfd[0]);
    n = write(pfd[1], wbuf, 1000);
    close(pfd[1]);
    status = wait();
    if (n != 1000) {
        printf("  write returned %d (expected 1000)\n", n);
        fail("large write", "short write");
    } else if (status != 0) {
        fail("large write", "reader did not receive all 1000 bytes in order");
    } else {
        pass("large write");
    }
}

/*
 * Test 12: The reader goes away in the middle of a large write.
 * The child reads 100 bytes and exits; the parent's 1000-byte write must
 * return the number of bytes delivered so far (not hang, not -1), and the
 * next write must fail with EPIPE (-1).
 */
static void test_large_write_epipe(void)
{
    int pfd[2];
    static char wbuf[1000];
    int n, n2, pid;

    printf("\n--- Test 12: reader exits during a large write ---\n");
    if (pipe(pfd) < 0) { fail("large write epipe", "pipe() failed"); return; }
    memset(wbuf, 'x', sizeof(wbuf));

    pid = fork();
    if (pid < 0) { fail("large write epipe", "fork() failed"); return; }
    if (pid == 0) {
        char rbuf[100];
        int got = 0, r;
        close(pfd[1]);
        while (got < 100 && (r = read(pfd[0], rbuf, 100 - got)) > 0) got += r;
        close(pfd[0]);
        exit(0);
    }

    close(pfd[0]);
    n = write(pfd[1], wbuf, 1000);
    n2 = write(pfd[1], wbuf, 10);
    close(pfd[1]);
    wait();
    if (n >= 100 && n < 1000 && n2 == -1) {
        printf("  delivered %d bytes before the reader left\n", n);
        pass("large write epipe");
    } else {
        printf("  write returned %d, then %d\n", n, n2);
        fail("large write epipe", "expected a partial count, then -1");
    }
}

/*
 * Test 13: Writes of at most PIPE_BUF bytes are atomic.
 * Two children each write three 300-byte messages ('A's and 'B's) into the
 * same pipe while the parent reads it in small pieces.  Every message must
 * arrive in one piece: the stream is six 300-byte runs of a single letter.
 */
static void test_atomic_writes(void)
{
    int pfd[2];
    static char stream[1800];
    char rbuf[50];
    int total = 0, r, c, k, i, ok;
    char letters[2] = { 'A', 'B' };

    printf("\n--- Test 13: atomic writes of <= PIPE_BUF bytes ---\n");
    if (pipe(pfd) < 0) { fail("atomic writes", "pipe() failed"); return; }

    for (c = 0; c < 2; c++) {
        int pid = fork();
        if (pid < 0) { fail("atomic writes", "fork() failed"); return; }
        if (pid == 0) {
            static char msg[300];
            close(pfd[0]);
            memset(msg, letters[c], sizeof(msg));
            for (k = 0; k < 3; k++)
                if (write(pfd[1], msg, sizeof(msg)) != (int)sizeof(msg)) exit(1);
            close(pfd[1]);
            exit(0);
        }
    }

    close(pfd[1]);
    while (total < (int)sizeof(stream) &&
           (r = read(pfd[0], rbuf, sizeof(rbuf))) > 0) {
        for (i = 0; i < r && total < (int)sizeof(stream); i++)
            stream[total++] = rbuf[i];
    }
    close(pfd[0]);
    wait();
    wait();

    ok = (total == 1800);
    for (k = 0; ok && k < 6; k++)
        for (i = 1; i < 300; i++)
            if (stream[k * 300 + i] != stream[k * 300]) { ok = 0; break; }
    if (ok) {
        pass("atomic writes");
    } else {
        printf("  got %d bytes; a message was split or mixed\n", total);
        fail("atomic writes", "300-byte messages interleaved");
    }
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
    test_large_write();
    test_large_write_epipe();
    test_atomic_writes();

    print_test_results();

    return tests_failed;
}
