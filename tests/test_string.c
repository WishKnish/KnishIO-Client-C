#include "unity.h"
#include "knishio/utils/string.h"
#include <string.h>

/* Test basic string creation and destruction */
void test_string_creation(void) {
    knishio_string_t *str = knishio_string_create();
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT_EQUAL(0, knishio_string_length(str));
    TEST_ASSERT_TRUE(knishio_string_is_empty(str));
    knishio_string_destroy(str);
}

/* Test string creation with initial content */
void test_string_creation_with_content(void) {
    const char *content = "Hello, World!";
    knishio_string_t *str = knishio_string_create_with_cstr(content);
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT_EQUAL(strlen(content), knishio_string_length(str));
    TEST_ASSERT_FALSE(knishio_string_is_empty(str));
    TEST_ASSERT_EQUAL_STRING(content, knishio_string_cstr(str));
    knishio_string_destroy(str);
}

/* Test string appending */
void test_string_append(void) {
    knishio_string_t *str = knishio_string_create_with_cstr("Hello");
    TEST_ASSERT_NOT_NULL(str);
    
    TEST_ASSERT_TRUE(knishio_string_append(str, ", "));
    TEST_ASSERT_TRUE(knishio_string_append(str, "World!"));
    
    TEST_ASSERT_EQUAL_STRING("Hello, World!", knishio_string_cstr(str));
    TEST_ASSERT_EQUAL(13, knishio_string_length(str));
    
    knishio_string_destroy(str);
}

/* Test string utilities */
void test_string_utilities(void) {
    knishio_string_t *str = knishio_string_create_with_cstr("1234567890abcdef");
    TEST_ASSERT_NOT_NULL(str);
    
    TEST_ASSERT_TRUE(knishio_string_is_hex(str));
    TEST_ASSERT_FALSE(knishio_string_is_numeric(str));
    
    knishio_string_destroy(str);
    
    str = knishio_string_create_with_cstr("1234567890");
    TEST_ASSERT_NOT_NULL(str);
    
    TEST_ASSERT_TRUE(knishio_string_is_hex(str));
    TEST_ASSERT_TRUE(knishio_string_is_numeric(str));
    
    knishio_string_destroy(str);
}

/* String test suite entry point */
void test_string_suite(void) {
    RUN_TEST(test_string_creation);
    RUN_TEST(test_string_creation_with_content);
    RUN_TEST(test_string_append);
    RUN_TEST(test_string_utilities);
}