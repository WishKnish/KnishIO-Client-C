/**
 * @file test_json.c
 * @brief JSON infrastructure tests for KnishIO C SDK
 */

#include "unity.h"
#include "knishio/json/parser.h"
#include "knishio/json/builder.h"
#include "knishio/json/serializers.h"
#include "knishio/utils/memory.h"
#include <string.h>

void setUp(void) {
    /* Setup for each test */
}

void tearDown(void) {
    /* Cleanup for each test */
}

/* Basic JSON parsing tests */
void test_json_parse_simple_string(void) {
    const char *json_str = "{\"key\": \"value\"}";
    knishio_json_t *json = knishio_json_parse(json_str, NULL);
    
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_EQUAL(KNISHIO_JSON_OBJECT, knishio_json_get_type(json));
    
    knishio_json_t *value = knishio_json_object_get(json, "key");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(KNISHIO_JSON_STRING, knishio_json_get_type(value));
    TEST_ASSERT_EQUAL_STRING("value", knishio_json_get_string(value));
    
    knishio_json_free(value);
    knishio_json_free(json);
}

/* Error handling tests */
void test_json_error_handling(void) {
    char *error_msg = NULL;
    
    /* Test invalid JSON */
    knishio_json_t *json = knishio_json_parse("{invalid json", &error_msg);
    TEST_ASSERT_NULL(json);
    TEST_ASSERT_NOT_NULL(error_msg);
    
    knishio_free(error_msg);
    
    /* Test NULL input */
    json = knishio_json_parse(NULL, &error_msg);
    TEST_ASSERT_NULL(json);
    TEST_ASSERT_NOT_NULL(error_msg);
    
    knishio_free(error_msg);
}

void test_json_validation(void) {
    /* Test valid JSON */
    TEST_ASSERT_TRUE(knishio_json_validate("{\"key\": \"value\"}", NULL));
    
    /* Test invalid JSON */
    TEST_ASSERT_FALSE(knishio_json_validate("{invalid", NULL));
    
    /* Test empty string */
    TEST_ASSERT_FALSE(knishio_json_validate("", NULL));
    
    /* Test NULL */
    TEST_ASSERT_FALSE(knishio_json_validate(NULL, NULL));
}

/* Test runner */
int main(void) {
    UNITY_BEGIN();
    
    /* Basic parsing tests */
    RUN_TEST(test_json_parse_simple_string);
    
    /* Error handling tests */
    RUN_TEST(test_json_error_handling);
    RUN_TEST(test_json_validation);
    
    return UNITY_END();
}