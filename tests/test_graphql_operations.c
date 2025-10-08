/**
 * @file test_graphql_operations.c
 * @brief Comprehensive tests for GraphQL operations implementation
 * 
 * Tests all GraphQL operations for compatibility with JavaScript SDK:
 * - Query operations (balance, wallet, token, atom, etc.)
 * - Mutation operations (transfer, create, request, etc.)
 * - Subscription operations (real-time updates)
 * - High-level integration operations
 * - Complex transaction patterns
 */

#include "knishio/operations/graphql_operations.h"
#include "knishio/query/queries.h"
#include "knishio/mutation/mutations.h"
#include "knishio/subscribe/subscriptions.h"
#include "knishio/client.h"
#include "knishio/wallet.h"
#include "knishio/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test configuration */
#define TEST_ENDPOINT_URL "http://localhost:8080/graphql"
#define TEST_WS_URL "ws://localhost:8080/graphql"
#define TEST_CELL_SLUG "test_cell"
#define TEST_SECRET "test_secret_12345"
#define TEST_TOKEN_SLUG "TEST"
#define TEST_WALLET_ADDRESS "Kk4xB5c2fE7hJ8iK9lM2nO3pQ4rS5tU6vW7xY8zA1B"

/* Global test state */
static knishio_client_t* test_client = NULL;
static knishio_graphql_operations_t* test_ops = NULL;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helper macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("✗ %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

#define TEST_ASSERT_ERROR(result, expected, message) \
    TEST_ASSERT(result == expected, message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT(ptr != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT(ptr == NULL, message)

/* Test setup and teardown */
static knishio_error_t setup_test_environment(void) {
    printf("Setting up test environment...\n");
    
    // Create client
    knishio_error_t result = knishio_client_create(&test_client, TEST_SECRET);
    if (result != KNISHIO_SUCCESS) {
        printf("Failed to create client: %d\n", result);
        return result;
    }
    
    // Create GraphQL operations
    result = knishio_graphql_operations_create(
        &test_ops, test_client, TEST_ENDPOINT_URL, TEST_CELL_SLUG);
    if (result != KNISHIO_SUCCESS) {
        printf("Failed to create GraphQL operations: %d\n", result);
        knishio_client_free(test_client);
        return result;
    }
    
    // Enable debug mode for testing
    knishio_graphql_operations_set_debug(test_ops, true);
    
    printf("Test environment setup complete.\n");
    return KNISHIO_SUCCESS;
}

static void teardown_test_environment(void) {
    printf("Tearing down test environment...\n");
    
    if (test_ops) {
        knishio_graphql_operations_free(test_ops);
        test_ops = NULL;
    }
    
    if (test_client) {
        knishio_client_free(test_client);
        test_client = NULL;
    }
    
    printf("Test environment teardown complete.\n");
}

/* Query operation tests */
static void test_query_operations(void) {
    printf("\n=== Testing Query Operations ===\n");
    
    // Test balance query
    knishio_response_balance_t* balance_response = NULL;
    knishio_error_t result = knishio_operations_query_balance_simple(
        test_ops, TEST_WALLET_ADDRESS, TEST_TOKEN_SLUG, &balance_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Query balance operation");
    if (result == KNISHIO_SUCCESS) {
        TEST_ASSERT_NOT_NULL(balance_response, "Balance response created");
        
        const char* balance = knishio_response_balance_get_amount(balance_response);
        TEST_ASSERT_NOT_NULL(balance, "Balance amount retrieved");
        
        printf("  Balance: %s %s\n", balance ? balance : "0", TEST_TOKEN_SLUG);
        
        knishio_response_balance_free(balance_response);
    }
    
    // Test wallet list query
    knishio_query_wallet_list_params_t wallet_params = {0};
    wallet_params.token_slug = TEST_TOKEN_SLUG;
    
    knishio_response_wallet_list_t* wallet_response = NULL;
    result = knishio_query_wallet_list(
        knishio_operations_get_graphql_client(test_ops), &wallet_params, &wallet_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Query wallet list operation");
    if (result == KNISHIO_SUCCESS && wallet_response) {
        printf("  Wallet list query completed\n");
        knishio_response_wallet_list_free(wallet_response);
    }
    
    // Test token query
    knishio_query_token_params_t token_params = {0};
    token_params.slug = TEST_TOKEN_SLUG;
    token_params.limit = 10;
    
    knishio_response_token_t* token_response = NULL;
    result = knishio_query_token(
        knishio_operations_get_graphql_client(test_ops), &token_params, &token_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Query token operation");
    if (result == KNISHIO_SUCCESS && token_response) {
        printf("  Token query completed\n");
        knishio_response_token_free(token_response);
    }
    
    // Test atom query
    const char* test_isotopes[] = {"V", "M"};
    knishio_query_atom_params_t atom_params = {0};
    atom_params.isotopes = test_isotopes;
    atom_params.isotope_count = 2;
    atom_params.limit = 5;
    
    knishio_response_atom_t* atom_response = NULL;
    result = knishio_query_atom(
        knishio_operations_get_graphql_client(test_ops), &atom_params, &atom_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Query atom operation");
    if (result == KNISHIO_SUCCESS && atom_response) {
        printf("  Atom query completed\n");
        knishio_response_atom_free(atom_response);
    }
}

/* Mutation operation tests */
static void test_mutation_operations(void) {
    printf("\n=== Testing Mutation Operations ===\n");
    
    // Create test wallet for mutations
    knishio_wallet_t* test_wallet = NULL;
    knishio_error_t result = knishio_client_create_wallet(
        test_client, TEST_TOKEN_SLUG, &test_wallet);
    
    if (result != KNISHIO_SUCCESS) {
        printf("Warning: Could not create test wallet for mutations\n");
        return;
    }
    
    // Test token creation
    knishio_response_create_token_t* create_response = NULL;
    result = knishio_operations_create_token_simple(
        test_ops, test_wallet, "NEWTOKEN", "1000000", 
        "{\"name\":\"Test Token\",\"symbol\":\"TST\"}", &create_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Create token operation");
    if (result == KNISHIO_SUCCESS && create_response) {
        const char* molecular_hash = knishio_response_create_token_get_hash(create_response);
        printf("  Created token with molecular hash: %s\n", 
               molecular_hash ? molecular_hash : "unknown");
        knishio_response_create_token_free(create_response);
    }
    
    // Test token request
    knishio_response_request_tokens_t* request_response = NULL;
    result = knishio_operations_request_tokens_simple(
        test_ops, test_wallet, TEST_TOKEN_SLUG, "100", &request_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Request tokens operation");
    if (result == KNISHIO_SUCCESS && request_response) {
        const char* molecular_hash = knishio_response_request_tokens_get_hash(request_response);
        printf("  Requested tokens with molecular hash: %s\n", 
               molecular_hash ? molecular_hash : "unknown");
        knishio_response_request_tokens_free(request_response);
    }
    
    // Test metadata creation
    knishio_response_create_meta_t* meta_response = NULL;
    result = knishio_operations_create_meta_simple(
        test_ops, test_wallet, "profile", "user123", 
        "{\"name\":\"Test User\",\"email\":\"test@example.com\"}", &meta_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Create metadata operation");
    if (result == KNISHIO_SUCCESS && meta_response) {
        printf("  Created metadata successfully\n");
        knishio_response_create_meta_free(meta_response);
    }
    
    knishio_wallet_free(test_wallet);
}

/* Authentication tests */
static void test_authentication_operations(void) {
    printf("\n=== Testing Authentication Operations ===\n");
    
    // Test profile authentication
    knishio_response_request_authorization_t* auth_response = NULL;
    knishio_error_t result = knishio_operations_authenticate_profile(
        test_ops, TEST_SECRET, TEST_CELL_SLUG, &auth_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Profile authentication");
    if (result == KNISHIO_SUCCESS && auth_response) {
        bool success = knishio_response_request_authorization_is_success(auth_response);
        TEST_ASSERT(success, "Authentication successful");
        
        const char* auth_token = knishio_response_request_authorization_get_token(auth_response);
        if (auth_token) {
            printf("  Received auth token: %.20s...\n", auth_token);
            
            // Set auth token for subsequent operations
            knishio_graphql_operations_set_auth_token(test_ops, auth_token);
        }
        
        knishio_response_request_authorization_free(auth_response);
    }
    
    // Test guest authentication
    knishio_response_request_authorization_guest_t* guest_response = NULL;
    result = knishio_operations_authenticate_guest(
        test_ops, TEST_CELL_SLUG, &guest_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Guest authentication");
    if (result == KNISHIO_SUCCESS && guest_response) {
        bool success = knishio_response_request_authorization_guest_is_success(guest_response);
        TEST_ASSERT(success, "Guest authentication successful");
        knishio_response_request_authorization_guest_free(guest_response);
    }
}

/* Complex transaction pattern tests */
static void test_complex_transactions(void) {
    printf("\n=== Testing Complex Transaction Patterns ===\n");
    
    // Create test wallets
    knishio_wallet_t* source_wallet = NULL;
    knishio_wallet_t* party1_wallet = NULL;
    knishio_wallet_t* party2_wallet = NULL;
    
    knishio_error_t result = knishio_client_create_wallet(test_client, TEST_TOKEN_SLUG, &source_wallet);
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Create source wallet for complex transactions");
    
    result = knishio_client_create_wallet(test_client, TEST_TOKEN_SLUG, &party1_wallet);
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Create party1 wallet for complex transactions");
    
    result = knishio_client_create_wallet(test_client, "TOKEN2", &party2_wallet);
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Create party2 wallet for complex transactions");
    
    if (source_wallet && party1_wallet && party2_wallet) {
        // Test multi-party transfer
        knishio_transfer_recipient_t recipients[] = {
            {"Kk4xB5c2fE7hJ8iK9lM2nO3pQ4rS5tU6vW7xY8zA1B", "50", "Payment 1"},
            {"Kk4xB5c2fE7hJ8iK9lM2nO3pQ4rS5tU6vW7xY8zA2C", "30", "Payment 2"},
            {"Kk4xB5c2fE7hJ8iK9lM2nO3pQ4rS5tU6vW7xY8zA3D", "20", "Payment 3"}
        };
        
        knishio_response_transfer_tokens_t* multi_response = NULL;
        result = knishio_operations_transfer_tokens_multi(
            test_ops, source_wallet, recipients, 3, TEST_TOKEN_SLUG, &multi_response);
        
        TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Multi-party token transfer");
        if (result == KNISHIO_SUCCESS && multi_response) {
            printf("  Multi-party transfer completed\n");
            knishio_response_transfer_tokens_free(multi_response);
        }
        
        // Test token swap
        knishio_response_transfer_tokens_t* swap_response = NULL;
        result = knishio_operations_swap_tokens(
            test_ops, party1_wallet, party2_wallet, 
            TEST_TOKEN_SLUG, "100", "TOKEN2", "200", &swap_response);
        
        TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Token swap operation");
        if (result == KNISHIO_SUCCESS && swap_response) {
            printf("  Token swap completed\n");
            knishio_response_transfer_tokens_free(swap_response);
        }
    }
    
    // Clean up
    if (source_wallet) knishio_wallet_free(source_wallet);
    if (party1_wallet) knishio_wallet_free(party1_wallet);
    if (party2_wallet) knishio_wallet_free(party2_wallet);
}

/* Subscription callback for testing */
static void test_wallet_callback(const char* data, const char* error, void* user_data) {
    int* callback_count = (int*)user_data;
    (*callback_count)++;
    
    if (error) {
        printf("  Subscription error: %s\n", error);
    } else {
        printf("  Received wallet update: %.100s...\n", data);
    }
}

static void test_molecule_callback(const char* data, const char* error, void* user_data) {
    int* callback_count = (int*)user_data;
    (*callback_count)++;
    
    if (error) {
        printf("  Subscription error: %s\n", error);
    } else {
        printf("  Received molecule update: %.100s...\n", data);
    }
}

/* Subscription tests */
static void test_subscription_operations(void) {
    printf("\n=== Testing Subscription Operations ===\n");
    
    // Set WebSocket configuration
    knishio_websocket_config_t ws_config = {0};
    ws_config.ws_url = TEST_WS_URL;
    ws_config.protocol = "graphql-ws";
    ws_config.timeout_ms = 5000;
    ws_config.heartbeat_interval_ms = 30000;
    ws_config.auto_reconnect = true;
    ws_config.max_reconnect_attempts = 3;
    
    knishio_error_t result = knishio_graphql_client_set_websocket_config(
        knishio_operations_get_graphql_client(test_ops), &ws_config);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "WebSocket configuration");
    
    // Test wallet subscription
    int wallet_callback_count = 0;
    knishio_subscription_handle_t* wallet_handle = NULL;
    
    result = knishio_operations_subscribe_wallet_balance(
        test_ops, "test_bundle_hash", test_wallet_callback, 
        &wallet_callback_count, &wallet_handle);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Wallet balance subscription");
    if (result == KNISHIO_SUCCESS && wallet_handle) {
        TEST_ASSERT(knishio_subscription_is_active(wallet_handle), 
                   "Wallet subscription is active");
        
        // Wait briefly for potential callbacks
        usleep(1000000); // 1 second
        
        printf("  Wallet subscription received %d callbacks\n", wallet_callback_count);
        
        // Unsubscribe
        result = knishio_subscription_unsubscribe(wallet_handle);
        TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Unsubscribe wallet balance");
    }
    
    // Test molecule subscription
    int molecule_callback_count = 0;
    knishio_subscription_handle_t* molecule_handle = NULL;
    
    result = knishio_operations_subscribe_molecules(
        test_ops, TEST_CELL_SLUG, test_molecule_callback, 
        &molecule_callback_count, &molecule_handle);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Molecule subscription");
    if (result == KNISHIO_SUCCESS && molecule_handle) {
        TEST_ASSERT(knishio_subscription_is_active(molecule_handle), 
                   "Molecule subscription is active");
        
        // Wait briefly for potential callbacks
        usleep(1000000); // 1 second
        
        printf("  Molecule subscription received %d callbacks\n", molecule_callback_count);
        
        // Unsubscribe
        result = knishio_subscription_unsubscribe(molecule_handle);
        TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Unsubscribe molecules");
    }
}

/* Utility function tests */
static void test_utility_functions(void) {
    printf("\n=== Testing Utility Functions ===\n");
    
    // Test balance string retrieval
    char* balance_str = NULL;
    knishio_error_t result = knishio_operations_get_balance_string(
        test_ops, TEST_WALLET_ADDRESS, TEST_TOKEN_SLUG, &balance_str);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Get balance string");
    if (result == KNISHIO_SUCCESS) {
        TEST_ASSERT_NOT_NULL(balance_str, "Balance string retrieved");
        printf("  Balance string: %s\n", balance_str ? balance_str : "null");
        free(balance_str);
    }
    
    // Test sufficient balance check
    bool sufficient = false;
    result = knishio_operations_check_sufficient_balance(
        test_ops, TEST_WALLET_ADDRESS, TEST_TOKEN_SLUG, "10", &sufficient);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Check sufficient balance");
    printf("  Sufficient balance for 10 %s: %s\n", TEST_TOKEN_SLUG, sufficient ? "Yes" : "No");
    
    // Test raw GraphQL query
    char* raw_response = NULL;
    result = knishio_operations_execute_raw_query(
        test_ops, 
        "query { __typename }", 
        "{}", 
        &raw_response);
    
    TEST_ASSERT_ERROR(result, KNISHIO_SUCCESS, "Execute raw GraphQL query");
    if (result == KNISHIO_SUCCESS && raw_response) {
        printf("  Raw query response: %.100s...\n", raw_response);
        free(raw_response);
    }
}

/* Performance tests */
static void test_performance(void) {
    printf("\n=== Testing Performance ===\n");
    
    clock_t start_time = clock();
    
    // Perform multiple balance queries to test performance
    const int query_count = 10;
    int successful_queries = 0;
    
    for (int i = 0; i < query_count; i++) {
        knishio_response_balance_t* response = NULL;
        knishio_error_t result = knishio_operations_query_balance_simple(
            test_ops, TEST_WALLET_ADDRESS, TEST_TOKEN_SLUG, &response);
        
        if (result == KNISHIO_SUCCESS) {
            successful_queries++;
            if (response) {
                knishio_response_balance_free(response);
            }
        }
    }
    
    clock_t end_time = clock();
    double elapsed_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("  Executed %d/%d balance queries in %.3f seconds\n", 
           successful_queries, query_count, elapsed_time);
    printf("  Average time per query: %.3f seconds\n", elapsed_time / query_count);
    
    TEST_ASSERT(successful_queries > 0, "At least one query succeeded");
    TEST_ASSERT(elapsed_time < 30.0, "Queries completed within reasonable time");
}

/* Main test runner */
int main(int argc, char* argv[]) {
    printf("KnishIO C SDK GraphQL Operations Test Suite\n");
    printf("==========================================\n");
    
    // Setup test environment
    knishio_error_t setup_result = setup_test_environment();
    if (setup_result != KNISHIO_SUCCESS) {
        printf("Failed to setup test environment: %d\n", setup_result);
        return 1;
    }
    
    // Run all test suites
    test_query_operations();
    test_mutation_operations();
    test_authentication_operations();
    test_complex_transactions();
    test_subscription_operations();
    test_utility_functions();
    test_performance();
    
    // Teardown test environment
    teardown_test_environment();
    
    // Print test results
    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Total tests: %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed.\n");
        return 1;
    }
}