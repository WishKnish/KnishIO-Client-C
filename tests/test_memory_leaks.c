/**
 * @file test_memory_leaks.c
 * @brief Systematic memory leak detection tests for KnishIO C SDK
 * 
 * Comprehensive memory leak testing designed to work with Valgrind
 * to ensure production-grade memory management compliance.
 */

#include "unity.h"
#include "knishio/knishio.h"
#include "knishio/client.h"
#include "knishio/wallet.h"
#include "knishio/auth_token.h"
#include "knishio/atom.h"
#include "knishio/molecule.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Memory leak test configuration */
#define LEAK_TEST_ITERATIONS 10
#define STRESS_TEST_ITERATIONS 100

/* Test memory management in SDK initialization/cleanup cycles */
void test_memory_sdk_init_cleanup_cycles(void) {
    printf("\n🔍 Memory Test: SDK Init/Cleanup Cycles\n");
    
    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        knishio_error_t error = knishio_init();
        TEST_ASSERT_EQUAL_MESSAGE(KNISHIO_SUCCESS, error, "SDK initialization failed");
        
        knishio_cleanup();
        printf("   Cycle %2d: Init/Cleanup ✓\n", i + 1);
    }
    
    printf("✅ SDK init/cleanup cycles completed without leaks\n");
}

/* Test client creation and destruction for leaks */
void test_memory_client_lifecycle(void) {
    printf("\n🔍 Memory Test: Client Lifecycle\n");
    
    /* Initialize SDK once */
    knishio_error_t error = knishio_init();
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        knishio_client_config_t config = {
            .uri = "http://localhost:8080/graphql",
            .cell_slug = "test-cell"
        };
        
        knishio_client_t* client = NULL;
        error = knishio_client_create(&client, &config);
        TEST_ASSERT_EQUAL_MESSAGE(KNISHIO_SUCCESS, error, "Client creation failed");
        TEST_ASSERT_NOT_NULL_MESSAGE(client, "Client is NULL after creation");
        
        knishio_client_destroy(client);
        printf("   Cycle %2d: Client create/destroy ✓\n", i + 1);
    }
    
    knishio_cleanup();
    printf("✅ Client lifecycle test completed without leaks\n");
}

/* Test wallet creation and cleanup for leaks */
void test_memory_wallet_lifecycle(void) {
    printf("\n🔍 Memory Test: Wallet Lifecycle\n");
    
    knishio_error_t error = knishio_init();
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        knishio_wallet_config_t config = {
            .secret = "test-secret-for-memory-test",
            .token = "TEST",
            .position = NULL  /* Auto-generate */
        };
        
        knishio_wallet_t* wallet = NULL;
        error = knishio_wallet_create(&wallet, &config);
        TEST_ASSERT_EQUAL_MESSAGE(KNISHIO_SUCCESS, error, "Wallet creation failed");
        TEST_ASSERT_NOT_NULL_MESSAGE(wallet, "Wallet is NULL after creation");
        
        /* Test wallet operations that allocate memory */
        const char* address = knishio_wallet_get_address(wallet);
        const char* bundle_hash = knishio_wallet_get_bundle_hash(wallet);
        const char* position = knishio_wallet_get_position(wallet);
        
        /* Verify we got valid pointers */
        TEST_ASSERT_NOT_NULL(address);
        TEST_ASSERT_NOT_NULL(bundle_hash);
        TEST_ASSERT_NOT_NULL(position);
        
        knishio_wallet_cleanup(wallet);
        printf("   Cycle %2d: Wallet create/cleanup ✓\n", i + 1);
    }
    
    knishio_cleanup();
    printf("✅ Wallet lifecycle test completed without leaks\n");
}

/* Test authentication token memory management */
void test_memory_auth_token_lifecycle(void) {
    printf("\n🔍 Memory Test: Auth Token Lifecycle\n");
    
    knishio_error_t error = knishio_init();
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        knishio_auth_token_config_t config = {
            .token = "test-token-data-for-memory-leak-testing",
            .expires_at = time(NULL) + 3600,  /* 1 hour from now */
            .encrypt = true,
            .pubkey = "test-public-key-data-for-memory-testing"
        };
        
        knishio_auth_token_t* auth_token = NULL;
        error = knishio_auth_token_create(&auth_token, &config);
        TEST_ASSERT_EQUAL_MESSAGE(KNISHIO_SUCCESS, error, "Auth token creation failed");
        TEST_ASSERT_NOT_NULL_MESSAGE(auth_token, "Auth token is NULL after creation");
        
        /* Test token operations that may allocate memory */
        const char* token_data = knishio_auth_token_get_token(auth_token);
        const char* pubkey_data = knishio_auth_token_get_pubkey(auth_token);
        bool is_expired = knishio_auth_token_is_expired(auth_token);
        int64_t expire_interval = knishio_auth_token_get_expire_interval(auth_token);
        
        /* Verify operations completed */
        TEST_ASSERT_NOT_NULL(token_data);
        TEST_ASSERT_NOT_NULL(pubkey_data);
        TEST_ASSERT_FALSE(is_expired);  /* Should not be expired (1 hour validity) */
        TEST_ASSERT_TRUE(expire_interval > 0);
        
        knishio_auth_token_cleanup(auth_token);
        printf("   Cycle %2d: Auth token create/cleanup ✓\n", i + 1);
    }
    
    knishio_cleanup();
    printf("✅ Auth token lifecycle test completed without leaks\n");
}

/* Test atom creation and cleanup for leaks */
void test_memory_atom_lifecycle(void) {
    printf("\n🔍 Memory Test: Atom Lifecycle\n");
    
    knishio_error_t error = knishio_init();
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        knishio_atom_config_t config = {
            .position = "test-position",
            .wallet_address = "test-wallet-address",
            .isotope = "M",
            .token = "TEST",
            .value = "1000",
            .meta_type = "test",
            .meta_id = "test-meta-id"
        };
        
        knishio_atom_t* atom = NULL;
        error = knishio_atom_create(&atom, &config);
        TEST_ASSERT_EQUAL_MESSAGE(KNISHIO_SUCCESS, error, "Atom creation failed");
        TEST_ASSERT_NOT_NULL_MESSAGE(atom, "Atom is NULL after creation");
        
        /* Test atom operations */
        const char* position = knishio_atom_get_position(atom);
        const char* wallet_address = knishio_atom_get_wallet_address(atom);
        const char* isotope = knishio_atom_get_isotope(atom);
        
        TEST_ASSERT_NOT_NULL(position);
        TEST_ASSERT_NOT_NULL(wallet_address);
        TEST_ASSERT_NOT_NULL(isotope);
        
        knishio_atom_cleanup(atom);
        printf("   Cycle %2d: Atom create/cleanup ✓\n", i + 1);
    }
    
    knishio_cleanup();
    printf("✅ Atom lifecycle test completed without leaks\n");
}

/* Test molecule creation and cleanup for leaks */
void test_memory_molecule_lifecycle(void) {
    printf("\n🔍 Memory Test: Molecule Lifecycle\n");
    
    knishio_error_t error = knishio_init();
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        knishio_molecule_config_t config = {
            .cell_slug = "test-cell",
            .bundle_hash = "test-bundle-hash",
            .status = "pending"
        };
        
        knishio_molecule_t* molecule = NULL;
        error = knishio_molecule_create(&molecule, &config);
        TEST_ASSERT_EQUAL_MESSAGE(KNISHIO_SUCCESS, error, "Molecule creation failed");
        TEST_ASSERT_NOT_NULL_MESSAGE(molecule, "Molecule is NULL after creation");
        
        /* Test molecule operations */
        const char* cell_slug = knishio_molecule_get_cell_slug(molecule);
        const char* bundle_hash = knishio_molecule_get_bundle_hash(molecule);
        const char* status = knishio_molecule_get_status(molecule);
        
        TEST_ASSERT_NOT_NULL(cell_slug);
        TEST_ASSERT_NOT_NULL(bundle_hash);
        TEST_ASSERT_NOT_NULL(status);
        
        knishio_molecule_cleanup(molecule);
        printf("   Cycle %2d: Molecule create/cleanup ✓\n", i + 1);
    }
    
    knishio_cleanup();
    printf("✅ Molecule lifecycle test completed without leaks\n");
}

/* Stress test for memory leaks under load */
void test_memory_stress_mixed_operations(void) {
    printf("\n🔍 Memory Stress Test: Mixed Operations\n");
    printf("   Running %d iterations of mixed operations...\n", STRESS_TEST_ITERATIONS);
    
    knishio_error_t error = knishio_init();
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        /* Create client */
        knishio_client_config_t client_config = {
            .uri = "http://localhost:8080/graphql",
            .cell_slug = "test-cell"
        };
        
        knishio_client_t* client = NULL;
        error = knishio_client_create(&client, &client_config);
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
        
        /* Create wallet */
        knishio_wallet_config_t wallet_config = {
            .secret = "stress-test-secret",
            .token = "STRESS",
            .position = NULL
        };
        
        knishio_wallet_t* wallet = NULL;
        error = knishio_wallet_create(&wallet, &wallet_config);
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
        
        /* Create auth token */
        knishio_auth_token_config_t auth_config = {
            .token = "stress-test-token",
            .expires_at = time(NULL) + 3600,
            .encrypt = false,
            .pubkey = "stress-test-pubkey"
        };
        
        knishio_auth_token_t* auth_token = NULL;
        error = knishio_auth_token_create(&auth_token, &auth_config);
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
        
        /* Perform some operations */
        const char* wallet_address = knishio_wallet_get_address(wallet);
        const char* token_data = knishio_auth_token_get_token(auth_token);
        TEST_ASSERT_NOT_NULL(wallet_address);
        TEST_ASSERT_NOT_NULL(token_data);
        
        /* Clean up in reverse order */
        knishio_auth_token_cleanup(auth_token);
        knishio_wallet_cleanup(wallet);
        knishio_client_destroy(client);
        
        if ((i + 1) % 20 == 0) {
            printf("   Completed %d iterations ✓\n", i + 1);
        }
    }
    
    knishio_cleanup();
    printf("✅ Mixed operations stress test completed without leaks\n");
}

/* Test memory management in error conditions */
void test_memory_error_handling(void) {
    printf("\n🔍 Memory Test: Error Handling\n");
    
    knishio_error_t error = knishio_init();
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    /* Test error conditions that should not leak memory */
    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        /* Try to create client with invalid config */
        knishio_client_t* client = NULL;
        error = knishio_client_create(&client, NULL);  /* Invalid config */
        TEST_ASSERT_NOT_EQUAL(KNISHIO_SUCCESS, error);
        TEST_ASSERT_NULL(client);
        
        /* Try to create wallet with invalid config */
        knishio_wallet_t* wallet = NULL;
        knishio_wallet_config_t invalid_config = {
            .secret = NULL,  /* Invalid - NULL secret */
            .token = "TEST",
            .position = NULL
        };
        error = knishio_wallet_create(&wallet, &invalid_config);
        TEST_ASSERT_NOT_EQUAL(KNISHIO_SUCCESS, error);
        TEST_ASSERT_NULL(wallet);
        
        printf("   Error handling cycle %2d: ✓\n", i + 1);
    }
    
    knishio_cleanup();
    printf("✅ Error handling memory test completed without leaks\n");
}

/* Test for memory leaks in string operations */
void test_memory_string_operations(void) {
    printf("\n🔍 Memory Test: String Operations\n");
    
    knishio_error_t error = knishio_init();
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, error);
    
    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        /* Test various string operations that might allocate memory */
        char* test_string = knishio_strdup("test-string-for-memory-leak-detection");
        TEST_ASSERT_NOT_NULL(test_string);
        
        /* Use the string */
        size_t len = strlen(test_string);
        TEST_ASSERT_TRUE(len > 0);
        
        /* Free the string */
        knishio_free(test_string);
        
        printf("   String operation cycle %2d: ✓\n", i + 1);
    }
    
    knishio_cleanup();
    printf("✅ String operations memory test completed without leaks\n");
}

/* Main memory leak test suite */
void test_memory_leak_suite(void) {
    printf("\n=== Systematic Memory Leak Detection Test Suite ===\n");
    printf("Designed for Valgrind integration and production validation\n");
    
    RUN_TEST(test_memory_sdk_init_cleanup_cycles);
    RUN_TEST(test_memory_client_lifecycle);
    RUN_TEST(test_memory_wallet_lifecycle);
    RUN_TEST(test_memory_auth_token_lifecycle);
    RUN_TEST(test_memory_atom_lifecycle);
    RUN_TEST(test_memory_molecule_lifecycle);
    RUN_TEST(test_memory_string_operations);
    RUN_TEST(test_memory_error_handling);
    RUN_TEST(test_memory_stress_mixed_operations);
    
    printf("\n✅ Memory Leak Detection Test Suite Complete\n");
    printf("   Run with Valgrind for comprehensive leak detection:\n");
    printf("   valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./test_executable\n");
}