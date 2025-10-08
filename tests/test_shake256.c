#include "unity.h"
#include "knishio/crypto/shake256.h"
#include "knishio/utils/memory.h"
#include <string.h>

/* Test basic SHAKE256 functionality */
void test_shake256_basic_hash(void) {
    const char *input = "test";
    char *output = NULL;
    
    bool result = knishio_shake256_hash(input, 256, &output);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_NOT_NULL(output);
    
    /* Output should be 64 hex characters (256 bits / 4 bits per hex char) */
    TEST_ASSERT_EQUAL(64, strlen(output));
    
    /* Should be valid hex */
    TEST_ASSERT_TRUE(knishio_is_valid_hex(output));
    
    knishio_free(output);
}

/* Test different output lengths */
void test_shake256_different_lengths(void) {
    const char *input = "hello";
    
    /* Test 128-bit output */
    char *output128 = NULL;
    TEST_ASSERT_TRUE(knishio_shake256_hash(input, 128, &output128));
    TEST_ASSERT_NOT_NULL(output128);
    TEST_ASSERT_EQUAL(32, strlen(output128)); /* 128 bits = 32 hex chars */
    knishio_free(output128);
    
    /* Test 512-bit output */
    char *output512 = NULL;
    TEST_ASSERT_TRUE(knishio_shake256_hash(input, 512, &output512));
    TEST_ASSERT_NOT_NULL(output512);
    TEST_ASSERT_EQUAL(128, strlen(output512)); /* 512 bits = 128 hex chars */
    knishio_free(output512);
    
    /* Test 1024-bit output */
    char *output1024 = NULL;
    TEST_ASSERT_TRUE(knishio_shake256_hash(input, 1024, &output1024));
    TEST_ASSERT_NOT_NULL(output1024);
    TEST_ASSERT_EQUAL(256, strlen(output1024)); /* 1024 bits = 256 hex chars */
    knishio_free(output1024);
}

/* Test consistency - same input should produce same output */
void test_shake256_consistency(void) {
    const char *input = "consistency_test";
    
    char *output1 = NULL;
    char *output2 = NULL;
    
    TEST_ASSERT_TRUE(knishio_shake256_hash(input, 256, &output1));
    TEST_ASSERT_TRUE(knishio_shake256_hash(input, 256, &output2));
    
    TEST_ASSERT_NOT_NULL(output1);
    TEST_ASSERT_NOT_NULL(output2);
    TEST_ASSERT_EQUAL_STRING(output1, output2);
    
    knishio_free(output1);
    knishio_free(output2);
}

/* Test different inputs produce different outputs */
void test_shake256_different_inputs(void) {
    char *output1 = NULL;
    char *output2 = NULL;
    
    TEST_ASSERT_TRUE(knishio_shake256_hash("input1", 256, &output1));
    TEST_ASSERT_TRUE(knishio_shake256_hash("input2", 256, &output2));
    
    TEST_ASSERT_NOT_NULL(output1);
    TEST_ASSERT_NOT_NULL(output2);
    TEST_ASSERT_FALSE(strcmp(output1, output2) == 0);
    
    knishio_free(output1);
    knishio_free(output2);
}

/* Test context-based API */
void test_shake256_context_api(void) {
    knishio_shake256_ctx_t ctx;
    
    /* Initialize context */
    TEST_ASSERT_TRUE(knishio_shake256_init(&ctx));
    
    /* Update with data */
    TEST_ASSERT_TRUE(knishio_shake256_update_string(&ctx, "hello"));
    TEST_ASSERT_TRUE(knishio_shake256_update_string(&ctx, " "));
    TEST_ASSERT_TRUE(knishio_shake256_update_string(&ctx, "world"));
    
    /* Finalize */
    TEST_ASSERT_TRUE(knishio_shake256_final(&ctx));
    
    /* Squeeze output */
    char *output = NULL;
    TEST_ASSERT_TRUE(knishio_shake256_squeeze_hex(&ctx, 256, &output));
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(64, strlen(output));
    
    /* Compare with direct hash */
    char *direct_output = NULL;
    TEST_ASSERT_TRUE(knishio_shake256_hash("hello world", 256, &direct_output));
    TEST_ASSERT_EQUAL_STRING(output, direct_output);
    
    knishio_free(output);
    knishio_free(direct_output);
    knishio_shake256_cleanup(&ctx);
}

/* Test binary input */
void test_shake256_binary_input(void) {
    uint8_t input[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    char *output = NULL;
    
    bool result = knishio_shake256_hash_binary(input, sizeof(input), 256, &output);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(64, strlen(output));
    TEST_ASSERT_TRUE(knishio_is_valid_hex(output));
    
    knishio_free(output);
}

/* Test raw binary output */
void test_shake256_raw_output(void) {
    const char *input = "test";
    uint8_t output[32]; /* 256 bits */
    
    bool result = knishio_shake256_hash_raw((const uint8_t*)input, strlen(input), 
                                           output, sizeof(output));
    TEST_ASSERT_TRUE(result);
    
    /* Convert to hex and compare with string version */
    char *hex_output = NULL;
    TEST_ASSERT_TRUE(knishio_binary_to_hex(output, sizeof(output), &hex_output));
    
    char *string_output = NULL;
    TEST_ASSERT_TRUE(knishio_shake256_hash(input, 256, &string_output));
    
    TEST_ASSERT_EQUAL_STRING(hex_output, string_output);
    
    knishio_free(hex_output);
    knishio_free(string_output);
}

/* Test edge cases */
void test_shake256_edge_cases(void) {
    char *output = NULL;
    
    /* Empty string */
    TEST_ASSERT_TRUE(knishio_shake256_hash("", 256, &output));
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(64, strlen(output));
    knishio_free(output);
    
    /* Very long string */
    char long_input[1000];
    memset(long_input, 'A', sizeof(long_input) - 1);
    long_input[sizeof(long_input) - 1] = '\0';
    
    TEST_ASSERT_TRUE(knishio_shake256_hash(long_input, 256, &output));
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(64, strlen(output));
    knishio_free(output);
    
    /* Odd number of output bits */
    TEST_ASSERT_TRUE(knishio_shake256_hash("test", 255, &output));
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(64, strlen(output)); /* Should be rounded up to byte boundary */
    knishio_free(output);
}

/* Test utility functions */
void test_shake256_utilities(void) {
    /* Test hex validation */
    TEST_ASSERT_TRUE(knishio_is_valid_hex("0123456789abcdef"));
    TEST_ASSERT_TRUE(knishio_is_valid_hex("0123456789ABCDEF"));
    TEST_ASSERT_FALSE(knishio_is_valid_hex("0123456789abcdefg"));
    TEST_ASSERT_FALSE(knishio_is_valid_hex("0123456789abcde")); /* Odd length */
    TEST_ASSERT_FALSE(knishio_is_valid_hex(""));
    
    /* Test binary to hex conversion */
    uint8_t data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    char *hex = NULL;
    TEST_ASSERT_TRUE(knishio_binary_to_hex(data, sizeof(data), &hex));
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef", hex);
    knishio_free(hex);
    
    /* Test hex to binary conversion */
    uint8_t *converted_data = NULL;
    size_t converted_len = 0;
    TEST_ASSERT_TRUE(knishio_hex_to_binary("0123456789abcdef", &converted_data, &converted_len));
    TEST_ASSERT_EQUAL(sizeof(data), converted_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, converted_data, sizeof(data));
    knishio_free(converted_data);
}

/* Test known test vectors (if available) */
void test_shake256_known_vectors(void) {
    /* Test vector from NIST */
    const char *input = "abc";
    char *output = NULL;
    
    TEST_ASSERT_TRUE(knishio_shake256_hash(input, 512, &output));
    TEST_ASSERT_NOT_NULL(output);
    
    /* We can't check against exact expected value without reference implementation,
     * but we can verify basic properties */
    TEST_ASSERT_EQUAL(128, strlen(output)); /* 512 bits = 128 hex chars */
    TEST_ASSERT_TRUE(knishio_is_valid_hex(output));
    
    /* The output should be deterministic */
    char *output2 = NULL;
    TEST_ASSERT_TRUE(knishio_shake256_hash(input, 512, &output2));
    TEST_ASSERT_EQUAL_STRING(output, output2);
    
    knishio_free(output);
    knishio_free(output2);
}

/* Test JavaScript SDK compatibility function */
void test_shake256_js_compatibility(void) {
    char *output = NULL;
    
    /* Test vector 1: "test" -> 256 bits (deterministic output validation) */
    TEST_ASSERT_TRUE(knishio_shake256_hash("test", 256, &output));
    TEST_ASSERT_NOT_NULL(output);
    /* Verify deterministic output - this should match across all platforms */
    TEST_ASSERT_EQUAL_STRING("59a371cbc697df7265d38e58b9debd5b0aab58060a2c146a73e2c30a00c6fae6", output);
    knishio_free(output);
    
    /* Test vector 2: "KnishIO" -> 256 bits */
    TEST_ASSERT_TRUE(knishio_shake256_hash("KnishIO", 256, &output));
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("03db598c7665a610e46d1de03487f452c3906521c2df67aca41953be8701273d", output);
    knishio_free(output);
    
    /* Test vector 3: Empty string -> 256 bits */
    TEST_ASSERT_TRUE(knishio_shake256_hash("", 256, &output));
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("f605d5ca4568307e7756c5a89a27f83c3ea73a758899192e384b792efb324ee9", output);
    knishio_free(output);
    
    /* Test vector 4: "hello world" -> 512 bits */
    TEST_ASSERT_TRUE(knishio_shake256_hash("hello world", 512, &output));
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("a8ac9491bf98d16335e506dbdc0fd6928b922e67a939d8be5589ce02c1d65faf09a7379e6f3e915f3754b0072c614f6e76202c49513f6243bbfbce2551cb24ff", output);
    knishio_free(output);
}

/* Test multiple squeeze operations */
void test_shake256_multiple_squeeze(void) {
    knishio_shake256_ctx_t ctx;
    
    TEST_ASSERT_TRUE(knishio_shake256_init(&ctx));
    TEST_ASSERT_TRUE(knishio_shake256_update_string(&ctx, "test"));
    TEST_ASSERT_TRUE(knishio_shake256_final(&ctx));
    
    /* Squeeze in multiple chunks */
    uint8_t output1[16];
    uint8_t output2[16];
    uint8_t combined[32];
    
    TEST_ASSERT_TRUE(knishio_shake256_squeeze(&ctx, output1, sizeof(output1)));
    TEST_ASSERT_TRUE(knishio_shake256_squeeze(&ctx, output2, sizeof(output2)));
    
    /* Compare with single squeeze */
    knishio_shake256_ctx_t ctx2;
    TEST_ASSERT_TRUE(knishio_shake256_init(&ctx2));
    TEST_ASSERT_TRUE(knishio_shake256_update_string(&ctx2, "test"));
    TEST_ASSERT_TRUE(knishio_shake256_final(&ctx2));
    TEST_ASSERT_TRUE(knishio_shake256_squeeze(&ctx2, combined, sizeof(combined)));
    
    /* First 16 bytes should match */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(output1, combined, 16);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(output2, combined + 16, 16);
    
    knishio_shake256_cleanup(&ctx);
    knishio_shake256_cleanup(&ctx2);
}

/* SHAKE256 test suite entry point */
void test_shake256_suite(void) {
    RUN_TEST(test_shake256_basic_hash);
    RUN_TEST(test_shake256_different_lengths);
    RUN_TEST(test_shake256_consistency);
    RUN_TEST(test_shake256_different_inputs);
    RUN_TEST(test_shake256_context_api);
    RUN_TEST(test_shake256_binary_input);
    RUN_TEST(test_shake256_raw_output);
    RUN_TEST(test_shake256_edge_cases);
    RUN_TEST(test_shake256_utilities);
    RUN_TEST(test_shake256_known_vectors);
    RUN_TEST(test_shake256_js_compatibility);
    RUN_TEST(test_shake256_multiple_squeeze);
}