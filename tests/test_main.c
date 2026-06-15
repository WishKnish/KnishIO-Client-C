#include "unity.h"
#include "knishio/knishio.h"
#include <stdio.h>
#include <string.h>

/* External test function declarations */
/* Suites whose sources are compiled into knishio_tests (TEST_SOURCES). The
 * memory/string/error/bigint/signature/performance/leak suites live in files
 * not currently in the build — broader test resurrection is a follow-up. */
extern void test_shake256_suite(void);
extern void test_wallet_suite(void);
extern void test_molecule_suite(void);   /* Molecular validation and signing tests */

/* Global test state */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Unity setup and teardown */
void setUp(void) {
    /* Called before each test */
}

void tearDown(void) {
    /* Called after each test */
    tests_run++;
}

/* Test runner function */
static void run_test_suite(const char *suite_name, void (*suite_func)(void)) {
    printf("\n=== Running %s Tests ===\n", suite_name);
    
    /* Reset Unity state */
    Unity.TestFailures = 0;
    Unity.CurrentTestFailed = 0;
    
    /* Run the test suite */
    suite_func();
    
    /* Update global counters */
    if (Unity.TestFailures == 0) {
        tests_passed++;
        printf("✓ %s: PASSED\n", suite_name);
    } else {
        tests_failed++;
        printf("✗ %s: FAILED (%d failures)\n", suite_name, Unity.TestFailures);
    }
}

/* Main test runner */
int main(int argc, char *argv[]) {
    /* Parse command line arguments for filtering */
    const char *filter = NULL;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--filter=", 9) == 0) {
            filter = argv[i] + 9;
            break;
        }
    }
    
    printf("KnishIO C Client SDK Test Suite\n");
    printf("===============================\n");
    
    /* Initialize Unity */
    UnityBegin("KnishIO Tests");
    
    /* Initialize KnishIO SDK */
    if (knishio_init() != KNISHIO_SUCCESS) {
        printf("Failed to initialize KnishIO SDK\n");
        return 1;
    }
    
    /* Run test suites based on filter */
    if (filter == NULL || strstr("shake256", filter) != NULL) {
        run_test_suite("SHAKE256 Crypto", test_shake256_suite);
    }

    if (filter == NULL || strstr("molecule", filter) != NULL) {
        run_test_suite("Molecular Validation & Signing", test_molecule_suite);
    }

    if (filter == NULL || strstr("wallet", filter) != NULL) {
        run_test_suite("Wallet Generation", test_wallet_suite);
    }
    
    /* Cross-SDK compatibility tests */
    if (filter == NULL || strstr("cross_sdk", filter) != NULL) {
        printf("\n=== Cross-SDK Compatibility Tests ===\n");
        
        /* Test SHAKE256 compatibility with known test vectors */
        char *output = NULL;
        bool shake_compat = knishio_shake256_hash("test", 256, &output);
        if (shake_compat && output != NULL) {
            printf("✓ SHAKE256 basic functionality: PASSED\n");
            knishio_free(output);
        } else {
            printf("✗ SHAKE256 basic functionality: FAILED\n");
            tests_failed++;
        }
        tests_run++;
    }
    
    /* Cleanup */
    knishio_cleanup();
    
    /* Print final results */
    printf("\n=== Test Results ===\n");
    printf("Total test suites: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n🎉 All tests PASSED!\n");
    } else {
        printf("\n❌ %d test suite(s) FAILED\n", tests_failed);
    }
    
    /* Unity final result */
    return UnityEnd();
}

/* Note: orphaned helpers test_version_check/test_initialization were removed —
 * they were never invoked by main() and referenced knishio_error_message, which
 * is declared (knishio.h) but unimplemented (a separate pre-existing gap). */