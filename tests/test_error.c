#include "unity.h"
#include "knishio/error/context.h"
#include "knishio/utils/memory.h"
#include <string.h>

/* Test basic error context creation */
void test_error_context_creation(void) {
    knishio_error_context_t *ctx = knishio_error_context_create(
        KNISHIO_ERROR_MEMORY, 
        "Test error message"
    );
    
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_MEMORY, knishio_error_context_get_code(ctx));
    TEST_ASSERT_EQUAL_STRING("Test error message", knishio_error_context_get_message(ctx));
    
    knishio_error_context_destroy(ctx);
}

/* Test error context with source location */
void test_error_context_detailed(void) {
    knishio_error_context_t *ctx = knishio_error_context_create_detailed(
        KNISHIO_ERROR_CRYPTO,
        "Crypto operation failed",
        "test.c",
        42,
        "test_function"
    );
    
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_CRYPTO, knishio_error_context_get_code(ctx));
    TEST_ASSERT_EQUAL_STRING("Crypto operation failed", knishio_error_context_get_message(ctx));
    TEST_ASSERT_EQUAL_STRING("test.c", knishio_error_context_get_file(ctx));
    TEST_ASSERT_EQUAL(42, knishio_error_context_get_line(ctx));
    TEST_ASSERT_EQUAL_STRING("test_function", knishio_error_context_get_function(ctx));
    
    knishio_error_context_destroy(ctx);
}

/* Test error message lookup */
void test_error_messages(void) {
    TEST_ASSERT_EQUAL_STRING("Success", knishio_error_message(KNISHIO_SUCCESS));
    TEST_ASSERT_EQUAL_STRING("Memory allocation failed", knishio_error_message(KNISHIO_ERROR_MEMORY));
    TEST_ASSERT_EQUAL_STRING("Invalid arguments", knishio_error_message(KNISHIO_ERROR_INVALID_ARGS));
    TEST_ASSERT_EQUAL_STRING("Unknown error", knishio_error_message(-9999));
}

/* Test error utility functions */
void test_error_utilities(void) {
    TEST_ASSERT_TRUE(knishio_error_is_success(KNISHIO_SUCCESS));
    TEST_ASSERT_FALSE(knishio_error_is_success(KNISHIO_ERROR_MEMORY));
    
    TEST_ASSERT_TRUE(knishio_error_is_category(KNISHIO_ERROR_AUTH, -100));
    TEST_ASSERT_TRUE(knishio_error_is_category(KNISHIO_ERROR_UNAUTHENTICATED, -100));
    TEST_ASSERT_FALSE(knishio_error_is_category(KNISHIO_ERROR_MEMORY, -100));
}

/* Test error chaining */
void test_error_chaining(void) {
    /* Create root cause */
    knishio_error_context_t *cause = knishio_error_context_create(
        KNISHIO_ERROR_NETWORK,
        "Connection failed"
    );
    TEST_ASSERT_NOT_NULL(cause);
    
    /* Create main error with cause */
    knishio_error_context_t *main_error = knishio_error_context_create(
        KNISHIO_ERROR_AUTH,
        "Authentication failed"
    );
    TEST_ASSERT_NOT_NULL(main_error);
    
    TEST_ASSERT_TRUE(knishio_error_context_set_cause(main_error, cause));
    
    /* Verify chaining */
    const knishio_error_context_t *retrieved_cause = knishio_error_context_get_cause(main_error);
    TEST_ASSERT_NOT_NULL(retrieved_cause);
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_NETWORK, knishio_error_context_get_code(retrieved_cause));
    
    knishio_error_context_destroy(main_error); /* Should also destroy cause */
}

/* Test error formatting */
void test_error_formatting(void) {
    knishio_error_context_t *ctx = knishio_error_context_create_detailed(
        KNISHIO_ERROR_INVALID_ARGS,
        "Invalid parameter",
        "test.c",
        100,
        "test_function"
    );
    
    char *formatted = knishio_error_context_format(ctx, false);
    TEST_ASSERT_NOT_NULL(formatted);
    TEST_ASSERT_TRUE(strstr(formatted, "Invalid parameter") != NULL);
    TEST_ASSERT_TRUE(strstr(formatted, "test.c") != NULL);
    
    knishio_free(formatted);
    knishio_error_context_destroy(ctx);
}

/* Error test suite entry point */
void test_error_suite(void) {
    RUN_TEST(test_error_context_creation);
    RUN_TEST(test_error_context_detailed);
    RUN_TEST(test_error_messages);
    RUN_TEST(test_error_utilities);
    RUN_TEST(test_error_chaining);
    RUN_TEST(test_error_formatting);
}