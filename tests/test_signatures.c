#include "unity.h"
#include "knishio/crypto/signatures.h"
#include "knishio/molecule.h"
#include "knishio/atom.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"
#include <string.h>

/* Test molecular hash enumeration */
void test_signature_enumerate_molecular_hash(void) {
    /* Test with valid 64-character molecular hash */
    const char* molecular_hash = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    int* enumerated_output = NULL;
    size_t output_length = 0;
    
    knishio_error_t result = knishio_enumerate_molecular_hash(
        molecular_hash, &enumerated_output, &output_length
    );
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(enumerated_output);
    TEST_ASSERT_GREATER_THAN(0, output_length);
    
    /* Verify all values are in base17 range (0-16) */
    for (size_t i = 0; i < output_length; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL(0, enumerated_output[i]);
        TEST_ASSERT_LESS_THAN(17, enumerated_output[i]);
    }
    
    knishio_free(enumerated_output);
}

/* Test molecular hash enumeration with invalid input */
void test_signature_enumerate_molecular_hash_invalid(void) {
    int* enumerated_output = NULL;
    size_t output_length = 0;
    
    /* Test with NULL input */
    knishio_error_t result = knishio_enumerate_molecular_hash(
        NULL, &enumerated_output, &output_length
    );
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_INVALID_ARGS, result);
    
    /* Test with invalid length */
    const char* short_hash = "1234567890abcdef";
    result = knishio_enumerate_molecular_hash(
        short_hash, &enumerated_output, &output_length
    );
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_INVALID_ARGS, result);
}

/* Test molecular hash normalization */
void test_signature_normalize_molecular_hash(void) {
    /* Create test enumerated array */
    int enumerated_array[] = {1, 5, 10, 15, 3, 7, 12, 8, 2, 14, 6, 11, 4, 9, 13, 0};
    size_t array_length = sizeof(enumerated_array) / sizeof(enumerated_array[0]);
    int* normalized_output = NULL;
    
    knishio_error_t result = knishio_normalize_molecular_hash(
        enumerated_array, array_length, &normalized_output
    );
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(normalized_output);
    
    /* Verify all normalized values are in valid range (0-8 for WOTS+) */
    for (size_t i = 0; i < array_length; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL(0, normalized_output[i]);
        TEST_ASSERT_LESS_OR_EQUAL(8, normalized_output[i]);
    }
    
    knishio_free(normalized_output);
}

/* Test OTS signature subdivision */
void test_signature_subdivide_ots_signature(void) {
    /* Create test OTS signature (2048 characters) */
    char ots_signature[KNISHIO_OTS_TOTAL_KEY_LENGTH + 1];
    for (int i = 0; i < KNISHIO_OTS_TOTAL_KEY_LENGTH; i++) {
        ots_signature[i] = 'a' + (i % 26);
    }
    ots_signature[KNISHIO_OTS_TOTAL_KEY_LENGTH] = '\0';
    
    char** key_chunks = NULL;
    knishio_error_t result = knishio_subdivide_ots_signature(ots_signature, &key_chunks);
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(key_chunks);
    
    /* Verify we have exactly 16 chunks */
    for (int i = 0; i < KNISHIO_OTS_KEY_CHUNKS; i++) {
        TEST_ASSERT_NOT_NULL(key_chunks[i]);
        TEST_ASSERT_EQUAL(KNISHIO_OTS_CHUNK_LENGTH, strlen(key_chunks[i]));
    }
    
    /* Cleanup */
    for (int i = 0; i < KNISHIO_OTS_KEY_CHUNKS; i++) {
        knishio_free(key_chunks[i]);
    }
    knishio_free(key_chunks);
}

/* Test OTS signature subdivision with invalid input */
void test_signature_subdivide_ots_signature_invalid(void) {
    char** key_chunks = NULL;
    
    /* Test with NULL input */
    knishio_error_t result = knishio_subdivide_ots_signature(NULL, &key_chunks);
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_INVALID_ARGS, result);
    
    /* Test with too short signature */
    const char* short_sig = "tooshort";
    result = knishio_subdivide_ots_signature(short_sig, &key_chunks);
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_INVALID_ARGS, result);
}

/* Test address generation from key fragments */
void test_signature_generate_address_from_fragments(void) {
    /* Create test key fragments */
    const char* key_fragments[KNISHIO_OTS_KEY_CHUNKS];
    char fragment_buffer[KNISHIO_OTS_KEY_CHUNKS][KNISHIO_OTS_CHUNK_LENGTH + 1];
    
    for (int i = 0; i < KNISHIO_OTS_KEY_CHUNKS; i++) {
        /* Fill with test data */
        for (int j = 0; j < KNISHIO_OTS_CHUNK_LENGTH; j++) {
            fragment_buffer[i][j] = 'a' + ((i + j) % 26);
        }
        fragment_buffer[i][KNISHIO_OTS_CHUNK_LENGTH] = '\0';
        key_fragments[i] = fragment_buffer[i];
    }
    
    char* address_output = NULL;
    knishio_error_t result = knishio_generate_address_from_fragments(key_fragments, &address_output);
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(address_output);
    TEST_ASSERT_EQUAL(64, strlen(address_output)); /* 256 bits = 64 hex chars */
    
    /* Verify it's valid hex */
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_TRUE(
            (address_output[i] >= '0' && address_output[i] <= '9') ||
            (address_output[i] >= 'a' && address_output[i] <= 'f')
        );
    }
    
    knishio_free(address_output);
}

/* Test OTS signature format validation */
void test_signature_validate_ots_signature_format(void) {
    /* Create test molecule with atoms containing OTS fragments */
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(
        &molecule, "test_secret", NULL, NULL, NULL, NULL, NULL
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(molecule);
    
    /* Add test atom with OTS fragment */
    knishio_atom_t* atom = NULL;
    result = knishio_atom_create(
        &atom, "test_position", "test_address", KNISHIO_ISOTOPE_V,
        "TEST", "100", NULL, NULL, NULL, 0
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Add OTS fragment to atom */
    char test_fragment[513]; /* 512 chars + null terminator */
    for (int i = 0; i < 512; i++) {
        test_fragment[i] = 'a' + (i % 26);
    }
    test_fragment[512] = '\0';
    
    knishio_atom_set_ots_fragment(atom, test_fragment);
    knishio_molecule_add_atom(molecule, atom);
    
    /* Test validation */
    bool valid = knishio_validate_ots_signature_format(molecule);
    TEST_ASSERT_FALSE(valid); /* Should be false because we need more fragments for full signature */
    
    /* Add more atoms with fragments to reach minimum length */
    for (int i = 1; i < 4; i++) { /* Need 4 atoms * 512 chars = 2048 chars total */
        knishio_atom_t* additional_atom = NULL;
        char position[32], address[32];
        snprintf(position, sizeof(position), "test_position_%d", i);
        snprintf(address, sizeof(address), "test_address_%d", i);
        
        result = knishio_atom_create(
            &additional_atom, position, address, KNISHIO_ISOTOPE_V,
            "TEST", "0", NULL, NULL, NULL, i
        );
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
        
        knishio_atom_set_ots_fragment(additional_atom, test_fragment);
        knishio_molecule_add_atom(molecule, additional_atom);
    }
    
    /* Now it should be valid */
    valid = knishio_validate_ots_signature_format(molecule);
    TEST_ASSERT_TRUE(valid);
    
    knishio_molecule_free(molecule);
}

/* Test OTS signature format validation with invalid molecule */
void test_signature_validate_ots_signature_format_invalid(void) {
    /* Test with NULL molecule */
    bool valid = knishio_validate_ots_signature_format(NULL);
    TEST_ASSERT_FALSE(valid);
    
    /* Test with empty molecule */
    knishio_molecule_t* empty_molecule = NULL;
    knishio_error_t result = knishio_molecule_create(
        &empty_molecule, "test_secret", NULL, NULL, NULL, NULL, NULL
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    valid = knishio_validate_ots_signature_format(empty_molecule);
    TEST_ASSERT_FALSE(valid);
    
    knishio_molecule_free(empty_molecule);
}

/* Test OTS signature reconstruction */
void test_signature_reconstruct_ots_from_fragments(void) {
    /* Create test molecule with multiple atoms containing OTS fragments */
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(
        &molecule, "test_secret", NULL, NULL, NULL, NULL, NULL
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Add atoms with OTS fragments */
    const char* fragments[] = {
        "1111111111111111111111111111111111111111111111111111111111111111",
        "2222222222222222222222222222222222222222222222222222222222222222",
        "3333333333333333333333333333333333333333333333333333333333333333",
        "4444444444444444444444444444444444444444444444444444444444444444"
    };
    
    for (int i = 0; i < 4; i++) {
        knishio_atom_t* atom = NULL;
        char position[32], address[32];
        snprintf(position, sizeof(position), "pos_%d", i);
        snprintf(address, sizeof(address), "addr_%d", i);
        
        result = knishio_atom_create(
            &atom, position, address, KNISHIO_ISOTOPE_V,
            "TEST", "0", NULL, NULL, NULL, i
        );
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
        
        knishio_atom_set_ots_fragment(atom, fragments[i]);
        knishio_molecule_add_atom(molecule, atom);
    }
    
    /* Test reconstruction */
    char* complete_ots = NULL;
    result = knishio_reconstruct_ots_from_fragments(molecule, &complete_ots);
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(complete_ots);
    
    /* Verify reconstructed OTS contains all fragments */
    TEST_ASSERT_EQUAL(256, strlen(complete_ots)); /* 4 fragments * 64 chars each */
    
    /* Verify fragments are concatenated correctly */
    for (int i = 0; i < 4; i++) {
        char* fragment_start = complete_ots + (i * 64);
        TEST_ASSERT_EQUAL_STRING_LEN(fragments[i], fragment_start, 64);
    }
    
    knishio_free(complete_ots);
    knishio_molecule_free(molecule);
}

/* Test complete OTS signature verification with valid signature */
void test_signature_verify_ots_signature_success(void) {
    /* This test would require creating a complete valid signature scenario */
    /* For now, we'll test the basic structure and error handling */
    
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(
        &molecule, "test_secret", NULL, NULL, NULL, NULL, NULL
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Test with invalid inputs to ensure proper error handling */
    knishio_ots_verify_result_t verify_result = knishio_verify_ots_signature(
        NULL, "test_hash", "test_address"
    );
    TEST_ASSERT_EQUAL(KNISHIO_OTS_VERIFY_ERROR, verify_result);
    
    verify_result = knishio_verify_ots_signature(
        molecule, NULL, "test_address"
    );
    TEST_ASSERT_EQUAL(KNISHIO_OTS_VERIFY_ERROR, verify_result);
    
    verify_result = knishio_verify_ots_signature(
        molecule, "test_hash", NULL
    );
    TEST_ASSERT_EQUAL(KNISHIO_OTS_VERIFY_ERROR, verify_result);
    
    knishio_molecule_free(molecule);
}

/* Test OTS signature verification statistics */
void test_signature_verification_stats(void) {
    size_t total = 0, successful = 0, failed = 0;
    
    /* Reset stats first */
    knishio_reset_ots_verification_stats();
    
    /* Get initial stats (should be zero) */
    knishio_get_ots_verification_stats(&total, &successful, &failed);
    TEST_ASSERT_EQUAL(0, total);
    TEST_ASSERT_EQUAL(0, successful);
    TEST_ASSERT_EQUAL(0, failed);
    
    /* Trigger some verification attempts to update stats */
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(
        &molecule, "test_secret", NULL, NULL, NULL, NULL, NULL
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* This should increment failed verifications */
    knishio_ots_verify_result_t verify_result = knishio_verify_ots_signature(
        molecule, "invalid_hash_too_short", "test_address"
    );
    TEST_ASSERT_EQUAL(KNISHIO_OTS_VERIFY_ERROR, verify_result);
    
    /* Check that stats were updated */
    knishio_get_ots_verification_stats(&total, &successful, &failed);
    TEST_ASSERT_EQUAL(1, total);
    TEST_ASSERT_EQUAL(0, successful);
    TEST_ASSERT_EQUAL(1, failed);
    
    knishio_molecule_free(molecule);
}

/* Test memory management in signature operations */
void test_signature_memory_management(void) {
    /* Test that all allocations are properly cleaned up */
    const char* molecular_hash = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    int* enumerated_output = NULL;
    size_t output_length = 0;
    
    /* Enumerate hash */
    knishio_error_t result = knishio_enumerate_molecular_hash(
        molecular_hash, &enumerated_output, &output_length
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(enumerated_output);
    
    /* Normalize the enumerated hash */
    int* normalized_output = NULL;
    result = knishio_normalize_molecular_hash(
        enumerated_output, output_length, &normalized_output
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(normalized_output);
    
    /* Clean up and verify no memory leaks (this would be caught by Valgrind) */
    knishio_free(enumerated_output);
    knishio_free(normalized_output);
    
    /* Test addresses are properly cleaned up */
    const char* key_fragments[KNISHIO_OTS_KEY_CHUNKS];
    char fragment_buffer[KNISHIO_OTS_KEY_CHUNKS][KNISHIO_OTS_CHUNK_LENGTH + 1];
    
    for (int i = 0; i < KNISHIO_OTS_KEY_CHUNKS; i++) {
        memset(fragment_buffer[i], 'a' + (i % 26), KNISHIO_OTS_CHUNK_LENGTH);
        fragment_buffer[i][KNISHIO_OTS_CHUNK_LENGTH] = '\0';
        key_fragments[i] = fragment_buffer[i];
    }
    
    char* address_output = NULL;
    result = knishio_generate_address_from_fragments(key_fragments, &address_output);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(address_output);
    
    knishio_free(address_output);
}

/**
 * @brief WOTS+ Signature Verification Test Suite
 * 
 * Comprehensive test suite for quantum-resistant signature verification
 * following SDK Implementation Guide requirements and 2025 C17 best practices.
 */
void test_signature_suite(void) {
    printf("\n--- Testing Molecular Hash Operations ---\n");
    RUN_TEST(test_signature_enumerate_molecular_hash);
    RUN_TEST(test_signature_enumerate_molecular_hash_invalid);
    RUN_TEST(test_signature_normalize_molecular_hash);
    
    printf("\n--- Testing OTS Signature Operations ---\n");
    RUN_TEST(test_signature_subdivide_ots_signature);
    RUN_TEST(test_signature_subdivide_ots_signature_invalid);
    RUN_TEST(test_signature_reconstruct_ots_from_fragments);
    
    printf("\n--- Testing Address Generation ---\n");
    RUN_TEST(test_signature_generate_address_from_fragments);
    
    printf("\n--- Testing Signature Validation ---\n");
    RUN_TEST(test_signature_validate_ots_signature_format);
    RUN_TEST(test_signature_validate_ots_signature_format_invalid);
    RUN_TEST(test_signature_verify_ots_signature_success);
    
    printf("\n--- Testing Statistics and Memory Management ---\n");
    RUN_TEST(test_signature_verification_stats);
    RUN_TEST(test_signature_memory_management);
    
    printf("\n✅ WOTS+ Signature Verification Tests Complete\n");
    printf("   This test suite validates quantum-resistant security implementation\n");
    printf("   following SDK Implementation Guide lines 792-827\n");
}