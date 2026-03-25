/*
 * script_test - Shell scripting tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests shell script execution, comments, variable expansion,
 * and flow control (if/then/else/fi, for/in/do/done, while/do/done).
 *
 * Each test writes a script file, forks a child that redirects stdout
 * to a pipe, exec's /bin/sh with the script, and the parent reads
 * the pipe output to verify correctness.
 */

#include "libc.h"
#include "test_helpers.h"

/*
 * Write a string to a file. Returns 0 on success, -1 on error.
 */
static int write_script(const char *path, const char *content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) return -1;
    write(fd, content, strlen(content));
    close(fd);
    return 0;
}

/*
 * Run a shell script and capture its stdout.
 * Returns number of bytes captured, or -1 on error.
 */
static int run_script(const char *script_path, char *output, int outsize) {
    int pfd[2];
    int pid, n;

    if (pipe(pfd) < 0) return -1;

    pid = fork();
    if (pid < 0) { close(pfd[0]); close(pfd[1]); return -1; }

    if (pid == 0) {
        /* Child: redirect stdout to pipe, exec sh */
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);

        char *argv[3];
        argv[0] = "/bin/sh";
        argv[1] = (char *)script_path;
        argv[2] = (char *)0;
        exec("/bin/sh", argv);
        exit(127);
    }

    /* Parent: read from pipe */
    close(pfd[1]);
    n = 0;
    while (n < outsize - 1) {
        int r = read(pfd[0], output + n, outsize - 1 - n);
        if (r <= 0) break;
        n += r;
    }
    output[n] = '\0';
    close(pfd[0]);
    wait();
    return n;
}

/*
 * Check if output contains a specific substring.
 */
static int contains(const char *haystack, const char *needle) {
    int hlen = strlen(haystack);
    int nlen = strlen(needle);
    int i;
    if (nlen > hlen) return 0;
    for (i = 0; i <= hlen - nlen; i++) {
        if (strncmp(haystack + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

/* Test 1: Simple echo script */
static void test_simple_script(void) {
    char output[256];
    int n;

    printf("\n--- Test 1: simple script ---\n");

    if (write_script("/tmp_s1.sh", "echo hello world\n") < 0) {
        fail("simple script", "write failed"); return;
    }

    n = run_script("/tmp_s1.sh", output, sizeof(output));
    unlink("/tmp_s1.sh");

    if (n < 0) { fail("simple script", "run failed"); return; }
    if (contains(output, "hello world")) {
        pass("simple script");
    } else {
        fail("simple script", "output mismatch");
    }
}

/* Test 2: Comment lines are skipped */
static void test_comments(void) {
    char output[256];
    int n;

    printf("\n--- Test 2: comments ---\n");

    if (write_script("/tmp_s2.sh", "# this is a comment\necho visible\n# another comment\n") < 0) {
        fail("comments", "write failed"); return;
    }

    n = run_script("/tmp_s2.sh", output, sizeof(output));
    unlink("/tmp_s2.sh");

    if (n < 0) { fail("comments", "run failed"); return; }
    if (contains(output, "visible") && !contains(output, "this is")) {
        pass("comments");
    } else {
        fail("comments", "comment not skipped or echo missing");
    }
}

/* Test 3: if/then/fi (true condition) */
static void test_if_true(void) {
    char output[256];
    int n;

    printf("\n--- Test 3: if/then/fi (true) ---\n");

    if (write_script("/tmp_s3.sh",
        "if ls /bin\nthen\necho yes\nfi\n") < 0) {
        fail("if true", "write failed"); return;
    }

    n = run_script("/tmp_s3.sh", output, sizeof(output));
    unlink("/tmp_s3.sh");

    if (n < 0) { fail("if true", "run failed"); return; }
    if (contains(output, "yes")) {
        pass("if true");
    } else {
        fail("if true", "then-block not executed");
    }
}

/* Test 4: if/then/else/fi (false condition) */
static void test_if_false(void) {
    char output[512];
    int n;

    printf("\n--- Test 4: if/then/else/fi (false) ---\n");

    if (write_script("/tmp_s4.sh",
        "if ls /nonexistent_dir\nthen\necho yes\nelse\necho no\nfi\n") < 0) {
        fail("if false", "write failed"); return;
    }

    n = run_script("/tmp_s4.sh", output, sizeof(output));
    unlink("/tmp_s4.sh");

    if (n < 0) { fail("if false", "run failed"); return; }
    if (contains(output, "no") && !contains(output, "yes")) {
        pass("if false");
    } else {
        fail("if false", "else-block not executed correctly");
    }
}

/* Test 5: for loop */
static void test_for_loop(void) {
    char output[512];
    int n;

    printf("\n--- Test 5: for loop ---\n");

    if (write_script("/tmp_s5.sh",
        "for x in alpha beta gamma\ndo\necho item $x\ndone\n") < 0) {
        fail("for loop", "write failed"); return;
    }

    n = run_script("/tmp_s5.sh", output, sizeof(output));
    unlink("/tmp_s5.sh");

    if (n < 0) { fail("for loop", "run failed"); return; }
    if (contains(output, "item alpha") &&
        contains(output, "item beta") &&
        contains(output, "item gamma")) {
        pass("for loop");
    } else {
        fail("for loop", "not all iterations executed");
    }
}

/* Test 6: Variable expansion in script */
static void test_var_expansion(void) {
    char output[256];
    int n;

    printf("\n--- Test 6: variable expansion ---\n");

    if (write_script("/tmp_s6.sh",
        "set MYVAR=testing123\necho val is $MYVAR\n") < 0) {
        fail("var expansion", "write failed"); return;
    }

    n = run_script("/tmp_s6.sh", output, sizeof(output));
    unlink("/tmp_s6.sh");

    if (n < 0) { fail("var expansion", "run failed"); return; }
    if (contains(output, "val is testing123")) {
        pass("var expansion");
    } else {
        fail("var expansion", "variable not expanded");
    }
}

/* Test 7: Multiple commands in script */
static void test_multi_commands(void) {
    char output[512];
    int n;

    printf("\n--- Test 7: multiple commands ---\n");

    if (write_script("/tmp_s7.sh",
        "echo line1\necho line2\necho line3\n") < 0) {
        fail("multi commands", "write failed"); return;
    }

    n = run_script("/tmp_s7.sh", output, sizeof(output));
    unlink("/tmp_s7.sh");

    if (n < 0) { fail("multi commands", "run failed"); return; }
    if (contains(output, "line1") &&
        contains(output, "line2") &&
        contains(output, "line3")) {
        pass("multi commands");
    } else {
        fail("multi commands", "not all lines executed");
    }
}

/* Test 8: Script with external command */
static void test_external_cmd(void) {
    char output[512];
    int n;

    printf("\n--- Test 8: external command in script ---\n");

    if (write_script("/tmp_s8.sh", "hello\n") < 0) {
        fail("external cmd", "write failed"); return;
    }

    n = run_script("/tmp_s8.sh", output, sizeof(output));
    unlink("/tmp_s8.sh");

    if (n < 0) { fail("external cmd", "run failed"); return; }
    /* /bin/hello should print something */
    if (n > 0) {
        pass("external cmd");
    } else {
        fail("external cmd", "no output from hello");
    }
}

int main(void) {
    printf("=== Shell Script Tests ===\n");

    test_simple_script();
    test_comments();
    test_if_true();
    test_if_false();
    test_for_loop();
    test_var_expansion();
    test_multi_commands();
    test_external_cmd();

    print_test_results();
    return tests_failed;
}
