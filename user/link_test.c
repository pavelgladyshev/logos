/*
 * link_test - Hard link, unlink, and rename tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests hard link creation, link count tracking, unlink semantics,
 * rename within and across directories, and error cases.
 */

#include "libc.h"
#include "test_helpers.h"

#define FT_FILE 1

/* Helper: create a file with given content */
static int create_file(const char *path, const char *content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) return -1;
    write(fd, content, strlen(content));
    close(fd);
    return 0;
}

/* Helper: read file content into buffer, return length */
static int read_file(const char *path, char *buf, int bufsize) {
    int fd = open(path, O_RDONLY);
    int n;
    if (fd < 0) return -1;
    n = read(fd, buf, bufsize - 1);
    close(fd);
    if (n < 0) n = 0;
    buf[n] = '\0';
    return n;
}

/* Test 1: Basic hard link — both paths read same content */
static void test_basic_link(void) {
    char buf[64];
    int r;

    printf("\n--- Test 1: basic hard link ---\n");

    if (create_file("/lk_orig.txt", "shared data") < 0) {
        fail("basic link", "create original failed"); return;
    }

    r = link("/lk_orig.txt", "/lk_link.txt");
    if (r < 0) { fail("basic link", "link() failed"); unlink("/lk_orig.txt"); return; }

    /* Read via both paths */
    if (read_file("/lk_orig.txt", buf, sizeof(buf)) < 0) {
        fail("basic link", "read original failed"); goto cleanup1;
    }
    if (strcmp(buf, "shared data") != 0) {
        fail("basic link", "original content wrong"); goto cleanup1;
    }

    if (read_file("/lk_link.txt", buf, sizeof(buf)) < 0) {
        fail("basic link", "read link failed"); goto cleanup1;
    }
    if (strcmp(buf, "shared data") != 0) {
        fail("basic link", "link content wrong"); goto cleanup1;
    }

    pass("basic link");
cleanup1:
    unlink("/lk_link.txt");
    unlink("/lk_orig.txt");
}

/* Test 2: Link count tracking via stat */
static void test_link_count(void) {
    struct stat_info si;
    int r;

    printf("\n--- Test 2: link count ---\n");

    if (create_file("/lk_cnt.txt", "data") < 0) {
        fail("link count", "create failed"); return;
    }

    r = stat("/lk_cnt.txt", &si);
    if (r < 0 || si.link_count != 1) {
        fail("link count", "initial count not 1"); unlink("/lk_cnt.txt"); return;
    }

    link("/lk_cnt.txt", "/lk_cnt2.txt");
    r = stat("/lk_cnt.txt", &si);
    if (r < 0 || si.link_count != 2) {
        fail("link count", "count not 2 after link"); goto cleanup2;
    }

    link("/lk_cnt.txt", "/lk_cnt3.txt");
    r = stat("/lk_cnt.txt", &si);
    if (r < 0 || si.link_count != 3) {
        fail("link count", "count not 3 after second link"); goto cleanup2b;
    }

    pass("link count");
cleanup2b:
    unlink("/lk_cnt3.txt");
cleanup2:
    unlink("/lk_cnt2.txt");
    unlink("/lk_cnt.txt");
}

/* Test 3: Unlink one of two links — file survives */
static void test_unlink_one(void) {
    char buf[64];
    struct stat_info si;
    int r;

    printf("\n--- Test 3: unlink one of two ---\n");

    if (create_file("/lk_u1.txt", "survive") < 0) {
        fail("unlink one", "create failed"); return;
    }
    link("/lk_u1.txt", "/lk_u2.txt");

    /* Unlink the original */
    unlink("/lk_u1.txt");

    /* File should still be accessible via the link */
    if (read_file("/lk_u2.txt", buf, sizeof(buf)) < 0) {
        fail("unlink one", "read via link failed"); unlink("/lk_u2.txt"); return;
    }
    if (strcmp(buf, "survive") != 0) {
        fail("unlink one", "content wrong"); unlink("/lk_u2.txt"); return;
    }

    /* Link count should be 1 */
    r = stat("/lk_u2.txt", &si);
    if (r < 0 || si.link_count != 1) {
        fail("unlink one", "link count not 1"); unlink("/lk_u2.txt"); return;
    }

    /* Original path should not exist */
    if (stat("/lk_u1.txt", &si) >= 0) {
        fail("unlink one", "original path still exists"); unlink("/lk_u2.txt"); return;
    }

    unlink("/lk_u2.txt");
    pass("unlink one");
}

/* Test 4: Unlink sole reference — file freed */
static void test_unlink_sole(void) {
    struct stat_info si;

    printf("\n--- Test 4: unlink sole reference ---\n");

    if (create_file("/lk_sole.txt", "gone") < 0) {
        fail("unlink sole", "create failed"); return;
    }

    unlink("/lk_sole.txt");

    /* File should not exist anymore */
    if (stat("/lk_sole.txt", &si) >= 0) {
        fail("unlink sole", "file still exists");
    } else {
        pass("unlink sole");
    }
}

/* Test 5: Rename file within directory */
static void test_rename_same_dir(void) {
    char buf[64];
    struct stat_info si;

    printf("\n--- Test 5: rename within dir ---\n");

    if (create_file("/rn_old.txt", "moved data") < 0) {
        fail("rename same dir", "create failed"); return;
    }

    if (rename("/rn_old.txt", "/rn_new.txt") < 0) {
        fail("rename same dir", "rename failed"); unlink("/rn_old.txt"); return;
    }

    /* Old path should not exist */
    if (stat("/rn_old.txt", &si) >= 0) {
        fail("rename same dir", "old path still exists"); unlink("/rn_new.txt"); return;
    }

    /* New path should have the content */
    if (read_file("/rn_new.txt", buf, sizeof(buf)) < 0) {
        fail("rename same dir", "read new path failed"); unlink("/rn_new.txt"); return;
    }
    if (strcmp(buf, "moved data") != 0) {
        fail("rename same dir", "content mismatch"); unlink("/rn_new.txt"); return;
    }

    unlink("/rn_new.txt");
    pass("rename same dir");
}

/* Test 6: Rename across directories */
static void test_rename_across_dirs(void) {
    char buf[64];
    struct stat_info si;

    printf("\n--- Test 6: rename across dirs ---\n");

    mkdir("/rn_src");
    mkdir("/rn_dst");

    if (create_file("/rn_src/file.txt", "cross dir") < 0) {
        fail("rename across", "create failed"); goto cleanup6;
    }

    if (rename("/rn_src/file.txt", "/rn_dst/file.txt") < 0) {
        fail("rename across", "rename failed"); unlink("/rn_src/file.txt"); goto cleanup6;
    }

    /* Verify source gone, dest has content */
    if (stat("/rn_src/file.txt", &si) >= 0) {
        fail("rename across", "source still exists"); goto cleanup6;
    }

    if (read_file("/rn_dst/file.txt", buf, sizeof(buf)) < 0) {
        fail("rename across", "read dest failed"); goto cleanup6;
    }
    if (strcmp(buf, "cross dir") != 0) {
        fail("rename across", "content mismatch"); goto cleanup6;
    }

    pass("rename across");
cleanup6:
    unlink("/rn_dst/file.txt");
    unlink("/rn_src/file.txt");
    rmdir("/rn_dst");
    rmdir("/rn_src");
}

/* Test 7: Link to non-existent target fails */
static void test_link_noent(void) {
    int r;

    printf("\n--- Test 7: link to non-existent ---\n");

    r = link("/no_such_file_xyz", "/lk_noent.txt");
    if (r < 0) {
        pass("link noent");
    } else {
        fail("link noent", "should have failed");
        unlink("/lk_noent.txt");
    }
}

/* Test 8: Unlink non-existent file fails */
static void test_unlink_noent(void) {
    int r;

    printf("\n--- Test 8: unlink non-existent ---\n");

    r = unlink("/no_such_file_xyz");
    if (r < 0) {
        pass("unlink noent");
    } else {
        fail("unlink noent", "should have failed");
    }
}

int main(void) {
    printf("=== Hard Link & Rename Tests ===\n");

    test_basic_link();
    test_link_count();
    test_unlink_one();
    test_unlink_sole();
    test_rename_same_dir();
    test_rename_across_dirs();
    test_link_noent();
    test_unlink_noent();

    print_test_results();
    return tests_failed;
}
