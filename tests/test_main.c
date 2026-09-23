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
extern void test_encrypted_transport_suite(void);  /* PQ-transport fail-closed behaviour */

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
    
    Unity.CurrentTestFailed = 0;

    /* Unity's failure counter accumulates across suites, so UnityEnd() (the process exit
     * status) covers every suite. It used to be reset here, which made the exit status
     * report only the LAST suite and hid failing assertions in the others behind a green
     * ctest. */
    const UNITY_COUNTER_TYPE failures_before = Unity.TestFailures;

    suite_func();

    const UNITY_COUNTER_TYPE suite_failures = Unity.TestFailures - failures_before;
    if (suite_failures == 0) {
        tests_passed++;
        printf("✓ %s: PASSED\n", suite_name);
    } else {
        tests_failed++;
        printf("✗ %s: FAILED (%lu failures)\n", suite_name, (unsigned long)suite_failures);
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

    if (filter == NULL || strstr("encrypted_transport", filter) != NULL) {
        run_test_suite("Encrypted Transport Fail-Closed", test_encrypted_transport_suite);
    }
    
    /* Cross-SDK compatibility tests */
    if (filter == NULL || strstr("cross_sdk", filter) != NULL) {
        printf("\n=== Cross-SDK Compatibility Tests ===\n");

        /* Known answers from the cross-SDK master, sdks/shared-test-results/
         * cross-platform-test-vectors.json .vectors.shake256.tests[]: the vectors
         * abc_32_bytes and empty_string_32_bytes. Every SDK asserts the same values. */
        static const struct {
            const char *name;
            const char *input;
            const char *expected;
        } master_vectors[] = {
            { "abc_32_bytes", "abc",
              "483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739" },
            { "empty_string_32_bytes", "",
              "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f" },
        };
        for (size_t v = 0; v < sizeof(master_vectors) / sizeof(master_vectors[0]); v++) {
            char *output = NULL;
            const bool hashed = knishio_shake256_hash(master_vectors[v].input, 256, &output);
            if (hashed && output != NULL && strcmp(output, master_vectors[v].expected) == 0) {
                printf("✓ SHAKE256 master vector %s: PASSED\n", master_vectors[v].name);
            } else {
                printf("✗ SHAKE256 master vector %s: FAILED (expected %s, got %s)\n",
                       master_vectors[v].name, master_vectors[v].expected,
                       output != NULL ? output : "(no output)");
                tests_failed++;
            }
            knishio_free(output);
            tests_run++;
        }
    }
    
    /* Cleanup */
    knishio_cleanup();
    
    /* Print final results */
    printf("\n=== Test Results ===\n");
    printf("Total test suites: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_run == 0) {
        /* A filter that matches no suite (a typo such as --filter=molecules) runs nothing,
         * which must not read as a pass. */
        printf("\n❌ No test suite matched --filter=%s\n", filter ? filter : "");
        return 1;
    }
    if (tests_failed == 0) {
        printf("\n🎉 All tests PASSED!\n");
    } else {
        printf("\n❌ %d test suite(s) FAILED\n", tests_failed);
    }
    
    /* Non-zero if ANY suite failed. The cross-SDK block above counts into tests_failed
     * without going through Unity, so both are checked. */
    const int unity_failures = UnityEnd();
    return (unity_failures != 0 || tests_failed != 0) ? 1 : 0;
}

/* Note: orphaned helpers test_version_check/test_initialization were removed —
 * they were never invoked by main() and referenced knishio_error_message, which
 * is declared (knishio.h) but unimplemented (a separate pre-existing gap). */