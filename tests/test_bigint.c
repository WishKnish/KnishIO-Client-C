#include "unity.h"
#include "knishio/crypto/bigint.h"
#include "knishio/utils/memory.h"
#include <string.h>

/* Test basic BigInt initialization */
void test_bigint_basic_init(void) {
    knishio_bigint_t bigint;
    
    TEST_ASSERT_TRUE(knishio_bigint_init(&bigint));
    TEST_ASSERT_TRUE(bigint.initialized);
    
    knishio_bigint_cleanup(&bigint);
    TEST_ASSERT_FALSE(bigint.initialized);
}

/* Test BigInt initialization from hex */
void test_bigint_init_hex(void) {
    knishio_bigint_t bigint;
    char *hex_result = NULL;
    
    /* Test basic hex initialization */
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&bigint, "1234567890abcdef"));
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&bigint, &hex_result));
    TEST_ASSERT_EQUAL_STRING("1234567890abcdef", hex_result);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&bigint);
    
    /* Test with 0x prefix */
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&bigint, "0x1234567890abcdef"));
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&bigint, &hex_result));
    TEST_ASSERT_EQUAL_STRING("1234567890abcdef", hex_result);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&bigint);
    
    /* Test uppercase hex */
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&bigint, "1234567890ABCDEF"));
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&bigint, &hex_result));
    TEST_ASSERT_EQUAL_STRING("1234567890abcdef", hex_result);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&bigint);
}

/* Test BigInt initialization from decimal */
void test_bigint_init_dec(void) {
    knishio_bigint_t bigint;
    char *dec_result = NULL;
    
    TEST_ASSERT_TRUE(knishio_bigint_init_dec(&bigint, "1234567890"));
    TEST_ASSERT_TRUE(knishio_bigint_to_dec(&bigint, &dec_result));
    TEST_ASSERT_EQUAL_STRING("1234567890", dec_result);
    
    knishio_free(dec_result);
    knishio_bigint_cleanup(&bigint);
}

/* Test BigInt initialization from unsigned long */
void test_bigint_init_ui(void) {
    knishio_bigint_t bigint;
    char *hex_result = NULL;
    
    TEST_ASSERT_TRUE(knishio_bigint_init_ui(&bigint, 0x12345678UL));
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&bigint, &hex_result));
    TEST_ASSERT_EQUAL_STRING("12345678", hex_result);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&bigint);
}

/* Test BigInt copy operation */
void test_bigint_copy(void) {
    knishio_bigint_t source, dest;
    char *hex_result = NULL;
    
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&source, "1234567890abcdef"));
    TEST_ASSERT_TRUE(knishio_bigint_copy(&dest, &source));
    
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&dest, &hex_result));
    TEST_ASSERT_EQUAL_STRING("1234567890abcdef", hex_result);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&source);
    knishio_bigint_cleanup(&dest);
}

/* Test BigInt addition */
void test_bigint_addition(void) {
    knishio_bigint_t a, b, result;
    char *hex_result = NULL;
    
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&a, "1000"));
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&b, "2000"));
    TEST_ASSERT_TRUE(knishio_bigint_init(&result));
    
    TEST_ASSERT_TRUE(knishio_bigint_add(&result, &a, &b));
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&result, &hex_result));
    TEST_ASSERT_EQUAL_STRING("3000", hex_result);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&a);
    knishio_bigint_cleanup(&b);
    knishio_bigint_cleanup(&result);
}

/* Test BigInt addition with unsigned long */
void test_bigint_addition_ui(void) {
    knishio_bigint_t a, result;
    char *hex_result = NULL;
    
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&a, "1000"));
    TEST_ASSERT_TRUE(knishio_bigint_init(&result));
    
    TEST_ASSERT_TRUE(knishio_bigint_add_ui(&result, &a, 0x1000UL));
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&result, &hex_result));
    TEST_ASSERT_EQUAL_STRING("2000", hex_result);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&a);
    knishio_bigint_cleanup(&result);
}

/* Test BigInt multiplication */
void test_bigint_multiplication(void) {
    knishio_bigint_t a, b, result;
    char *hex_result = NULL;
    
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&a, "1000"));
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&b, "10"));
    TEST_ASSERT_TRUE(knishio_bigint_init(&result));
    
    TEST_ASSERT_TRUE(knishio_bigint_mul(&result, &a, &b));
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&result, &hex_result));
    TEST_ASSERT_EQUAL_STRING("10000", hex_result);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&a);
    knishio_bigint_cleanup(&b);
    knishio_bigint_cleanup(&result);
}

/* Test BigInt multiplication with unsigned long */
void test_bigint_multiplication_ui(void) {
    knishio_bigint_t a, result;
    char *hex_result = NULL;
    
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&a, "1000"));
    TEST_ASSERT_TRUE(knishio_bigint_init(&result));
    
    TEST_ASSERT_TRUE(knishio_bigint_mul_ui(&result, &a, 16UL));
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&result, &hex_result));
    TEST_ASSERT_EQUAL_STRING("10000", hex_result);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&a);
    knishio_bigint_cleanup(&result);
}

/* Test BigInt division and modulo */
void test_bigint_divmod(void) {
    knishio_bigint_t dividend, quotient, remainder;
    char *hex_result = NULL;
    unsigned long rem_val;
    
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&dividend, "12345"));
    TEST_ASSERT_TRUE(knishio_bigint_init(&quotient));
    TEST_ASSERT_TRUE(knishio_bigint_init(&remainder));
    
    rem_val = knishio_bigint_divmod_ui(&quotient, &remainder, &dividend, 16UL);
    TEST_ASSERT_NOT_EQUAL(ULONG_MAX, rem_val);
    
    /* 0x12345 / 16 = 0x1234 remainder 5 */
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&quotient, &hex_result));
    TEST_ASSERT_EQUAL_STRING("1234", hex_result);
    TEST_ASSERT_EQUAL(5UL, rem_val);
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&dividend);
    knishio_bigint_cleanup(&quotient);
    knishio_bigint_cleanup(&remainder);
}

/* Test BigInt comparison operations */
void test_bigint_comparison(void) {
    knishio_bigint_t a, b, zero;
    
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&a, "1000"));
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&b, "2000"));
    TEST_ASSERT_TRUE(knishio_bigint_init_ui(&zero, 0UL));
    
    /* Test comparison between BigInts */
    TEST_ASSERT_TRUE(knishio_bigint_cmp(&a, &b) < 0);  /* a < b */
    TEST_ASSERT_TRUE(knishio_bigint_cmp(&b, &a) > 0);  /* b > a */
    TEST_ASSERT_TRUE(knishio_bigint_cmp(&a, &a) == 0); /* a == a */
    
    /* Test comparison with unsigned long */
    TEST_ASSERT_TRUE(knishio_bigint_cmp_ui(&a, 0x1000UL) == 0);
    TEST_ASSERT_TRUE(knishio_bigint_cmp_ui(&a, 0x999UL) > 0);
    TEST_ASSERT_TRUE(knishio_bigint_cmp_ui(&a, 0x2000UL) < 0);
    
    /* Test zero check */
    TEST_ASSERT_TRUE(knishio_bigint_is_zero(&zero));
    TEST_ASSERT_FALSE(knishio_bigint_is_zero(&a));
    
    knishio_bigint_cleanup(&a);
    knishio_bigint_cleanup(&b);
    knishio_bigint_cleanup(&zero);
}

/* Test large number operations (wallet key derivation scale) */
void test_bigint_large_numbers(void) {
    knishio_bigint_t secret, position, result;
    char *hex_result = NULL;
    
    /* Test with large 2048-bit-like numbers */
    const char *large_secret = "123456789abcdef0fedcba987654321023456789abcdef0fedcba9876543210123456789abcdef0fedcba987654321023456789abcdef0fedcba9876543210";
    const char *large_position = "1000000000000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000000000000";
    
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&secret, large_secret));
    TEST_ASSERT_TRUE(knishio_bigint_init_hex(&position, large_position));
    TEST_ASSERT_TRUE(knishio_bigint_init(&result));
    
    TEST_ASSERT_TRUE(knishio_bigint_add(&result, &secret, &position));
    TEST_ASSERT_TRUE(knishio_bigint_to_hex(&result, &hex_result));
    
    /* Verify result is not NULL and has reasonable length */
    TEST_ASSERT_NOT_NULL(hex_result);
    TEST_ASSERT_TRUE(strlen(hex_result) > 120); /* Should be large */
    
    knishio_free(hex_result);
    knishio_bigint_cleanup(&secret);
    knishio_bigint_cleanup(&position);
    knishio_bigint_cleanup(&result);
}

/* Test wallet key derivation function (BigInt specific) */
void test_bigint_wallet_key_derivation(void) {
    char *key_hex = NULL;
    
    /* Test basic wallet key derivation */
    const char *secret = "alice-secret-2025";
    const char *position = "W1";
    
    /* First convert strings to proper hex format (simulate SHAKE256 output) */
    const char *secret_hex = "123456789abcdef0fedcba987654321023456789abcdef0fedcba9876543210";
    const char *position_hex = "0000000000000000000000000000000000000000000000000000000000000001";
    
    TEST_ASSERT_TRUE(knishio_wallet_key_from_hex(secret_hex, position_hex, &key_hex));
    TEST_ASSERT_NOT_NULL(key_hex);
    TEST_ASSERT_EQUAL_STRING("123456789abcdef0fedcba987654321023456789abcdef0fedcba9876543211", key_hex);
    
    knishio_free(key_hex);
}

/* Test base conversion functionality */
void test_base_conversion(void) {
    char *output = NULL;
    
    /* Test binary to decimal conversion */
    TEST_ASSERT_TRUE(knishio_bigint_base_convert("1010", 2, 10, "01", "0123456789", &output));
    TEST_ASSERT_EQUAL_STRING("10", output);
    knishio_free(output);
    
    /* Test decimal to hex conversion */
    TEST_ASSERT_TRUE(knishio_bigint_base_convert("255", 10, 16, "0123456789", "0123456789abcdef", &output));
    TEST_ASSERT_EQUAL_STRING("ff", output);
    knishio_free(output);
    
    /* Test hex to decimal conversion */
    TEST_ASSERT_TRUE(knishio_bigint_base_convert("ff", 16, 10, "0123456789abcdef", "0123456789", &output));
    TEST_ASSERT_EQUAL_STRING("255", output);
    knishio_free(output);
}

/* Test JavaScript SDK compatibility vectors */
void test_js_compatibility_vectors(void) {
    char *result = NULL;
    
    /* Test vector 1: Basic addition */
    const char *secret1 = "1000000000000000000000000000000000000000000000000000000000000000";
    const char *position1 = "0000000000000000000000000000000000000000000000000000000000000001";
    const char *expected1 = "1000000000000000000000000000000000000000000000000000000000000001";
    
    TEST_ASSERT_TRUE(knishio_wallet_key_from_hex(secret1, position1, &result));
    TEST_ASSERT_EQUAL_STRING(expected1, result);
    knishio_free(result);
    
    /* Test vector 2: Larger addition */
    const char *secret2 = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    const char *position2 = "0000000000000000000000000000000000000000000000000000000000000001";
    const char *expected2 = "10000000000000000000000000000000000000000000000000000000000000000";
    
    TEST_ASSERT_TRUE(knishio_wallet_key_from_hex(secret2, position2, &result));
    TEST_ASSERT_EQUAL_STRING(expected2, result);
    knishio_free(result);
    
    /* Test vector 3: Complex position */
    const char *secret3 = "123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0";
    const char *position3 = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
    const char *expected3 = "11111111111111101111111111111110111111111111111011111111111111100";
    
    TEST_ASSERT_TRUE(knishio_wallet_key_from_hex(secret3, position3, &result));
    TEST_ASSERT_EQUAL_STRING(expected3, result);
    knishio_free(result);
}

/* Test error handling */
void test_bigint_error_handling(void) {
    knishio_bigint_t bigint;
    char *result = NULL;
    
    /* Test NULL parameters */
    TEST_ASSERT_FALSE(knishio_bigint_init(NULL));
    TEST_ASSERT_FALSE(knishio_bigint_init_hex(NULL, "1234"));
    TEST_ASSERT_FALSE(knishio_bigint_init_hex(&bigint, NULL));
    
    /* Test invalid hex string */
    TEST_ASSERT_FALSE(knishio_bigint_init_hex(&bigint, "gggg"));
    TEST_ASSERT_FALSE(knishio_bigint_init_hex(&bigint, "123g"));
    
    /* Test invalid decimal string */
    TEST_ASSERT_FALSE(knishio_bigint_init_dec(&bigint, "123a"));
    TEST_ASSERT_FALSE(knishio_bigint_init_dec(&bigint, "12.34"));
    
    /* Test wallet key derivation with invalid parameters */
    TEST_ASSERT_FALSE(knishio_wallet_key_from_hex(NULL, "1234", &result));
    TEST_ASSERT_FALSE(knishio_wallet_key_from_hex("1234", NULL, &result));
    TEST_ASSERT_FALSE(knishio_wallet_key_from_hex("1234", "5678", NULL));
    TEST_ASSERT_FALSE(knishio_wallet_key_from_hex("gggg", "1234", &result));
}

/* Test self-test function */
void test_bigint_self_test(void) {
    TEST_ASSERT_TRUE(knishio_bigint_self_test());
}

/* BigInt test suite entry point */
void test_bigint_suite(void) {
    RUN_TEST(test_bigint_basic_init);
    RUN_TEST(test_bigint_init_hex);
    RUN_TEST(test_bigint_init_dec);
    RUN_TEST(test_bigint_init_ui);
    RUN_TEST(test_bigint_copy);
    RUN_TEST(test_bigint_addition);
    RUN_TEST(test_bigint_addition_ui);
    RUN_TEST(test_bigint_multiplication);
    RUN_TEST(test_bigint_multiplication_ui);
    RUN_TEST(test_bigint_divmod);
    RUN_TEST(test_bigint_comparison);
    RUN_TEST(test_bigint_large_numbers);
    RUN_TEST(test_bigint_wallet_key_derivation);
    RUN_TEST(test_base_conversion);
    RUN_TEST(test_js_compatibility_vectors);
    RUN_TEST(test_bigint_error_handling);
    RUN_TEST(test_bigint_self_test);
}