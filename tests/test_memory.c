#include "unity.h"
#include "knishio/utils/memory.h"
#include <string.h>

/* Test memory allocation and deallocation */
void test_memory_basic_allocation(void) {
    void *ptr = knishio_malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);
    
    /* Memory should be zeroed */
    uint8_t *bytes = (uint8_t*)ptr;
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL(0, bytes[i]);
    }
    
    knishio_free(ptr);
}

/* Test memory reallocation */
void test_memory_reallocation(void) {
    void *ptr = knishio_malloc(50);
    TEST_ASSERT_NOT_NULL(ptr);
    
    /* Fill with test data */
    memset(ptr, 0xAA, 50);
    
    /* Reallocate to larger size */
    ptr = knishio_realloc(ptr, 100);
    TEST_ASSERT_NOT_NULL(ptr);
    
    /* Original data should be preserved */
    uint8_t *bytes = (uint8_t*)ptr;
    for (int i = 0; i < 50; i++) {
        TEST_ASSERT_EQUAL(0xAA, bytes[i]);
    }
    
    knishio_free(ptr);
}

/* Test string duplication */
void test_memory_string_dup(void) {
    const char *original = "Hello, World!";
    char *copy = knishio_strdup(original);
    
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_STRING(original, copy);
    TEST_ASSERT_NOT_EQUAL(original, copy); /* Different pointers */
    
    knishio_free(copy);
}

/* Test string duplication with length limit */
void test_memory_string_ndup(void) {
    const char *original = "Hello, World!";
    char *copy = knishio_strndup(original, 5);
    
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_STRING("Hello", copy);
    TEST_ASSERT_EQUAL(5, strlen(copy));
    
    knishio_free(copy);
}

/* Test secure zero function */
void test_memory_secure_zero(void) {
    uint8_t buffer[64];
    
    /* Fill with test data */
    memset(buffer, 0xFF, sizeof(buffer));
    
    /* Verify it's filled */
    for (size_t i = 0; i < sizeof(buffer); i++) {
        TEST_ASSERT_EQUAL(0xFF, buffer[i]);
    }
    
    /* Secure zero */
    knishio_secure_zero(buffer, sizeof(buffer));
    
    /* Verify it's zeroed */
    for (size_t i = 0; i < sizeof(buffer); i++) {
        TEST_ASSERT_EQUAL(0, buffer[i]);
    }
}

/* Test reference counting */
void test_memory_reference_counting(void) {
    /* Create test data */
    char *data = knishio_strdup("Test data");
    TEST_ASSERT_NOT_NULL(data);
    
    /* Create reference */
    knishio_ref_t *ref = knishio_ref_create(data, (knishio_destructor_t)knishio_free);
    TEST_ASSERT_NOT_NULL(ref);
    TEST_ASSERT_EQUAL(1, knishio_ref_count(ref));
    TEST_ASSERT_TRUE(knishio_ref_is_valid(ref));
    TEST_ASSERT_EQUAL(data, knishio_ref_data(ref));
    
    /* Retain reference */
    TEST_ASSERT_EQUAL(2, knishio_ref_retain(ref));
    TEST_ASSERT_EQUAL(2, knishio_ref_count(ref));
    
    /* Release once */
    TEST_ASSERT_EQUAL(1, knishio_ref_release(ref));
    TEST_ASSERT_TRUE(knishio_ref_is_valid(ref));
    
    /* Release final reference - should destroy */
    TEST_ASSERT_EQUAL(0, knishio_ref_release(ref));
}

/* Test memory pool allocation */
void test_memory_pool_basic(void) {
    knishio_pool_t *pool = knishio_pool_create(1024, 8);
    TEST_ASSERT_NOT_NULL(pool);
    
    /* Allocate some memory */
    void *ptr1 = knishio_pool_alloc(pool, 64);
    TEST_ASSERT_NOT_NULL(ptr1);
    
    void *ptr2 = knishio_pool_alloc(pool, 128);
    TEST_ASSERT_NOT_NULL(ptr2);
    
    /* Pointers should be different */
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr2);
    
    /* Get pool statistics */
    size_t total, used, free;
    knishio_pool_stats(pool, &total, &used, &free);
    TEST_ASSERT_GREATER_THAN(0, total);
    TEST_ASSERT_GREATER_THAN(0, used);
    TEST_ASSERT_EQUAL(total - used, free);
    
    knishio_pool_destroy(pool);
}

/* Test pool reset functionality */
void test_memory_pool_reset(void) {
    knishio_pool_t *pool = knishio_pool_create(512, 8);
    TEST_ASSERT_NOT_NULL(pool);
    
    /* Allocate some memory */
    void *ptr = knishio_pool_alloc(pool, 256);
    TEST_ASSERT_NOT_NULL(ptr);
    
    /* Check used memory */
    size_t total, used, free;
    knishio_pool_stats(pool, &total, &used, &free);
    TEST_ASSERT_GREATER_THAN(0, used);
    
    /* Reset pool */
    knishio_pool_reset(pool);
    
    /* Used memory should be zero */
    knishio_pool_stats(pool, &total, &used, &free);
    TEST_ASSERT_EQUAL(0, used);
    TEST_ASSERT_EQUAL(total, free);
    
    knishio_pool_destroy(pool);
}

/* Test pool chaining (automatic expansion) */
void test_memory_pool_chaining(void) {
    knishio_pool_t *pool = knishio_pool_create(128, 8);
    TEST_ASSERT_NOT_NULL(pool);
    
    /* Allocate more than initial capacity */
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = knishio_pool_alloc(pool, 32);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }
    
    /* All pointers should be valid and different */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            TEST_ASSERT_NOT_EQUAL(ptrs[i], ptrs[j]);
        }
    }
    
    knishio_pool_destroy(pool);
}

/* Test calloc functionality */
void test_memory_pool_calloc(void) {
    knishio_pool_t *pool = knishio_pool_create(256, 8);
    TEST_ASSERT_NOT_NULL(pool);
    
    /* Allocate zeroed memory */
    uint8_t *ptr = (uint8_t*)knishio_pool_calloc(pool, 64);
    TEST_ASSERT_NOT_NULL(ptr);
    
    /* Verify it's zeroed */
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL(0, ptr[i]);
    }
    
    knishio_pool_destroy(pool);
}

#ifdef KNISHIO_DEBUG
/* Test memory leak detection (debug builds only) */
void test_memory_leak_detection(void) {
    size_t initial_allocations, initial_bytes;
    knishio_memory_stats(&initial_allocations, &initial_bytes);
    
    /* Allocate some memory */
    void *ptr1 = knishio_malloc(100);
    void *ptr2 = knishio_malloc(200);
    
    size_t current_allocations, current_bytes;
    knishio_memory_stats(&current_allocations, &current_bytes);
    
    /* Should have 2 more allocations */
    TEST_ASSERT_EQUAL(initial_allocations + 2, current_allocations);
    TEST_ASSERT_GREATER_OR_EQUAL(initial_bytes + 300, current_bytes);
    
    /* Free memory */
    knishio_free(ptr1);
    knishio_free(ptr2);
    
    knishio_memory_stats(&current_allocations, &current_bytes);
    TEST_ASSERT_EQUAL(initial_allocations, current_allocations);
    TEST_ASSERT_EQUAL(initial_bytes, current_bytes);
}
#endif

/* Memory test suite entry point */
void test_memory_suite(void) {
    RUN_TEST(test_memory_basic_allocation);
    RUN_TEST(test_memory_reallocation);
    RUN_TEST(test_memory_string_dup);
    RUN_TEST(test_memory_string_ndup);
    RUN_TEST(test_memory_secure_zero);
    RUN_TEST(test_memory_reference_counting);
    RUN_TEST(test_memory_pool_basic);
    RUN_TEST(test_memory_pool_reset);
    RUN_TEST(test_memory_pool_chaining);
    RUN_TEST(test_memory_pool_calloc);
    
#ifdef KNISHIO_DEBUG
    RUN_TEST(test_memory_leak_detection);
#endif
}