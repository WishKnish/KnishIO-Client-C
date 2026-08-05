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
    bool skipped;          /**< true when a required fixture was absent; NEVER a pass */
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
    molecule_test_result_t buffer_family;
    molecule_test_result_t wots_roundtrip;
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
    /* Cross-validation COVERAGE, not just its verdict.
     *
     * cross_sdk_compatible on its own cannot distinguish "validated all seven peers and
     * they all passed" from "validated nothing and therefore found no failures". Both
     * used to serialise as true. Recording how many peers were actually validated makes
     * a vacuous pass detectable by the orchestrator and by anyone reading the file. */
    int cross_peers_expected;
    int cross_peers_validated;
    bool cross_validation_ran;
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
 * Test 5b: ML-KEM768 cross-SDK VECTOR assertion (cycle 136)
 * Asserts the committed cross-platform-test-vectors.json `vectors.mlkem768`:
 *   keygen  → wallet ML-KEM pubkey (base64) == expectedPubkey ("nzuf1Bq2…")
 *   decrypt → decapsulate + AES-256-GCM the frozen {cipherText,encryptedMessage} == expectedPlaintext
 * C's AES-GCM is OpenSSL EVP (portable) → both asserts run on ARM + x86.
 * Reads the vendored vector; SKIPS (returns pass) if absent (standalone-CI safe).
 */
static bool test_mlkem768_vector_assertion(test_results_t *results) {
    (void)results;
    log_message("\n5b. ML-KEM768 Cross-SDK Vector Assertion", COLOR_BLUE);

    /* Locate the committed cross-platform-test-vectors.json (env override → vendored → shared dir) */
    const char *candidates[3];
    int n = 0;
    char shared_candidate[1024];
    const char *env_path = getenv("KNISHIO_CROSS_PLATFORM_VECTORS");
    if (env_path) candidates[n++] = env_path;
    candidates[n++] = "tests/fixtures/cross-platform-test-vectors.json";
    const char *shared_dir = getenv("KNISHIO_SHARED_RESULTS");
    if (shared_dir) {
        snprintf(shared_candidate, sizeof(shared_candidate), "%s/cross-platform-test-vectors.json", shared_dir);
        candidates[n++] = shared_candidate;
    }

    char *file_content = NULL;
    for (int i = 0; i < n && !file_content; i++) {
        FILE *f = fopen(candidates[i], "r");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            file_content = malloc((size_t)sz + 1);
            if (file_content) {
                size_t rd = fread(file_content, 1, (size_t)sz, f);
                file_content[rd] = '\0';
            }
        }
        fclose(f);
    }
    if (!file_content) {
        log_test("ML-KEM768 vector file present", true, "SKIPPED (vector file absent — standalone CI)");
        return true; /* skip, not fail */
    }

    /* Declare all locals up front (single cleanup label, mirrors test_mlkem768) */
    bool ok = false;
    bool keygen_ok = false;
    bool decrypt_ok = false;
    cJSON *root = NULL;
    knishio_wallet_t *wallet = NULL;
    char *seed_hex = NULL;
    char *pubkey_b64 = NULL;
    unsigned char *kem_bytes = NULL;
    unsigned char *enc_bytes = NULL;
    uint8_t *plaintext_out = NULL;
    char *plaintext_str = NULL;
    cJSON *parsed = NULL;
    size_t kem_len = 0, enc_len = 0, plaintext_len = 0;
    knishio_mlkem768_keypair_t keypair = {0};
    knishio_mlkem768_ciphertext_t kem_ct = {0};
    knishio_mlkem768_shared_secret_t ss = {0};
    uint8_t seed_bytes[64];

    root = cJSON_Parse(file_content);
    free(file_content);
    cJSON *mlkem = root ? cJSON_GetObjectItem(cJSON_GetObjectItem(root, "vectors"), "mlkem768") : NULL;
    cJSON *kg = mlkem ? cJSON_GetObjectItem(mlkem, "keygen") : NULL;
    cJSON *dec = mlkem ? cJSON_GetObjectItem(mlkem, "decrypt") : NULL;
    if (!kg || !dec) {
        log_test("ML-KEM768 vector parse", false, "missing vectors.mlkem768");
        goto vcleanup;
    }

    const char *secret = cJSON_GetStringValue(cJSON_GetObjectItem(kg, "secret"));
    const char *token = cJSON_GetStringValue(cJSON_GetObjectItem(kg, "token"));
    const char *position = cJSON_GetStringValue(cJSON_GetObjectItem(kg, "position"));
    const char *expected_pubkey = cJSON_GetStringValue(cJSON_GetObjectItem(kg, "expectedPubkey"));
    const char *cipher_text_b64 = cJSON_GetStringValue(cJSON_GetObjectItem(dec, "cipherText"));
    const char *enc_msg_b64 = cJSON_GetStringValue(cJSON_GetObjectItem(dec, "encryptedMessage"));
    const char *expected_plaintext = cJSON_GetStringValue(cJSON_GetObjectItem(dec, "expectedPlaintext"));
    if (!secret || !token || !position || !expected_pubkey || !cipher_text_b64 || !enc_msg_b64 || !expected_plaintext) {
        log_test("ML-KEM768 vector fields", false, "missing field(s)");
        goto vcleanup;
    }

    /* --- keygen assertion --- */
    if (knishio_wallet_create_simple(&wallet, secret, token, position) != KNISHIO_SUCCESS) {
        log_test("ML-KEM768 vector wallet create", false, "wallet create failed");
        goto vcleanup;
    }
    if (!knishio_generate_secret(wallet->private_key, 128, &seed_hex) || strlen(seed_hex) != 128) {
        log_test("ML-KEM768 vector seed derive", false, "seed derive failed");
        goto vcleanup;
    }
    for (int i = 0; i < 64; i++) {
        char hp[3] = {seed_hex[i * 2], seed_hex[i * 2 + 1], '\0'};
        seed_bytes[i] = (uint8_t)strtol(hp, NULL, 16);
    }
    if (knishio_mlkem768_keypair_from_seed(&keypair, seed_bytes, 64) != KNISHIO_SUCCESS) {
        log_test("ML-KEM768 vector keypair", false, "keygen failed");
        goto vcleanup;
    }
    if (!knishio_base64_encode(keypair.public_key, 1184, &pubkey_b64)) {
        log_test("ML-KEM768 vector pubkey encode", false, "b64 encode failed");
        goto vcleanup;
    }
    keygen_ok = (strcmp(pubkey_b64, expected_pubkey) == 0);
    log_test("ML-KEM768 keygen pubkey matches vector", keygen_ok, keygen_ok ? NULL : "pubkey mismatch");

    /* --- decrypt assertion (OpenSSL EVP AES-GCM → portable) --- */
    if (!knishio_base64_decode(cipher_text_b64, &kem_bytes, &kem_len) || kem_len != sizeof(kem_ct.ciphertext)) {
        log_test("ML-KEM768 vector cipherText decode", false, "bad KEM ciphertext length");
        goto vfinish;
    }
    memcpy(kem_ct.ciphertext, kem_bytes, kem_len);
    if (knishio_mlkem768_decapsulate(keypair.private_key, &kem_ct, &ss) != KNISHIO_SUCCESS) {
        log_test("ML-KEM768 vector decapsulate", false, "decapsulate failed");
        goto vfinish;
    }
    if (!knishio_base64_decode(enc_msg_b64, &enc_bytes, &enc_len)) {
        log_test("ML-KEM768 vector encryptedMessage decode", false, "b64 decode failed");
        goto vfinish;
    }
    if (knishio_aes_gcm_decrypt(enc_bytes, enc_len, ss.shared_secret, &plaintext_out, &plaintext_len) != KNISHIO_SUCCESS) {
        log_test("ML-KEM768 vector AES-GCM decrypt", false, "aes-gcm decrypt failed");
        goto vfinish;
    }
    if (plaintext_out && plaintext_len > 0) {
        plaintext_str = malloc(plaintext_len + 1);
        if (plaintext_str) {
            memcpy(plaintext_str, plaintext_out, plaintext_len);
            plaintext_str[plaintext_len] = '\0';
            parsed = cJSON_Parse(plaintext_str); /* JS JSON.stringify'd the plaintext → unwrap */
            if (parsed && cJSON_IsString(parsed)) {
                const char *unwrapped = cJSON_GetStringValue(parsed);
                decrypt_ok = (unwrapped && strcmp(unwrapped, expected_plaintext) == 0);
            }
        }
    }
    log_test("ML-KEM768 frozen sample decrypts to vector plaintext", decrypt_ok, decrypt_ok ? NULL : "plaintext mismatch");

vfinish:
    ok = keygen_ok && decrypt_ok;

vcleanup:
    if (parsed) cJSON_Delete(parsed);
    if (plaintext_str) free(plaintext_str);
    if (plaintext_out) free(plaintext_out);
    if (enc_bytes) free(enc_bytes);
    if (kem_bytes) free(kem_bytes);
    if (pubkey_b64) free(pubkey_b64);
    if (seed_hex) free(seed_hex);
    if (wallet) knishio_wallet_free(wallet);
    if (root) cJSON_Delete(root);
    return ok;
}

/**
 * Test 6: Negative Test Cases (Anti-Cheating)
 * Validates that invalid molecules properly fail validation
 */
/* ---- Shared canonical-vector helpers (used by W1 and B1) ---------------------------- */

/* Read the canonical vectors: explicit override, the vendored fixture, then the
 * orchestrator's shared directory. Returns NULL if none is readable. */
static cJSON *load_canonical_vectors(void) {
    const char *candidates[3];
    int count = 0;
    char shared_path[MAX_PATH_LENGTH];

    const char *env_vectors = getenv("KNISHIO_CANONICAL_VECTORS");
    if (env_vectors) candidates[count++] = env_vectors;
    candidates[count++] = "tests/fixtures/canonical-patent-vectors.json";
    const char *shared_dir = getenv("KNISHIO_SHARED_RESULTS");
    if (shared_dir) {
        snprintf(shared_path, sizeof(shared_path), "%s/canonical-patent-vectors.json", shared_dir);
        candidates[count++] = shared_path;
    }

    for (int i = 0; i < count; i++) {
        FILE *f = fopen(candidates[i], "r");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        cJSON *parsed = NULL;
        if (size > 0) {
            char *buf = malloc((size_t)size + 1);
            if (buf) {
                size_t n = fread(buf, 1, (size_t)size, f);
                buf[n] = '\0';
                parsed = cJSON_Parse(buf);
                free(buf);
            }
        }
        fclose(f);
        if (parsed) return parsed;
    }
    return NULL;
}

static int vec_int(const cJSON *obj, const char *key) {
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    return (item && cJSON_IsNumber(item)) ? (int)cJSON_GetNumberValue(item) : 0;
}

static const char *vec_str(const cJSON *obj, const char *key) {
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    return (item && cJSON_IsString(item)) ? cJSON_GetStringValue(item) : "";
}

/* ---- W1. WOTS+ roundtrip, vector-driven ---------------------------------------------
 *
 * Asserts the OTS address primitive directly, independent of any molecule. The protocol
 * address is a TWO-PASS derivation: hash each of the 16 key chunks 16 times, join the
 * public fragments, digest = SHAKE256(joined, 8192), address = SHAKE256(digest, 256).
 * A single-pass derivation produces a different, wrong address — the canonical vector was
 * corrected for exactly that across all SDKs.
 *
 * This isolates the primitive from the plumbing: if this passes and CheckMolecule's OTS
 * verification fails, the fault is in the molecule path, not the crypto. C and C++ were
 * the only SDKs without this coverage.
 */
static bool test_wots_roundtrip(test_results_t *results) {
    log_message("\nW1. WOTS+ Roundtrip Test (OTS address, vector-driven)", COLOR_BLUE);

    cJSON *vectors_root = load_canonical_vectors();
    if (!vectors_root) {
        const char *require = getenv("KNISHIO_REQUIRE_VECTORS");
        bool must_have = require && strcmp(require, "true") == 0;
        results->wots_roundtrip.passed = false;
        results->wots_roundtrip.skipped = !must_have;
        results->wots_roundtrip.validation_error =
            safe_strdup("canonical-patent-vectors.json absent");
        if (must_have) {
            log_message("  FAILED: canonical-patent-vectors.json absent "
                        "(KNISHIO_REQUIRE_VECTORS=true)", COLOR_RED);
            return false;
        }
        log_message("  SKIPPED: canonical-patent-vectors.json absent (standalone CI)", COLOR_YELLOW);
        return true;
    }

    const cJSON *vectors = cJSON_GetObjectItem(vectors_root, "vectors");
    const cJSON *family = cJSON_GetObjectItem(vectors, "wots_roundtrip");
    const cJSON *tests = family ? cJSON_GetObjectItem(family, "tests") : NULL;
    if (!tests) {
        results->wots_roundtrip.passed = false;
        results->wots_roundtrip.validation_error = safe_strdup("wots_roundtrip absent from vectors");
        log_message("  FAILED: wots_roundtrip absent from vectors", COLOR_RED);
        cJSON_Delete(vectors_root);
        return false;
    }

    bool all_pass = true;
    int case_count = 0;
    char *last_address = NULL;

    const cJSON *tv = NULL;
    cJSON_ArrayForEach(tv, tests) {
        const char *name = vec_str(tv, "name");
        const char *secret = vec_str(tv, "secret");
        const char *token = vec_str(tv, "token");
        const char *position = vec_str(tv, "position");
        const char *expected = vec_str(tv, "expectedOtsAddress");

        char *private_key = NULL;
        char *address = NULL;
        bool ok = knishio_generate_wallet_key(secret, token, position, &private_key);

        /* The key must be 2048 hex chars = 16 chunks of 128, or the chunking below is
         * silently wrong rather than absent. */
        ok = ok && private_key && strlen(private_key) == KNISHIO_PRIVKEY_LENGTH;
        ok = ok && knishio_generate_address(private_key, &address);
        ok = ok && address && strlen(address) == KNISHIO_ADDRESS_LENGTH;

        bool matches = ok && safe_strcmp(address, expected);

        char detail[200];
        if (!ok) {
            snprintf(detail, sizeof(detail), "key/address generation failed");
        } else if (!matches) {
            snprintf(detail, sizeof(detail), "got %s, expected %s", address, expected);
        }

        char label[160];
        snprintf(label, sizeof(label), "%s: OTS address matches canonical vector (two-pass)", name);
        log_test(label, matches, matches ? NULL : detail);
        all_pass = all_pass && matches;
        case_count++;

        if (address) {
            if (last_address) knishio_free(last_address);
            last_address = safe_strdup(address);
            knishio_free(address);
        }
        if (private_key) knishio_free(private_key);
    }

    results->wots_roundtrip.passed = all_pass;
    results->wots_roundtrip.skipped = false;
    results->wots_roundtrip.atom_count = case_count;
    results->wots_roundtrip.molecular_hash = last_address;  /* the derived OTS address */
    if (!all_pass) {
        results->wots_roundtrip.validation_error =
            safe_strdup("OTS address does not match the canonical vector");
    }

    cJSON_Delete(vectors_root);
    return all_pass;
}

/* ---- B1. Buffer family (B-isotope deposit + withdraw), vector-driven ---------------- */

/* A built buffer molecule and the three wallets it aliases. The molecule does NOT own the
 * wallets (see the note in knishio_molecule_free), so the caller frees all four. */
typedef struct {
    knishio_molecule_t *molecule;
    knishio_wallet_t *source;
    knishio_wallet_t *middle;      /* buffer wallet (deposit) or recipient (withdraw) */
    knishio_wallet_t *remainder;
} buffer_case_t;

static void buffer_case_free(buffer_case_t *c) {
    if (c->molecule) knishio_molecule_free(c->molecule);
    if (c->source) knishio_wallet_free(c->source);
    if (c->middle) knishio_wallet_free(c->middle);
    if (c->remainder) knishio_wallet_free(c->remainder);
    memset(c, 0, sizeof(*c));
}

/* Build (but do not sign) a buffer molecule using the SDK's own builders. Fresh positions
 * throughout, so each wallet has a usable OTS key and molecular hashes are not frozen. */
static bool buffer_case_build(buffer_case_t *c, const char *secret, const char *bundle,
                              const char *token, bool deposit, int balance, int amount) {
    memset(c, 0, sizeof(*c));
    char *p1 = NULL, *p2 = NULL, *p3 = NULL;
    bool ok = false;

    if (!knishio_generate_position(&p1) || !knishio_generate_position(&p2) ||
        !knishio_generate_position(&p3)) {
        goto done;
    }
    if (knishio_wallet_create_simple(&c->source, secret, token, p1) != KNISHIO_SUCCESS ||
        knishio_wallet_create_simple(&c->middle, secret, token, p2) != KNISHIO_SUCCESS ||
        knishio_wallet_create_simple(&c->remainder, secret, token, p3) != KNISHIO_SUCCESS) {
        goto done;
    }
    c->source->balance = balance;

    if (knishio_molecule_create(&c->molecule, secret, bundle, c->source, c->remainder,
                                NULL, "V4") != KNISHIO_SUCCESS) {
        goto done;
    }
    knishio_error_t e = deposit
        ? knishio_molecule_init_deposit_buffer(c->molecule, c->middle, amount)
        : knishio_molecule_init_withdraw_buffer(c->molecule, c->middle, amount);
    if (e != KNISHIO_SUCCESS) {
        goto done;
    }
    set_canonical_timestamps(c->molecule);
    ok = true;

done:
    if (p1) knishio_free(p1);
    if (p2) knishio_free(p2);
    if (p3) knishio_free(p3);
    if (!ok) buffer_case_free(c);
    return ok;
}

/* First (or last) atom of a given isotope, in emission order — the tamper targets the
 * buffer_conservation_negative vector names. */
static knishio_atom_t *buffer_pick_atom(knishio_molecule_t *m, knishio_isotope_t iso, bool first) {
    knishio_atom_t *found = NULL;
    for (size_t i = 0; i < m->atom_count; i++) {
        knishio_atom_t *a = knishio_molecule_get_atom(m, i);
        if (!a || a->isotope != iso) continue;
        if (first) return a;
        found = a;
    }
    return found;
}

/**
 * B1. Buffer family: the three-atom V->B->V deposit and B->V->B withdraw every other SDK
 * builds. For each vector case we build + sign with the real builders and assert the atom
 * shape and values match, the combined V+B sum is 0 (the full-balance debit conserves even
 * for a PARTIAL operation), and knishio_molecule_check accepts — which exercises the
 * cross-isotope bypass, since a buffer molecule's V atoms alone do NOT sum to zero.
 *
 * The negative cases then tamper one field of a valid molecule and require rejection with
 * KNISHIO_ERROR_INVALID_STATE specifically. That code is precise here: every other path
 * returning it now sits inside the !cross_isotope branch, and a hash mismatch has its own
 * code — so for a buffer molecule it can only have come from the conservation delegate. A
 * bare "was rejected" would also pass if some unrelated check happened to fire.
 *
 * Molecular hashes are NOT frozen (positions are random). Skips if the fixture is absent,
 * and fails hard instead when KNISHIO_REQUIRE_VECTORS=true.
 */
static bool test_buffer_family(test_results_t *results) {
    log_message("\nB1. Buffer Family Test (deposit + withdraw, vector-driven)", COLOR_BLUE);

    cJSON *vectors_root = load_canonical_vectors();
    if (!vectors_root) {
        /* In an orchestrated cross-SDK run the vectors are mandatory: silently skipping
         * parity coverage is the false green this gate exists to stop. */
        const char *require = getenv("KNISHIO_REQUIRE_VECTORS");
        bool must_have = require && strcmp(require, "true") == 0;
        results->buffer_family.passed = false;
        results->buffer_family.skipped = !must_have;
        results->buffer_family.validation_error =
            safe_strdup("canonical-patent-vectors.json absent");
        if (must_have) {
            log_message("  FAILED: canonical-patent-vectors.json absent "
                        "(KNISHIO_REQUIRE_VECTORS=true)", COLOR_RED);
            return false;
        }
        log_message("  SKIPPED: canonical-patent-vectors.json absent (standalone CI)", COLOR_YELLOW);
        return true;  /* skipped and recorded as such — never counted as a pass */
    }

    const cJSON *vectors = cJSON_GetObjectItem(vectors_root, "vectors");
    const char *token = "BUFTOK";
    char *secret = NULL;
    char *bundle = NULL;
    bool all_pass = true;
    int atom_total = 0;
    char *last_hash = NULL;

    if (!knishio_generate_secret("buffer-family-self-test-seed", 2048, &secret) ||
        !knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)) {
        results->buffer_family.passed = false;
        results->buffer_family.validation_error = safe_strdup("secret/bundle generation failed");
        if (secret) knishio_free(secret);
        cJSON_Delete(vectors_root);
        return false;
    }

    /* ---- Positive cases: deposit (V -> B -> V) then withdraw (B -> V -> B) ---- */
    const struct { const char *key; bool deposit; const char *middle_field; } families[] = {
        { "buffer_deposit_conservation",  true,  "expectedBufferValue" },
        { "buffer_withdraw_conservation", false, "expectedRecipientValue" },
    };

    for (size_t fam = 0; fam < sizeof(families) / sizeof(families[0]); fam++) {
        const cJSON *family = cJSON_GetObjectItem(vectors, families[fam].key);
        const cJSON *tests = family ? cJSON_GetObjectItem(family, "tests") : NULL;
        if (!tests) {
            printf("  %s✗%s %s absent from vectors\n", COLOR_RED, COLOR_RESET, families[fam].key);
            all_pass = false;
            continue;
        }

        const cJSON *tv = NULL;
        cJSON_ArrayForEach(tv, tests) {
            const char *name = vec_str(tv, "name");
            int balance = vec_int(tv, "sourceBalance");
            int amount = vec_int(tv, "amount");

            buffer_case_t c;
            if (!buffer_case_build(&c, secret, bundle, token, families[fam].deposit,
                                   balance, amount)) {
                log_test(name, false, "builder setup failed");
                all_pass = false;
                continue;
            }

            bool ok = (knishio_molecule_sign(c.molecule, bundle, false, true) == KNISHIO_SUCCESS);

            /* Shape: exactly three atoms, isotopes and values as the vector states. */
            const knishio_isotope_t outer = families[fam].deposit
                ? KNISHIO_ISOTOPE_V : KNISHIO_ISOTOPE_B;
            const knishio_isotope_t middle = families[fam].deposit
                ? KNISHIO_ISOTOPE_B : KNISHIO_ISOTOPE_V;
            ok = ok && c.molecule->atom_count == 3;

            long long sum = 0;
            if (ok) {
                const knishio_atom_t *a0 = knishio_molecule_get_atom(c.molecule, 0);
                const knishio_atom_t *a1 = knishio_molecule_get_atom(c.molecule, 1);
                const knishio_atom_t *a2 = knishio_molecule_get_atom(c.molecule, 2);
                ok = a0 && a1 && a2 &&
                     a0->isotope == outer  && safe_strcmp(a0->value, vec_str(tv, "expectedSourceValue")) &&
                     a1->isotope == middle && safe_strcmp(a1->value, vec_str(tv, families[fam].middle_field)) &&
                     a2->isotope == outer  && safe_strcmp(a2->value, vec_str(tv, "expectedRemainderValue"));
                for (size_t i = 0; i < c.molecule->atom_count; i++) {
                    const knishio_atom_t *a = knishio_molecule_get_atom(c.molecule, i);
                    if (a && a->value &&
                        (a->isotope == KNISHIO_ISOTOPE_V || a->isotope == KNISHIO_ISOTOPE_B)) {
                        sum += strtoll(a->value, NULL, 10);
                    }
                }
            }

            char sum_str[32];
            snprintf(sum_str, sizeof(sum_str), "%lld", sum);
            ok = ok && safe_strcmp(sum_str, vec_str(tv, "expectedSum"));

            /* And the verifier must ACCEPT it — the cross-isotope bypass in action. */
            ok = ok && (knishio_molecule_check(c.molecule, c.source) == KNISHIO_SUCCESS);

            char label[160];
            snprintf(label, sizeof(label), "%s %s conserves (V+B sum 0; cross-isotope bypass)",
                     families[fam].deposit ? "deposit" : "withdraw", name);
            log_test(label, ok, ok ? NULL : "shape, sum or verification failed");
            all_pass = all_pass && ok;

            atom_total += (int)c.molecule->atom_count;
            if (c.molecule->molecular_hash) {
                if (last_hash) knishio_free(last_hash);
                last_hash = safe_strdup(c.molecule->molecular_hash);
            }
            buffer_case_free(&c);
        }
    }

    /* ---- Negative cases: tampered molecules the verifier MUST reject ----
     * A positive-only suite never observes a rejection, so it cannot tell a real
     * conservation check apart from an absent one. */
    const cJSON *negative = cJSON_GetObjectItem(vectors, "buffer_conservation_negative");
    const cJSON *negative_tests = negative ? cJSON_GetObjectItem(negative, "tests") : NULL;
    if (negative_tests) {
        const cJSON *tv = NULL;
        cJSON_ArrayForEach(tv, negative_tests) {
            const char *name = vec_str(tv, "name");
            const cJSON *tamper = cJSON_GetObjectItem(tv, "tamper");
            const char *target = vec_str(tamper, "target");
            const char *field = vec_str(tamper, "field");
            const char *to = vec_str(tamper, "to");
            bool deposit = strcmp(vec_str(tv, "buildFrom"), "deposit") == 0;

            buffer_case_t c;
            if (!buffer_case_build(&c, secret, bundle, token, deposit,
                                   vec_int(tv, "sourceBalance"), vec_int(tv, "amount"))) {
                log_test(name, false, "builder setup failed");
                all_pass = false;
                continue;
            }

            /* Follow the vector's recipe: build valid, mutate ONE field, re-sign. */
            knishio_isotope_t iso = (target[strlen(target) - 1] == 'V')
                ? KNISHIO_ISOTOPE_V : KNISHIO_ISOTOPE_B;
            knishio_atom_t *victim = buffer_pick_atom(c.molecule, iso,
                                                      strncmp(target, "first", 5) == 0);
            bool tampered = false;
            if (victim) {
                if (strcmp(field, "value") == 0) {
                    knishio_free(victim->value);
                    victim->value = knishio_strdup(to);
                    tampered = true;
                } else if (strcmp(field, "metaType") == 0) {
                    knishio_free(victim->meta_type);
                    victim->meta_type = knishio_strdup(to);
                    tampered = true;
                }
            }

            bool ok = false;
            char detail[160];
            if (!tampered) {
                snprintf(detail, sizeof(detail), "could not apply tamper %s/%s", target, field);
            } else {
                knishio_molecule_sign(c.molecule, bundle, false, true);
                knishio_error_t verdict = knishio_molecule_check(c.molecule, c.source);

                /* Require the code the tampered field should produce, not merely "rejected".
                 * A value tamper must break conservation; a metaType tamper must break the
                 * B/F meta shape. Accepting any rejection would let an unrelated check —
                 * OTS, ContinuID — pass this test while conservation did nothing. */
                knishio_error_t want = (strcmp(field, "metaType") == 0)
                    ? KNISHIO_ERROR_META_MISSING
                    : KNISHIO_ERROR_TRANSFER_UNBALANCED;
                ok = (verdict == want);
                if (verdict == KNISHIO_SUCCESS) {
                    snprintf(detail, sizeof(detail), "ACCEPTED a molecule that %s",
                             vec_str(tv, "reason"));
                } else {
                    snprintf(detail, sizeof(detail),
                             "rejected with error %d, but the %s tamper should give %d",
                             (int)verdict, field, (int)want);
                }
            }

            char label[160];
            snprintf(label, sizeof(label), "negative %s rejected", name);
            log_test(label, ok, ok ? NULL : detail);
            all_pass = all_pass && ok;
            buffer_case_free(&c);
        }
    } else {
        const char *require = getenv("KNISHIO_REQUIRE_VECTORS");
        if (require && strcmp(require, "true") == 0) {
            log_message("  FAILED: buffer_conservation_negative absent "
                        "(KNISHIO_REQUIRE_VECTORS=true)", COLOR_RED);
            all_pass = false;
        } else {
            log_message("  SKIPPED: buffer_conservation_negative absent "
                        "(vector not yet vendored)", COLOR_YELLOW);
        }
    }

    results->buffer_family.passed = all_pass;
    results->buffer_family.skipped = false;
    results->buffer_family.atom_count = atom_total;
    results->buffer_family.molecular_hash = last_hash;
    if (!all_pass) {
        results->buffer_family.validation_error =
            safe_strdup("buffer family conservation or rejection assertions failed");
    }

    knishio_free(secret);
    knishio_free(bundle);
    cJSON_Delete(vectors_root);
    return all_pass;
}

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
    
    /* Round 1 generates molecules and does not cross-validate. It therefore has NO
     * opinion on cross-SDK compatibility, and must not assert one.
     *
     * This used to set cross_sdk_compatible = true, so a round-1 results file claimed
     * full cross-SDK compatibility having validated nothing at all. Round 2 normally
     * overwrites the file and corrects it, which hid the problem — but any consumer
     * reading a round-1 file, or reading after a standalone round-1 run, was handed a
     * fabricated pass. Leave the verdict unset and mark that no cross-validation ran. */
    const char* disable_cross_validation = getenv("KNISHIO_DISABLE_CROSS_VALIDATION");
    if (disable_cross_validation && strcmp(disable_cross_validation, "true") == 0) {
        log_message("  ⏭️  Cross-validation disabled for Round 1 (molecule generation only)", COLOR_YELLOW);
        results->cross_validation_ran = false;
        results->cross_sdk_compatible = false;
        results->cross_peers_validated = 0;
        results->cross_peers_expected = 0;
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

    /* No shared results directory in Round 2 is a HARD FAILURE, not a skip.
     *
     * This returned true — "compatible" — when it could not find a single peer molecule
     * to check. Absence of evidence was reported as evidence of compatibility, which is
     * the one thing a cross-validation test must never do. Round 2 exists to check peers;
     * if the peers are not there, the check did not happen and cannot have passed. */
    results->cross_validation_ran = true;
    if (stat(results_dir_path, &st) == -1) {
        log_message("  ❌ Shared results directory not found — cross-validation CANNOT run", COLOR_RED);
        log_message("     Round 2 requires peer molecules. Reporting failure, not a skip.", COLOR_RED);
        results->cross_sdk_compatible = false;
        results->cross_peers_validated = 0;
        results->cross_peers_expected = 7;
        return false;
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
    int peers_seen = 0;

    results->cross_peers_expected = num_sdks;

    printf("\n");
    for (int i = 0; i < num_sdks; i++) {
        printf("  🧪 Validating %s SDK molecules:\n", sdk_names[i]);

        /* A peer whose results file is absent is an UNVALIDATED peer, not an absent one.
         *
         * This used to `continue` silently, contributing nothing to either counter, so six
         * missing peers left total_validations reflecting only the one file that happened
         * to be present — and all_valid below then compared that truncated count against
         * itself and passed. Count the peer as seen-and-failed so the coverage check at the
         * end can tell that we did not look at everything we were supposed to. */
        FILE* file = fopen(sdk_files[i], "r");
        if (!file) {
            printf("    ❌ Results file not found for %s — peer NOT validated\n", sdk_names[i]);
            continue;
        }
        peers_seen++;
        
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
                                            
                                            /* Build the TESTSEED wallet — all 8 SDKs share this keypair.
                                             * (cycle 138) The old hardcoded 736-hex secret + knishio_wallet_create
                                             * [which re-derives from a SEED] produced the WRONG ML-KEM keypair; the
                                             * weak encrypt-to-their-pubkey check never used our keypair so it went
                                             * unnoticed. Use generate_secret("TESTSEED") + create_simple [takes a
                                             * SECRET directly], matching test_mlkem768 + the c136 vector test. */
                                            const char* test_token = "ENCRYPT";
                                            const char* test_position = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";

                                            char* test_secret = NULL;
                                            knishio_wallet_t* test_wallet = NULL;
                                            bool wallet_result = knishio_generate_secret("TESTSEED", 2048, &test_secret) &&
                                                                 test_secret &&
                                                                 knishio_wallet_create_simple(&test_wallet, test_secret, test_token, test_position) == KNISHIO_SUCCESS;
                                            
                                            if (wallet_result && test_wallet) {
                                                /* STRONG cross-SDK check (cycle 138): decrypt THEIR encryptedData
                                                 * with our TESTSEED keypair (all 8 SDKs share it) and assert the
                                                 * plaintext — real decrypt-interop, not the old weak encrypt-to-pubkey.
                                                 * Re-derive the keypair (the wallet doesn't store the ML-KEM privkey;
                                                 * mirrors test_mlkem768_vector_assertion). C's AES-GCM is OpenSSL EVP
                                                 * (portable) → no AES-NI guard needed. */
                                                cJSON* ct_item = cJSON_GetObjectItem(encrypted_data, "cipherText");
                                                cJSON* em_item = cJSON_GetObjectItem(encrypted_data, "encryptedMessage");
                                                char* seed_hex = NULL;
                                                uint8_t* ct_bytes = NULL;
                                                uint8_t* em_bytes = NULL;
                                                uint8_t* decrypted = NULL;
                                                size_t ct_len = 0, em_len = 0, dec_len = 0;
                                                knishio_mlkem768_keypair_t cv_keypair = {0};
                                                knishio_mlkem768_ciphertext_t cv_ct = {0};
                                                knishio_mlkem768_shared_secret_t cv_ss = {0};

                                                if (ct_item && cJSON_IsString(ct_item) && em_item && cJSON_IsString(em_item) &&
                                                    knishio_generate_secret(test_wallet->private_key, 128, &seed_hex) &&
                                                    seed_hex && strlen(seed_hex) == 128) {
                                                    uint8_t seed_bytes[64];
                                                    for (int b = 0; b < 64; b++) {
                                                        char hp[3] = {seed_hex[b * 2], seed_hex[b * 2 + 1], '\0'};
                                                        seed_bytes[b] = (uint8_t)strtol(hp, NULL, 16);
                                                    }
                                                    if (knishio_mlkem768_keypair_from_seed(&cv_keypair, seed_bytes, 64) == KNISHIO_SUCCESS &&
                                                        knishio_base64_decode(cJSON_GetStringValue(ct_item), &ct_bytes, &ct_len) &&
                                                        ct_len == sizeof(cv_ct.ciphertext) &&
                                                        knishio_base64_decode(cJSON_GetStringValue(em_item), &em_bytes, &em_len)) {
                                                        memcpy(cv_ct.ciphertext, ct_bytes, ct_len);
                                                        if (knishio_mlkem768_decapsulate(cv_keypair.private_key, &cv_ct, &cv_ss) == KNISHIO_SUCCESS &&
                                                            knishio_aes_gcm_decrypt(em_bytes, em_len, cv_ss.shared_secret, &decrypted, &dec_len) == KNISHIO_SUCCESS &&
                                                            decrypted && dec_len > 0) {
                                                            char* dj = malloc(dec_len + 1);
                                                            if (dj) {
                                                                memcpy(dj, decrypted, dec_len);
                                                                dj[dec_len] = '\0';
                                                                cJSON* parsed = cJSON_Parse(dj); /* JS JSON.stringify'd the plaintext */
                                                                if (parsed && cJSON_IsString(parsed)) {
                                                                    const char* unwrapped = cJSON_GetStringValue(parsed);
                                                                    is_valid = (unwrapped && strcmp(unwrapped, expected_plaintext) == 0);
                                                                }
                                                                if (parsed) cJSON_Delete(parsed);
                                                                free(dj);
                                                            }
                                                        }
                                                    }
                                                }
                                                if (is_valid) {
                                                    printf("    ✅ Successfully decrypted %s's ML-KEM768 message\n", sdk_names[i]);
                                                } else {
                                                    printf("    ❌ Failed to decrypt %s's ML-KEM768 message\n", sdk_names[i]);
                                                }
                                                if (seed_hex) free(seed_hex);
                                                if (ct_bytes) free(ct_bytes);
                                                if (em_bytes) free(em_bytes);
                                                if (decrypted) free(decrypted);
                                                
                                                knishio_wallet_free(test_wallet);
                                            } else {
                                                is_valid = false;  /* Wallet creation failed */
                                            }
                                            if (test_secret) free(test_secret);
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
    
    /* COVERAGE FLOOR.
     *
     * `passed_validations == total_validations` is vacuously true when both are zero, and
     * both are initialised to zero. Validating nothing therefore reported full cross-SDK
     * compatibility. Every counter-based check of this shape needs a floor: it is not
     * enough that nothing failed, something has to have actually been checked.
     *
     * Two independent conditions now have to hold:
     *   1. every peer we were supposed to validate was present and validated
     *   2. every individual molecule validation that ran, passed
     * Condition 1 is the one that was missing, and it is the reason this test could not
     * turn red no matter how badly cross-SDK compatibility had regressed. */
    results->cross_peers_validated = peers_seen;

    bool full_coverage = (peers_seen == results->cross_peers_expected);
    bool nothing_failed = (total_validations > 0 && passed_validations == total_validations);
    bool all_valid = full_coverage && nothing_failed;

    printf("\n");
    printf("  📊 Cross-validation coverage: %d/%d peer SDKs, %d/%d molecule checks passed\n",
           peers_seen, results->cross_peers_expected, passed_validations, total_validations);

    if (!full_coverage) {
        log_message("  ❌ Incomplete coverage — not every peer SDK was validated", COLOR_RED);
    }
    if (total_validations == 0) {
        log_message("  ❌ Zero molecule validations ran — nothing was actually checked", COLOR_RED);
    } else if (passed_validations != total_validations) {
        log_message("  ❌ Some cross-SDK molecules failed validation", COLOR_RED);
    }

    if (all_valid) {
        log_message("  ✅ All cross-SDK molecules validated successfully", COLOR_GREEN);
        log_message("  ✅ Cross-SDK Compatible: YES", COLOR_GREEN);
    } else {
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

    /* WOTS+ roundtrip. molecularHash carries the derived OTS address; atomCount the number
     * of vector cases run. */
    cJSON *wots_roundtrip = cJSON_CreateObject();
    cJSON_AddItemToObject(wots_roundtrip, "passed", cJSON_CreateBool(g_results.wots_roundtrip.passed));
    cJSON_AddItemToObject(wots_roundtrip, "skipped", cJSON_CreateBool(g_results.wots_roundtrip.skipped));
    cJSON_AddItemToObject(wots_roundtrip, "molecularHash", cJSON_CreateString(g_results.wots_roundtrip.molecular_hash ? g_results.wots_roundtrip.molecular_hash : ""));
    cJSON_AddItemToObject(wots_roundtrip, "atomCount", cJSON_CreateNumber(g_results.wots_roundtrip.atom_count));
    cJSON_AddItemToObject(wots_roundtrip, "validationError", cJSON_CreateString(g_results.wots_roundtrip.validation_error ? g_results.wots_roundtrip.validation_error : "null"));
    cJSON_AddItemToObject(tests, "wotsRoundtrip", wots_roundtrip);

    /* Buffer family test (B-isotope deposit + withdraw). "skipped" is serialised so a
     * missing fixture is visibly distinct from a pass — the two were indistinguishable
     * when this key was silently dropped altogether. */
    cJSON *buffer_family = cJSON_CreateObject();
    cJSON_AddItemToObject(buffer_family, "passed", cJSON_CreateBool(g_results.buffer_family.passed));
    cJSON_AddItemToObject(buffer_family, "skipped", cJSON_CreateBool(g_results.buffer_family.skipped));
    cJSON_AddItemToObject(buffer_family, "molecularHash", cJSON_CreateString(g_results.buffer_family.molecular_hash ? g_results.buffer_family.molecular_hash : ""));
    cJSON_AddItemToObject(buffer_family, "atomCount", cJSON_CreateNumber(g_results.buffer_family.atom_count));
    cJSON_AddItemToObject(buffer_family, "validationError", cJSON_CreateString(g_results.buffer_family.validation_error ? g_results.buffer_family.validation_error : "null"));
    cJSON_AddItemToObject(tests, "bufferFamily", buffer_family);

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
    
    /* Cross-SDK compatibility, plus the coverage behind the verdict.
     *
     * crossSdkCompatible alone is not falsifiable by a reader: true could mean "checked
     * seven peers, all good" or "checked nothing". crossValidation.targetsValidated makes
     * the difference visible, and lets the orchestrator assert a floor. */
    cJSON_AddItemToObject(root, "crossSdkCompatible", cJSON_CreateBool(g_results.cross_sdk_compatible));

    cJSON *cross_validation = cJSON_CreateObject();
    cJSON_AddItemToObject(cross_validation, "ran", cJSON_CreateBool(g_results.cross_validation_ran));
    cJSON_AddItemToObject(cross_validation, "targetsExpected", cJSON_CreateNumber(g_results.cross_peers_expected));
    cJSON_AddItemToObject(cross_validation, "targetsValidated", cJSON_CreateNumber(g_results.cross_peers_validated));
    cJSON_AddItemToObject(root, "crossValidation", cross_validation);

    /* Run identity. The shared results directory holds one mutable file per SDK with no
     * record of which run wrote it, so a later standalone run silently replaces the
     * evidence an already-published report was built from. Stamping the orchestrator's
     * run id lets the coherence gate assert identity instead of guessing from mtimes. */
    const char *run_id = getenv("KNISHIO_RUN_ID");
    cJSON_AddItemToObject(root, "runId", run_id && *run_id
        ? cJSON_CreateString(run_id)
        : cJSON_CreateNull());

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
    int total_tests = 11; // crypto + 3 base + 3 extended (token/wallet/shadow) + WOTS + buffer family + ML-KEM768 + negative
    int passed_tests = 0;
    if (g_results.crypto.passed) passed_tests++;
    if (g_results.meta_creation.passed) passed_tests++;
    if (g_results.simple_transfer.passed) passed_tests++;
    if (g_results.complex_transfer.passed) passed_tests++;
    if (g_results.token_creation.passed) passed_tests++;
    if (g_results.wallet_creation.passed) passed_tests++;
    if (g_results.shadow_wallet_claim.passed) passed_tests++;
    if (g_results.wots_roundtrip.passed) passed_tests++;
    if (g_results.buffer_family.passed) passed_tests++;
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
        if (!g_results.wots_roundtrip.passed) {
            printf("  - wotsRoundtrip: %s%s\n",
                   g_results.wots_roundtrip.validation_error ? g_results.wots_roundtrip.validation_error : "Validation failed",
                   g_results.wots_roundtrip.skipped ? " (SKIPPED — missing coverage, not a pass)" : "");
        }
        if (!g_results.buffer_family.passed) {
            printf("  - bufferFamily: %s%s\n",
                   g_results.buffer_family.validation_error ? g_results.buffer_family.validation_error : "Validation failed",
                   g_results.buffer_family.skipped ? " (SKIPPED — missing coverage, not a pass)" : "");
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

                            /* Copy each molecule test result (passed / molecularHash / atomCount)
                             * so the Round 2 rewrite doesn't zero them out. */
                            struct { const char* key; molecule_test_result_t* dst; } test_map[] = {
                                { "metaCreation",      &g_results.meta_creation },
                                { "simpleTransfer",    &g_results.simple_transfer },
                                { "complexTransfer",   &g_results.complex_transfer },
                                { "tokenCreation",     &g_results.token_creation },
                                { "walletCreation",    &g_results.wallet_creation },
                                { "shadowWalletClaim", &g_results.shadow_wallet_claim },
                                { "bufferFamily",      &g_results.buffer_family },
                                { "wotsRoundtrip",     &g_results.wots_roundtrip },
                            };
                            for (size_t tm = 0; tm < sizeof(test_map) / sizeof(test_map[0]); tm++) {
                                cJSON* t = cJSON_GetObjectItem(existing_tests, test_map[tm].key);
                                if (!t) continue;
                                cJSON* tp = cJSON_GetObjectItem(t, "passed");
                                if (tp && cJSON_IsBool(tp)) test_map[tm].dst->passed = cJSON_IsTrue(tp);
                                /* Only bufferFamily emits "skipped"; absent elsewhere, so this
                                 * is a no-op for the others. Losing it would turn a Round 1
                                 * skip into what reads as a Round 2 failure. */
                                cJSON* ts = cJSON_GetObjectItem(t, "skipped");
                                if (ts && cJSON_IsBool(ts)) test_map[tm].dst->skipped = cJSON_IsTrue(ts);
                                cJSON* tve = cJSON_GetObjectItem(t, "validationError");
                                if (tve && cJSON_IsString(tve) && strcmp(cJSON_GetStringValue(tve), "null") != 0) {
                                    test_map[tm].dst->validation_error = safe_strdup(cJSON_GetStringValue(tve));
                                }
                                cJSON* th = cJSON_GetObjectItem(t, "molecularHash");
                                if (th && cJSON_IsString(th) && strlen(cJSON_GetStringValue(th)) > 0) {
                                    test_map[tm].dst->molecular_hash = safe_strdup(cJSON_GetStringValue(th));
                                }
                                cJSON* ta = cJSON_GetObjectItem(t, "atomCount");
                                if (ta && cJSON_IsNumber(ta)) test_map[tm].dst->atom_count = (int)cJSON_GetNumberValue(ta);
                                cJSON* tr = cJSON_GetObjectItem(t, "hasRemainder");
                                if (tr && cJSON_IsBool(tr)) test_map[tm].dst->has_remainder = cJSON_IsTrue(tr);
                            }

                            /* Copy mlkem768 test result */
                            cJSON* mlkem_test = cJSON_GetObjectItem(existing_tests, "mlkem768");
                            if (mlkem_test) {
                                cJSON* mp = cJSON_GetObjectItem(mlkem_test, "passed");
                                if (mp && cJSON_IsBool(mp)) g_results.mlkem768.passed = cJSON_IsTrue(mp);
                                cJSON* mk = cJSON_GetObjectItem(mlkem_test, "publicKeyGenerated");
                                if (mk && cJSON_IsBool(mk)) g_results.mlkem768.public_key_generated = cJSON_IsTrue(mk);
                                cJSON* me = cJSON_GetObjectItem(mlkem_test, "encryptionSuccess");
                                if (me && cJSON_IsBool(me)) g_results.mlkem768.encryption_success = cJSON_IsTrue(me);
                                cJSON* md = cJSON_GetObjectItem(mlkem_test, "decryptionSuccess");
                                if (md && cJSON_IsBool(md)) g_results.mlkem768.decryption_success = cJSON_IsTrue(md);
                                cJSON* ml = cJSON_GetObjectItem(mlkem_test, "plaintextLength");
                                if (ml && cJSON_IsNumber(ml)) g_results.mlkem768.plaintext_length = (int)cJSON_GetNumberValue(ml);
                            }

                            /* Preserve negative-case results. Omitted from test_map above
                             * because negative_test_result_t is a different struct type, and
                             * so it was silently dropped on every Round-2 rewrite: C's log
                             * printed "Tests Passed: 9/9" while the results file it wrote said
                             * negativeCases {passed:false, testCount:0}. The summary and the
                             * serialized results disagreed, which is the whole class of defect
                             * this harness now gates against. */
                            cJSON* negative_test = cJSON_GetObjectItem(existing_tests, "negativeCases");
                            if (negative_test) {
                                cJSON* np = cJSON_GetObjectItem(negative_test, "passed");
                                if (np && cJSON_IsBool(np)) g_results.negative_cases.passed = cJSON_IsTrue(np);
                                cJSON* nd = cJSON_GetObjectItem(negative_test, "description");
                                if (nd && cJSON_IsString(nd) && strlen(cJSON_GetStringValue(nd)) > 0) {
                                    g_results.negative_cases.description = safe_strdup(cJSON_GetStringValue(nd));
                                }
                                cJSON* nc = cJSON_GetObjectItem(negative_test, "testCount");
                                if (nc && cJSON_IsNumber(nc)) g_results.negative_cases.test_count = (int)cJSON_GetNumberValue(nc);
                            }

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

                            cJSON* token_creation = cJSON_GetObjectItem(existing_molecules, "tokenCreation");
                            if (token_creation && cJSON_IsString(token_creation)) {
                                const char* token_creation_str = cJSON_GetStringValue(token_creation);
                                if (token_creation_str && strlen(token_creation_str) > 0) {
                                    g_results.molecules_token_creation = safe_strdup(token_creation_str);
                                }
                            }

                            cJSON* wallet_creation = cJSON_GetObjectItem(existing_molecules, "walletCreation");
                            if (wallet_creation && cJSON_IsString(wallet_creation)) {
                                const char* wallet_creation_str = cJSON_GetStringValue(wallet_creation);
                                if (wallet_creation_str && strlen(wallet_creation_str) > 0) {
                                    g_results.molecules_wallet_creation = safe_strdup(wallet_creation_str);
                                }
                            }

                            cJSON* shadow_claim = cJSON_GetObjectItem(existing_molecules, "shadowWalletClaim");
                            if (shadow_claim && cJSON_IsString(shadow_claim)) {
                                const char* shadow_claim_str = cJSON_GetStringValue(shadow_claim);
                                if (shadow_claim_str && strlen(shadow_claim_str) > 0) {
                                    g_results.molecules_shadow_wallet_claim = safe_strdup(shadow_claim_str);
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
    bool wots_result = test_wots_roundtrip(&g_results);
    bool buffer_result = test_buffer_family(&g_results);
    bool mlkem768_result = test_mlkem768(&g_results, tests_config);
    bool mlkem768_vector_result = test_mlkem768_vector_assertion(&g_results);
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
    int total_tests = 12; // crypto + 3 base + 3 extended (token/wallet/shadow) + WOTS + buffer family + ML-KEM768 + ML-KEM768 vector + negative
    int passed_tests = (crypto_result ? 1 : 0) + (meta_result ? 1 : 0) +
                      (simple_result ? 1 : 0) + (complex_result ? 1 : 0) +
                      (token_result ? 1 : 0) + (wallet_result ? 1 : 0) + (shadow_result ? 1 : 0) +
                      (wots_result ? 1 : 0) + (buffer_result ? 1 : 0) +
                      (mlkem768_result ? 1 : 0) + (mlkem768_vector_result ? 1 : 0) + (negative_result ? 1 : 0);

    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}