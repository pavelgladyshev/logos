/*
 * shm_test - Shared memory tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests shmget, shmat, shmdt, fork with shared memory,
 * and refcount cleanup on exit.
 */

#include "libc.h"
#include "test_helpers.h"

/* Test 1: Basic create and attach */
static void test_basic_create(void) {
    int id;
    int *ptr;

    printf("\n--- Test 1: basic create and attach ---\n");

    id = shmget(42, IPC_CREAT);
    if (id < 0) { fail("basic create", "shmget failed"); return; }

    ptr = (int *)shmat(id);
    if (ptr == (void *)0) { fail("basic create", "shmat failed"); return; }

    /* Write and read back */
    ptr[0] = 12345;
    if (ptr[0] != 12345) { fail("basic create", "read/write mismatch"); shmdt(id); return; }

    shmdt(id);
    pass("basic create");
}

/* Test 2: IPC_EXCL flag */
static void test_excl(void) {
    int id1, id2;

    printf("\n--- Test 2: IPC_EXCL ---\n");

    id1 = shmget(100, IPC_CREAT);
    if (id1 < 0) { fail("IPC_EXCL", "first shmget failed"); return; }

    /* Should fail with IPC_CREAT | IPC_EXCL on same key */
    id2 = shmget(100, IPC_CREAT | IPC_EXCL);
    if (id2 >= 0) { fail("IPC_EXCL", "should have failed"); shmdt(id1); return; }

    /* Should succeed without IPC_EXCL on same key */
    id2 = shmget(100, IPC_CREAT);
    if (id2 != id1) { fail("IPC_EXCL", "should return same id"); shmdt(id1); return; }

    shmdt(id1);
    pass("IPC_EXCL");
}

/* Test 3: Lookup without IPC_CREAT fails for unknown key */
static void test_lookup_fail(void) {
    int id;

    printf("\n--- Test 3: lookup without IPC_CREAT ---\n");

    id = shmget(9999, 0);  /* No IPC_CREAT, key doesn't exist */
    if (id >= 0) { fail("lookup fail", "should have returned -1"); return; }

    pass("lookup fail");
}

/* Test 4: Fork shares memory */
static void test_fork_shared(void) {
    int id;
    int *ptr;
    int pid;

    printf("\n--- Test 4: fork shares memory ---\n");

    id = shmget(200, IPC_CREAT);
    if (id < 0) { fail("fork shared", "shmget failed"); return; }

    ptr = (int *)shmat(id);
    if (ptr == (void *)0) { fail("fork shared", "shmat failed"); return; }

    ptr[0] = 0;  /* Initial value */

    pid = fork();
    if (pid < 0) {
        fail("fork shared", "fork failed");
        shmdt(id);
        return;
    }

    if (pid == 0) {
        /* Child: write to shared memory */
        ptr[0] = 9999;
        exit(0);
    }

    /* Parent: wait for child, then check */
    wait();
    if (ptr[0] == 9999) {
        pass("fork shared");
    } else {
        fail("fork shared", "child write not visible");
    }

    shmdt(id);
}

/* Test 5: Detach and re-create with same key */
static void test_reuse_key(void) {
    int id1, id2;
    int *ptr;

    printf("\n--- Test 5: reuse key after detach ---\n");

    id1 = shmget(300, IPC_CREAT);
    if (id1 < 0) { fail("reuse key", "first shmget failed"); return; }

    ptr = (int *)shmat(id1);
    if (ptr == (void *)0) { fail("reuse key", "shmat failed"); return; }

    ptr[0] = 42;
    shmdt(id1);  /* refcount drops to 0, segment freed */

    /* Re-create with same key — should get zeroed memory */
    id2 = shmget(300, IPC_CREAT);
    if (id2 < 0) { fail("reuse key", "second shmget failed"); return; }

    ptr = (int *)shmat(id2);
    if (ptr == (void *)0) { fail("reuse key", "second shmat failed"); return; }

    if (ptr[0] != 0) { fail("reuse key", "memory not zeroed"); shmdt(id2); return; }

    shmdt(id2);
    pass("reuse key");
}

int main(void) {
    printf("=== Shared Memory Tests ===\n");

    test_basic_create();
    test_excl();
    test_lookup_fail();
    test_fork_shared();
    test_reuse_key();

    print_test_results();
    return tests_failed;
}
