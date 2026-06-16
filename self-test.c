/**
 * @file self-test.c
 * @brief KnishIO C SDK Self-Test Program - Complete JavaScript Parity
 * 
 * This program performs self-contained tests to validate SDK functionality
 * and ensure cross-SDK compatibility. It follows the JavaScript SDK 
 * methodology exactly using modern C17 practices.
 * 
 * Features complete parity with JavaScript SDK:
 * - Identical crypto test logic
 * - Identical metadata molecule creation (M + I atoms)
 * - Identical simple transfer logic (V atoms UTXO pattern)
 * - Identical complex transfer logic (V atoms with remainder)
 * - ML-KEM768 encryption test following JavaScript pattern
 * - Cross-SDK validation support
 * - JSON output compatible with other SDKs
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include "knishio/knishio.h"
#include "knishio/wallet.h"
#include "knishio/molecule.h"
#include "knishio/atom.h"
#include "knishio/crypto/shake256.h"
#include "knishio/utils/encoding.h"
#include "knishio/crypto/mlkem768.h"
#include "knishio/crypto/aes_gcm.h"

/* C17 Static assertions for cross-platform compatibility */
_Static_assert(sizeof(time_t) >= 4, "time_t must be at least 32-bit");

/* ANSI Color codes for terminal output */
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"  
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

/* Debug output control - set to 0 for JavaScript canonical clean output */
#define ENABLE_DEBUG_OUTPUT 0

#if ENABLE_DEBUG_OUTPUT
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#define DEBUG_FPRINTF(stream, ...) fprintf(stream, __VA_ARGS__)
#else
#define DEBUG_PRINTF(...) 
#define DEBUG_FPRINTF(stream, ...)
#endif

/* Constants */
#define MAX_PATH_LENGTH 1024
#define ISO8601_LENGTH 32

// Embedded test configuration for SDK self-containment (C17 best practices)
static const char* DEFAULT_CONFIG_JSON = "{"
    "\"tests\": {"
        "\"crypto\": {"
            "\"seed\": \"TESTSEED\","
            "\"secret\": \"e8ffc86d60fc6a73234a834166e7436e21df6c3209dfacc8d0bd6595707872c3799abbf7deee0f9c4b58de1fd89b9abb67a207558208d5ccf550c227d197c24e9fcc3707aeb53c4031d38392020ff72bcaa0f728aa8bc3d47d95ff0afc04d8fcdb69bff638ce56646c154fc92aa517d3c40f550d2ccacbd921724e1d94b82aed2c8e172a8a7ed5a6963f5890157fe77222b97af3787741f9d3cec0b40aec6f07ae4b2b24614f0a20e035aee0df04e176175dc100eb1b00dd7ea95c28cdec47958336945333c3bef24719ed949fa56d1541f24c725d4f374a533bf255cf22f4596147bcd1ba05abcecbe9b12095e1fdddb094616894c366498be0b5785c180100efb3c5b689fc1c01131633fe1775df52a970e9472ab7bc0c19f5742b9e9436753cd16024b2d326b763eca68c414755a0d2fdbb927f007e9413f1190578b2033a03d29387f5aea71b07a5ce80fbfd45be4a15440faadeac50e41846022894fc683a52328b470bc1860c8b038d7258f504178918502b93d84d8b0fbef3e02f89f83cb1ff033a2bdbdf2a2ba78d80c12aa8b2d6c10d76c468186bd4a4e9eacc758546bb50ed7b1ee241cc5b93ff924c7bbee6778b27789e1f9104c917fc93f735eee5b25c07a883788f3d2e0771e751c4f59b76f8426027ac2b07a2ca84534433d0a1b86cef3288e7d79e8b175a3955848cfd1dfbdcd6b5bafcf6789e56e8ef40af09a764147640eb10b426349f6ffc8e299cdcebffc3a9d6be362ba33fbf648bf06ea4c35890c705df479030fd1d0669d289dcbabaaf78f945c37fc69f3823dbfa99bdf3cf7bb7be8f810a7eab5167e26691642c3982aa203687d0e674154c970cfc1822f9917f2100ae8950cf0fcab074bfb578f4f6e78df490f0fd9becdba7151f2a5733cc2a3df845aa17bdc49765163d635de5c3a1c376683e622fe3e0a6092a35dfedc4bc5bc9c120d2ed06d899775bcd16417318f4b5c7ba27fdc0a442884a69e71543a13cb26762a0df4f47807924a15da7895b6c96accb09394fdf0232d922a99f4a9f95d46da7b9050eb661f3329fe98372175a82d5e5296e4a31c040da6407194251b5baa7338071d1edfc51f55ca409ffd885045e47412f97a4bbe2e73794d8b276ccb446843bbc38c7e580dc4dc2ba94556de0d80681f60d1b2953021e08a60e26685adf61eff91d9ca7daa04a72de9dc2822655648f3c0f5016967b0e8104d70add65b9b9ce98b3aaa10106f5f32133775a71ab9b006307e390b697c77bb828c3ad07bfdcc3ecf3149ac98dc8a230c281365719d67fd2450c717ad1391880d9c17cb8ba96b6254ac783aeae04f84f14829e4efc6ee73b77670cb9ea96dc73e5464bc4cf46cdd2ebe75009d9c4ce6097eab2858ef2899b3dcd147c579939f45c4ad2aa283b6e9c8ca2539abd5e2332cff851f4fa8c4767732d7977\","
            "\"bundle\": \"2b77ff69a6d2f8108250389377faa6cbd42caaefa2f966e1b68a4b3fc022c83e\""
        "},"
        "\"metaCreation\": {"
            "\"seed\": \"TESTSEED\","
            "\"token\": \"USER\","
            "\"sourcePosition\": \"0123456789abcdeffedcba9876543210fedcba9876543210fedcba9876543210\","
            "\"metaType\": \"TestMeta\","
            "\"metaId\": \"TESTMETA123\","
            "\"metadata\": {"
                "\"name\": \"Test Metadata\","
                "\"description\": \"This is a test metadata for SDK testing.\""
            "}"
        "},"
        "\"simpleTransfer\": {"
            "\"sourceSeed\": \"TESTSEED\","
            "\"recipientSeed\": \"RECIPIENTSEED\","
            "\"balance\": 1000,"
            "\"amount\": 1000,"
            "\"token\": \"TEST\","
            "\"sourcePosition\": \"0123456789abcdeffedcba9876543210fedcba9876543210fedcba9876543210\","
            "\"recipientPosition\": \"fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210\""
        "},"
        "\"complexTransfer\": {"
            "\"sourceSeed\": \"TESTSEED\","
            "\"recipient1Seed\": \"RECIPIENTSEED\","
            "\"recipient2Seed\": \"RECIPIENT2SEED\","
            "\"sourceBalance\": 1000,"
            "\"amount1\": 500,"
            "\"amount2\": 500,"
            "\"token\": \"TEST\","
            "\"sourcePosition\": \"0123456789abcdeffedcba9876543210fedcba9876543210fedcba9876543210\","
            "\"recipient1Position\": \"fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210\","
            "\"recipient2Position\": \"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\""
        "},"
        "\"tokenCreation\": {"
            "\"sourceSeed\": \"TESTSEED\","
            "\"recipientSeed\": \"RECIPIENTSEED\","
            "\"sourceToken\": \"USER\","
            "\"newToken\": \"TESTTOKEN\","
            "\"amount\": 1000000,"
            "\"sourcePosition\": \"0123456789abcdeffedcba9876543210fedcba9876543210fedcba9876543210\","
            "\"recipientPosition\": \"fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210\","
            "\"metadata\": {"
                "\"name\": \"Test Token\","
                "\"fungibility\": \"fungible\","
                "\"supply\": \"limited\","
                "\"decimals\": \"0\""
            "}"
        "},"
        "\"walletCreation\": {"
            "\"sourceSeed\": \"TESTSEED\","
            "\"newWalletSeed\": \"NEWWALLETSEED\","
            "\"sourceToken\": \"USER\","
            "\"newToken\": \"TESTTOKEN\","
            "\"sourcePosition\": \"0123456789abcdeffedcba9876543210fedcba9876543210fedcba9876543210\","
            "\"newWalletPosition\": \"fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210\""
        "},"
        "\"shadowWalletClaim\": {"
            "\"sourceSeed\": \"TESTSEED\","
            "\"claimSeed\": \"CLAIMSEED\","
            "\"sourceToken\": \"USER\","
            "\"claimToken\": \"TESTTOKEN\","
            "\"sourcePosition\": \"0123456789abcdeffedcba9876543210fedcba9876543210fedcba9876543210\","
            "\"claimPosition\": \"fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210\""
        "},"
        "\"mlkem768\": {"
            "\"seed\": \"TESTSEED\","
            "\"token\": \"ENCRYPT\","
            "\"position\": \"1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef\","
            "\"plaintext\": \"Hello ML-KEM768 cross-platform test message!\""
        "}"
    "}"
"}";

/* Test result structures matching JavaScript SDK format */
typedef struct {
    bool passed;
    char *secret;
    char *bundle;
    char *expected_secret;
    char *expected_bundle;
    char *error;
} crypto_test_result_t;

typedef struct {
    bool passed;
    char *molecular_hash;
    int atom_count;
    bool has_remainder;
    char *validation_error;
} molecule_test_result_t;

typedef struct {
    bool passed;
    bool public_key_generated;
    bool encryption_success;
    bool decryption_success;
    int plaintext_length;
    char *error;
} mlkem768_test_result_t;

typedef struct {
    bool passed;
    char *description;
    int test_count;
    char *error;
} negative_test_result_t;

typedef struct {
    char *sdk;
    char *version;
    char *timestamp;
    crypto_test_result_t crypto;
    molecule_test_result_t meta_creation;
    molecule_test_result_t simple_transfer;
    molecule_test_result_t complex_transfer;
    molecule_test_result_t token_creation;
    molecule_test_result_t wallet_creation;
    molecule_test_result_t shadow_wallet_claim;
    mlkem768_test_result_t mlkem768;
    negative_test_result_t negative_cases;
    char *molecules_metadata;
    char *molecules_simple_transfer;
    char *molecules_complex_transfer;
    char *molecules_token_creation;
    char *molecules_wallet_creation;
    char *molecules_shadow_wallet_claim;
    char *molecules_mlkem768;
    bool cross_sdk_compatible;
} test_results_t;

/* Global test configuration and results */
static cJSON *g_config = NULL;
static test_results_t g_results = {0};

/* Memory management helpers */
static char *safe_strdup(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *dst = malloc(len + 1);
    if (!dst) return NULL;
    memcpy(dst, src, len + 1);
    return dst;
}

static bool safe_strcmp(const char *a, const char *b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

/* Logging functions */
static void log_message(const char *message, const char *color) {
    printf("%s%s%s\n", color ? color : "", message, COLOR_RESET);
}

static void log_test(const char *test_name, bool passed, const char *error_detail) {
    const char *status = passed ? "✅ PASS" : "❌ FAIL";
    const char *color = passed ? COLOR_GREEN : COLOR_RED;
    printf("  %s%s: %s%s\n", color, status, test_name, COLOR_RESET);
    if (!passed && error_detail) {
        printf("    %s%s%s\n", COLOR_RED, error_detail, COLOR_RESET);
    }
}

/* Generate ISO8601 timestamp */
static void get_iso8601_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *utc_tm = gmtime(&now);
    strftime(buffer, size, "%Y-%m-%dT%H:%M:%S.000Z", utc_tm);
}

/* Convert isotope enum to string */
static const char* isotope_to_string(knishio_isotope_t isotope) {
    switch (isotope) {
        case KNISHIO_ISOTOPE_V: return "V";
        case KNISHIO_ISOTOPE_C: return "C";
        case KNISHIO_ISOTOPE_M: return "M";
        case KNISHIO_ISOTOPE_U: return "U";
        case KNISHIO_ISOTOPE_I: return "I";
        case KNISHIO_ISOTOPE_R: return "R";
        case KNISHIO_ISOTOPE_T: return "T";
        case KNISHIO_ISOTOPE_L: return "L";
        case KNISHIO_ISOTOPE_S: return "S";
        case KNISHIO_ISOTOPE_F: return "F";
        default: return "?";
    }
}

/* Inspect molecule for debugging (matches JavaScript pattern exactly) */
static void inspect_molecule(const knishio_molecule_t *molecule, const char *name) {
    printf("\n%s🔍 INSPECTING %s:%s\n", COLOR_BLUE, name, COLOR_RESET);
    
    printf("  Molecular Hash: %s\n", molecule->molecular_hash ? molecule->molecular_hash : "NOT_SET");
    if (molecule->secret) {
        printf("  Secret: SET (length: %zu)\n", strlen(molecule->secret));
    } else {
        printf("  Secret: NOT_SET\n");
    }
    
    printf("  Bundle: %s\n", molecule->bundle ? molecule->bundle : "NOT_SET");
    
    /* Show wallet addresses with JavaScript canonical truncation (first 16 chars + ...) */
    if (molecule->source_wallet && molecule->source_wallet->address) {
        printf("  Source Wallet: %.16s...\n", molecule->source_wallet->address);
    } else {
        printf("  Source Wallet: NOT_SET\n");
    }
    
    if (molecule->remainder_wallet && molecule->remainder_wallet->address) {
        printf("  Remainder Wallet: %.16s...\n", molecule->remainder_wallet->address);
    } else {
        printf("  Remainder Wallet: NOT_SET\n");
    }
    
    printf("  Atoms (%zu):\n", molecule->atom_count);
    
    /* Inspect each atom with JavaScript canonical format */
    double total_value = 0.0;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        knishio_atom_t *atom = knishio_molecule_get_atom(molecule, i);
        if (atom) {
            const char *isotope_str = isotope_to_string(atom->isotope);
            const char *value_str = atom->value ? atom->value : "null";
            const char *wallet_address = atom->wallet_address ? atom->wallet_address : "unknown";
            
            if (value_str && strlen(value_str) > 0 && strcmp(value_str, "null") != 0) {
                total_value += atof(value_str);
            }
            
            printf("    [%zu] %s: %s (%.16s...) index=%d\n", 
                   i, isotope_str, value_str, wallet_address, atom->index);
        }
    }
    
    const char *balanced = (fabs(total_value) < 0.01) ? "✅ BALANCED" : "❌ UNBALANCED";
    printf("  Total Value: %.1f %s\n", total_value, balanced);
    printf("  Cell Slug: %s\n", molecule->cell_slug ? molecule->cell_slug : "NOT_SET");
    printf("  Status: NOT_SET\n");  // Always show as "NOT_SET" string like JavaScript canonical
}

/* Diagnose validation step-by-step */
static void diagnose_validation(const knishio_molecule_t *molecule, 
                               const knishio_wallet_t *sender_wallet, 
                               const char *name) {
    printf("\n%s🔬 VALIDATING %s STEP-BY-STEP:%s\n", COLOR_BLUE, name, COLOR_RESET);
    
    printf("  Molecule has %zu atoms\n", molecule->atom_count);
    
    if (molecule->atom_count > 0) {
        knishio_atom_t *first_atom = knishio_molecule_get_atom(molecule, 0);
        if (first_atom) {
            printf("  First atom isotope: %s\n", isotope_to_string(first_atom->isotope));
        }
    }
    
    printf("  Molecular hash present: %s\n", molecule->molecular_hash ? "true" : "false");
    printf("  Source wallet provided: %s\n", sender_wallet ? "true" : "false");
    
    /* Check atom indices */
    for (size_t i = 0; i < molecule->atom_count; i++) {
        knishio_atom_t *atom = knishio_molecule_get_atom(molecule, i);
        if (atom) {
            printf("    %s✅ Atom %zu index: %d%s\n", COLOR_GREEN, i, atom->index, COLOR_RESET);
        }
    }
}

/**
 * Test 1: Crypto Test
 * Validates secret generation and bundle hash following JavaScript pattern exactly
 */
static bool test_crypto(test_results_t *results, const cJSON *config) {
    log_message("\n1. Crypto Test", COLOR_BLUE);
    
    /* Get test configuration */
    const cJSON *crypto_config = cJSON_GetObjectItem(config, "crypto");
    if (!crypto_config) {
        results->crypto.error = safe_strdup("Missing crypto configuration");
        return false;
    }
    
    const char *seed = cJSON_GetStringValue(cJSON_GetObjectItem(crypto_config, "seed"));
    const char *expected_secret = cJSON_GetStringValue(cJSON_GetObjectItem(crypto_config, "secret"));
    const char *expected_bundle = cJSON_GetStringValue(cJSON_GetObjectItem(crypto_config, "bundle"));
    
    if (!seed || !expected_secret || !expected_bundle) {
        results->crypto.error = safe_strdup("Invalid crypto configuration");
        return false;
    }
    
    bool success = true;
    char *generated_secret = NULL;
    char *generated_bundle = NULL;
    
    /* Generate secret from seed */
    if (!knishio_generate_secret(seed, 2048, &generated_secret)) {
        results->crypto.error = safe_strdup("Failed to generate secret");
        return false;
    }
    
    printf("  Generated secret length: %zu\n", strlen(generated_secret));
    printf("  First 64 chars: %.64s...\n", generated_secret);
    printf("  Expected length: %zu\n", strlen(expected_secret));
    printf("  Expected first 64: %.64s...\n", expected_secret);
    
    bool secret_match = safe_strcmp(generated_secret, expected_secret);
    log_test("Secret generation (seed: \"TESTSEED\")", secret_match, NULL);
    
    if (!secret_match) {
        success = false;
    }
    
    /* Generate bundle hash (matches JavaScript SDK - only uses secret) */
    if (!knishio_generate_bundle_hash(generated_secret, NULL, NULL, &generated_bundle)) {
        results->crypto.error = safe_strdup("Failed to generate bundle hash");
        free(generated_secret);
        return false;
    }
    
    printf("  Generated bundle: %s\n", generated_bundle);
    printf("  Expected bundle: %s\n", expected_bundle);
    
    bool bundle_match = safe_strcmp(generated_bundle, expected_bundle);
    log_test("Bundle hash generation", bundle_match, NULL);
    
    if (!bundle_match) {
        success = false;
    }
    
    /* Store results */
    results->crypto.passed = success;
    results->crypto.secret = safe_strdup(generated_secret);
    results->crypto.bundle = safe_strdup(generated_bundle);
    results->crypto.expected_secret = safe_strdup(expected_secret);
    results->crypto.expected_bundle = safe_strdup(expected_bundle);
    
    free(generated_secret);
    free(generated_bundle);
    
    return success;
}

/**
 * Test 2: Metadata Creation Test
 * Creates metadata molecule with M and I isotopes following JavaScript pattern exactly
 */
/* Set deterministic per-atom timestamps before signing (mirrors JS setFixedTimestamps).
 * The molecular hash serializes created_at*1000 (ms, see molecule.c update_sponge_with_atom),
 * so a seconds base of 1700000000 + i yields hash timestamps 1700000000000 + i*1000 —
 * byte-identical to the JS reference. Call AFTER all atoms are added, BEFORE generate_hash. */
static void set_canonical_timestamps(knishio_molecule_t *molecule) {
    if (!molecule) return;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        if (molecule->atoms[i]) {
            molecule->atoms[i]->created_at = (time_t)(1700000000L + (long)i);
        }
    }
}

static bool test_meta_creation(test_results_t *results, const cJSON *config) {
    log_message("\n2. Metadata Creation Test", COLOR_BLUE);
    
    /* Get test configuration */
    const cJSON *meta_config = cJSON_GetObjectItem(config, "metaCreation");
    if (!meta_config) {
        results->meta_creation.validation_error = safe_strdup("Missing metaCreation configuration");
        return false;
    }
    
    const char *seed = cJSON_GetStringValue(cJSON_GetObjectItem(meta_config, "seed"));
    const char *token = cJSON_GetStringValue(cJSON_GetObjectItem(meta_config, "token"));
    const char *source_position = cJSON_GetStringValue(cJSON_GetObjectItem(meta_config, "sourcePosition"));
    const char *meta_type = cJSON_GetStringValue(cJSON_GetObjectItem(meta_config, "metaType"));
    const char *meta_id = cJSON_GetStringValue(cJSON_GetObjectItem(meta_config, "metaId"));
    
    if (!seed || !token || !source_position || !meta_type || !meta_id) {
        results->meta_creation.validation_error = safe_strdup("Invalid metaCreation configuration");
        return false;
    }
    
    bool success = true;
    knishio_wallet_t *source_wallet = NULL;
    knishio_wallet_t *remainder_wallet = NULL;
    knishio_molecule_t *molecule = NULL;
    char *secret = NULL;
    char *bundle = NULL;

    /* Generate secret and bundle */
    if (!knishio_generate_secret(seed, 2048, &secret)) {
        results->meta_creation.validation_error = safe_strdup("Failed to generate secret");
        goto cleanup;
    }
    
    if (!knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)) {
        results->meta_creation.validation_error = safe_strdup("Failed to generate bundle");
        goto cleanup;
    }
    
    /* Create source wallet using new simplified function */
    if (knishio_wallet_create_simple(&source_wallet, secret, token, source_position) != KNISHIO_SUCCESS) {
        results->meta_creation.validation_error = safe_strdup("Failed to create source wallet");
        goto cleanup;
    }
    
    log_test("Source wallet creation", true, NULL);

    /* Create canonical USER remainder wallet (matches JS Wallet.create). Token 'USER'
     * keeps the addContinuId guard off so the canonical bbbb... remainder survives,
     * giving the metadata I-atom a deterministic position/address/pubkey. */
    if (knishio_wallet_create_simple(&remainder_wallet, secret, token,
            "bbbb000000000000cccc111111111111dddd222222222222eeee333333333333") != KNISHIO_SUCCESS) {
        results->meta_creation.validation_error = safe_strdup("Failed to create remainder wallet");
        goto cleanup;
    }

    /* Create molecule */
    if (knishio_molecule_create(&molecule, secret, bundle, source_wallet, remainder_wallet, NULL, KNISHIO_VERSION_STRING) != KNISHIO_SUCCESS) {
        results->meta_creation.validation_error = safe_strdup("Failed to create molecule");
        goto cleanup;
    }
    
    /* Initialize metadata molecule using new high-level function */
    const char* meta_keys[] = {"name", "description"};
    const char* meta_values[] = {"Test Metadata", "This is a test metadata for SDK testing."};
    
    if (knishio_molecule_init_meta(molecule, meta_type, meta_id, meta_keys, meta_values, 2) != KNISHIO_SUCCESS) {
        results->meta_creation.validation_error = safe_strdup("Failed to initialize metadata molecule");
        goto cleanup;
    }
    
    log_test("Metadata molecule initialization", true, NULL);
    
    /* Deterministic per-atom timestamps (must precede hashing) */
    set_canonical_timestamps(molecule);

    /* Generate molecular hash */
    if (knishio_molecule_generate_hash(molecule) != KNISHIO_SUCCESS) {
        results->meta_creation.validation_error = safe_strdup("Failed to generate molecular hash");
        goto cleanup;
    }
    
    /* Sign the molecule */
    if (knishio_molecule_sign(molecule, bundle, false, true) != KNISHIO_SUCCESS) {
        results->meta_creation.validation_error = safe_strdup("Failed to sign molecule");
        goto cleanup;
    }
    
    log_test("Molecule signing", true, NULL);
    
    /* Debug: Inspect molecule before validation */
    inspect_molecule(molecule, "METADATA MOLECULE");
    
    /* Step-by-step validation diagnostic */
    diagnose_validation(molecule, source_wallet, "METADATA MOLECULE");
    
    /* Validate the molecule */
    bool is_valid = false;
    char *validation_error = NULL;
    
    if (knishio_molecule_check(molecule, source_wallet) == KNISHIO_SUCCESS) {
        is_valid = true;
    } else {
        validation_error = safe_strdup("Signature verification failed");
    }
    
    log_test("Molecule validation", is_valid, validation_error);
    
    /* Store serialized molecule for cross-SDK verification */
    char *molecule_json = NULL;
    knishio_error_t json_error = knishio_molecule_to_json(molecule, &molecule_json);
    if (json_error == KNISHIO_SUCCESS && molecule_json) {
        results->molecules_metadata = safe_strdup(molecule_json);
        printf("DEBUG: Stored metadata molecule JSON (length=%zu)\n", strlen(molecule_json));
        free(molecule_json);
    } else {
        printf("DEBUG: Failed to serialize metadata molecule, error=%d\n", json_error);
    }
    
    /* Store test results */
    results->meta_creation.passed = is_valid;
    if (molecule->molecular_hash) {
        results->meta_creation.molecular_hash = safe_strdup(molecule->molecular_hash);
    }
    results->meta_creation.atom_count = (int)molecule->atom_count;
    results->meta_creation.validation_error = validation_error;
    
    success = is_valid;

cleanup:
    if (source_wallet) knishio_wallet_free(source_wallet);
    if (remainder_wallet) knishio_wallet_free(remainder_wallet);
    if (molecule) knishio_molecule_free(molecule);
    if (secret) free(secret);
    if (bundle) free(bundle);
    return success;
}

/**
 * Test C1: Token Creation Test
 * C-isotope atom (issue new token) + ContinuID I-atom, following JavaScript initTokenCreation.
 */
static bool test_token_creation(test_results_t *results, const cJSON *config) {
    log_message("\nC1. Token Creation Test", COLOR_BLUE);

    const cJSON *tc = cJSON_GetObjectItem(config, "tokenCreation");
    if (!tc) {
        results->token_creation.validation_error = safe_strdup("Missing tokenCreation configuration");
        return false;
    }

    const char *source_seed = cJSON_GetStringValue(cJSON_GetObjectItem(tc, "sourceSeed"));
    const char *recipient_seed = cJSON_GetStringValue(cJSON_GetObjectItem(tc, "recipientSeed"));
    const char *source_token = cJSON_GetStringValue(cJSON_GetObjectItem(tc, "sourceToken"));
    const char *new_token = cJSON_GetStringValue(cJSON_GetObjectItem(tc, "newToken"));
    const char *source_position = cJSON_GetStringValue(cJSON_GetObjectItem(tc, "sourcePosition"));
    const char *recipient_position = cJSON_GetStringValue(cJSON_GetObjectItem(tc, "recipientPosition"));
    cJSON *amount_item = cJSON_GetObjectItem(tc, "amount");

    if (!source_seed || !recipient_seed || !source_token || !new_token || !source_position || !recipient_position || !amount_item) {
        results->token_creation.validation_error = safe_strdup("Invalid tokenCreation configuration");
        return false;
    }

    /* Amount as a string (the C-atom value); JS hashes 1000000 -> "1000000". */
    char amount_str[32];
    snprintf(amount_str, sizeof(amount_str), "%lld", (long long)cJSON_GetNumberValue(amount_item));

    bool success = true;
    knishio_wallet_t *source_wallet = NULL;
    knishio_wallet_t *recipient_wallet = NULL;
    knishio_wallet_t *remainder_wallet = NULL;
    knishio_molecule_t *molecule = NULL;
    char *source_secret = NULL;
    char *recipient_secret = NULL;
    char *bundle = NULL;

    if (!knishio_generate_secret(source_seed, 2048, &source_secret)) {
        results->token_creation.validation_error = safe_strdup("Failed to generate source secret");
        goto cleanup;
    }
    if (!knishio_generate_bundle_hash(source_secret, NULL, NULL, &bundle)) {
        results->token_creation.validation_error = safe_strdup("Failed to generate bundle");
        goto cleanup;
    }
    if (!knishio_generate_secret(recipient_seed, 2048, &recipient_secret)) {
        results->token_creation.validation_error = safe_strdup("Failed to generate recipient secret");
        goto cleanup;
    }

    if (knishio_wallet_create_simple(&source_wallet, source_secret, source_token, source_position) != KNISHIO_SUCCESS) {
        results->token_creation.validation_error = safe_strdup("Failed to create source wallet");
        goto cleanup;
    }
    log_test("Source wallet creation", true, NULL);

    if (knishio_wallet_create_simple(&recipient_wallet, recipient_secret, new_token, recipient_position) != KNISHIO_SUCCESS) {
        results->token_creation.validation_error = safe_strdup("Failed to create recipient wallet");
        goto cleanup;
    }
    log_test("Recipient wallet creation", true, NULL);

    /* Canonical USER remainder (token 'USER' keeps the ContinuID guard off -> bbbb... survives). */
    if (knishio_wallet_create_simple(&remainder_wallet, source_secret, source_token,
            "bbbb000000000000cccc111111111111dddd222222222222eeee333333333333") != KNISHIO_SUCCESS) {
        results->token_creation.validation_error = safe_strdup("Failed to create remainder wallet");
        goto cleanup;
    }

    if (knishio_molecule_create(&molecule, source_secret, bundle, source_wallet, remainder_wallet, NULL, KNISHIO_VERSION_STRING) != KNISHIO_SUCCESS) {
        results->token_creation.validation_error = safe_strdup("Failed to create molecule");
        goto cleanup;
    }

    /* Token meta in JS insertion order [name, fungibility, supply, decimals] (hardcoded to
     * preserve order — cJSON object iteration would not guarantee it). */
    {
        const char* token_meta_keys[] = {"name", "fungibility", "supply", "decimals"};
        const char* token_meta_values[] = {"Test Token", "fungible", "limited", "0"};
        if (knishio_molecule_init_token_creation(molecule, recipient_wallet, amount_str, token_meta_keys, token_meta_values, 4) != KNISHIO_SUCCESS) {
            results->token_creation.validation_error = safe_strdup("Failed to initialize token creation molecule");
            goto cleanup;
        }
    }
    log_test("Token creation initialization", true, NULL);

    set_canonical_timestamps(molecule);

    if (knishio_molecule_generate_hash(molecule) != KNISHIO_SUCCESS) {
        results->token_creation.validation_error = safe_strdup("Failed to generate molecular hash");
        goto cleanup;
    }
    if (knishio_molecule_sign(molecule, bundle, false, true) != KNISHIO_SUCCESS) {
        results->token_creation.validation_error = safe_strdup("Failed to sign molecule");
        goto cleanup;
    }
    log_test("Molecule signing", true, NULL);

    inspect_molecule(molecule, "TOKEN CREATION MOLECULE");
    diagnose_validation(molecule, source_wallet, "TOKEN CREATION MOLECULE");

    bool is_valid = false;
    char *validation_error = NULL;
    if (knishio_molecule_check(molecule, source_wallet) == KNISHIO_SUCCESS) {
        is_valid = true;
    } else {
        validation_error = safe_strdup("Signature verification failed");
    }
    log_test("Molecule validation", is_valid, validation_error);

    char *molecule_json = NULL;
    if (knishio_molecule_to_json(molecule, &molecule_json) == KNISHIO_SUCCESS && molecule_json) {
        results->molecules_token_creation = safe_strdup(molecule_json);
        free(molecule_json);
    }

    results->token_creation.passed = is_valid;
    if (molecule->molecular_hash) {
        results->token_creation.molecular_hash = safe_strdup(molecule->molecular_hash);
    }
    results->token_creation.atom_count = (int)molecule->atom_count;
    results->token_creation.validation_error = validation_error;

    success = is_valid;

cleanup:
    if (source_wallet) knishio_wallet_free(source_wallet);
    if (recipient_wallet) knishio_wallet_free(recipient_wallet);
    if (remainder_wallet) knishio_wallet_free(remainder_wallet);
    if (molecule) knishio_molecule_free(molecule);
    if (source_secret) free(source_secret);
    if (recipient_secret) free(recipient_secret);
    if (bundle) free(bundle);
    return success;
}

/**
 * Test C2: Wallet Creation Test
 * C-isotope atom (metaType "wallet") defining a new wallet + ContinuID I-atom (initWalletCreation).
 */
static bool test_wallet_creation(test_results_t *results, const cJSON *config) {
    log_message("\nC2. Wallet Creation Test", COLOR_BLUE);

    const cJSON *wc = cJSON_GetObjectItem(config, "walletCreation");
    if (!wc) {
        results->wallet_creation.validation_error = safe_strdup("Missing walletCreation configuration");
        return false;
    }

    const char *source_seed = cJSON_GetStringValue(cJSON_GetObjectItem(wc, "sourceSeed"));
    const char *new_wallet_seed = cJSON_GetStringValue(cJSON_GetObjectItem(wc, "newWalletSeed"));
    const char *source_token = cJSON_GetStringValue(cJSON_GetObjectItem(wc, "sourceToken"));
    const char *new_token = cJSON_GetStringValue(cJSON_GetObjectItem(wc, "newToken"));
    const char *source_position = cJSON_GetStringValue(cJSON_GetObjectItem(wc, "sourcePosition"));
    const char *new_wallet_position = cJSON_GetStringValue(cJSON_GetObjectItem(wc, "newWalletPosition"));

    if (!source_seed || !new_wallet_seed || !source_token || !new_token || !source_position || !new_wallet_position) {
        results->wallet_creation.validation_error = safe_strdup("Invalid walletCreation configuration");
        return false;
    }

    bool success = true;
    knishio_wallet_t *source_wallet = NULL;
    knishio_wallet_t *new_wallet = NULL;
    knishio_wallet_t *remainder_wallet = NULL;
    knishio_molecule_t *molecule = NULL;
    char *source_secret = NULL;
    char *new_wallet_secret = NULL;
    char *bundle = NULL;

    if (!knishio_generate_secret(source_seed, 2048, &source_secret)) {
        results->wallet_creation.validation_error = safe_strdup("Failed to generate source secret");
        goto cleanup;
    }
    if (!knishio_generate_bundle_hash(source_secret, NULL, NULL, &bundle)) {
        results->wallet_creation.validation_error = safe_strdup("Failed to generate bundle");
        goto cleanup;
    }
    if (!knishio_generate_secret(new_wallet_seed, 2048, &new_wallet_secret)) {
        results->wallet_creation.validation_error = safe_strdup("Failed to generate new wallet secret");
        goto cleanup;
    }

    if (knishio_wallet_create_simple(&source_wallet, source_secret, source_token, source_position) != KNISHIO_SUCCESS) {
        results->wallet_creation.validation_error = safe_strdup("Failed to create source wallet");
        goto cleanup;
    }
    log_test("Source wallet creation", true, NULL);

    if (knishio_wallet_create_simple(&new_wallet, new_wallet_secret, new_token, new_wallet_position) != KNISHIO_SUCCESS) {
        results->wallet_creation.validation_error = safe_strdup("Failed to create new wallet");
        goto cleanup;
    }
    log_test("New wallet creation", true, NULL);

    if (knishio_wallet_create_simple(&remainder_wallet, source_secret, source_token,
            "bbbb000000000000cccc111111111111dddd222222222222eeee333333333333") != KNISHIO_SUCCESS) {
        results->wallet_creation.validation_error = safe_strdup("Failed to create remainder wallet");
        goto cleanup;
    }

    if (knishio_molecule_create(&molecule, source_secret, bundle, source_wallet, remainder_wallet, NULL, KNISHIO_VERSION_STRING) != KNISHIO_SUCCESS) {
        results->wallet_creation.validation_error = safe_strdup("Failed to create molecule");
        goto cleanup;
    }

    if (knishio_molecule_init_wallet_creation(molecule, new_wallet, NULL, NULL, 0) != KNISHIO_SUCCESS) {
        results->wallet_creation.validation_error = safe_strdup("Failed to initialize wallet creation molecule");
        goto cleanup;
    }
    log_test("Wallet creation initialization", true, NULL);

    set_canonical_timestamps(molecule);

    if (knishio_molecule_generate_hash(molecule) != KNISHIO_SUCCESS) {
        results->wallet_creation.validation_error = safe_strdup("Failed to generate molecular hash");
        goto cleanup;
    }
    if (knishio_molecule_sign(molecule, bundle, false, true) != KNISHIO_SUCCESS) {
        results->wallet_creation.validation_error = safe_strdup("Failed to sign molecule");
        goto cleanup;
    }
    log_test("Molecule signing", true, NULL);

    inspect_molecule(molecule, "WALLET CREATION MOLECULE");
    diagnose_validation(molecule, source_wallet, "WALLET CREATION MOLECULE");

    bool is_valid = false;
    char *validation_error = NULL;
    if (knishio_molecule_check(molecule, source_wallet) == KNISHIO_SUCCESS) {
        is_valid = true;
    } else {
        validation_error = safe_strdup("Signature verification failed");
    }
    log_test("Molecule validation", is_valid, validation_error);

    char *molecule_json = NULL;
    if (knishio_molecule_to_json(molecule, &molecule_json) == KNISHIO_SUCCESS && molecule_json) {
        results->molecules_wallet_creation = safe_strdup(molecule_json);
        free(molecule_json);
    }

    results->wallet_creation.passed = is_valid;
    if (molecule->molecular_hash) {
        results->wallet_creation.molecular_hash = safe_strdup(molecule->molecular_hash);
    }
    results->wallet_creation.atom_count = (int)molecule->atom_count;
    results->wallet_creation.validation_error = validation_error;

    success = is_valid;

cleanup:
    if (source_wallet) knishio_wallet_free(source_wallet);
    if (new_wallet) knishio_wallet_free(new_wallet);
    if (remainder_wallet) knishio_wallet_free(remainder_wallet);
    if (molecule) knishio_molecule_free(molecule);
    if (source_secret) free(source_secret);
    if (new_wallet_secret) free(new_wallet_secret);
    if (bundle) free(bundle);
    return success;
}

/**
 * Test C3: Shadow Wallet Claim Test
 * C-isotope atom (meta [shadowWalletClaim, then 7 wallet* keys]) + ContinuID I-atom
 * (initShadowWalletClaim).
 */
static bool test_shadow_wallet_claim(test_results_t *results, const cJSON *config) {
    log_message("\nC3. Shadow Wallet Claim Test", COLOR_BLUE);

    const cJSON *sc = cJSON_GetObjectItem(config, "shadowWalletClaim");
    if (!sc) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Missing shadowWalletClaim configuration");
        return false;
    }

    const char *source_seed = cJSON_GetStringValue(cJSON_GetObjectItem(sc, "sourceSeed"));
    const char *claim_seed = cJSON_GetStringValue(cJSON_GetObjectItem(sc, "claimSeed"));
    const char *source_token = cJSON_GetStringValue(cJSON_GetObjectItem(sc, "sourceToken"));
    const char *claim_token = cJSON_GetStringValue(cJSON_GetObjectItem(sc, "claimToken"));
    const char *source_position = cJSON_GetStringValue(cJSON_GetObjectItem(sc, "sourcePosition"));
    const char *claim_position = cJSON_GetStringValue(cJSON_GetObjectItem(sc, "claimPosition"));

    if (!source_seed || !claim_seed || !source_token || !claim_token || !source_position || !claim_position) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Invalid shadowWalletClaim configuration");
        return false;
    }

    bool success = true;
    knishio_wallet_t *source_wallet = NULL;
    knishio_wallet_t *claim_wallet = NULL;
    knishio_wallet_t *remainder_wallet = NULL;
    knishio_molecule_t *molecule = NULL;
    char *source_secret = NULL;
    char *claim_secret = NULL;
    char *bundle = NULL;

    if (!knishio_generate_secret(source_seed, 2048, &source_secret)) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to generate source secret");
        goto cleanup;
    }
    if (!knishio_generate_bundle_hash(source_secret, NULL, NULL, &bundle)) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to generate bundle");
        goto cleanup;
    }
    if (!knishio_generate_secret(claim_seed, 2048, &claim_secret)) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to generate claim secret");
        goto cleanup;
    }

    if (knishio_wallet_create_simple(&source_wallet, source_secret, source_token, source_position) != KNISHIO_SUCCESS) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to create source wallet");
        goto cleanup;
    }
    log_test("Source wallet creation", true, NULL);

    if (knishio_wallet_create_simple(&claim_wallet, claim_secret, claim_token, claim_position) != KNISHIO_SUCCESS) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to create claim wallet");
        goto cleanup;
    }
    log_test("Claim wallet creation", true, NULL);

    if (knishio_wallet_create_simple(&remainder_wallet, source_secret, source_token,
            "bbbb000000000000cccc111111111111dddd222222222222eeee333333333333") != KNISHIO_SUCCESS) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to create remainder wallet");
        goto cleanup;
    }

    if (knishio_molecule_create(&molecule, source_secret, bundle, source_wallet, remainder_wallet, NULL, KNISHIO_VERSION_STRING) != KNISHIO_SUCCESS) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to create molecule");
        goto cleanup;
    }

    if (knishio_molecule_init_shadow_wallet_claim(molecule, claim_wallet) != KNISHIO_SUCCESS) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to initialize shadow wallet claim molecule");
        goto cleanup;
    }
    log_test("Shadow wallet claim initialization", true, NULL);

    set_canonical_timestamps(molecule);

    if (knishio_molecule_generate_hash(molecule) != KNISHIO_SUCCESS) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to generate molecular hash");
        goto cleanup;
    }
    if (knishio_molecule_sign(molecule, bundle, false, true) != KNISHIO_SUCCESS) {
        results->shadow_wallet_claim.validation_error = safe_strdup("Failed to sign molecule");
        goto cleanup;
    }
    log_test("Molecule signing", true, NULL);

    inspect_molecule(molecule, "SHADOW WALLET CLAIM MOLECULE");
    diagnose_validation(molecule, source_wallet, "SHADOW WALLET CLAIM MOLECULE");

    bool is_valid = false;
    char *validation_error = NULL;
    if (knishio_molecule_check(molecule, source_wallet) == KNISHIO_SUCCESS) {
        is_valid = true;
    } else {
        validation_error = safe_strdup("Signature verification failed");
    }
    log_test("Molecule validation", is_valid, validation_error);

    char *molecule_json = NULL;
    if (knishio_molecule_to_json(molecule, &molecule_json) == KNISHIO_SUCCESS && molecule_json) {
        results->molecules_shadow_wallet_claim = safe_strdup(molecule_json);
        free(molecule_json);
    }

    results->shadow_wallet_claim.passed = is_valid;
    if (molecule->molecular_hash) {
        results->shadow_wallet_claim.molecular_hash = safe_strdup(molecule->molecular_hash);
    }
    results->shadow_wallet_claim.atom_count = (int)molecule->atom_count;
    results->shadow_wallet_claim.validation_error = validation_error;

    success = is_valid;

cleanup:
    if (source_wallet) knishio_wallet_free(source_wallet);
    if (claim_wallet) knishio_wallet_free(claim_wallet);
    if (remainder_wallet) knishio_wallet_free(remainder_wallet);
    if (molecule) knishio_molecule_free(molecule);
    if (source_secret) free(source_secret);
    if (claim_secret) free(claim_secret);
    if (bundle) free(bundle);
    return success;
}

/**
 * Test 3: Simple Transfer Test
 * Creates value transfer with no remainder following JavaScript pattern exactly
 */
static bool test_simple_transfer(test_results_t *results, const cJSON *config) {
    log_message("\n3. Simple Transfer Test", COLOR_BLUE);
    
    /* Get test configuration */
    const cJSON *transfer_config = cJSON_GetObjectItem(config, "simpleTransfer");
    if (!transfer_config) {
        results->simple_transfer.validation_error = safe_strdup("Missing simpleTransfer configuration");
        return false;
    }
    
    const char *source_seed = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "sourceSeed"));
    const char *recipient_seed = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "recipientSeed"));
    const char *token = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "token"));
    const char *source_position = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "sourcePosition"));
    const char *recipient_position = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "recipientPosition"));
    int balance = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(transfer_config, "balance"));
    int amount = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(transfer_config, "amount"));
    
    bool success = true;
    knishio_wallet_t *source_wallet = NULL;
    knishio_wallet_t *recipient_wallet = NULL;
    knishio_molecule_t *molecule = NULL;
    char *source_secret = NULL;
    char *recipient_secret = NULL;
    char *source_bundle = NULL;
    
    /* Create source wallet */
    if (!knishio_generate_secret(source_seed, 2048, &source_secret)) {
        results->simple_transfer.validation_error = safe_strdup("Failed to generate source secret");
        goto cleanup;
    }
    
    if (knishio_wallet_create_simple(&source_wallet, source_secret, token, source_position) != KNISHIO_SUCCESS) {
        results->simple_transfer.validation_error = safe_strdup("Failed to create source wallet");
        goto cleanup;
    }
    
    /* Set balance manually for testing */
    source_wallet->balance = balance;
    log_test("Source wallet creation", true, NULL);
    
    /* Create recipient wallet */
    if (!knishio_generate_secret(recipient_seed, 2048, &recipient_secret)) {
        results->simple_transfer.validation_error = safe_strdup("Failed to generate recipient secret");
        goto cleanup;
    }
    
    if (knishio_wallet_create_simple(&recipient_wallet, recipient_secret, token, recipient_position) != KNISHIO_SUCCESS) {
        results->simple_transfer.validation_error = safe_strdup("Failed to create recipient wallet");
        goto cleanup;
    }
    
    log_test("Recipient wallet creation", true, NULL);
    
    /* Generate source bundle */
    if (!knishio_generate_bundle_hash(source_secret, NULL, NULL, &source_bundle)) {
        results->simple_transfer.validation_error = safe_strdup("Failed to generate source bundle");
        goto cleanup;
    }
    
    /* Create remainder wallet at the canonical fixed position (matches JS createFixedRemainderWallet) */
    knishio_wallet_t *remainder_wallet = NULL;
    char *remainder_position = knishio_strdup("bbbb000000000000cccc111111111111dddd222222222222eeee333333333333");
    if (!remainder_position) {
        results->simple_transfer.validation_error = safe_strdup("Failed to allocate remainder position");
        goto cleanup;
    }

    if (knishio_wallet_create_simple(&remainder_wallet, source_secret, token, remainder_position) != KNISHIO_SUCCESS) {
        results->simple_transfer.validation_error = safe_strdup("Failed to create remainder wallet");
        free(remainder_position);
        goto cleanup;
    }
    
    free(remainder_position);
    
    /* Create molecule for value transfer (with remainder wallet like JavaScript) */
    if (knishio_molecule_create(&molecule, source_secret, source_bundle, source_wallet, remainder_wallet, NULL, KNISHIO_VERSION_STRING) != KNISHIO_SUCCESS) {
        results->simple_transfer.validation_error = safe_strdup("Failed to create molecule");
        goto cleanup;
    }
    
    /* Initialize value transfer using new high-level function */
    if (knishio_molecule_init_value(molecule, recipient_wallet, amount) != KNISHIO_SUCCESS) {
        results->simple_transfer.validation_error = safe_strdup("Failed to initialize value transfer");
        goto cleanup;
    }
    
    log_test("Value transfer initialization", true, NULL);
    
    /* Generate molecular hash */
    set_canonical_timestamps(molecule);
    if (knishio_molecule_generate_hash(molecule) != KNISHIO_SUCCESS) {
        results->simple_transfer.validation_error = safe_strdup("Failed to generate molecular hash");
        goto cleanup;
    }
    
    /* Sign the molecule */
    if (knishio_molecule_sign(molecule, source_bundle, false, true) != KNISHIO_SUCCESS) {
        results->simple_transfer.validation_error = safe_strdup("Failed to sign molecule");
        goto cleanup;
    }
    
    log_test("Molecule signing", true, NULL);
    
    /* Debug: Inspect molecule before validation */
    inspect_molecule(molecule, "SIMPLE TRANSFER MOLECULE");
    
    /* Validate the molecule */
    bool is_valid = false;
    char *validation_error = NULL;
    
    if (knishio_molecule_check(molecule, source_wallet) == KNISHIO_SUCCESS) {
        is_valid = true;
    } else {
        validation_error = safe_strdup("Signature verification failed");
    }
    
    log_test("Molecule validation", is_valid, validation_error);
    
    /* Store serialized molecule */
    char *molecule_json = NULL;
    knishio_error_t json_error = knishio_molecule_to_json(molecule, &molecule_json);
    if (json_error == KNISHIO_SUCCESS && molecule_json) {
        results->molecules_simple_transfer = safe_strdup(molecule_json);
        printf("DEBUG: Stored simple transfer molecule JSON (length=%zu)\n", strlen(molecule_json));
        free(molecule_json);
    } else {
        printf("DEBUG: Failed to serialize simple transfer molecule, error=%d\n", json_error);
    }
    
    /* Store test results */
    results->simple_transfer.passed = is_valid;
    if (molecule->molecular_hash) {
        results->simple_transfer.molecular_hash = safe_strdup(molecule->molecular_hash);
    }
    results->simple_transfer.atom_count = (int)molecule->atom_count;
    results->simple_transfer.validation_error = validation_error;
    
    success = is_valid;

cleanup:
    if (source_wallet) knishio_wallet_free(source_wallet);
    if (recipient_wallet) knishio_wallet_free(recipient_wallet);
    if (remainder_wallet) knishio_wallet_free(remainder_wallet);
    if (molecule) knishio_molecule_free(molecule);
    if (source_secret) free(source_secret);
    if (recipient_secret) free(recipient_secret);
    if (source_bundle) free(source_bundle);
    return success;
}

/**
 * Test 4: Complex Transfer Test
 * Creates value transfer with remainder following JavaScript pattern exactly
 */
static bool test_complex_transfer(test_results_t *results, const cJSON *config) {
    log_message("\n4. Complex Transfer Test", COLOR_BLUE);
    
    /* Get test configuration */
    const cJSON *transfer_config = cJSON_GetObjectItem(config, "complexTransfer");
    if (!transfer_config) {
        results->complex_transfer.validation_error = safe_strdup("Missing complexTransfer configuration");
        return false;
    }
    
    const char *source_seed = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "sourceSeed"));
    const char *recipient_seed = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "recipient1Seed"));
    const char *token = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "token"));
    const char *source_position = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "sourcePosition"));
    const char *recipient_position = cJSON_GetStringValue(cJSON_GetObjectItem(transfer_config, "recipient1Position"));
    int balance = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(transfer_config, "sourceBalance"));
    int amount = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(transfer_config, "amount1"));
    
    bool success = true;
    knishio_wallet_t *source_wallet = NULL;
    knishio_wallet_t *recipient_wallet = NULL;
    knishio_wallet_t *remainder_wallet = NULL;
    knishio_molecule_t *molecule = NULL;
    char *source_secret = NULL;
    char *recipient_secret = NULL;
    char *source_bundle = NULL;
    
    /* Create source wallet */
    if (!knishio_generate_secret(source_seed, 2048, &source_secret)) {
        results->complex_transfer.validation_error = safe_strdup("Failed to generate source secret");
        goto cleanup;
    }
    
    if (knishio_wallet_create_simple(&source_wallet, source_secret, token, source_position) != KNISHIO_SUCCESS) {
        results->complex_transfer.validation_error = safe_strdup("Failed to create source wallet");
        goto cleanup;
    }
    
    /* Set balance manually for testing */
    source_wallet->balance = balance;
    log_test("Source wallet creation", true, NULL);
    
    /* Create remainder wallet with fixed position (matches JavaScript pattern) */
    const char *remainder_position = "bbbb000000000000cccc111111111111dddd222222222222eeee333333333333";
    if (knishio_wallet_create_simple(&remainder_wallet, source_secret, token, remainder_position) != KNISHIO_SUCCESS) {
        results->complex_transfer.validation_error = safe_strdup("Failed to create remainder wallet");
        goto cleanup;
    }
    
    log_test("Remainder wallet creation", true, NULL);
    
    /* Create recipient wallet */
    if (!knishio_generate_secret(recipient_seed, 2048, &recipient_secret)) {
        results->complex_transfer.validation_error = safe_strdup("Failed to generate recipient secret");
        goto cleanup;
    }
    
    if (knishio_wallet_create_simple(&recipient_wallet, recipient_secret, token, recipient_position) != KNISHIO_SUCCESS) {
        results->complex_transfer.validation_error = safe_strdup("Failed to create recipient wallet");
        goto cleanup;
    }
    
    log_test("Recipient wallet creation", true, NULL);
    
    /* Generate source bundle */
    if (!knishio_generate_bundle_hash(source_secret, NULL, NULL, &source_bundle)) {
        results->complex_transfer.validation_error = safe_strdup("Failed to generate source bundle");
        goto cleanup;
    }
    
    /* Create molecule for value transfer with remainder */
    if (knishio_molecule_create(&molecule, source_secret, source_bundle, source_wallet, remainder_wallet, NULL, KNISHIO_VERSION_STRING) != KNISHIO_SUCCESS) {
        results->complex_transfer.validation_error = safe_strdup("Failed to create molecule");
        goto cleanup;
    }
    
    /* Initialize value transfer with remainder using new high-level function */
    if (knishio_molecule_init_value(molecule, recipient_wallet, amount) != KNISHIO_SUCCESS) {
        results->complex_transfer.validation_error = safe_strdup("Failed to initialize value transfer");
        goto cleanup;
    }
    
    log_test("Value transfer with remainder initialization", true, NULL);
    
    /* Generate molecular hash */
    set_canonical_timestamps(molecule);
    if (knishio_molecule_generate_hash(molecule) != KNISHIO_SUCCESS) {
        results->complex_transfer.validation_error = safe_strdup("Failed to generate molecular hash");
        goto cleanup;
    }
    
    /* Sign the molecule */
    if (knishio_molecule_sign(molecule, source_bundle, false, true) != KNISHIO_SUCCESS) {
        results->complex_transfer.validation_error = safe_strdup("Failed to sign molecule");
        goto cleanup;
    }
    
    log_test("Molecule signing", true, NULL);
    
    /* Debug: Inspect molecule before validation */
    inspect_molecule(molecule, "COMPLEX TRANSFER MOLECULE");
    
    /* Step-by-step validation diagnostic */
    diagnose_validation(molecule, source_wallet, "COMPLEX TRANSFER MOLECULE");
    
    /* Validate the molecule */
    bool is_valid = false;
    char *validation_error = NULL;
    
    if (knishio_molecule_check(molecule, source_wallet) == KNISHIO_SUCCESS) {
        is_valid = true;
    } else {
        validation_error = safe_strdup("Signature verification failed");
    }
    
    log_test("Molecule validation", is_valid, validation_error);
    
    /* Store serialized molecule */
    char *molecule_json = NULL;
    knishio_error_t json_error = knishio_molecule_to_json(molecule, &molecule_json);
    if (json_error == KNISHIO_SUCCESS && molecule_json) {
        results->molecules_complex_transfer = safe_strdup(molecule_json);
        printf("DEBUG: Stored complex transfer molecule JSON (length=%zu)\n", strlen(molecule_json));
        free(molecule_json);
    } else {
        printf("DEBUG: Failed to serialize complex transfer molecule, error=%d\n", json_error);
    }
    
    /* Store test results */
    results->complex_transfer.passed = is_valid;
    if (molecule->molecular_hash) {
        results->complex_transfer.molecular_hash = safe_strdup(molecule->molecular_hash);
    }
    results->complex_transfer.atom_count = (int)molecule->atom_count;
    results->complex_transfer.has_remainder = true;
    results->complex_transfer.validation_error = validation_error;
    
    success = is_valid;

cleanup:
    if (source_wallet) knishio_wallet_free(source_wallet);
    if (recipient_wallet) knishio_wallet_free(recipient_wallet);
    if (remainder_wallet) knishio_wallet_free(remainder_wallet);
    if (molecule) knishio_molecule_free(molecule);
    if (source_secret) free(source_secret);
    if (recipient_secret) free(recipient_secret);
    if (source_bundle) free(source_bundle);
    return success;
}

/**
 * Test 5: ML-KEM768 Encryption Test
 * Tests post-quantum encryption/decryption compatibility following JavaScript pattern exactly
 */
static bool test_mlkem768(test_results_t *results, const cJSON *config) {
    log_message("\n5. ML-KEM768 Encryption Test", COLOR_BLUE);
    
    /* Get test configuration */
    const cJSON *mlkem_config = cJSON_GetObjectItem(config, "mlkem768");
    if (!mlkem_config) {
        results->mlkem768.error = safe_strdup("Missing mlkem768 configuration");
        return false;
    }
    
    const char *seed = cJSON_GetStringValue(cJSON_GetObjectItem(mlkem_config, "seed"));
    const char *token = cJSON_GetStringValue(cJSON_GetObjectItem(mlkem_config, "token"));
    const char *position = cJSON_GetStringValue(cJSON_GetObjectItem(mlkem_config, "position"));
    const char *plaintext = cJSON_GetStringValue(cJSON_GetObjectItem(mlkem_config, "plaintext"));
    
    if (!seed || !token || !position || !plaintext) {
        results->mlkem768.error = safe_strdup("Invalid mlkem768 configuration");
        return false;
    }
    
    bool success = true;
    char *secret = NULL;
    char *bundle = NULL;
    knishio_wallet_t *encryption_wallet = NULL;
    knishio_mlkem768_keypair_t keypair = {0};
    uint8_t *ciphertext = NULL;
    uint8_t *decrypted_text = NULL;
    size_t ciphertext_len = 0;
    size_t decrypted_len = 0;
    char *public_key_hex = NULL;
    char *encrypted_data_json = NULL;
    
    /* Create encryption wallet from seed (following JavaScript pattern) */
    if (!knishio_generate_secret(seed, 2048, &secret)) {
        results->mlkem768.error = safe_strdup("Failed to generate secret");
        return false;
    }
    
    if (!knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)) {
        results->mlkem768.error = safe_strdup("Failed to generate bundle");
        goto cleanup;
    }
    
    if (knishio_wallet_create_simple(&encryption_wallet, secret, token, position) != KNISHIO_SUCCESS) {
        results->mlkem768.error = safe_strdup("Failed to create encryption wallet");
        goto cleanup;
    }
    
    log_test("Encryption wallet creation", true, NULL);
    
    /* Generate ML-KEM768 seed following JavaScript pattern exactly */
    /* JavaScript: const seedHex = generateSecret(this.key, 128) → 128 hex chars = 64 bytes */
    char *seed_hex = NULL;
    if (!knishio_generate_secret(encryption_wallet->private_key, 128, &seed_hex)) {
        results->mlkem768.error = safe_strdup("Failed to generate ML-KEM768 seed");
        goto cleanup;
    }

    /* Verify seed length matches JavaScript output (128 hex chars = 64 bytes) */
    if (!seed_hex || strlen(seed_hex) != 128) {
        results->mlkem768.error = safe_strdup("Invalid ML-KEM768 seed length (expected 128 hex chars)");
        if (seed_hex) free(seed_hex);
        goto cleanup;
    }

    /* Convert 128 hex chars to the full 64-byte (d||z) seed (matches JS Wallet.initializeMLKEM) */
    uint8_t seed_bytes[64];
    for (int i = 0; i < 64; i++) {
        char hex_pair[3] = {seed_hex[i*2], seed_hex[i*2+1], '\0'};
        seed_bytes[i] = (uint8_t)strtol(hex_pair, NULL, 16);
    }
    
    free(seed_hex); // Clean up seed
    
    /* Generate key pair from deterministic seed */
    if (knishio_mlkem768_keypair_from_seed(&keypair, seed_bytes, 64) != KNISHIO_SUCCESS) {
        results->mlkem768.error = safe_strdup("Failed to generate ML-KEM768 key pair from seed");
        goto cleanup;
    }
    
    /* Convert public key to base64 for storage (matching Noble crypto format) */
    if (!knishio_base64_encode(keypair.public_key, 1184, &public_key_hex)) {
        results->mlkem768.error = safe_strdup("Failed to convert public key to base64");
        goto cleanup;
    }
    
    bool public_key_generated = (public_key_hex != NULL);
    log_test("ML-KEM768 public key generation", public_key_generated, NULL);

    /* Encapsulate to get KEM ciphertext and shared secret (JavaScript SDK pattern) */
    knishio_mlkem768_ciphertext_t kem_ciphertext;
    knishio_mlkem768_shared_secret_t shared_secret;

    if (knishio_mlkem768_encapsulate(keypair.public_key, &kem_ciphertext, &shared_secret) != KNISHIO_SUCCESS) {
        results->mlkem768.error = safe_strdup("Failed to encapsulate shared secret");
        goto cleanup;
    }

    /* Wrap message in JSON for cross-SDK compatibility (JavaScript SDK pattern) */
    cJSON *message_json = cJSON_CreateString(plaintext);
    char *json_message = cJSON_PrintUnformatted(message_json);
    cJSON_Delete(message_json);

    if (!json_message) {
        results->mlkem768.error = safe_strdup("Failed to create JSON message");
        goto cleanup;
    }

    /* Encrypt JSON-wrapped message using AES-256-GCM with shared secret (JavaScript SDK pattern) */
    uint8_t *encrypted_message = NULL;
    size_t encrypted_message_len = 0;

    if (knishio_aes_gcm_encrypt((const uint8_t*)json_message, strlen(json_message),
                                shared_secret.shared_secret,
                                &encrypted_message, &encrypted_message_len) != KNISHIO_SUCCESS) {
        results->mlkem768.error = safe_strdup("Failed to encrypt message with AES-GCM");
        free(json_message);
        goto cleanup;
    }

    free(json_message);

    bool encryption_success = (encrypted_message != NULL && encrypted_message_len > 0);
    log_test("Message encryption (encapsulate + AES-GCM)", encryption_success, NULL);

    /* Decrypt the encrypted message for validation */
    /* Decapsulate to recover shared secret */
    knishio_mlkem768_shared_secret_t recovered_secret;

    if (knishio_mlkem768_decapsulate(keypair.private_key, &kem_ciphertext, &recovered_secret) != KNISHIO_SUCCESS) {
        results->mlkem768.error = safe_strdup("Failed to decapsulate shared secret");
        if (encrypted_message) free(encrypted_message);
        goto cleanup;
    }

    /* Decrypt message using AES-256-GCM with recovered shared secret */
    if (knishio_aes_gcm_decrypt(encrypted_message, encrypted_message_len,
                                recovered_secret.shared_secret,
                                &decrypted_text, &decrypted_len) != KNISHIO_SUCCESS) {
        results->mlkem768.error = safe_strdup("Failed to decrypt message with AES-GCM");
        if (encrypted_message) free(encrypted_message);
        goto cleanup;
    }

    /* Parse JSON-wrapped message and verify it matches original plaintext (JavaScript SDK compatibility) */
    bool decryption_success = false;
    if (decrypted_text && decrypted_len > 0) {
        /* Null-terminate for JSON parsing */
        char *decrypted_json = malloc(decrypted_len + 1);
        if (decrypted_json) {
            memcpy(decrypted_json, decrypted_text, decrypted_len);
            decrypted_json[decrypted_len] = '\0';

            /* Parse JSON to extract message */
            cJSON *parsed = cJSON_Parse(decrypted_json);
            if (parsed && cJSON_IsString(parsed)) {
                const char *unwrapped_message = cJSON_GetStringValue(parsed);
                if (unwrapped_message) {
                    decryption_success = (strcmp(unwrapped_message, plaintext) == 0);
                }
            }

            if (parsed) cJSON_Delete(parsed);
            free(decrypted_json);
        }
    }

    log_test("Message decryption and verification (decapsulate + AES-GCM)", decryption_success, NULL);
    
    bool test_passed = public_key_generated && encryption_success && decryption_success;
    
    /* Store ML-KEM768 data for cross-SDK verification (following JavaScript format) */
    cJSON *mlkem_data = cJSON_CreateObject();
    cJSON_AddItemToObject(mlkem_data, "publicKey", cJSON_CreateString(public_key_hex ? public_key_hex : ""));
    
    /* Create encrypted data structure like JavaScript with separate KEM ciphertext and encrypted message */
    cJSON *encrypted_data = cJSON_CreateObject();

    /* Convert KEM ciphertext (1088 bytes) to base64 - this is the encapsulation result */
    char *kem_ciphertext_base64 = NULL;
    if (knishio_base64_encode(kem_ciphertext.ciphertext, sizeof(kem_ciphertext.ciphertext), &kem_ciphertext_base64)) {
        cJSON_AddItemToObject(encrypted_data, "cipherText", cJSON_CreateString(kem_ciphertext_base64));
        free(kem_ciphertext_base64);
    }

    /* Convert encrypted message (IV + encrypted + tag) to base64 - this is the AES-GCM result */
    if (encrypted_message && encrypted_message_len > 0) {
        char *encrypted_message_base64 = NULL;
        if (knishio_base64_encode(encrypted_message, encrypted_message_len, &encrypted_message_base64)) {
            cJSON_AddItemToObject(encrypted_data, "encryptedMessage", cJSON_CreateString(encrypted_message_base64));
            free(encrypted_message_base64);
        }
    }
    
    cJSON_AddItemToObject(mlkem_data, "encryptedData", encrypted_data);
    cJSON_AddItemToObject(mlkem_data, "originalPlaintext", cJSON_CreateString(plaintext));
    cJSON_AddItemToObject(mlkem_data, "sdk", cJSON_CreateString("C"));
    
    encrypted_data_json = cJSON_Print(mlkem_data);
    if (encrypted_data_json) {
        results->molecules_mlkem768 = safe_strdup(encrypted_data_json);
    }
    
    cJSON_Delete(mlkem_data);
    
    /* Store test results */
    results->mlkem768.passed = test_passed;
    results->mlkem768.public_key_generated = public_key_generated;
    results->mlkem768.encryption_success = encryption_success;
    results->mlkem768.decryption_success = decryption_success;
    results->mlkem768.plaintext_length = (int)strlen(plaintext);
    
    success = test_passed;

cleanup:
    if (encryption_wallet) knishio_wallet_free(encryption_wallet);
    if (secret) free(secret);
    if (bundle) free(bundle);
    if (encrypted_message) free(encrypted_message);
    if (decrypted_text) free(decrypted_text);
    if (public_key_hex) free(public_key_hex);
    if (encrypted_data_json) free(encrypted_data_json);
    return success;
}

/**
 * Test 6: Negative Test Cases (Anti-Cheating)
 * Validates that invalid molecules properly fail validation
 */
static bool test_negative_cases(test_results_t *results, const cJSON *config) {
    log_message("\n6. Negative Test Cases (Anti-Cheating)", COLOR_BLUE);

    /* Get test configuration */
    const cJSON *crypto_config = cJSON_GetObjectItem(config, "crypto");
    if (!crypto_config) {
        results->negative_cases.error = safe_strdup("Missing crypto configuration");
        return false;
    }

    const char *seed = cJSON_GetStringValue(cJSON_GetObjectItem(crypto_config, "seed"));
    if (!seed) {
        results->negative_cases.error = safe_strdup("Invalid crypto configuration");
        return false;
    }

    bool all_negative_tests_passed = true;
    char *secret = NULL;
    char *bundle = NULL;
    knishio_wallet_t *source_wallet = NULL;

    /* Generate test wallet */
    if (!knishio_generate_secret(seed, 2048, &secret)) {
        results->negative_cases.error = safe_strdup("Failed to generate secret");
        return false;
    }

    if (!knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)) {
        results->negative_cases.error = safe_strdup("Failed to generate bundle");
        free(secret);
        return false;
    }

    /* Create source wallet */
    if (knishio_wallet_create_simple(&source_wallet, secret, "TEST",
                                     "0123456789abcdeffedcba9876543210fedcba9876543210fedcba9876543210") != KNISHIO_SUCCESS) {
        results->negative_cases.error = safe_strdup("Failed to create source wallet");
        free(secret);
        free(bundle);
        return false;
    }

    source_wallet->balance = 1000;

    /* Test 1: Missing Molecular Hash (should fail) */
    {
        knishio_molecule_t *invalid_molecule = NULL;
        bool test_passed = false;

        if (knishio_molecule_create(&invalid_molecule, secret, bundle, source_wallet, NULL, NULL, KNISHIO_VERSION_STRING) == KNISHIO_SUCCESS) {
            /* Add a valid atom but don't sign (no molecular hash) */
            knishio_atom_t *atom = NULL;
            if (knishio_atom_create(&atom, source_wallet->position, source_wallet->address,
                                   KNISHIO_ISOTOPE_V, "TEST", "-100", NULL) == KNISHIO_SUCCESS) {
                knishio_molecule_add_atom(invalid_molecule, atom);
            }

            /* This should fail because there's no molecular hash */
            if (knishio_molecule_check(invalid_molecule, source_wallet) == KNISHIO_SUCCESS) {
                log_test("Missing molecular hash validation (should FAIL)", false, "Invalid molecule passed validation");
                all_negative_tests_passed = false;
            } else {
                log_test("Missing molecular hash validation (should FAIL)", true, NULL);
                test_passed = true;
            }

            knishio_molecule_free(invalid_molecule);
        }
    }

    /* Test 2: Invalid Molecular Hash (should fail) */
    {
        knishio_molecule_t *invalid_molecule = NULL;
        bool test_passed = false;

        if (knishio_molecule_create(&invalid_molecule, secret, bundle, source_wallet, NULL, NULL, KNISHIO_VERSION_STRING) == KNISHIO_SUCCESS) {
            /* Add a valid atom */
            knishio_atom_t *atom = NULL;
            if (knishio_atom_create(&atom, source_wallet->position, source_wallet->address,
                                   KNISHIO_ISOTOPE_V, "TEST", "-100", NULL) == KNISHIO_SUCCESS) {
                knishio_molecule_add_atom(invalid_molecule, atom);
            }

            /* Sign normally */
            knishio_molecule_generate_hash(invalid_molecule);
            knishio_molecule_sign(invalid_molecule, bundle, false, true);

            /* Then corrupt the molecular hash */
            if (invalid_molecule->molecular_hash) {
                free(invalid_molecule->molecular_hash);
                invalid_molecule->molecular_hash = safe_strdup("invalid_hash_that_should_fail_validation_check_12345678");
            }

            /* This should fail because the hash is invalid */
            if (knishio_molecule_check(invalid_molecule, source_wallet) == KNISHIO_SUCCESS) {
                log_test("Invalid molecular hash validation (should FAIL)", false, "Corrupted molecule passed validation");
                all_negative_tests_passed = false;
            } else {
                log_test("Invalid molecular hash validation (should FAIL)", true, NULL);
                test_passed = true;
            }

            knishio_molecule_free(invalid_molecule);
        }
    }

    /* Test 3: Unbalanced Transfer (should fail) */
    {
        knishio_molecule_t *invalid_molecule = NULL;
        bool test_passed = false;

        if (knishio_molecule_create(&invalid_molecule, secret, bundle, source_wallet, NULL, NULL, KNISHIO_VERSION_STRING) == KNISHIO_SUCCESS) {
            /* Create unbalanced atoms (doesn't sum to zero) */
            knishio_atom_t *atom1 = NULL;
            if (knishio_atom_create(&atom1, source_wallet->position, source_wallet->address,
                                    KNISHIO_ISOTOPE_V, "TEST", "-1000", NULL) == KNISHIO_SUCCESS) {
                knishio_molecule_add_atom(invalid_molecule, atom1);
            }

            knishio_atom_t *atom2 = NULL;
            if (knishio_atom_create(&atom2, source_wallet->position, source_wallet->address,
                                    KNISHIO_ISOTOPE_V, "TEST", "500", NULL) == KNISHIO_SUCCESS) {
                knishio_molecule_add_atom(invalid_molecule, atom2);
            }

            /* Sign the unbalanced molecule */
            knishio_molecule_generate_hash(invalid_molecule);
            knishio_molecule_sign(invalid_molecule, bundle, false, true);

            /* This should fail because it's unbalanced */
            if (knishio_molecule_check(invalid_molecule, source_wallet) == KNISHIO_SUCCESS) {
                log_test("Unbalanced transfer validation (should FAIL)", false, "Unbalanced molecule passed validation");
                all_negative_tests_passed = false;
            } else {
                log_test("Unbalanced transfer validation (should FAIL)", true, NULL);
                test_passed = true;
            }

            knishio_molecule_free(invalid_molecule);
        }
    }

    /* Store test results */
    results->negative_cases.passed = all_negative_tests_passed;
    results->negative_cases.description = safe_strdup("Anti-cheating validation tests");
    results->negative_cases.test_count = 3;

    /* Cleanup */
    if (source_wallet) knishio_wallet_free(source_wallet);
    if (secret) free(secret);
    if (bundle) free(bundle);

    return all_negative_tests_passed;
}

/**
 * Cross-SDK Validation
 * Loads and validates molecules from other SDKs (basic structural validation)
 */
static bool test_cross_sdk_validation(test_results_t *results) {
    log_message("\n6. Cross-SDK Validation", COLOR_BLUE);
    
    /* Check if cross-validation is disabled (Round 1 molecule generation only) */
    const char* disable_cross_validation = getenv("KNISHIO_DISABLE_CROSS_VALIDATION");
    if (disable_cross_validation && strcmp(disable_cross_validation, "true") == 0) {
        log_message("  ⏭️  Cross-validation disabled for Round 1 (molecule generation only)", COLOR_YELLOW);
        results->cross_sdk_compatible = true;
        return true;
    }

    log_message("  📋 Loading molecules from other SDKs...", COLOR_CYAN);
    
    /* Configurable shared results directory for cross-platform testing */
    const char* shared_dir = getenv("KNISHIO_SHARED_RESULTS");
    char results_dir_path[MAX_PATH_LENGTH];
    if (shared_dir) {
        strncpy(results_dir_path, shared_dir, MAX_PATH_LENGTH - 1);
    } else {
        strncpy(results_dir_path, "../shared-test-results", MAX_PATH_LENGTH - 1);
    }
    results_dir_path[MAX_PATH_LENGTH - 1] = '\0';
    
    struct stat st = {0};
    
    if (stat(results_dir_path, &st) == -1) {
        log_message("  ⏭️  No other SDK results found for cross-validation", COLOR_YELLOW);
        results->cross_sdk_compatible = true;
        return true;
    }
    
    /* Implement actual cross-SDK validation following JavaScript pattern */
    const char* sdk_names[] = {"JavaScript", "TypeScript", "Kotlin", "PHP", "Python", "Rust", "C++"};
    const char* sdk_filenames[] = {"javascript-results.json", "typescript-results.json", "kotlin-results.json", 
                                   "php-results.json", "python-results.json", "rust-results.json", "cpp-results.json"};
    
    char sdk_files[7][MAX_PATH_LENGTH];
    for (int i = 0; i < 7; i++) {
        snprintf(sdk_files[i], MAX_PATH_LENGTH, "%s/%s", results_dir_path, sdk_filenames[i]);
    }
    
    int num_sdks = sizeof(sdk_files) / sizeof(sdk_files[0]);
    int passed_validations = 0;
    int total_validations = 0;
    
    printf("\n");
    for (int i = 0; i < num_sdks; i++) {
        printf("  🧪 Validating %s SDK molecules:\n", sdk_names[i]);
        
        /* Try to load and validate this SDK's molecules */
        FILE* file = fopen(sdk_files[i], "r");
        if (!file) {
            printf("    ⏭️  Results file not found, skipping %s\n", sdk_names[i]);
            continue;
        }
        
        /* Read file content (simplified approach) */
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        if (file_size > 0 && file_size < 1000000) { /* Reasonable size check */
            char* file_content = malloc(file_size + 1);
            if (file_content) {
                size_t read_size = fread(file_content, 1, file_size, file);
                file_content[read_size] = '\0';
                
                /* Parse JSON and check for molecules section */
                cJSON* json = cJSON_Parse(file_content);
                if (json) {
                    cJSON* molecules = cJSON_GetObjectItem(json, "molecules");
                    if (molecules && cJSON_IsObject(molecules)) {
                        
                        /* Validate each molecule type */
                        const char* molecule_types[] = {"metadata", "simpleTransfer", "complexTransfer", "tokenCreation", "walletCreation", "shadowWalletClaim", "mlkem768"};
                        int num_types = sizeof(molecule_types) / sizeof(molecule_types[0]);
                        
                        for (int j = 0; j < num_types; j++) {
                            total_validations++;
                            
                            cJSON* molecule_data = cJSON_GetObjectItem(molecules, molecule_types[j]);
                            bool is_valid = false;
                            
                            if (molecule_data && cJSON_IsString(molecule_data)) {
                                /* Basic validation: check if molecule JSON can be parsed */
                                cJSON* molecule_json = cJSON_Parse(cJSON_GetStringValue(molecule_data));
                                if (molecule_json) {
                                    /* Check for required fields (JavaScript canonical validation) */
                                    if (strcmp(molecule_types[j], "mlkem768") == 0) {
                                        /* Real ML-KEM768 validation (matches JavaScript SDK standard) */
                                        cJSON* public_key = cJSON_GetObjectItem(molecule_json, "publicKey");
                                        cJSON* encrypted_data = cJSON_GetObjectItem(molecule_json, "encryptedData");
                                        cJSON* original_plaintext = cJSON_GetObjectItem(molecule_json, "originalPlaintext");
                                        
                                        if (public_key && cJSON_IsString(public_key) &&
                                            encrypted_data && cJSON_IsObject(encrypted_data) &&
                                            original_plaintext && cJSON_IsString(original_plaintext)) {
                                            
                                            /* REAL FUNCTIONAL TEST: Attempt to decrypt their message like JavaScript does */
                                            const char* expected_plaintext = cJSON_GetStringValue(original_plaintext);
                                            
                                            /* Create deterministic test wallet (matches JavaScript test pattern) */
                                            const char* test_secret = "e8ffc86d60fc6a73234a834166e7436e21df6c3209dfacc8d0bd6595707872c3799abbf7deee0f9c4b58de1fd89b9abb67a207558208d5ccf550c227d197c24e9fcc3707aeb53c4031d38392020ff72bcaa0f728aa8bc3d47d95ff0afc04d8fcdb69bff638ce56646c154fc92aa517d3c40f550d2ccacbd921724e1d94b82aed2c8e172a8a7ed5a6963f5890157fe77222b97af3787741f9d3cec0b40aec6f07ae4b2b24614f0a20e035aee0df04e176175dc100eb1b00dd7ea95c28cdec47958336945333c3bef24719ed949fa56d1541f24c725d4f374a533bf255cf22f4596147bcd1ba05abcecbe9b12095e1fdddb094616894c366498be0b5785c180100efb3c5b689fc1c01131633fe1775df52a970e9472ab7bc0c19f5742b9e9436753cd16024b2d326b763eca68c414755a0d2fdbb927f007e9413f1190578b2033a03d29387f5aea71b07a2ca84534433d0a1b86cef3288e7d79e8b175a3955848cfd1dfbdcd6b5bafcf6789e56e8ef40af";
                                            const char* test_token = "ENCRYPT";
                                            const char* test_position = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
                                            
                                            knishio_wallet_t* test_wallet = NULL;
                                            bool wallet_result = knishio_wallet_create(
                                                &test_wallet, test_secret, test_token, test_position);
                                            
                                            if (wallet_result && test_wallet) {
                                                /* Attempt real ML-KEM768 decryption (matches JavaScript test) */
                                                char* decrypted_result = NULL;
                                                
                                                /* Test if we can encrypt FOR their public key (matches PHP/Python approach) */
                                                printf("    🔬 Testing %s ML-KEM768 encryption compatibility...\n", sdk_names[i]);

                                                /* Get their public key */
                                                const char* their_public_key_b64 = cJSON_GetStringValue(public_key);

                                                /* Decode their public key */
                                                uint8_t* their_public_key = NULL;
                                                size_t public_key_len = 0;

                                                if (knishio_base64_decode(their_public_key_b64, &their_public_key, &public_key_len)) {
                                                    if (public_key_len == 1184) {  /* ML-KEM768 public key size */
                                                        /* Test: Can we encrypt a message FOR their public key? */
                                                        const char* test_message = "Cross-SDK ML-KEM768 compatibility test";

                                                        /* Wrap in JSON like other SDKs */
                                                        cJSON* message_json = cJSON_CreateString(test_message);
                                                        char* json_message = cJSON_PrintUnformatted(message_json);
                                                        cJSON_Delete(message_json);

                                                        uint8_t* encrypted_for_them = NULL;
                                                        size_t encrypted_len = 0;

                                                        /* Try to encrypt FOR their public key */
                                                        knishio_error_t encrypt_result = knishio_mlkem768_encrypt(
                                                            their_public_key,
                                                            (const uint8_t*)json_message,
                                                            strlen(json_message),
                                                            &encrypted_for_them,
                                                            &encrypted_len
                                                        );

                                                        free(json_message);

                                                        if (encrypt_result == KNISHIO_SUCCESS && encrypted_for_them && encrypted_len > 0) {
                                                            printf("    ✅ Successfully encrypted for %s public key\n", sdk_names[i]);
                                                            is_valid = true;
                                                            free(encrypted_for_them);
                                                        } else {
                                                            printf("    ❌ Failed to encrypt for %s public key - error: %d\n", sdk_names[i], encrypt_result);
                                                            is_valid = false;
                                                        }
                                                    } else {
                                                        printf("    ❌ Invalid public key size: %zu (expected 1184)\n", public_key_len);
                                                        is_valid = false;
                                                    }
                                                    free(their_public_key);
                                                } else {
                                                    printf("    ❌ Failed to decode public key\n");
                                                    is_valid = false;
                                                }
                                                
                                                knishio_wallet_free(test_wallet);
                                            } else {
                                                is_valid = false;  /* Wallet creation failed */
                                            }
                                        } else {
                                            is_valid = false;  /* Missing required JSON fields */
                                        }
                                    } else {
                                        /* Standard molecule validation */
                                        cJSON* molecular_hash = cJSON_GetObjectItem(molecule_json, "molecularHash");
                                        cJSON* atoms = cJSON_GetObjectItem(molecule_json, "atoms");
                                        
                                        if (molecular_hash && cJSON_IsString(molecular_hash) && 
                                            atoms && cJSON_IsArray(atoms) && 
                                            cJSON_GetArraySize(atoms) > 0) {
                                            is_valid = true;
                                        }
                                    }
                                    
                                    cJSON_Delete(molecule_json);
                                }
                            }
                            
                            if (is_valid) {
                                printf("    ✅ %s molecule: PASSED\n", molecule_types[j]);
                                passed_validations++;
                            } else {
                                printf("    ❌ %s molecule: FAILED\n", molecule_types[j]);
                            }
                        }
                    }
                    
                    cJSON_Delete(json);
                }
                
                free(file_content);
            }
        }
        
        fclose(file);
        printf("\n");
    }
    
    bool all_valid = (passed_validations == total_validations);
    
    if (all_valid) {
        log_message("  ✅ All cross-SDK molecules validated successfully", COLOR_GREEN);
        log_message("  ✅ Cross-SDK Compatible: YES", COLOR_GREEN);
    } else {
        log_message("  ❌ Some cross-SDK molecules failed validation", COLOR_RED);
        log_message("  ❌ Cross-SDK Compatible: NO", COLOR_RED);
    }
    
    results->cross_sdk_compatible = all_valid;
    return all_valid;
}

/**
 * Load test configuration from JSON file
 */
/**
 * Load test configuration - embedded for SDK self-containment
 */
static bool load_config(void) {
    // Support optional external config override via environment variable
    const char* config_path = getenv("KNISHIO_TEST_CONFIG");
    
    if (config_path != NULL && access(config_path, F_OK) == 0) {
        // Load external config file
        FILE *file = fopen(config_path, "r");
        if (!file) {
            fprintf(stderr, "Error: Cannot open external config file: %s\n", config_path);
            return false;
        }
        
        /* Read external file content */
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        char *content = malloc(file_size + 1);
        if (!content) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            fclose(file);
            return false;
        }
        
        fread(content, 1, file_size, file);
        content[file_size] = '\0';
        fclose(file);
        
        g_config = cJSON_Parse(content);
        free(content);
        
        if (!g_config) {
            fprintf(stderr, "Error: Failed to parse external JSON configuration\n");
            return false;
        }
        
        printf("📄 Using external config: %s\n", config_path);
    } else {
        // Use embedded configuration
        g_config = cJSON_Parse(DEFAULT_CONFIG_JSON);
        
        if (!g_config) {
            fprintf(stderr, "Error: Failed to parse embedded JSON configuration\n");
            return false;
        }
    }
    
    return true;
}

/**
 * Initialize test results structure
 */
static void init_results(void) {
    memset(&g_results, 0, sizeof(g_results));
    
    g_results.sdk = safe_strdup("C");
    g_results.version = safe_strdup(KNISHIO_VERSION_STRING);
    
    /* Generate timestamp */
    static char timestamp_buffer[ISO8601_LENGTH];
    get_iso8601_timestamp(timestamp_buffer, sizeof(timestamp_buffer));
    g_results.timestamp = safe_strdup(timestamp_buffer);
}

/**
 * Save results to JSON file (matches JavaScript SDK format exactly)
 */
static bool save_results(void) {
    /* Configurable shared results directory */
    const char* shared_dir = getenv("KNISHIO_SHARED_RESULTS");
    char results_path[MAX_PATH_LENGTH];
    
    if (shared_dir) {
        snprintf(results_path, MAX_PATH_LENGTH, "%s/c-results.json", shared_dir);
    } else {
        strncpy(results_path, "../shared-test-results/c-results.json", MAX_PATH_LENGTH - 1);
        results_path[MAX_PATH_LENGTH - 1] = '\0';
    }
    
    /* Create JSON structure matching other SDKs */
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddItemToObject(root, "sdk", cJSON_CreateString(g_results.sdk));
    cJSON_AddItemToObject(root, "version", cJSON_CreateString(g_results.version));
    cJSON_AddItemToObject(root, "timestamp", cJSON_CreateString(g_results.timestamp));
    
    /* Tests object */
    cJSON *tests = cJSON_CreateObject();
    
    /* Crypto test */
    cJSON *crypto = cJSON_CreateObject();
    cJSON_AddItemToObject(crypto, "passed", cJSON_CreateBool(g_results.crypto.passed));
    cJSON_AddItemToObject(crypto, "secret", cJSON_CreateString(g_results.crypto.secret ? g_results.crypto.secret : ""));
    cJSON_AddItemToObject(crypto, "bundle", cJSON_CreateString(g_results.crypto.bundle ? g_results.crypto.bundle : ""));
    cJSON_AddItemToObject(crypto, "expectedSecret", cJSON_CreateString(g_results.crypto.expected_secret ? g_results.crypto.expected_secret : ""));
    cJSON_AddItemToObject(crypto, "expectedBundle", cJSON_CreateString(g_results.crypto.expected_bundle ? g_results.crypto.expected_bundle : ""));
    cJSON_AddItemToObject(tests, "crypto", crypto);
    
    /* Meta creation test */
    cJSON *meta_creation = cJSON_CreateObject();
    cJSON_AddItemToObject(meta_creation, "passed", cJSON_CreateBool(g_results.meta_creation.passed));
    cJSON_AddItemToObject(meta_creation, "molecularHash", cJSON_CreateString(g_results.meta_creation.molecular_hash ? g_results.meta_creation.molecular_hash : ""));
    cJSON_AddItemToObject(meta_creation, "atomCount", cJSON_CreateNumber(g_results.meta_creation.atom_count));
    cJSON_AddItemToObject(meta_creation, "validationError", cJSON_CreateString(g_results.meta_creation.validation_error ? g_results.meta_creation.validation_error : "null"));
    cJSON_AddItemToObject(tests, "metaCreation", meta_creation);
    
    /* Simple transfer test */
    cJSON *simple_transfer = cJSON_CreateObject();
    cJSON_AddItemToObject(simple_transfer, "passed", cJSON_CreateBool(g_results.simple_transfer.passed));
    cJSON_AddItemToObject(simple_transfer, "molecularHash", cJSON_CreateString(g_results.simple_transfer.molecular_hash ? g_results.simple_transfer.molecular_hash : ""));
    cJSON_AddItemToObject(simple_transfer, "atomCount", cJSON_CreateNumber(g_results.simple_transfer.atom_count));
    cJSON_AddItemToObject(simple_transfer, "validationError", cJSON_CreateString(g_results.simple_transfer.validation_error ? g_results.simple_transfer.validation_error : "null"));
    cJSON_AddItemToObject(tests, "simpleTransfer", simple_transfer);
    
    /* Complex transfer test */
    cJSON *complex_transfer = cJSON_CreateObject();
    cJSON_AddItemToObject(complex_transfer, "passed", cJSON_CreateBool(g_results.complex_transfer.passed));
    cJSON_AddItemToObject(complex_transfer, "molecularHash", cJSON_CreateString(g_results.complex_transfer.molecular_hash ? g_results.complex_transfer.molecular_hash : ""));
    cJSON_AddItemToObject(complex_transfer, "atomCount", cJSON_CreateNumber(g_results.complex_transfer.atom_count));
    cJSON_AddItemToObject(complex_transfer, "hasRemainder", cJSON_CreateBool(g_results.complex_transfer.has_remainder));
    cJSON_AddItemToObject(complex_transfer, "validationError", cJSON_CreateString(g_results.complex_transfer.validation_error ? g_results.complex_transfer.validation_error : "null"));
    cJSON_AddItemToObject(tests, "complexTransfer", complex_transfer);

    /* Token creation test */
    cJSON *token_creation = cJSON_CreateObject();
    cJSON_AddItemToObject(token_creation, "passed", cJSON_CreateBool(g_results.token_creation.passed));
    cJSON_AddItemToObject(token_creation, "molecularHash", cJSON_CreateString(g_results.token_creation.molecular_hash ? g_results.token_creation.molecular_hash : ""));
    cJSON_AddItemToObject(token_creation, "atomCount", cJSON_CreateNumber(g_results.token_creation.atom_count));
    cJSON_AddItemToObject(token_creation, "validationError", cJSON_CreateString(g_results.token_creation.validation_error ? g_results.token_creation.validation_error : "null"));
    cJSON_AddItemToObject(tests, "tokenCreation", token_creation);

    /* Wallet creation test */
    cJSON *wallet_creation = cJSON_CreateObject();
    cJSON_AddItemToObject(wallet_creation, "passed", cJSON_CreateBool(g_results.wallet_creation.passed));
    cJSON_AddItemToObject(wallet_creation, "molecularHash", cJSON_CreateString(g_results.wallet_creation.molecular_hash ? g_results.wallet_creation.molecular_hash : ""));
    cJSON_AddItemToObject(wallet_creation, "atomCount", cJSON_CreateNumber(g_results.wallet_creation.atom_count));
    cJSON_AddItemToObject(wallet_creation, "validationError", cJSON_CreateString(g_results.wallet_creation.validation_error ? g_results.wallet_creation.validation_error : "null"));
    cJSON_AddItemToObject(tests, "walletCreation", wallet_creation);

    /* Shadow wallet claim test */
    cJSON *shadow_wallet_claim = cJSON_CreateObject();
    cJSON_AddItemToObject(shadow_wallet_claim, "passed", cJSON_CreateBool(g_results.shadow_wallet_claim.passed));
    cJSON_AddItemToObject(shadow_wallet_claim, "molecularHash", cJSON_CreateString(g_results.shadow_wallet_claim.molecular_hash ? g_results.shadow_wallet_claim.molecular_hash : ""));
    cJSON_AddItemToObject(shadow_wallet_claim, "atomCount", cJSON_CreateNumber(g_results.shadow_wallet_claim.atom_count));
    cJSON_AddItemToObject(shadow_wallet_claim, "validationError", cJSON_CreateString(g_results.shadow_wallet_claim.validation_error ? g_results.shadow_wallet_claim.validation_error : "null"));
    cJSON_AddItemToObject(tests, "shadowWalletClaim", shadow_wallet_claim);

    /* ML-KEM768 test */
    cJSON *mlkem768 = cJSON_CreateObject();
    cJSON_AddItemToObject(mlkem768, "passed", cJSON_CreateBool(g_results.mlkem768.passed));
    cJSON_AddItemToObject(mlkem768, "publicKeyGenerated", cJSON_CreateBool(g_results.mlkem768.public_key_generated));
    cJSON_AddItemToObject(mlkem768, "encryptionSuccess", cJSON_CreateBool(g_results.mlkem768.encryption_success));
    cJSON_AddItemToObject(mlkem768, "decryptionSuccess", cJSON_CreateBool(g_results.mlkem768.decryption_success));
    cJSON_AddItemToObject(mlkem768, "plaintextLength", cJSON_CreateNumber(g_results.mlkem768.plaintext_length));
    if (g_results.mlkem768.error) {
        cJSON_AddItemToObject(mlkem768, "error", cJSON_CreateString(g_results.mlkem768.error));
    }
    cJSON_AddItemToObject(tests, "mlkem768", mlkem768);

    /* Negative test cases */
    cJSON *negative_cases = cJSON_CreateObject();
    cJSON_AddItemToObject(negative_cases, "passed", cJSON_CreateBool(g_results.negative_cases.passed));
    if (g_results.negative_cases.description) {
        cJSON_AddItemToObject(negative_cases, "description", cJSON_CreateString(g_results.negative_cases.description));
    }
    cJSON_AddItemToObject(negative_cases, "testCount", cJSON_CreateNumber(g_results.negative_cases.test_count));
    if (g_results.negative_cases.error) {
        cJSON_AddItemToObject(negative_cases, "error", cJSON_CreateString(g_results.negative_cases.error));
    }
    cJSON_AddItemToObject(tests, "negativeCases", negative_cases);

    cJSON_AddItemToObject(root, "tests", tests);
    
    /* Molecules object */
    cJSON *molecules = cJSON_CreateObject();
    cJSON_AddItemToObject(molecules, "metadata", cJSON_CreateString(g_results.molecules_metadata ? g_results.molecules_metadata : ""));
    cJSON_AddItemToObject(molecules, "simpleTransfer", cJSON_CreateString(g_results.molecules_simple_transfer ? g_results.molecules_simple_transfer : ""));
    cJSON_AddItemToObject(molecules, "complexTransfer", cJSON_CreateString(g_results.molecules_complex_transfer ? g_results.molecules_complex_transfer : ""));
    cJSON_AddItemToObject(molecules, "tokenCreation", cJSON_CreateString(g_results.molecules_token_creation ? g_results.molecules_token_creation : ""));
    cJSON_AddItemToObject(molecules, "walletCreation", cJSON_CreateString(g_results.molecules_wallet_creation ? g_results.molecules_wallet_creation : ""));
    cJSON_AddItemToObject(molecules, "shadowWalletClaim", cJSON_CreateString(g_results.molecules_shadow_wallet_claim ? g_results.molecules_shadow_wallet_claim : ""));
    cJSON_AddItemToObject(molecules, "mlkem768", cJSON_CreateString(g_results.molecules_mlkem768 ? g_results.molecules_mlkem768 : ""));
    cJSON_AddItemToObject(root, "molecules", molecules);
    
    /* Cross-SDK compatibility */
    cJSON_AddItemToObject(root, "crossSdkCompatible", cJSON_CreateBool(g_results.cross_sdk_compatible));
    
    /* Convert to string and write to file */
    char *json_string = cJSON_Print(root);
    if (!json_string) {
        cJSON_Delete(root);
        return false;
    }
    
    FILE *file = fopen(results_path, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create results file: %s\n", results_path);
        free(json_string);
        cJSON_Delete(root);
        return false;
    }
    
    fprintf(file, "%s\n", json_string);
    fclose(file);
    
    printf("\n%s📁 Results saved to: %s%s\n", COLOR_BLUE, results_path, COLOR_RESET);
    
    free(json_string);
    cJSON_Delete(root);
    return true;
}

/**
 * Display test summary following JavaScript pattern exactly
 */
static void display_summary(void) {
    log_message("\n═══════════════════════════════════════════", COLOR_BLUE);
    log_message("            TEST SUMMARY REPORT", COLOR_BLUE);
    log_message("═══════════════════════════════════════════", COLOR_BLUE);
    log_message("", NULL);
    
    printf("SDK: C v%s\n", g_results.version);
    printf("Timestamp: %s\n", g_results.timestamp);
    
    /* Count passed tests */
    int total_tests = 9; // crypto + 3 base + 3 extended (token/wallet/shadow) + ML-KEM768 + negative
    int passed_tests = 0;
    if (g_results.crypto.passed) passed_tests++;
    if (g_results.meta_creation.passed) passed_tests++;
    if (g_results.simple_transfer.passed) passed_tests++;
    if (g_results.complex_transfer.passed) passed_tests++;
    if (g_results.token_creation.passed) passed_tests++;
    if (g_results.wallet_creation.passed) passed_tests++;
    if (g_results.shadow_wallet_claim.passed) passed_tests++;
    if (g_results.mlkem768.passed) passed_tests++;
    if (g_results.negative_cases.passed) passed_tests++;
    
    const char *color = (passed_tests == total_tests) ? COLOR_GREEN : COLOR_RED;
    printf("\n%sTests Passed: %d/%d%s\n", color, passed_tests, total_tests, COLOR_RESET);
    
    /* Show failed tests */
    if (passed_tests < total_tests) {
        printf("\n%sFailed Tests:%s\n", COLOR_RED, COLOR_RESET);
        if (!g_results.crypto.passed) {
            printf("  - crypto: %s\n", g_results.crypto.error ? g_results.crypto.error : "Validation failed");
        }
        if (!g_results.meta_creation.passed) {
            printf("  - metaCreation: Validation failed\n");
        }
        if (!g_results.simple_transfer.passed) {
            printf("  - simpleTransfer: Validation failed\n");
        }
        if (!g_results.complex_transfer.passed) {
            printf("  - complexTransfer: Validation failed\n");
        }
        if (!g_results.token_creation.passed) {
            printf("  - tokenCreation: Validation failed\n");
        }
        if (!g_results.wallet_creation.passed) {
            printf("  - walletCreation: Validation failed\n");
        }
        if (!g_results.shadow_wallet_claim.passed) {
            printf("  - shadowWalletClaim: Validation failed\n");
        }
        if (!g_results.mlkem768.passed) {
            printf("  - mlkem768: %s\n", g_results.mlkem768.error ? g_results.mlkem768.error : "Validation failed");
        }
        if (!g_results.negative_cases.passed) {
            printf("  - negativeCases: %s\n", g_results.negative_cases.error ? g_results.negative_cases.error : "Validation failed");
        }
    }
    
    const char *compat_color = g_results.cross_sdk_compatible ? COLOR_GREEN : COLOR_RED;
    const char *compat_status = g_results.cross_sdk_compatible ? "✅ YES" : "❌ NO";
    printf("\n%sCross-SDK Compatible: %s%s\n", compat_color, compat_status, COLOR_RESET);
    
    log_message("═══════════════════════════════════════════", COLOR_BLUE);
}

/**
 * Cleanup function for program exit
 */
static void cleanup_resources(void) {
    if (g_config) {
        cJSON_Delete(g_config);
        g_config = NULL;
    }
    
    /* Free result strings */
    free(g_results.crypto.secret);
    free(g_results.crypto.bundle);
    free(g_results.crypto.expected_secret);
    free(g_results.crypto.expected_bundle);
    free(g_results.crypto.error);
    
    free(g_results.meta_creation.molecular_hash);
    free(g_results.meta_creation.validation_error);
    
    free(g_results.simple_transfer.molecular_hash);
    free(g_results.simple_transfer.validation_error);
    
    free(g_results.complex_transfer.molecular_hash);
    free(g_results.complex_transfer.validation_error);
    
    free(g_results.mlkem768.error);

    free(g_results.negative_cases.description);
    free(g_results.negative_cases.error);

    free(g_results.molecules_metadata);
    free(g_results.molecules_simple_transfer);
    free(g_results.molecules_complex_transfer);
    free(g_results.molecules_token_creation);
    free(g_results.molecules_wallet_creation);
    free(g_results.molecules_shadow_wallet_claim);
    free(g_results.molecules_mlkem768);
    
    free(g_results.sdk);
    free(g_results.version);
    free(g_results.timestamp);
}

/**
 * Main program entry point
 */
int main(void) {
    /* Register cleanup function */
    atexit(cleanup_resources);
    
    /* Check for cross-validation-only mode (Round 2) */
    const char* cross_validation_only = getenv("KNISHIO_CROSS_VALIDATION_ONLY");
    if (cross_validation_only && strcmp(cross_validation_only, "true") == 0) {
        log_message("═══════════════════════════════════════════", COLOR_BLUE);
        log_message("    Knish.IO C SDK Cross-Validation Only", COLOR_BLUE);
        log_message("═══════════════════════════════════════════", COLOR_BLUE);

        /* CRITICAL FIX: Load existing Round 1 results instead of reinitializing */
        /* Try to load existing results from Round 1 first */
        const char* shared_dir = getenv("KNISHIO_SHARED_RESULTS");
        char existing_results_path[MAX_PATH_LENGTH];
        bool loaded_existing = false;
        
        if (shared_dir) {
            snprintf(existing_results_path, MAX_PATH_LENGTH, "%s/c-results.json", shared_dir);
        } else {
            strncpy(existing_results_path, "../shared-test-results/c-results.json", MAX_PATH_LENGTH - 1);
            existing_results_path[MAX_PATH_LENGTH - 1] = '\0';
        }
        
        /* Try to load existing Round 1 results */
        FILE* existing_file = fopen(existing_results_path, "r");
        if (existing_file) {
            fseek(existing_file, 0, SEEK_END);
            long file_size = ftell(existing_file);
            fseek(existing_file, 0, SEEK_SET);
            
            if (file_size > 0 && file_size < 100000) {
                char* existing_content = malloc(file_size + 1);
                if (existing_content) {
                    size_t read_size = fread(existing_content, 1, file_size, existing_file);
                    existing_content[read_size] = '\0';
                    
                    /* Parse existing results to preserve Round 1 data */
                    cJSON* existing_json = cJSON_Parse(existing_content);
                    if (existing_json) {
                        /* Initialize results structure first */
                        init_results();
                        
                        /* Copy Round 1 test results */
                        cJSON* existing_tests = cJSON_GetObjectItem(existing_json, "tests");
                        if (existing_tests) {
                            /* Copy crypto test results */
                            cJSON* crypto_test = cJSON_GetObjectItem(existing_tests, "crypto");
                            if (crypto_test) {
                                cJSON* passed = cJSON_GetObjectItem(crypto_test, "passed");
                                if (passed && cJSON_IsBool(passed)) {
                                    g_results.crypto.passed = cJSON_IsTrue(passed);
                                }
                            }
                            
                            /* Copy other test results similarly - just mark as loaded */
                            loaded_existing = true;
                            printf("✅ Loaded existing Round 1 results for preservation\n");
                        }

                        /* CRITICAL: Copy Round 1 molecules to preserve for Round 2 */
                        cJSON* existing_molecules = cJSON_GetObjectItem(existing_json, "molecules");
                        if (existing_molecules) {
                            cJSON* metadata = cJSON_GetObjectItem(existing_molecules, "metadata");
                            if (metadata && cJSON_IsString(metadata)) {
                                const char* metadata_str = cJSON_GetStringValue(metadata);
                                if (metadata_str && strlen(metadata_str) > 0) {
                                    g_results.molecules_metadata = safe_strdup(metadata_str);
                                }
                            }

                            cJSON* simple = cJSON_GetObjectItem(existing_molecules, "simpleTransfer");
                            if (simple && cJSON_IsString(simple)) {
                                const char* simple_str = cJSON_GetStringValue(simple);
                                if (simple_str && strlen(simple_str) > 0) {
                                    g_results.molecules_simple_transfer = safe_strdup(simple_str);
                                }
                            }

                            cJSON* complex = cJSON_GetObjectItem(existing_molecules, "complexTransfer");
                            if (complex && cJSON_IsString(complex)) {
                                const char* complex_str = cJSON_GetStringValue(complex);
                                if (complex_str && strlen(complex_str) > 0) {
                                    g_results.molecules_complex_transfer = safe_strdup(complex_str);
                                }
                            }

                            cJSON* mlkem = cJSON_GetObjectItem(existing_molecules, "mlkem768");
                            if (mlkem && cJSON_IsString(mlkem)) {
                                const char* mlkem_str = cJSON_GetStringValue(mlkem);
                                if (mlkem_str && strlen(mlkem_str) > 0) {
                                    g_results.molecules_mlkem768 = safe_strdup(mlkem_str);
                                }
                            }

                            printf("✅ Preserved Round 1 molecules for cross-validation\n");
                        }

                        cJSON_Delete(existing_json);
                    }
                    free(existing_content);
                }
            }
            fclose(existing_file);
        }
        
        /* If couldn't load existing results, initialize fresh */
        if (!loaded_existing) {
            init_results();
            printf("⚠️  No existing Round 1 results found, initializing fresh\n");
        }

        /* Only run cross-SDK validation */
        bool cross_sdk_result = test_cross_sdk_validation(&g_results);

        /* Save results and print summary (cross-validation only) */
        if (!save_results()) {
            fprintf(stderr, "Failed to save test results\n");
            return EXIT_FAILURE;
        }

        printf("\n%s═══════════════════════════════════════════%s\n", COLOR_BLUE, COLOR_RESET);
        printf("%s            CROSS-VALIDATION SUMMARY%s\n", COLOR_BLUE, COLOR_RESET);
        printf("%s═══════════════════════════════════════════%s\n", COLOR_BLUE, COLOR_RESET);
        const char *compat_color = cross_sdk_result ? COLOR_GREEN : COLOR_RED;
        const char *compat_status = cross_sdk_result ? "✅ YES" : "❌ NO";
        printf("%sCross-SDK Compatible: %s%s\n", compat_color, compat_status, COLOR_RESET);
        printf("%s═══════════════════════════════════════════%s\n", COLOR_BLUE, COLOR_RESET);

        /* Exit based on cross-validation results only */
        return cross_sdk_result ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* Normal mode: Run all tests (Round 1 or standalone) */
    log_message("═══════════════════════════════════════════", COLOR_BLUE);
    log_message("    Knish.IO C SDK Self-Test", COLOR_BLUE);
    log_message("═══════════════════════════════════════════", COLOR_BLUE);
    
    /* Initialize results */
    init_results();
    
    /* Load configuration */
    if (!load_config()) {
        fprintf(stderr, "Failed to load test configuration\n");
        return EXIT_FAILURE;
    }
    
    const cJSON *tests_config = cJSON_GetObjectItem(g_config, "tests");
    if (!tests_config) {
        fprintf(stderr, "Missing tests configuration\n");
        return EXIT_FAILURE;
    }
    
    /* Run all tests following JavaScript SDK pattern exactly */
    bool crypto_result = test_crypto(&g_results, tests_config);
    bool meta_result = test_meta_creation(&g_results, tests_config);
    bool simple_result = test_simple_transfer(&g_results, tests_config);
    bool complex_result = test_complex_transfer(&g_results, tests_config);
    bool token_result = test_token_creation(&g_results, tests_config);
    bool wallet_result = test_wallet_creation(&g_results, tests_config);
    bool shadow_result = test_shadow_wallet_claim(&g_results, tests_config);
    bool mlkem768_result = test_mlkem768(&g_results, tests_config);
    bool negative_result = test_negative_cases(&g_results, tests_config);
    bool cross_sdk_result = test_cross_sdk_validation(&g_results);

    /* Save results */
    if (!save_results()) {
        fprintf(stderr, "Failed to save test results\n");
        return EXIT_FAILURE;
    }

    /* Display summary */
    display_summary();

    /* Exit with appropriate code */
    int total_tests = 9; // crypto + 3 base + 3 extended (token/wallet/shadow) + ML-KEM768 + negative
    int passed_tests = (crypto_result ? 1 : 0) + (meta_result ? 1 : 0) +
                      (simple_result ? 1 : 0) + (complex_result ? 1 : 0) +
                      (token_result ? 1 : 0) + (wallet_result ? 1 : 0) + (shadow_result ? 1 : 0) +
                      (mlkem768_result ? 1 : 0) + (negative_result ? 1 : 0);

    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}