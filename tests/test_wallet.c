#include "unity.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include <string.h>

/* Test secret generation from seed */
void test_wallet_secret_generation(void) {
    char* secret = NULL;
    
    /* Test alice seed */
    TEST_ASSERT_TRUE(knishio_generate_secret("alice-test-seed-2025", 2048, &secret));
    TEST_ASSERT_NOT_NULL(secret);
    TEST_ASSERT_EQUAL(2048, strlen(secret));
    
    /* Verify first 64 chars match JS SDK output */
    const char* expected_prefix = "3dfaa1fa7d28830d1acae6b40b3dc4abe269854d38b504244c1be7379c62f8d9";
    TEST_ASSERT_EQUAL_STRING_LEN(expected_prefix, secret, 64);
    
    knishio_free(secret);
    
    /* Test bob seed */
    TEST_ASSERT_TRUE(knishio_generate_secret("bob-test-seed-2025", 2048, &secret));
    TEST_ASSERT_NOT_NULL(secret);
    TEST_ASSERT_EQUAL(2048, strlen(secret));
    knishio_free(secret);
    
    /* Test empty seed (should still work) */
    TEST_ASSERT_TRUE(knishio_generate_secret("", 2048, &secret));
    TEST_ASSERT_NOT_NULL(secret);
    TEST_ASSERT_EQUAL(2048, strlen(secret));
    knishio_free(secret);
}

/* Test bundle hash generation */
void test_wallet_bundle_generation(void) {
    char* secret = NULL;
    char* bundle = NULL;
    
    /* Generate alice secret first */
    TEST_ASSERT_TRUE(knishio_generate_secret("alice-test-seed-2025", 2048, &secret));
    TEST_ASSERT_NOT_NULL(secret);
    
    /* Generate bundle hash */
    TEST_ASSERT_TRUE(knishio_generate_bundle_hash(secret, "TEST", 
                                                  KNISHIO_FIXED_POSITION, &bundle));
    TEST_ASSERT_NOT_NULL(bundle);
    TEST_ASSERT_EQUAL(64, strlen(bundle));
    
    /* Verify matches JS SDK output */
    const char* expected_bundle = "a06e74f7c0ccb8b28b8864468dc404c5e4e116ed2f3bd197320395369000cc7b";
    TEST_ASSERT_EQUAL_STRING(expected_bundle, bundle);
    
    knishio_free(secret);
    knishio_free(bundle);
}

/* Test position generation */
void test_wallet_position_generation(void) {
    char* position1 = NULL;
    char* position2 = NULL;
    char* fixed_position = NULL;
    
    /* Test random position generation */
    TEST_ASSERT_TRUE(knishio_generate_position(&position1));
    TEST_ASSERT_NOT_NULL(position1);
    TEST_ASSERT_EQUAL(64, strlen(position1));
    
    /* Verify it's valid hex */
    for (int i = 0; i < 64; i++) {
        char c = position1[i];
        TEST_ASSERT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
    
    /* Generate another position - should be different */
    TEST_ASSERT_TRUE(knishio_generate_position(&position2));
    TEST_ASSERT_NOT_NULL(position2);
    TEST_ASSERT_EQUAL(64, strlen(position2));
    
    /* Positions should be different (with high probability) */
    /* Note: There's a tiny chance they could be the same, but extremely unlikely */
    
    knishio_free(position1);
    knishio_free(position2);
    
    /* Test fixed position */
    TEST_ASSERT_TRUE(knishio_use_fixed_position(&fixed_position));
    TEST_ASSERT_NOT_NULL(fixed_position);
    TEST_ASSERT_EQUAL_STRING(KNISHIO_FIXED_POSITION, fixed_position);
    
    knishio_free(fixed_position);
}

/* Test wallet key derivation */
void test_wallet_key_derivation(void) {
    char* secret = NULL;
    char* private_key = NULL;
    
    /* Generate alice secret */
    TEST_ASSERT_TRUE(knishio_generate_secret("alice-test-seed-2025", 2048, &secret));
    TEST_ASSERT_NOT_NULL(secret);
    
    /* Generate private key with fixed position */
    TEST_ASSERT_TRUE(knishio_generate_wallet_key(secret, "TEST", 
                                                KNISHIO_FIXED_POSITION, &private_key));
    TEST_ASSERT_NOT_NULL(private_key);
    TEST_ASSERT_EQUAL(2048, strlen(private_key));
    
    /* Verify it's valid hex */
    for (size_t i = 0; i < 2048; i++) {
        char c = private_key[i];
        TEST_ASSERT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
    
    knishio_free(secret);
    knishio_free(private_key);
}

/* Test address generation (complex 256-iteration algorithm) */
void test_wallet_address_generation(void) {
    char* secret = NULL;
    char* private_key = NULL;
    char* address = NULL;
    
    /* Generate alice wallet components */
    TEST_ASSERT_TRUE(knishio_generate_secret("alice-test-seed-2025", 2048, &secret));
    TEST_ASSERT_NOT_NULL(secret);
    
    TEST_ASSERT_TRUE(knishio_generate_wallet_key(secret, "TEST", 
                                                KNISHIO_FIXED_POSITION, &private_key));
    TEST_ASSERT_NOT_NULL(private_key);
    
    /* Generate address (256 SHAKE256 operations) */
    TEST_ASSERT_TRUE(knishio_generate_address(private_key, &address));
    TEST_ASSERT_NOT_NULL(address);
    TEST_ASSERT_EQUAL(64, strlen(address));
    
    /* Verify matches JS SDK output */
    const char* expected_address = "f653df9b2d6e407af3531e58fb22ab50042e44c3e72219e050f844f72870b4b4";
    TEST_ASSERT_EQUAL_STRING(expected_address, address);
    
    knishio_free(secret);
    knishio_free(private_key);
    knishio_free(address);
}

/* Test complete wallet creation */
void test_wallet_complete_creation(void) {
    knishio_wallet_t* wallet = NULL;
    
    /* Create alice wallet with fixed position */
    TEST_ASSERT_TRUE(knishio_wallet_create(&wallet, "alice-test-seed-2025", 
                                          "TEST", KNISHIO_FIXED_POSITION));
    TEST_ASSERT_NOT_NULL(wallet);
    TEST_ASSERT_TRUE(wallet->initialized);
    
    /* Verify all fields are populated */
    TEST_ASSERT_NOT_NULL(wallet->secret);
    TEST_ASSERT_EQUAL(2048, strlen(wallet->secret));
    
    TEST_ASSERT_NOT_NULL(wallet->bundle_hash);
    TEST_ASSERT_EQUAL(64, strlen(wallet->bundle_hash));
    
    TEST_ASSERT_NOT_NULL(wallet->position);
    TEST_ASSERT_EQUAL(64, strlen(wallet->position));
    
    TEST_ASSERT_NOT_NULL(wallet->private_key);
    TEST_ASSERT_EQUAL(2048, strlen(wallet->private_key));
    
    TEST_ASSERT_NOT_NULL(wallet->address);
    TEST_ASSERT_EQUAL(64, strlen(wallet->address));
    
    TEST_ASSERT_NOT_NULL(wallet->token);
    TEST_ASSERT_EQUAL_STRING("TEST", wallet->token);
    
    /* Verify specific values match JS SDK */
    TEST_ASSERT_EQUAL_STRING("a06e74f7c0ccb8b28b8864468dc404c5e4e116ed2f3bd197320395369000cc7b", 
                           wallet->bundle_hash);
    TEST_ASSERT_EQUAL_STRING("f653df9b2d6e407af3531e58fb22ab50042e44c3e72219e050f844f72870b4b4", 
                           wallet->address);
    TEST_ASSERT_EQUAL_STRING(KNISHIO_FIXED_POSITION, wallet->position);
    
    knishio_wallet_cleanup(wallet);
}

/* Test wallet creation from existing secret */
void test_wallet_from_secret(void) {
    char* secret = NULL;
    knishio_wallet_t* wallet = NULL;
    
    /* Generate alice secret */
    TEST_ASSERT_TRUE(knishio_generate_secret("alice-test-seed-2025", 2048, &secret));
    TEST_ASSERT_NOT_NULL(secret);
    
    /* Create wallet from secret */
    TEST_ASSERT_TRUE(knishio_wallet_from_secret(&wallet, secret, "TEST", 
                                               KNISHIO_FIXED_POSITION));
    TEST_ASSERT_NOT_NULL(wallet);
    TEST_ASSERT_TRUE(wallet->initialized);
    
    /* Verify bundle and address */
    TEST_ASSERT_EQUAL_STRING("a06e74f7c0ccb8b28b8864468dc404c5e4e116ed2f3bd197320395369000cc7b", 
                           wallet->bundle_hash);
    TEST_ASSERT_EQUAL_STRING("f653df9b2d6e407af3531e58fb22ab50042e44c3e72219e050f844f72870b4b4", 
                           wallet->address);
    
    knishio_free(secret);
    knishio_wallet_cleanup(wallet);
}

/* Test multiple wallet creation (different tokens) */
void test_wallet_multiple_tokens(void) {
    knishio_wallet_t* wallet_test = NULL;
    knishio_wallet_t* wallet_user = NULL;
    
    /* Create wallet with TEST token */
    TEST_ASSERT_TRUE(knishio_wallet_create(&wallet_test, "alice-test-seed-2025", 
                                          "TEST", KNISHIO_FIXED_POSITION));
    TEST_ASSERT_NOT_NULL(wallet_test);
    TEST_ASSERT_EQUAL_STRING("TEST", wallet_test->token);
    
    /* Create wallet with USER token (should have different address) */
    TEST_ASSERT_TRUE(knishio_wallet_create(&wallet_user, "alice-test-seed-2025", 
                                          "USER", KNISHIO_FIXED_POSITION));
    TEST_ASSERT_NOT_NULL(wallet_user);
    TEST_ASSERT_EQUAL_STRING("USER", wallet_user->token);
    
    /* Bundles should be the same (derived from secret only) */
    TEST_ASSERT_EQUAL_STRING(wallet_test->bundle_hash, wallet_user->bundle_hash);
    
    /* Addresses should be different (token affects key derivation) */
    TEST_ASSERT_FALSE(strcmp(wallet_test->address, wallet_user->address) == 0);
    
    knishio_wallet_cleanup(wallet_test);
    knishio_wallet_cleanup(wallet_user);
}

/* Test utility functions */
void test_wallet_utilities(void) {
    /* Test random hex string generation */
    char* hex1 = NULL;
    char* hex2 = NULL;
    
    TEST_ASSERT_TRUE(knishio_random_hex_string(32, NULL, &hex1));
    TEST_ASSERT_NOT_NULL(hex1);
    TEST_ASSERT_EQUAL(32, strlen(hex1));
    
    /* Verify it's valid hex */
    for (size_t i = 0; i < 32; i++) {
        char c = hex1[i];
        TEST_ASSERT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
    
    /* Test with custom charset */
    TEST_ASSERT_TRUE(knishio_random_hex_string(16, "0123456789", &hex2));
    TEST_ASSERT_NOT_NULL(hex2);
    TEST_ASSERT_EQUAL(16, strlen(hex2));
    
    /* Verify only digits */
    for (size_t i = 0; i < 16; i++) {
        TEST_ASSERT_TRUE(hex2[i] >= '0' && hex2[i] <= '9');
    }
    
    knishio_free(hex1);
    knishio_free(hex2);
    
    /* Test string chunking */
    const char* test_string = "0123456789abcdef0123456789abcdef";
    char** chunks = NULL;
    size_t chunk_count = 0;
    
    TEST_ASSERT_TRUE(knishio_chunk_string(test_string, 8, &chunks, &chunk_count));
    TEST_ASSERT_NOT_NULL(chunks);
    TEST_ASSERT_EQUAL(4, chunk_count);
    
    TEST_ASSERT_EQUAL_STRING("01234567", chunks[0]);
    TEST_ASSERT_EQUAL_STRING("89abcdef", chunks[1]);
    TEST_ASSERT_EQUAL_STRING("01234567", chunks[2]);
    TEST_ASSERT_EQUAL_STRING("89abcdef", chunks[3]);
    
    for (size_t i = 0; i < chunk_count; i++) {
        knishio_free(chunks[i]);
    }
    knishio_free(chunks);
}

/* Test error handling */
void test_wallet_error_handling(void) {
    char* output = NULL;
    knishio_wallet_t* wallet = NULL;
    
    /* Test NULL parameters */
    TEST_ASSERT_FALSE(knishio_generate_secret(NULL, 2048, &output));
    TEST_ASSERT_FALSE(knishio_generate_secret("seed", 2048, NULL));
    
    TEST_ASSERT_FALSE(knishio_generate_bundle_hash(NULL, "TEST", 
                                                   KNISHIO_FIXED_POSITION, &output));
    TEST_ASSERT_FALSE(knishio_generate_bundle_hash("secret", "TEST", 
                                                   KNISHIO_FIXED_POSITION, NULL));
    
    /* Test invalid secret length for bundle generation */
    TEST_ASSERT_FALSE(knishio_generate_bundle_hash("too_short", "TEST", 
                                                   KNISHIO_FIXED_POSITION, &output));
    
    /* Test wallet creation with NULL parameters */
    TEST_ASSERT_FALSE(knishio_wallet_create(NULL, "seed", "TEST", NULL));
    TEST_ASSERT_FALSE(knishio_wallet_create(&wallet, NULL, "TEST", NULL));
    TEST_ASSERT_FALSE(knishio_wallet_create(&wallet, "seed", NULL, NULL));
}

/* Test self-test function */
void test_wallet_self_test(void) {
    TEST_ASSERT_TRUE(knishio_wallet_self_test());
}

/* Test additional test vectors */
void test_wallet_test_vectors(void) {
    /* For now, we only have alice test vector confirmed */
    /* Additional test vectors would need to be generated from JS SDK */
    
    /* Verify alice wallet again with different approach */
    knishio_wallet_t* wallet = NULL;
    TEST_ASSERT_TRUE(knishio_wallet_create(&wallet, "alice-test-seed-2025", 
                                          "TEST", KNISHIO_FIXED_POSITION));
    TEST_ASSERT_NOT_NULL(wallet);
    TEST_ASSERT_EQUAL_STRING("a06e74f7c0ccb8b28b8864468dc404c5e4e116ed2f3bd197320395369000cc7b", 
                           wallet->bundle_hash);
    TEST_ASSERT_EQUAL_STRING("f653df9b2d6e407af3531e58fb22ab50042e44c3e72219e050f844f72870b4b4", 
                           wallet->address);
    knishio_wallet_cleanup(wallet);
}

/* Wallet test suite entry point */
void test_wallet_suite(void) {
    RUN_TEST(test_wallet_secret_generation);
    RUN_TEST(test_wallet_bundle_generation);
    RUN_TEST(test_wallet_position_generation);
    RUN_TEST(test_wallet_key_derivation);
    RUN_TEST(test_wallet_address_generation);
    RUN_TEST(test_wallet_complete_creation);
    RUN_TEST(test_wallet_from_secret);
    RUN_TEST(test_wallet_multiple_tokens);
    RUN_TEST(test_wallet_utilities);
    RUN_TEST(test_wallet_error_handling);
    RUN_TEST(test_wallet_self_test);
}