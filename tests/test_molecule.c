#include "unity.h"
#include "knishio/molecule.h"
#include "knishio/atom.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"
#include <string.h>
#include <time.h>

/* Test molecule creation and basic properties */
void test_molecule_creation(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(
        &molecule,
        "test_secret_key_for_molecule_creation", 
        "test_bundle_hash_64chars_0123456789abcdef0123456789abcdef01234567",
        NULL,  /* source_wallet */
        NULL,  /* remainder_wallet */
        "testcell.knish.io",
        "1.0.0"
    );
    
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(molecule);
    
    /* Verify basic properties */
    TEST_ASSERT_NOT_NULL(molecule->secret);
    TEST_ASSERT_EQUAL_STRING("test_secret_key_for_molecule_creation", molecule->secret);
    
    TEST_ASSERT_NOT_NULL(molecule->bundle);
    TEST_ASSERT_EQUAL_STRING("test_bundle_hash_64chars_0123456789abcdef0123456789abcdef01234567", molecule->bundle);
    
    TEST_ASSERT_NOT_NULL(molecule->cell_slug);
    TEST_ASSERT_EQUAL_STRING("testcell.knish.io", molecule->cell_slug);
    
    TEST_ASSERT_NOT_NULL(molecule->version);
    TEST_ASSERT_EQUAL_STRING("1.0.0", molecule->version);
    
    /* Verify initial state */
    TEST_ASSERT_EQUAL(0, molecule->atom_count);
    TEST_ASSERT_GREATER_THAN(0, molecule->atom_capacity);
    TEST_ASSERT_FALSE(molecule->is_signed);
    TEST_ASSERT_FALSE(molecule->is_verified);
    TEST_ASSERT_EQUAL(KNISHIO_MOLECULE_STATUS_UNKNOWN, molecule->status);
    
    /* Verify dynamic timestamp (should be recent) */
    time_t now = time(NULL);
    TEST_ASSERT_GREATER_OR_EQUAL(now - 60, molecule->created_at); /* Within last minute */
    TEST_ASSERT_LESS_OR_EQUAL(now, molecule->created_at);
    
    knishio_molecule_free(molecule);
}

/* Test molecule creation with NULL parameters */
void test_molecule_creation_with_nulls(void) {
    knishio_molecule_t* molecule = NULL;
    
    /* Should succeed with minimal parameters */
    knishio_error_t result = knishio_molecule_create(&molecule, NULL, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(molecule);
    
    /* Verify NULL fields are handled properly */
    TEST_ASSERT_NULL(molecule->secret);
    TEST_ASSERT_NULL(molecule->bundle);
    TEST_ASSERT_NULL(molecule->cell_slug);
    TEST_ASSERT_NULL(molecule->version);
    TEST_ASSERT_NULL(molecule->source_wallet);
    TEST_ASSERT_NULL(molecule->remainder_wallet);
    
    knishio_molecule_free(molecule);
    
    /* Should fail with NULL molecule pointer */
    result = knishio_molecule_create(NULL, "secret", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_INVALID_ARGS, result);
}

/* Test adding atoms to molecule */
void test_molecule_add_atoms(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, "test_secret", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Create and add test atoms */
    for (int i = 0; i < 5; i++) {
        knishio_atom_t* atom = NULL;
        char position[64], address[64], value[32];
        snprintf(position, sizeof(position), "test_position_%d", i);
        snprintf(address, sizeof(address), "test_address_%d", i);
        snprintf(value, sizeof(value), "%d", i * 100);
        
        result = knishio_atom_create(
            &atom, position, address, KNISHIO_ISOTOPE_V,
            "TEST", value, NULL
        );
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
        
        result = knishio_molecule_add_atom(molecule, atom);
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    }
    
    /* Verify atoms were added */
    TEST_ASSERT_EQUAL(5, molecule->atom_count);
    
    /* Test accessing atoms by index */
    for (size_t i = 0; i < molecule->atom_count; i++) {
        knishio_atom_t* atom = knishio_molecule_get_atom(molecule, i);
        TEST_ASSERT_NOT_NULL(atom);
        TEST_ASSERT_EQUAL(i, atom->index);
    }
    
    /* Test invalid atom index */
    knishio_atom_t* invalid_atom = knishio_molecule_get_atom(molecule, 100);
    TEST_ASSERT_NULL(invalid_atom);
    
    knishio_molecule_free(molecule);
}

/* Test molecule hash generation */
void test_molecule_generate_hash(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, "test_secret", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Add some atoms first */
    for (int i = 0; i < 3; i++) {
        knishio_atom_t* atom = NULL;
        char position[64], address[64];
        snprintf(position, sizeof(position), "pos_%d", i);
        snprintf(address, sizeof(address), "addr_%d", i);
        
        result = knishio_atom_create(
            &atom, position, address, KNISHIO_ISOTOPE_V,
            "TEST", "100", NULL
        );
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
        
        knishio_molecule_add_atom(molecule, atom);
    }
    
    /* Generate molecular hash */
    TEST_ASSERT_NULL(molecule->molecular_hash);
    result = knishio_molecule_generate_hash(molecule);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Verify hash was generated */
    TEST_ASSERT_NOT_NULL(molecule->molecular_hash);
    TEST_ASSERT_EQUAL(KNISHIO_MOLECULAR_HASH_LENGTH, strlen(molecule->molecular_hash));
    
    /* Verify it's valid hex */
    for (int i = 0; i < KNISHIO_MOLECULAR_HASH_LENGTH; i++) {
        char c = molecule->molecular_hash[i];
        TEST_ASSERT_TRUE(
            (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
        );
    }
    
    /* Test hash consistency - same molecule should produce same hash */
    char* first_hash = knishio_strdup(molecule->molecular_hash);
    knishio_free(molecule->molecular_hash);
    molecule->molecular_hash = NULL;
    
    result = knishio_molecule_generate_hash(molecule);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING(first_hash, molecule->molecular_hash);
    
    knishio_free(first_hash);
    knishio_molecule_free(molecule);
}

/* Test molecule hash generation with empty molecule */
void test_molecule_generate_hash_empty(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, "test_secret", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Try to generate hash without atoms */
    result = knishio_molecule_generate_hash(molecule);
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_ATOMS_MISSING, result);
    
    knishio_molecule_free(molecule);
}

/* Test molecule validation (check function) */
void test_molecule_check_basic_validation(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, "test_secret", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Test validation with empty molecule */
    result = knishio_molecule_check(molecule, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_ATOMS_MISSING, result);
    
    /* Add atoms and generate hash */
    knishio_atom_t* atom = NULL;
    result = knishio_atom_create(
        &atom, "test_position", "test_address", KNISHIO_ISOTOPE_V,
        "TEST", "100", NULL
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    knishio_molecule_add_atom(molecule, atom);
    
    /* Generate hash */
    result = knishio_molecule_generate_hash(molecule);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Now validation should pass (for unsigned molecule) */
    result = knishio_molecule_check(molecule, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    knishio_molecule_free(molecule);
}

/* Test molecule validation with hash mismatch */
void test_molecule_check_hash_mismatch(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, "test_secret", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Add atom and generate hash */
    knishio_atom_t* atom = NULL;
    result = knishio_atom_create(
        &atom, "test_position", "test_address", KNISHIO_ISOTOPE_V,
        "TEST", "100", NULL
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    knishio_molecule_add_atom(molecule, atom);
    
    result = knishio_molecule_generate_hash(molecule);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Corrupt the hash */
    if (molecule->molecular_hash && strlen(molecule->molecular_hash) > 0) {
        molecule->molecular_hash[0] = (molecule->molecular_hash[0] == 'a') ? 'b' : 'a';
    }
    
    /* Validation should fail with hash mismatch */
    result = knishio_molecule_check(molecule, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_ERROR_MOLECULAR_HASH_MISMATCH, result);
    
    knishio_molecule_free(molecule);
}

/* Test molecule filter atoms by isotope */
void test_molecule_filter_atoms_by_isotope(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, "test_secret", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Add atoms with different isotopes */
    knishio_isotope_t isotopes[] = {KNISHIO_ISOTOPE_V, KNISHIO_ISOTOPE_C, KNISHIO_ISOTOPE_V, KNISHIO_ISOTOPE_M, KNISHIO_ISOTOPE_V};
    const char* isotope_strings[] = {"V", "C", "V", "M", "V"};
    
    for (int i = 0; i < 5; i++) {
        knishio_atom_t* atom = NULL;
        char position[64], address[64];
        snprintf(position, sizeof(position), "pos_%d", i);
        snprintf(address, sizeof(address), "addr_%d", i);
        
        result = knishio_atom_create(
            &atom, position, address, isotopes[i],
            "TEST", "100", NULL
        );
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
        knishio_molecule_add_atom(molecule, atom);
    }
    
    /* Filter by isotope V */
    knishio_atom_t** filtered_atoms = NULL;
    size_t filtered_count = 0;
    
    result = knishio_molecule_filter_atoms_by_isotope(molecule, "V", &filtered_atoms, &filtered_count);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_EQUAL(3, filtered_count); /* Should find 3 V isotopes */
    TEST_ASSERT_NOT_NULL(filtered_atoms);
    
    /* Verify all filtered atoms have isotope V */
    for (size_t i = 0; i < filtered_count; i++) {
        TEST_ASSERT_EQUAL(KNISHIO_ISOTOPE_V, filtered_atoms[i]->isotope);
    }
    
    knishio_free(filtered_atoms);
    
    /* Filter by isotope C */
    result = knishio_molecule_filter_atoms_by_isotope(molecule, "C", &filtered_atoms, &filtered_count);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_EQUAL(1, filtered_count); /* Should find 1 C isotope */
    
    knishio_free(filtered_atoms);
    
    /* Filter by non-existent isotope */
    result = knishio_molecule_filter_atoms_by_isotope(molecule, "X", &filtered_atoms, &filtered_count);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, filtered_count); /* Should find 0 matches */
    
    knishio_molecule_free(molecule);
}

/* Test molecule next index generation */
void test_molecule_generate_next_index(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, "test_secret", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Empty molecule should start at index 0 */
    size_t next_index = knishio_molecule_generate_next_index(molecule);
    TEST_ASSERT_EQUAL(0, next_index);
    
    /* Add some atoms */
    for (int i = 0; i < 3; i++) {
        knishio_atom_t* atom = NULL;
        char position[64], address[64];
        snprintf(position, sizeof(position), "pos_%d", i);
        snprintf(address, sizeof(address), "addr_%d", i);
        
        result = knishio_atom_create(
            &atom, position, address, KNISHIO_ISOTOPE_V,
            "TEST", "100", NULL
        );
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
        knishio_molecule_add_atom(molecule, atom);
        
        /* Check that next index increments properly */
        size_t expected_next = i + 1;
        next_index = knishio_molecule_generate_next_index(molecule);
        TEST_ASSERT_EQUAL(expected_next, next_index);
    }
    
    knishio_molecule_free(molecule);
}

/* Test molecule with signature validation */
void test_molecule_check_with_signature_validation(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, "test_secret", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Add atoms with OTS fragments to simulate signed molecule */
    for (int i = 0; i < 4; i++) { /* 4 atoms * 512 chars = 2048 total (minimum OTS length) */
        knishio_atom_t* atom = NULL;
        char position[64], address[64], fragment[513];
        snprintf(position, sizeof(position), "pos_%d", i);
        snprintf(address, sizeof(address), "addr_%d", i);
        
        /* Create test OTS fragment */
        for (int j = 0; j < 512; j++) {
            fragment[j] = 'a' + ((i + j) % 26);
        }
        fragment[512] = '\0';
        
        result = knishio_atom_create(
            &atom, position, address, KNISHIO_ISOTOPE_V,
            "TEST", "100", NULL
        );
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
        
        knishio_atom_set_ots_fragment(atom, fragment);
        knishio_molecule_add_atom(molecule, atom);
    }
    
    /* Generate molecular hash */
    result = knishio_molecule_generate_hash(molecule);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Test validation without sender wallet (should validate format only) */
    result = knishio_molecule_check(molecule, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result); /* Should pass basic format validation */
    
    /* Test validation with invalid sender wallet (signature will fail) */
    knishio_wallet_t* test_wallet = NULL;
    bool wallet_created = knishio_wallet_create(&test_wallet, "test_secret", "TEST", "W1");
    if (wallet_created && test_wallet) {
        result = knishio_molecule_check(molecule, test_wallet);
        /* This will likely fail because the OTS signature is fake */
        /* The exact error depends on where the verification fails first */
        TEST_ASSERT_NOT_EQUAL(KNISHIO_SUCCESS, result);
        
        knishio_wallet_free(test_wallet);
    }
    
    knishio_molecule_free(molecule);
}

/* Test molecule JSON serialization structure (basic test) */
void test_molecule_to_json_basic(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(
        &molecule, "test_secret", "test_bundle", NULL, NULL, "test.cell", "1.0"
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Add a test atom */
    knishio_atom_t* atom = NULL;
    result = knishio_atom_create(
        &atom, "test_position", "test_address", KNISHIO_ISOTOPE_V,
        "TEST", "100", NULL
    );
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    knishio_molecule_add_atom(molecule, atom);
    
    /* Generate hash */
    result = knishio_molecule_generate_hash(molecule);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Test JSON serialization */
    char* json_output = NULL;
    result = knishio_molecule_to_json(molecule, &json_output);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(json_output);
    
    /* Basic JSON structure validation */
    TEST_ASSERT_TRUE(strlen(json_output) > 0);
    TEST_ASSERT_NOT_NULL(strstr(json_output, "molecularHash"));
    TEST_ASSERT_NOT_NULL(strstr(json_output, "atoms"));
    TEST_ASSERT_NOT_NULL(strstr(json_output, "cellSlug"));
    
    knishio_free(json_output);
    knishio_molecule_free(molecule);
}

/* Test molecule memory management and cleanup */
void test_molecule_memory_management(void) {
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, "test_secret", "test_bundle", NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    
    /* Add multiple atoms to test array management */
    for (int i = 0; i < 10; i++) {
        knishio_atom_t* atom = NULL;
        char position[64], address[64];
        snprintf(position, sizeof(position), "pos_%d", i);
        snprintf(address, sizeof(address), "addr_%d", i);
        
        result = knishio_atom_create(
            &atom, position, address, KNISHIO_ISOTOPE_V,
            "TEST", "100", NULL
        );
        TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
        knishio_molecule_add_atom(molecule, atom);
    }
    
    /* Test capacity expansion */
    TEST_ASSERT_EQUAL(10, molecule->atom_count);
    TEST_ASSERT_GREATER_OR_EQUAL(10, molecule->atom_capacity);
    
    /* Generate hash to test hash memory management */
    result = knishio_molecule_generate_hash(molecule);
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(molecule->molecular_hash);
    
    /* Free molecule (this tests proper cleanup of all allocated memory) */
    knishio_molecule_free(molecule);
    
    /* Test freeing NULL molecule (should not crash) */
    knishio_molecule_free(NULL);
}

/**
 * @brief Molecular Validation and Signing Test Suite
 * 
 * Comprehensive test suite for molecule operations including hash generation,
 * atom management, validation, and integration with signature verification.
 */
void test_molecule_suite(void) {
    printf("\n--- Testing Molecule Creation and Properties ---\n");
    RUN_TEST(test_molecule_creation);
    RUN_TEST(test_molecule_creation_with_nulls);
    
    printf("\n--- Testing Atom Management ---\n");
    RUN_TEST(test_molecule_add_atoms);
    RUN_TEST(test_molecule_filter_atoms_by_isotope);
    RUN_TEST(test_molecule_generate_next_index);
    
    printf("\n--- Testing Molecular Hash Generation ---\n");
    RUN_TEST(test_molecule_generate_hash);
    RUN_TEST(test_molecule_generate_hash_empty);
    
    printf("\n--- Testing Molecule Validation ---\n");
    RUN_TEST(test_molecule_check_basic_validation);
    RUN_TEST(test_molecule_check_hash_mismatch);
    RUN_TEST(test_molecule_check_with_signature_validation);
    
    printf("\n--- Testing JSON Serialization ---\n");
    RUN_TEST(test_molecule_to_json_basic);
    
    printf("\n--- Testing Memory Management ---\n");
    RUN_TEST(test_molecule_memory_management);
    
    printf("\n✅ Molecular Validation and Signing Tests Complete\n");
    printf("   This test suite validates molecule operations and integration\n");
    printf("   with WOTS+ signature verification system\n");
}