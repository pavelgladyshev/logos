/*
 * sem_test - Semaphore tests
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Tests semget, semwait, sempost, blocking behavior with fork,
 * and refcount cleanup on exit.
 */

#include "libc.h"
#include "test_helpers.h"

/* Test 1: Basic create and non-blocking wait/post */
static void test_basic_semaphore(void) {
    int id;
    int result;

    printf("\n--- Test 1: basic semaphore ---\n");

    id = semget(42, 1, IPC_CREAT);
    if (id < 0) { fail("basic sem", "semget failed"); return; }

    /* Wait should succeed (value was 1, now 0) */
    result = semwait(id);
    if (result != 0) { fail("basic sem", "semwait failed"); semclose(id); return; }

    /* Post should succeed (value back to 1) */
    result = sempost(id);
    if (result != 0) { fail("basic sem", "sempost failed"); semclose(id); return; }

    semclose(id);
    pass("basic sem");
}

/* Test 2: IPC_EXCL flag */
static void test_excl(void) {
    int id1, id2;

    printf("\n--- Test 2: IPC_EXCL ---\n");

    id1 = semget(100, 1, IPC_CREAT);
    if (id1 < 0) { fail("IPC_EXCL", "first semget failed"); return; }

    id2 = semget(100, 1, IPC_CREAT | IPC_EXCL);
    if (id2 >= 0) { fail("IPC_EXCL", "should have failed"); semclose(id1); return; }

    id2 = semget(100, 1, IPC_CREAT);
    if (id2 != id1) { fail("IPC_EXCL", "should return same id"); semclose(id1); return; }

    semclose(id1);
    pass("IPC_EXCL");
}

/* Test 3: Multiple wait/post cycles */
static void test_counting(void) {
    int id;
    int result;

    printf("\n--- Test 3: counting semaphore ---\n");

    id = semget(150, 3, IPC_CREAT);
    if (id < 0) { fail("counting", "semget failed"); return; }

    /* Wait 3 times (value goes 3 -> 2 -> 1 -> 0) */
    result = semwait(id);
    if (result != 0) { fail("counting", "wait 1 failed"); semclose(id); return; }
    result = semwait(id);
    if (result != 0) { fail("counting", "wait 2 failed"); semclose(id); return; }
    result = semwait(id);
    if (result != 0) { fail("counting", "wait 3 failed"); semclose(id); return; }

    /* Post twice (value goes 0 -> 1 -> 2) */
    sempost(id);
    sempost(id);

    /* Wait twice more should succeed */
    result = semwait(id);
    if (result != 0) { fail("counting", "wait 4 failed"); semclose(id); return; }
    result = semwait(id);
    if (result != 0) { fail("counting", "wait 5 failed"); semclose(id); return; }

    semclose(id);
    pass("counting");
}

/* Test 4: Blocking semwait with fork */
static void test_blocking(void) {
    int sem_id, shm_id;
    int *shared;
    int pid;

    printf("\n--- Test 4: blocking semwait ---\n");

    /* Use shared memory to communicate between parent and child */
    shm_id = shmget(300, IPC_CREAT);
    if (shm_id < 0) { fail("blocking", "shmget failed"); return; }

    shared = (int *)shmat(shm_id);
    if (shared == (void *)0) { fail("blocking", "shmat failed"); return; }

    shared[0] = 0;  /* Flag: child has not run yet */

    /* Create semaphore with initial value 0 (child will block) */
    sem_id = semget(301, 0, IPC_CREAT);
    if (sem_id < 0) { fail("blocking", "semget failed"); shmdt(shm_id); return; }

    pid = fork();
    if (pid < 0) {
        fail("blocking", "fork failed");
        semclose(sem_id);
        shmdt(shm_id);
        return;
    }

    if (pid == 0) {
        /* Child: block on semaphore, then set flag */
        semwait(sem_id);
        shared[0] = 1;
        exit(0);
    }

    /* Parent: post to unblock child, then wait for it */
    sempost(sem_id);
    wait();

    if (shared[0] == 1) {
        pass("blocking");
    } else {
        fail("blocking", "child did not run after sempost");
    }

    semclose(sem_id);
    shmdt(shm_id);
}

/* Test 5: Mutex pattern — fork with mutual exclusion */
static void test_mutex(void) {
    int sem_id, shm_id;
    int *shared;
    int pid;

    printf("\n--- Test 5: mutex pattern ---\n");

    shm_id = shmget(400, IPC_CREAT);
    if (shm_id < 0) { fail("mutex", "shmget failed"); return; }

    shared = (int *)shmat(shm_id);
    if (shared == (void *)0) { fail("mutex", "shmat failed"); return; }

    shared[0] = 0;  /* Counter */

    /* Mutex: initial value 1 */
    sem_id = semget(401, 1, IPC_CREAT);
    if (sem_id < 0) { fail("mutex", "semget failed"); shmdt(shm_id); return; }

    pid = fork();
    if (pid < 0) {
        fail("mutex", "fork failed");
        semclose(sem_id);
        shmdt(shm_id);
        return;
    }

    if (pid == 0) {
        /* Child: acquire mutex, increment, release */
        semwait(sem_id);
        shared[0] = shared[0] + 1;
        sempost(sem_id);
        exit(0);
    }

    /* Parent: acquire mutex, increment, release */
    semwait(sem_id);
    shared[0] = shared[0] + 1;
    sempost(sem_id);

    wait();

    if (shared[0] == 2) {
        pass("mutex");
    } else {
        fail("mutex", "expected counter == 2");
    }

    semclose(sem_id);
    shmdt(shm_id);
}

int main(void) {
    printf("=== Semaphore Tests ===\n");

    test_basic_semaphore();
    test_excl();
    test_counting();
    test_blocking();
    test_mutex();

    print_test_results();
    return tests_failed;
}
