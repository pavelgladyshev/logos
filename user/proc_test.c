/*
 * proc_test - Process management tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests fork semantics, wait, exec errors, getpid, kill, ps,
 * environment isolation, and process slot exhaustion.
 */

#include "libc.h"
#include "test_helpers.h"

/* Test 1: fork returns child PID to parent, 0 to child */
static void test_fork_return(void) {
    int pid;

    printf("\n--- Test 1: fork return values ---\n");

    pid = fork();
    if (pid < 0) { fail("fork return", "fork failed"); return; }

    if (pid == 0) {
        /* Child: fork returned 0 */
        exit(0);
    }

    /* Parent: fork returned child PID (> 0) */
    if (pid <= 0) { fail("fork return", "parent got non-positive PID"); wait(); return; }
    wait();
    pass("fork return");
}

/* Test 2: multiple children + wait collects all */
static void test_multi_wait(void) {
    int i, pid, collected;

    printf("\n--- Test 2: multiple children + wait ---\n");

    collected = 0;
    for (i = 0; i < 3; i++) {
        pid = fork();
        if (pid < 0) { fail("multi wait", "fork failed"); return; }
        if (pid == 0) {
            exit(10 + i);  /* Exit with 10, 11, 12 */
        }
    }

    /* Wait for all 3 */
    for (i = 0; i < 3; i++) {
        int code = wait();
        if (code >= 0) collected++;
    }

    if (collected == 3) {
        pass("multi wait");
    } else {
        fail("multi wait", "did not collect all children");
    }
}

/* Test 3: wait with no children returns -1 */
static void test_wait_no_children(void) {
    int r;

    printf("\n--- Test 3: wait with no children ---\n");

    r = wait();
    if (r == -1) {
        pass("wait no children");
    } else {
        fail("wait no children", "expected -1");
    }
}

/* Test 4: exec non-existent path returns -1 */
static void test_exec_noent(void) {
    int pid;

    printf("\n--- Test 4: exec non-existent ---\n");

    pid = fork();
    if (pid < 0) { fail("exec noent", "fork failed"); return; }

    if (pid == 0) {
        char *args[2];
        args[0] = "/no_such_program_xyz";
        args[1] = (char *)0;
        exec("/no_such_program_xyz", args);
        /* exec returns only on failure */
        exit(42);
    }

    {
        int code = wait();
        if (code == 42) {
            pass("exec noent");
        } else {
            fail("exec noent", "unexpected exit code");
        }
    }
}

/* Test 5: getpid differs between parent and child */
static void test_getpid_fork(void) {
    int parent_pid, pid;

    printf("\n--- Test 5: getpid in parent vs child ---\n");

    parent_pid = getpid();
    pid = fork();
    if (pid < 0) { fail("getpid fork", "fork failed"); return; }

    if (pid == 0) {
        int child_pid = getpid();
        /* Child PID should differ from parent */
        if (child_pid != parent_pid) {
            exit(0);
        } else {
            exit(1);
        }
    }

    {
        int code = wait();
        if (code == 0) {
            pass("getpid fork");
        } else {
            fail("getpid fork", "PIDs were the same");
        }
    }
}

/* Test 6: kill a child process */
static void test_kill_child(void) {
    int pid;

    printf("\n--- Test 6: kill child ---\n");

    pid = fork();
    if (pid < 0) { fail("kill child", "fork failed"); return; }

    if (pid == 0) {
        /* Child: loop forever (will be killed) */
        for (;;) { }
    }

    /* Parent: kill the child */
    {
        int r = kill(pid);
        if (r < 0) { fail("kill child", "kill returned error"); wait(); return; }
    }

    /* wait should return since child was killed */
    wait();
    pass("kill child");
}

/* Test 7: ps lists running processes */
static void test_ps_list(void) {
    struct proc_info info[8];
    int n, found_self;

    printf("\n--- Test 7: ps lists processes ---\n");

    n = ps(info, 8);
    if (n <= 0) { fail("ps list", "ps returned no processes"); return; }

    found_self = 0;
    {
        int my_pid = getpid();
        int i;
        for (i = 0; i < n; i++) {
            if (info[i].pid == my_pid) found_self = 1;
        }
    }

    if (found_self) {
        pass("ps list");
    } else {
        fail("ps list", "own PID not in ps output");
    }
}

/* Test 8: environment isolation across fork */
static void test_env_isolation(void) {
    int pid;

    printf("\n--- Test 8: env isolation ---\n");

    setenv("ISOL_TEST", "parent_val");

    pid = fork();
    if (pid < 0) { fail("env isolation", "fork failed"); return; }

    if (pid == 0) {
        /* Child modifies the variable */
        setenv("ISOL_TEST", "child_val");
        exit(0);
    }

    wait();

    /* Parent's value should be unchanged */
    {
        char *val = getenv("ISOL_TEST");
        if (val && strcmp(val, "parent_val") == 0) {
            pass("env isolation");
        } else {
            fail("env isolation", "parent env was modified");
        }
    }
    unsetenv("ISOL_TEST");
}

/* Test 9: child exit code passed via wait */
static void test_exit_code(void) {
    int pid;

    printf("\n--- Test 9: exit code via wait ---\n");

    pid = fork();
    if (pid < 0) { fail("exit code", "fork failed"); return; }

    if (pid == 0) {
        exit(99);
    }

    {
        int code = wait();
        if (code == 99) {
            pass("exit code");
        } else {
            fail("exit code", "wrong code returned");
        }
    }
}

/* Test 10: fork slot exhaustion */
static void test_fork_exhaustion(void) {
    int pids[8];
    int count = 0;
    int i, pid;

    printf("\n--- Test 10: fork slot exhaustion ---\n");

    /* Fork children until we can't anymore */
    for (i = 0; i < 8; i++) {
        pid = fork();
        if (pid < 0) break;  /* Slot exhaustion */
        if (pid == 0) {
            /* Child: wait to be killed */
            for (;;) { }
        }
        pids[count++] = pid;
    }

    if (count > 0 && pid < 0) {
        /* We hit the limit — that's the expected behavior */
        pass("fork exhaustion");
    } else if (count == 0) {
        fail("fork exhaustion", "couldn't fork at all");
    } else {
        fail("fork exhaustion", "never hit slot limit");
    }

    /* Clean up: kill all children */
    for (i = 0; i < count; i++) {
        kill(pids[i]);
    }
    for (i = 0; i < count; i++) {
        wait();
    }
}

int main(void) {
    printf("=== Process Management Tests ===\n");

    test_fork_return();
    test_multi_wait();
    test_wait_no_children();
    test_exec_noent();
    test_getpid_fork();
    test_kill_child();
    test_ps_list();
    test_env_isolation();
    test_exit_code();
    test_fork_exhaustion();

    print_test_results();
    return tests_failed;
}
