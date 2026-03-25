/*
 * fs_test - Filesystem operation tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests file create/read/write, stat, mkdir/rmdir/readdir,
 * chdir with relative paths, and file truncation.
 */

#include "libc.h"
#include "test_helpers.h"

#define FT_FILE 1
#define FT_DIR  2

/* Test 1: Create file, write, close, reopen, read, verify */
static void test_create_write_read(void) {
    int fd, n;
    char buf[64];

    printf("\n--- Test 1: create + write + read ---\n");

    fd = open("/tmp_crwr.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { fail("create+write+read", "open for write failed"); return; }

    n = write(fd, "hello world", 11);
    if (n != 11) { fail("create+write+read", "write returned wrong count"); close(fd); return; }
    close(fd);

    fd = open("/tmp_crwr.txt", O_RDONLY);
    if (fd < 0) { fail("create+write+read", "open for read failed"); return; }

    n = read(fd, buf, sizeof(buf));
    close(fd);

    if (n != 11) { fail("create+write+read", "read returned wrong count"); return; }
    buf[n] = '\0';
    if (strcmp(buf, "hello world") != 0) { fail("create+write+read", "content mismatch"); return; }

    unlink("/tmp_crwr.txt");
    pass("create+write+read");
}

/* Test 2: File append */
static void test_append(void) {
    int fd, n;
    char buf[64];

    printf("\n--- Test 2: file append ---\n");

    fd = open("/tmp_app.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { fail("append", "initial create failed"); return; }
    write(fd, "aaa", 3);
    close(fd);

    fd = open("/tmp_app.txt", O_WRONLY | O_APPEND);
    if (fd < 0) { fail("append", "open for append failed"); return; }
    write(fd, "bbb", 3);
    close(fd);

    fd = open("/tmp_app.txt", O_RDONLY);
    if (fd < 0) { fail("append", "open for read failed"); return; }
    n = read(fd, buf, sizeof(buf));
    close(fd);

    if (n != 6) { fail("append", "expected 6 bytes"); return; }
    buf[n] = '\0';
    if (strcmp(buf, "aaabbb") != 0) { fail("append", "content mismatch"); return; }

    unlink("/tmp_app.txt");
    pass("append");
}

/* Test 3: Truncate on reopen */
static void test_truncate(void) {
    int fd, n;
    char buf[64];

    printf("\n--- Test 3: truncate on reopen ---\n");

    fd = open("/tmp_trunc.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { fail("truncate", "initial create failed"); return; }
    write(fd, "long content here", 17);
    close(fd);

    /* Reopen with O_TRUNC — file should be emptied */
    fd = open("/tmp_trunc.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { fail("truncate", "truncate reopen failed"); return; }
    write(fd, "short", 5);
    close(fd);

    fd = open("/tmp_trunc.txt", O_RDONLY);
    if (fd < 0) { fail("truncate", "read after truncate failed"); return; }
    n = read(fd, buf, sizeof(buf));
    close(fd);

    if (n != 5) { fail("truncate", "expected 5 bytes"); return; }
    buf[n] = '\0';
    if (strcmp(buf, "short") != 0) { fail("truncate", "content mismatch"); return; }

    unlink("/tmp_trunc.txt");
    pass("truncate");
}

/* Test 4: Read past EOF returns 0 */
static void test_read_eof(void) {
    int fd, n;
    char buf[64];

    printf("\n--- Test 4: read past EOF ---\n");

    fd = open("/tmp_eof.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { fail("read EOF", "create failed"); return; }
    write(fd, "abc", 3);
    close(fd);

    fd = open("/tmp_eof.txt", O_RDONLY);
    if (fd < 0) { fail("read EOF", "open failed"); return; }

    n = read(fd, buf, sizeof(buf));  /* Read all 3 bytes */
    if (n != 3) { fail("read EOF", "first read wrong"); close(fd); return; }

    n = read(fd, buf, sizeof(buf));  /* Should return 0 (EOF) */
    close(fd);

    if (n != 0) { fail("read EOF", "expected 0 at EOF"); return; }

    unlink("/tmp_eof.txt");
    pass("read EOF");
}

/* Test 5: stat on file */
static void test_stat_file(void) {
    int fd, r;
    struct stat_info si;

    printf("\n--- Test 5: stat on file ---\n");

    fd = open("/tmp_stat.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { fail("stat file", "create failed"); return; }
    write(fd, "data", 4);
    close(fd);

    r = stat("/tmp_stat.txt", &si);
    if (r < 0) { fail("stat file", "stat failed"); unlink("/tmp_stat.txt"); return; }
    if (si.type != FT_FILE) { fail("stat file", "wrong type"); unlink("/tmp_stat.txt"); return; }
    if (si.size != 4) { fail("stat file", "wrong size"); unlink("/tmp_stat.txt"); return; }
    if (si.link_count != 1) { fail("stat file", "wrong link_count"); unlink("/tmp_stat.txt"); return; }

    unlink("/tmp_stat.txt");
    pass("stat file");
}

/* Test 6: stat on directory */
static void test_stat_dir(void) {
    struct stat_info si;
    int r;

    printf("\n--- Test 6: stat on directory ---\n");

    r = stat("/bin", &si);
    if (r < 0) { fail("stat dir", "stat /bin failed"); return; }
    if (si.type != FT_DIR) { fail("stat dir", "wrong type"); return; }

    pass("stat dir");
}

/* Test 7: stat on non-existent path */
static void test_stat_noent(void) {
    struct stat_info si;
    int r;

    printf("\n--- Test 7: stat on non-existent ---\n");

    r = stat("/no_such_file_xyz", &si);
    if (r >= 0) { fail("stat noent", "should have failed"); return; }

    pass("stat noent");
}

/* Test 8: mkdir + readdir */
static void test_mkdir_readdir(void) {
    int r, n, found;
    struct dirent entries[32];

    printf("\n--- Test 8: mkdir + readdir ---\n");

    r = mkdir("/tmp_dir");
    if (r < 0) { fail("mkdir+readdir", "mkdir failed"); return; }

    /* readdir on new empty dir */
    n = readdir("/tmp_dir", entries, 32);
    if (n != 0) { fail("mkdir+readdir", "new dir not empty"); rmdir("/tmp_dir"); return; }

    /* readdir on root should include tmp_dir */
    n = readdir("/", entries, 32);
    if (n <= 0) { fail("mkdir+readdir", "root readdir failed"); rmdir("/tmp_dir"); return; }

    found = 0;
    {
        int i;
        for (i = 0; i < n; i++) {
            if (strcmp(entries[i].name, "tmp_dir") == 0) found = 1;
        }
    }
    if (!found) { fail("mkdir+readdir", "tmp_dir not in root listing"); rmdir("/tmp_dir"); return; }

    rmdir("/tmp_dir");
    pass("mkdir+readdir");
}

/* Test 9: rmdir empty directory */
static void test_rmdir_empty(void) {
    int r;

    printf("\n--- Test 9: rmdir empty ---\n");

    mkdir("/tmp_rm");
    r = rmdir("/tmp_rm");
    if (r < 0) { fail("rmdir empty", "rmdir failed"); return; }

    /* Verify it's gone */
    {
        struct stat_info si;
        if (stat("/tmp_rm", &si) >= 0) { fail("rmdir empty", "dir still exists"); return; }
    }

    pass("rmdir empty");
}

/* Test 10: rmdir non-empty fails */
static void test_rmdir_notempty(void) {
    int fd, r;

    printf("\n--- Test 10: rmdir non-empty ---\n");

    mkdir("/tmp_rne");
    fd = open("/tmp_rne/file.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) close(fd);

    r = rmdir("/tmp_rne");
    if (r >= 0) { fail("rmdir notempty", "should have failed"); return; }

    /* Clean up */
    unlink("/tmp_rne/file.txt");
    rmdir("/tmp_rne");
    pass("rmdir notempty");
}

/* Test 11: chdir + relative path open */
static void test_chdir_relative(void) {
    int fd;

    printf("\n--- Test 11: chdir + relative path ---\n");

    if (chdir("/etc") < 0) { fail("chdir relative", "chdir /etc failed"); return; }

    fd = open("hello.txt", O_RDONLY);
    if (fd < 0) { fail("chdir relative", "open relative failed"); chdir("/"); return; }
    close(fd);

    chdir("/");
    pass("chdir relative");
}

/* Test 12: open non-existent file without O_CREAT fails */
static void test_open_noent(void) {
    int fd;

    printf("\n--- Test 12: open non-existent ---\n");

    fd = open("/no_such_file_abc", O_RDONLY);
    if (fd >= 0) { fail("open noent", "should have failed"); close(fd); return; }

    pass("open noent");
}

int main(void) {
    printf("=== Filesystem Tests ===\n");

    test_create_write_read();
    test_append();
    test_truncate();
    test_read_eof();
    test_stat_file();
    test_stat_dir();
    test_stat_noent();
    test_mkdir_readdir();
    test_rmdir_empty();
    test_rmdir_notempty();
    test_chdir_relative();
    test_open_noent();

    print_test_results();
    return tests_failed;
}
