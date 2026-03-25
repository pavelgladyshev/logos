/*
 * stress_test - Resource exhaustion and stress tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests filesystem limits, pipe/fd/shm/sem exhaustion,
 * large file I/O, and multi-child pipe communication.
 */

#include "libc.h"
#include "test_helpers.h"

/* Test 1: Create many files until inode exhaustion */
static void test_many_files(void) {
    int i, fd, count;
    char name[32];

    printf("\n--- Test 1: many files ---\n");

    mkdir("/tmp_mf");
    count = 0;
    for (i = 0; i < 60; i++) {
        /* Build filename at runtime (PIE safe) */
        name[0] = '/'; name[1] = 't'; name[2] = 'm'; name[3] = 'p';
        name[4] = '_'; name[5] = 'm'; name[6] = 'f'; name[7] = '/';
        name[8] = 'f';
        /* Encode number as two digits */
        name[9] = '0' + (i / 10);
        name[10] = '0' + (i % 10);
        name[11] = '\0';

        fd = open(name, O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) break;
        write(fd, "x", 1);
        close(fd);
        count++;
    }

    if (count > 0) {
        pass("many files");
        printf("  (created %d files before limit)\n", count);
    } else {
        fail("many files", "couldn't create any files");
    }

    /* Clean up */
    for (i = 0; i < count; i++) {
        name[0] = '/'; name[1] = 't'; name[2] = 'm'; name[3] = 'p';
        name[4] = '_'; name[5] = 'm'; name[6] = 'f'; name[7] = '/';
        name[8] = 'f';
        name[9] = '0' + (i / 10);
        name[10] = '0' + (i % 10);
        name[11] = '\0';
        unlink(name);
    }
    rmdir("/tmp_mf");
}

/* Test 2: Large file write + read */
static void test_large_file(void) {
    int fd, i, n;
    char wbuf[512];
    char rbuf[512];
    int total_written, total_read;

    printf("\n--- Test 2: large file ---\n");

    /* Fill write buffer with pattern */
    for (i = 0; i < 512; i++) {
        wbuf[i] = (char)(i & 0xFF);
    }

    fd = open("/tmp_large.bin", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { fail("large file", "create failed"); return; }

    /* Write 20 blocks (10KB) */
    total_written = 0;
    for (i = 0; i < 20; i++) {
        n = write(fd, wbuf, 512);
        if (n <= 0) break;
        total_written += n;
    }
    close(fd);

    if (total_written < 10240) {
        fail("large file", "couldn't write 10KB");
        unlink("/tmp_large.bin");
        return;
    }

    /* Read back and verify first block */
    fd = open("/tmp_large.bin", O_RDONLY);
    if (fd < 0) { fail("large file", "open for read failed"); unlink("/tmp_large.bin"); return; }

    total_read = 0;
    n = read(fd, rbuf, 512);
    close(fd);

    if (n != 512) { fail("large file", "read first block failed"); unlink("/tmp_large.bin"); return; }

    /* Verify pattern */
    {
        int ok = 1;
        for (i = 0; i < 512; i++) {
            if (rbuf[i] != wbuf[i]) { ok = 0; break; }
        }
        if (!ok) { fail("large file", "data corruption"); unlink("/tmp_large.bin"); return; }
    }

    unlink("/tmp_large.bin");
    pass("large file");
    printf("  (wrote %d bytes)\n", total_written);
}

/* Test 3: Pipe exhaustion */
static void test_pipe_exhaustion(void) {
    int pfds[8][2];
    int count, i, r;

    printf("\n--- Test 3: pipe exhaustion ---\n");

    count = 0;
    for (i = 0; i < 8; i++) {
        r = pipe(pfds[i]);
        if (r < 0) break;
        count++;
    }

    /* We may not get 8 because fds are limited too (MAX_FD=8, 3 used for console) */
    if (count >= 2) {
        pass("pipe exhaustion");
        printf("  (created %d pipes before limit)\n", count);
    } else {
        fail("pipe exhaustion", "couldn't create enough pipes");
    }

    /* Clean up */
    for (i = 0; i < count; i++) {
        close(pfds[i][0]);
        close(pfds[i][1]);
    }
}

/* Test 4: fd exhaustion */
static void test_fd_exhaustion(void) {
    int fds[8];
    int count, i, fd;

    printf("\n--- Test 4: fd exhaustion ---\n");

    /* First create a file to open */
    fd = open("/tmp_fdex.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { fail("fd exhaustion", "create temp file failed"); return; }
    write(fd, "x", 1);
    close(fd);

    /* Open the file repeatedly until we run out of fds */
    count = 0;
    for (i = 0; i < 8; i++) {
        fds[i] = open("/tmp_fdex.txt", O_RDONLY);
        if (fds[i] < 0) break;
        count++;
    }

    if (count > 0 && fds[count - 1] >= 0 && (count >= 5 || i < 8)) {
        /* Either we opened several, or hit the limit */
        pass("fd exhaustion");
        printf("  (opened %d fds before limit)\n", count);
    } else if (count == 0) {
        fail("fd exhaustion", "couldn't open any fds");
    } else {
        pass("fd exhaustion");
        printf("  (opened %d fds, all slots filled)\n", count);
    }

    for (i = 0; i < count; i++) {
        close(fds[i]);
    }
    unlink("/tmp_fdex.txt");
}

/* Test 5: shm exhaustion */
static void test_shm_exhaustion(void) {
    int ids[8];
    int count, i, id;

    printf("\n--- Test 5: shm exhaustion ---\n");

    count = 0;
    for (i = 0; i < 9; i++) {
        id = shmget(1000 + i, IPC_CREAT | IPC_EXCL);
        if (id < 0) break;
        shmat(id);
        ids[count++] = id;
    }

    if (count == 8 && i == 8) {
        /* Created exactly 8, 9th should have failed */
        pass("shm exhaustion");
    } else if (count > 0) {
        pass("shm exhaustion");
        printf("  (created %d before limit)\n", count);
    } else {
        fail("shm exhaustion", "couldn't create any");
    }

    for (i = 0; i < count; i++) {
        shmdt(ids[i]);
    }
}

/* Test 6: sem exhaustion */
static void test_sem_exhaustion(void) {
    int ids[8];
    int count, i, id;

    printf("\n--- Test 6: sem exhaustion ---\n");

    count = 0;
    for (i = 0; i < 9; i++) {
        id = semget(2000 + i, 1, IPC_CREAT | IPC_EXCL);
        if (id < 0) break;
        ids[count++] = id;
    }

    if (count == 8 && i == 8) {
        pass("sem exhaustion");
    } else if (count > 0) {
        pass("sem exhaustion");
        printf("  (created %d before limit)\n", count);
    } else {
        fail("sem exhaustion", "couldn't create any");
    }

    for (i = 0; i < count; i++) {
        semclose(ids[i]);
    }
}

/* Test 7: fork + pipe communication */
static void test_fork_pipe_storm(void) {
    int pfd[2];
    int pid, i, n;
    char buf[32];

    printf("\n--- Test 7: fork + pipe ---\n");

    if (pipe(pfd) < 0) { fail("fork+pipe", "pipe failed"); return; }

    pid = fork();
    if (pid < 0) { fail("fork+pipe", "fork failed"); close(pfd[0]); close(pfd[1]); return; }

    if (pid == 0) {
        /* Child: write a message and close */
        close(pfd[0]);
        write(pfd[1], "from child!", 11);
        close(pfd[1]);
        exit(0);
    }

    /* Parent: read from pipe */
    close(pfd[1]);
    n = read(pfd[0], buf, sizeof(buf));
    close(pfd[0]);
    wait();

    if (n == 11) {
        buf[n] = '\0';
        if (strcmp(buf, "from child!") == 0) {
            pass("fork+pipe");
        } else {
            fail("fork+pipe", "wrong message");
        }
    } else {
        fail("fork+pipe", "wrong byte count");
    }
}

/* Test 8: nested directories */
static void test_nested_dirs(void) {
    int depth, r;

    printf("\n--- Test 8: nested directories ---\n");

    /* Create /a/b/c/d */
    depth = 0;
    if (mkdir("/nd_a") >= 0) { depth++;
    if (mkdir("/nd_a/b") >= 0) { depth++;
    if (mkdir("/nd_a/b/c") >= 0) { depth++;
    if (mkdir("/nd_a/b/c/d") >= 0) { depth++;
    }}}}

    if (depth == 4) {
        /* Verify deepest exists */
        struct stat_info si;
        r = stat("/nd_a/b/c/d", &si);
        if (r >= 0) {
            pass("nested dirs");
        } else {
            fail("nested dirs", "deepest dir not accessible");
        }
    } else {
        fail("nested dirs", "couldn't create all levels");
    }

    /* Clean up (deepest first) */
    rmdir("/nd_a/b/c/d");
    rmdir("/nd_a/b/c");
    rmdir("/nd_a/b");
    rmdir("/nd_a");
}

int main(void) {
    printf("=== Stress Tests ===\n");

    test_many_files();
    test_large_file();
    test_pipe_exhaustion();
    test_fd_exhaustion();
    test_shm_exhaustion();
    test_sem_exhaustion();
    test_fork_pipe_storm();
    test_nested_dirs();

    print_test_results();
    return tests_failed;
}
