/*
 * Shared test helpers for logOS test programs
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "libc.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void pass(const char *name) {
    printf("  PASS: %s\n", name);
    tests_passed++;
}

static void fail(const char *name, const char *detail) {
    printf("  FAIL: %s - %s\n", name, detail);
    tests_failed++;
}

static void print_test_results(void) {
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
}

#endif /* TEST_HELPERS_H */
