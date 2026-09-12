/**
 * @file test_secret_storage.c
 * @brief Test suite for KnishIO C secret storage envelope layer and backends
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cjson/cJSON.h>

#include "knishio/storage/types.h"
#include "knishio/storage/envelope.h"
#include "knishio/storage/backend.h"
#include "knishio/utils/memory.h"

#ifndef KNISHIO_TEST_VECTORS_PATH
#define KNISHIO_TEST_VECTORS_PATH "tests/fixtures/cross-platform-test-vectors.json"
#endif

static int g_failures = 0;

static void check(bool ok, const char *name, const char *detail) {
    printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail ? " — " : "", detail ? detail : "");
    if (!ok) g_failures++;
}

static char *slurp(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = NULL;
    if (sz > 0) {
        buf = malloc((size_t)sz + 1);
        if (buf) {
            size_t rd = fread(buf, 1, (size_t)sz, f);
            buf[rd] = '\0';
        }
    }
    fclose(f);
    return buf;
}

/* ------------------------------------------------------------------ */
/* 1. Canonical cross-SDK vector decrypt test                         */
/* ------------------------------------------------------------------ */
static void test_cross_sdk_vector(const cJSON *vectors) {
    printf("\nTest 1: Cross-SDK test vector decryption\n");

    const cJSON *env_obj = cJSON_GetObjectItem(vectors, "secret_storage_envelope");
    check(env_obj != NULL, "fixture contains secret_storage_envelope", NULL);
    if (!env_obj) return;

    const cJSON *tests_arr = cJSON_GetObjectItem(env_obj, "tests");
    check(tests_arr && cJSON_GetArraySize(tests_arr) > 0, "fixture contains tests array", NULL);
    if (!tests_arr || cJSON_GetArraySize(tests_arr) == 0) return;

    const cJSON *test0 = cJSON_GetArrayItem(tests_arr, 0);
    const char *passphrase = cJSON_GetStringValue(cJSON_GetObjectItem(test0, "passphrase"));
    const char *expected_plaintext = cJSON_GetStringValue(cJSON_GetObjectItem(test0, "expectedPlaintext"));
    const cJSON *payload_json = cJSON_GetObjectItem(test0, "payload");

    check(passphrase != NULL, "test0 has passphrase", passphrase);
    check(expected_plaintext != NULL, "test0 has expectedPlaintext", expected_plaintext);
    check(payload_json != NULL, "test0 has payload object", NULL);
    if (!passphrase || !expected_plaintext || !payload_json) return;

    char *payload_str = cJSON_PrintUnformatted(payload_json);
    check(payload_str != NULL, "serialized payload JSON", NULL);
    if (!payload_str) return;

    char *decrypted = NULL;
    size_t decrypted_len = 0;
    knishio_error_t err = knishio_envelope_open_json(payload_str, passphrase, &decrypted, &decrypted_len);
    free(payload_str);

    char detail[256];
    snprintf(detail, sizeof(detail), "err=%d, got=%s", (int)err, decrypted ? decrypted : "(null)");
    check(err == KNISHIO_SUCCESS && decrypted != NULL, "envelope open succeeded", detail);

    if (decrypted) {
        check(strcmp(decrypted, expected_plaintext) == 0,
              "decrypted plaintext matches MASTER-SECRET-CROSS-SDK-PROBE", decrypted);
        check(decrypted_len == strlen(expected_plaintext),
              "decrypted length matches probe length", NULL);
        knishio_secure_zero(decrypted, decrypted_len);
        free(decrypted);
    }
}

/* ------------------------------------------------------------------ */
/* 2. Emitted metadata contract (camelCase, omit label when NULL)      */
/* ------------------------------------------------------------------ */
static void test_emitted_metadata_contract(void) {
    printf("\nTest 2: Emitted metadata contract (camelCase, omit label when NULL)\n");

    const char *secret = "TEST-EMITTED-METADATA-SECRET";
    const char *passphrase = "test-passphrase";
    const char *bundle_hash = "11223344556677889900aabbccddeeff11223344556677889900aabbccddeeff";

    /* Case A: label is NULL */
    knishio_secret_metadata_t meta_no_label;
    knishio_secret_metadata_init(&meta_no_label);
    meta_no_label.bundle_hash = strdup(bundle_hash);
    meta_no_label.label = NULL;
    meta_no_label.created_at = 1788558526546LL;
    meta_no_label.hardware_backed = false;
    meta_no_label.provider_type = strdup("aes-gcm");

    char *json_no_label = NULL;
    knishio_error_t err = knishio_envelope_seal_json(
        secret, strlen(secret), passphrase, &meta_no_label, &json_no_label
    );
    knishio_secret_metadata_cleanup(&meta_no_label);

    check(err == KNISHIO_SUCCESS && json_no_label != NULL, "seal with NULL label succeeded", NULL);

    if (json_no_label) {
        cJSON *root = cJSON_Parse(json_no_label);
        check(root != NULL, "parsed sealed JSON", NULL);
        if (root) {
            cJSON *meta = cJSON_GetObjectItem(root, "metadata");
            check(meta != NULL, "payload contains metadata object", NULL);
            if (meta) {
                /* Required camelCase keys */
                cJSON *bh = cJSON_GetObjectItem(meta, "bundleHash");
                check(bh && cJSON_IsString(bh) && strcmp(cJSON_GetStringValue(bh), bundle_hash) == 0,
                      "metadata.bundleHash is present and matches", NULL);

                cJSON *ca = cJSON_GetObjectItem(meta, "createdAt");
                check(ca && cJSON_IsNumber(ca) && (int64_t)ca->valuedouble == 1788558526546LL,
                      "metadata.createdAt is present and matches timestamp", NULL);

                cJSON *hb = cJSON_GetObjectItem(meta, "hardwareBacked");
                check(hb && cJSON_IsBool(hb) && !cJSON_IsTrue(hb),
                      "metadata.hardwareBacked is present and false", NULL);

                cJSON *pt = cJSON_GetObjectItem(meta, "providerType");
                check(pt && cJSON_IsString(pt) && strcmp(cJSON_GetStringValue(pt), "aes-gcm") == 0,
                      "metadata.providerType is present and aes-gcm", NULL);

                /* Forbidden snake_case keys */
                check(cJSON_GetObjectItem(meta, "bundle_hash") == NULL,
                      "forbidden bundle_hash is absent", NULL);
                check(cJSON_GetObjectItem(meta, "created_at") == NULL,
                      "forbidden created_at is absent", NULL);
                check(cJSON_GetObjectItem(meta, "hardware_backed") == NULL,
                      "forbidden hardware_backed is absent", NULL);
                check(cJSON_GetObjectItem(meta, "provider_type") == NULL,
                      "forbidden provider_type is absent", NULL);

                /* Optional label MUST be omitted when NULL */
                check(cJSON_GetObjectItem(meta, "label") == NULL,
                      "optional label is omitted when NULL (never null)", NULL);
            }
            cJSON_Delete(root);
        }
        free(json_no_label);
    }

    /* Case B: label is set */
    knishio_secret_metadata_t meta_with_label;
    knishio_secret_metadata_init(&meta_with_label);
    meta_with_label.bundle_hash = strdup(bundle_hash);
    meta_with_label.label = strdup("my-test-label");
    meta_with_label.created_at = 1788558526546LL;
    meta_with_label.hardware_backed = false;
    meta_with_label.provider_type = strdup("aes-gcm");

    char *json_with_label = NULL;
    err = knishio_envelope_seal_json(
        secret, strlen(secret), passphrase, &meta_with_label, &json_with_label
    );
    knishio_secret_metadata_cleanup(&meta_with_label);

    check(err == KNISHIO_SUCCESS && json_with_label != NULL, "seal with label succeeded", NULL);

    if (json_with_label) {
        cJSON *root = cJSON_Parse(json_with_label);
        if (root) {
            cJSON *meta = cJSON_GetObjectItem(root, "metadata");
            if (meta) {
                cJSON *lbl = cJSON_GetObjectItem(meta, "label");
                check(lbl && cJSON_IsString(lbl) && strcmp(cJSON_GetStringValue(lbl), "my-test-label") == 0,
                      "metadata.label is present and matches when provided", NULL);
            }
            cJSON_Delete(root);
        }
        free(json_with_label);
    }
}

/* ------------------------------------------------------------------ */
/* 3. Round-trip and wrong passphrase rejection                       */
/* ------------------------------------------------------------------ */
static void test_round_trip_and_wrong_passphrase(void) {
    printf("\nTest 3: Round trip and wrong passphrase rejection\n");

    const char *secret = "SECRET-FOR-ROUND-TRIP-VALIDATION-C-SDK-998877";
    const char *correct_pass = "correct-passphrase-alpha-omega";
    const char *wrong_pass = "wrong-passphrase-intruder";

    knishio_secret_metadata_t meta;
    knishio_secret_metadata_init(&meta);
    meta.bundle_hash = strdup("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
    meta.label = strdup("roundtrip");
    meta.created_at = 1700000000000LL;
    meta.hardware_backed = false;
    meta.provider_type = strdup("aes-gcm");

    knishio_encrypted_payload_t payload;
    knishio_error_t err = knishio_envelope_seal(secret, strlen(secret), correct_pass, &meta, &payload);
    knishio_secret_metadata_cleanup(&meta);

    check(err == KNISHIO_SUCCESS, "knishio_envelope_seal succeeded", NULL);

    /* Open with correct passphrase */
    char *plaintext = NULL;
    size_t plaintext_len = 0;
    err = knishio_envelope_open(&payload, correct_pass, &plaintext, &plaintext_len);
    check(err == KNISHIO_SUCCESS && plaintext != NULL, "open with correct passphrase succeeded", NULL);
    if (plaintext) {
        check(strcmp(plaintext, secret) == 0, "opened plaintext matches original", NULL);
        knishio_secure_zero(plaintext, plaintext_len);
        free(plaintext);
    }

    /* Open with wrong passphrase MUST fail */
    plaintext = NULL;
    plaintext_len = 0;
    err = knishio_envelope_open(&payload, wrong_pass, &plaintext, &plaintext_len);
    check(err != KNISHIO_SUCCESS, "open with wrong passphrase rejected (fails closed)", NULL);
    check(plaintext == NULL, "no plaintext returned on failure", NULL);

    /* Open with tampered ciphertext MUST fail */
    if (payload.ciphertext && strlen(payload.ciphertext) > 5) {
        payload.ciphertext[4] ^= 0x01;  /* Flip one base64 character */
        plaintext = NULL;
        plaintext_len = 0;
        err = knishio_envelope_open(&payload, correct_pass, &plaintext, &plaintext_len);
        check(err != KNISHIO_SUCCESS, "open with corrupted ciphertext rejected", NULL);
        check(plaintext == NULL, "no plaintext returned on corrupted ciphertext", NULL);
    }

    knishio_encrypted_payload_cleanup(&payload);
}

/* ------------------------------------------------------------------ */
/* 4. In-memory and file storage backends                             */
/* ------------------------------------------------------------------ */
static void test_storage_backends(void) {
    printf("\nTest 4: Storage backends (memory and file)\n");

    /* Memory backend */
    knishio_storage_backend_t *mem = NULL;
    knishio_error_t err = knishio_memory_storage_backend_create(&mem);
    check(err == KNISHIO_SUCCESS && mem != NULL, "created memory storage backend", NULL);

    if (mem) {
        char *val = NULL;
        err = mem->get_item(mem, "nonexistent", &val);
        check(err == KNISHIO_SUCCESS && val == NULL, "get nonexistent key returns NULL", NULL);

        err = mem->set_item(mem, "key1", "val1");
        check(err == KNISHIO_SUCCESS, "set_item key1", NULL);

        err = mem->get_item(mem, "key1", &val);
        check(err == KNISHIO_SUCCESS && val && strcmp(val, "val1") == 0, "get_item key1 returns val1", NULL);
        if (val) free(val);

        char **keys = NULL;
        size_t count = 0;
        err = mem->keys(mem, &keys, &count);
        check(err == KNISHIO_SUCCESS && count == 1, "keys count is 1", NULL);
        if (keys) {
            check(strcmp(keys[0], "key1") == 0, "keys[0] is key1", NULL);
            for (size_t i = 0; i < count; i++) free(keys[i]);
            free(keys);
        }

        bool existed = false;
        err = mem->remove_item(mem, "key1", &existed);
        check(err == KNISHIO_SUCCESS && existed, "remove_item key1 returned existed=true", NULL);

        err = mem->get_item(mem, "key1", &val);
        check(err == KNISHIO_SUCCESS && val == NULL, "key1 is gone after remove", NULL);

        knishio_storage_backend_free(mem);
    }

    /* File backend with 0600 permissions and persistence */
    char tmp_file[] = "/tmp/knishio_test_storage_XXXXXX";
    int fd = mkstemp(tmp_file);
    if (fd >= 0) close(fd);

    knishio_storage_backend_t *file_be = NULL;
    err = knishio_file_storage_backend_create(tmp_file, &file_be);
    check(err == KNISHIO_SUCCESS && file_be != NULL, "created file storage backend", NULL);

    if (file_be) {
        err = file_be->set_item(file_be, "persistKey", "persistVal");
        check(err == KNISHIO_SUCCESS, "set_item persistKey", NULL);

        /* Verify file permissions are 0600 */
        struct stat st;
        if (stat(tmp_file, &st) == 0) {
            mode_t perm = st.st_mode & 0777;
            char perm_str[32];
            snprintf(perm_str, sizeof(perm_str), "0%o (expected 0600)", (unsigned int)perm);
            check(perm == 0600, "file backend creates file with 0600 permissions", perm_str);
        } else {
            check(false, "stat file backend file", NULL);
        }

        /* Close backend */
        knishio_storage_backend_free(file_be);

        /* Reopen backend from same file to prove atomic persistence */
        knishio_storage_backend_t *reopened = NULL;
        err = knishio_file_storage_backend_create(tmp_file, &reopened);
        check(err == KNISHIO_SUCCESS && reopened != NULL, "reopened file storage backend", NULL);

        if (reopened) {
            char *val = NULL;
            err = reopened->get_item(reopened, "persistKey", &val);
            check(err == KNISHIO_SUCCESS && val && strcmp(val, "persistVal") == 0,
                  "reopened file backend retained stored key-value pair", NULL);
            if (val) free(val);

            knishio_storage_backend_free(reopened);
        }

        unlink(tmp_file);
    }
}

/* ------------------------------------------------------------------ */
/* 5. Secret recovery envelope, delete, list, and re-enrollment       */
/* ------------------------------------------------------------------ */
static void test_recovery_envelope_and_reenrollment(void) {
    printf("\nTest 5: Secret recovery envelope, delete, list, and re-enrollment\n");

    knishio_storage_backend_t *backend = NULL;
    knishio_error_t err = knishio_memory_storage_backend_create(&backend);
    check(err == KNISHIO_SUCCESS && backend != NULL, "created memory backend for recovery test", NULL);
    if (!backend) return;

    const char *bundle_hash = "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899";
    const char *secret = "MY-SUPER-SECRET-MASTER-KEY-1024";
    const char *primary_pass = "primary-passphrase-123";
    const char *recovery_pass = "backup-recovery-passphrase-456";
    const char *new_pass = "new-primary-passphrase-789";

    /* Storage options with recovery passphrase */
    knishio_storage_options_t opts;
    knishio_storage_options_init(&opts);
    opts.label = "recovery-test-label";
    opts.recovery_passphrase = recovery_pass;

    /* 1. Store secret with recovery passphrase */
    err = knishio_envelope_store_secret(backend, bundle_hash, secret, strlen(secret), primary_pass, &opts);
    check(err == KNISHIO_SUCCESS, "store_secret with recovery passphrase succeeded", NULL);

    /* Verify primary envelope exists in backend */
    char primary_key[128];
    snprintf(primary_key, sizeof(primary_key), "%s%s", KNISHIO_SECRET_STORAGE_PREFIX, bundle_hash);
    char *primary_raw = NULL;
    err = backend->get_item(backend, primary_key, &primary_raw);
    check(err == KNISHIO_SUCCESS && primary_raw != NULL, "primary envelope stored under knishio:secret:<bundleHash>", NULL);

    /* Verify recovery envelope exists in backend */
    char recovery_key[128];
    snprintf(recovery_key, sizeof(recovery_key), "%s%s", KNISHIO_RECOVERY_KEY_PREFIX, bundle_hash);
    char *recovery_raw = NULL;
    err = backend->get_item(backend, recovery_key, &recovery_raw);
    check(err == KNISHIO_SUCCESS && recovery_raw != NULL, "recovery envelope stored under knishio:recovery:<bundleHash>", NULL);

    /* Verify has_secret */
    bool exists = false;
    err = knishio_envelope_has_secret(backend, bundle_hash, &exists);
    check(err == KNISHIO_SUCCESS && exists, "has_secret returned true for stored bundle", NULL);

    /* Verify list_secrets excludes recovery keys */
    knishio_secret_metadata_t *meta_list = NULL;
    size_t count = 0;
    err = knishio_envelope_list_secrets(backend, &meta_list, &count);
    check(err == KNISHIO_SUCCESS && count == 1, "list_secrets returns exactly 1 secret", NULL);
    if (meta_list && count > 0) {
        check(meta_list[0].bundle_hash && strcmp(meta_list[0].bundle_hash, bundle_hash) == 0,
              "list_secrets metadata matches bundle_hash", NULL);
        check(meta_list[0].label && strcmp(meta_list[0].label, "recovery-test-label") == 0,
              "list_secrets metadata matches label", NULL);
        check(!meta_list[0].hardware_backed, "list_secrets hardwareBacked is false", NULL);
        check(meta_list[0].provider_type && strcmp(meta_list[0].provider_type, "aes-gcm") == 0,
              "list_secrets providerType is aes-gcm", NULL);
        knishio_secret_metadata_list_free(meta_list, count);
    }

    /* 2. Retrieve secret with primary passphrase */
    char *retrieved = NULL;
    size_t retrieved_len = 0;
    err = knishio_envelope_retrieve_secret(backend, bundle_hash, primary_pass, NULL, &retrieved, &retrieved_len);
    check(err == KNISHIO_SUCCESS && retrieved != NULL, "retrieve_secret with primary passphrase succeeded", NULL);
    if (retrieved) {
        check(strcmp(retrieved, secret) == 0, "retrieved secret matches original", NULL);
        knishio_secure_zero(retrieved, retrieved_len);
        free(retrieved);
    }

    /* 3. Simulate hardware KEK loss / corruption by destroying primary envelope */
    bool existed = false;
    err = backend->remove_item(backend, primary_key, &existed);
    check(err == KNISHIO_SUCCESS && existed, "destroyed primary envelope to simulate loss", NULL);

    /* Retrieve primary now fails / returns NULL */
    retrieved = NULL;
    retrieved_len = 0;
    err = knishio_envelope_retrieve_secret(backend, bundle_hash, primary_pass, NULL, &retrieved, &retrieved_len);
    check(err == KNISHIO_SUCCESS && retrieved == NULL, "retrieve after primary loss returns NULL", NULL);

    /* 4. Attempt recover with wrong passphrase -> must fail closed */
    err = knishio_envelope_recover_secret(backend, bundle_hash, "wrong-recovery-passphrase", new_pass, NULL);
    check(err != KNISHIO_SUCCESS, "recover_secret with wrong passphrase fails closed", NULL);

    /* Attempt recover on non-existent bundle -> must fail closed */
    err = knishio_envelope_recover_secret(backend, "nonexistent-bundle-hash-9999", recovery_pass, new_pass, NULL);
    check(err != KNISHIO_SUCCESS, "recover_secret on non-existent bundle fails closed", NULL);

    /* 5. Recover with correct recovery passphrase and re-enroll under new_pass */
    knishio_storage_options_t rec_opts;
    knishio_storage_options_init(&rec_opts);
    rec_opts.label = "re-enrolled-label";

    err = knishio_envelope_recover_secret(backend, bundle_hash, recovery_pass, new_pass, &rec_opts);
    check(err == KNISHIO_SUCCESS, "recover_secret with correct recovery passphrase succeeded", NULL);

    /* 6. Verify retrieved with new passphrase matches original secret */
    retrieved = NULL;
    retrieved_len = 0;
    err = knishio_envelope_retrieve_secret(backend, bundle_hash, new_pass, NULL, &retrieved, &retrieved_len);
    check(err == KNISHIO_SUCCESS && retrieved != NULL, "retrieve_secret with new passphrase succeeded", NULL);
    if (retrieved) {
        check(strcmp(retrieved, secret) == 0, "recovered secret matches original plaintext", NULL);
        knishio_secure_zero(retrieved, retrieved_len);
        free(retrieved);
    }

    /* Old passphrase now rejected */
    retrieved = NULL;
    retrieved_len = 0;
    err = knishio_envelope_retrieve_secret(backend, bundle_hash, primary_pass, NULL, &retrieved, &retrieved_len);
    check(err != KNISHIO_SUCCESS, "retrieve_secret with old passphrase fails closed", NULL);

    /* Verify new envelope ciphertext is different from original */
    char *new_primary_raw = NULL;
    err = backend->get_item(backend, primary_key, &new_primary_raw);
    check(err == KNISHIO_SUCCESS && new_primary_raw != NULL, "re-enrolled envelope exists", NULL);
    if (primary_raw && new_primary_raw) {
        check(strcmp(primary_raw, new_primary_raw) != 0, "re-enrolled envelope is fresh ciphertext", NULL);
    }
    if (primary_raw) free(primary_raw);
    if (new_primary_raw) free(new_primary_raw);
    if (recovery_raw) free(recovery_raw);

    /* 7. Delete secret removes BOTH primary and recovery envelopes */
    existed = false;
    err = knishio_envelope_delete_secret(backend, bundle_hash, &existed);
    check(err == KNISHIO_SUCCESS && existed, "delete_secret returned existed=true", NULL);

    /* Verify primary is deleted */
    char *val = NULL;
    err = backend->get_item(backend, primary_key, &val);
    check(err == KNISHIO_SUCCESS && val == NULL, "primary envelope deleted by delete_secret", NULL);

    /* Verify recovery is deleted */
    err = backend->get_item(backend, recovery_key, &val);
    check(err == KNISHIO_SUCCESS && val == NULL, "recovery envelope deleted by delete_secret", NULL);

    knishio_storage_backend_free(backend);
}

int main(void) {
    const char *env_path = getenv("KNISHIO_CROSS_PLATFORM_VECTORS");
    const char *path = env_path ? env_path : KNISHIO_TEST_VECTORS_PATH;
    char *text = slurp(path);
    if (!text) {
        fprintf(stderr, "test_secret_storage: cannot read cross-SDK vectors at %s\n", path);
        return 2;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    cJSON *vectors = root ? cJSON_GetObjectItem(root, "vectors") : NULL;
    if (!vectors) {
        fprintf(stderr, "test_secret_storage: %s has no `vectors` object\n", path);
        if (root) cJSON_Delete(root);
        return 2;
    }

    printf("=== KnishIO C Secret Storage Test Suite ===\n");

    test_cross_sdk_vector(vectors);
    test_emitted_metadata_contract();
    test_round_trip_and_wrong_passphrase();
    test_storage_backends();
    test_recovery_envelope_and_reenrollment();

    cJSON_Delete(root);

    printf("\nResult: %s (%d failure%s)\n",
           g_failures ? "FAILED" : "ALL TESTS PASSED",
           g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
