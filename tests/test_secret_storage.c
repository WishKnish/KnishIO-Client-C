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

#include "knishio/knishio.h"
#include "knishio/storage/types.h"
#include "knishio/storage/envelope.h"
#include "knishio/storage/backend.h"
#include "knishio/storage/provider.h"
#include "knishio/client.h"
#include "knishio/client_ops.h"
#include "knishio/wallet.h"
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

static void test_cross_sdk_recovery_vector(const cJSON *vectors) {
    printf("\nTest 1b: Cross-SDK recovery vector re-enrollment\n");

    const cJSON *env_obj = cJSON_GetObjectItem(vectors, "secret_storage_envelope");
    check(env_obj != NULL, "fixture contains secret_storage_envelope", NULL);
    if (!env_obj) return;

    const cJSON *tests_arr = cJSON_GetObjectItem(env_obj, "tests");
    check(tests_arr && cJSON_GetArraySize(tests_arr) > 1, "fixture contains at least 2 tests", NULL);
    if (!tests_arr || cJSON_GetArraySize(tests_arr) < 2) return;

    const cJSON *test1 = cJSON_GetArrayItem(tests_arr, 1);
    const char *storage_key = cJSON_GetStringValue(cJSON_GetObjectItem(test1, "storageKey"));
    const char *bundle_hash = cJSON_GetStringValue(cJSON_GetObjectItem(test1, "bundleHash"));
    const char *recovery_passphrase = cJSON_GetStringValue(cJSON_GetObjectItem(test1, "recoveryPassphrase"));
    const char *expected_plaintext = cJSON_GetStringValue(cJSON_GetObjectItem(test1, "expectedPlaintext"));
    const cJSON *payload_json = cJSON_GetObjectItem(test1, "payload");

    check(storage_key != NULL, "test1 has storageKey", storage_key);
    check(bundle_hash != NULL, "test1 has bundleHash", bundle_hash);
    check(recovery_passphrase != NULL, "test1 has recoveryPassphrase", recovery_passphrase);
    check(expected_plaintext != NULL, "test1 has expectedPlaintext", expected_plaintext);
    check(payload_json != NULL, "test1 has payload object", NULL);
    if (!storage_key || !bundle_hash || !recovery_passphrase || !expected_plaintext || !payload_json) return;

    char *payload_str = cJSON_PrintUnformatted(payload_json);
    check(payload_str != NULL, "serialized payload JSON", NULL);
    if (!payload_str) return;

    knishio_storage_backend_t *backend = NULL;
    knishio_error_t err = knishio_memory_storage_backend_create(&backend);
    check(err == KNISHIO_SUCCESS && backend != NULL, "created memory backend", NULL);
    if (!backend) {
        free(payload_str);
        return;
    }

    /* Seed only the recovery record */
    err = backend->set_item(backend, storage_key, payload_str);
    free(payload_str);
    check(err == KNISHIO_SUCCESS, "seeded recovery record", NULL);

    /* Assert knishio:secret:<bundleHash> is absent */
    char secret_key[128];
    snprintf(secret_key, sizeof(secret_key), "knishio:secret:%s", bundle_hash);
    char *initial_secret = NULL;
    err = backend->get_item(backend, secret_key, &initial_secret);
    check(err != KNISHIO_SUCCESS || initial_secret == NULL, "primary secret initially absent", NULL);
    if (initial_secret) free(initial_secret);

    /* Create provider with NULL default passphrase */
    knishio_secret_storage_provider_t *provider = NULL;
    err = knishio_aes_gcm_secret_storage_provider_create(backend, NULL, &provider);
    check(err == KNISHIO_SUCCESS && provider != NULL, "created aes-gcm provider", NULL);
    if (!provider) {
        knishio_storage_backend_free(backend);
        return;
    }

    const char *primary_pass = "xsdk-reenrolled-primary-pass";
    knishio_storage_options_t opts;
    knishio_storage_options_init(&opts);
    opts.passphrase = primary_pass;

    /* Recover secret */
    err = provider->recover_secret(provider, bundle_hash, recovery_passphrase, &opts);
    check(err == KNISHIO_SUCCESS, "provider recover_secret succeeded", NULL);

    /* Retrieve secret under re-enrolled primary passphrase */
    char *retrieved = NULL;
    size_t retrieved_len = 0;
    err = provider->retrieve_secret(provider, bundle_hash, &opts, &retrieved, &retrieved_len);
    check(err == KNISHIO_SUCCESS && retrieved != NULL, "provider retrieve_secret succeeded", NULL);
    if (retrieved) {
        check(strcmp(retrieved, expected_plaintext) == 0,
              "retrieved plaintext matches expectedPlaintext", retrieved);
        knishio_secure_free(retrieved, retrieved_len);
    }

    /* Assert both knishio:secret:<bundleHash> and knishio:recovery:<bundleHash> exist */
    char *secret_raw = NULL;
    err = backend->get_item(backend, secret_key, &secret_raw);
    check(err == KNISHIO_SUCCESS && secret_raw != NULL, "primary secret exists after recovery", NULL);

    char *rec_raw = NULL;
    err = backend->get_item(backend, storage_key, &rec_raw);
    check(err == KNISHIO_SUCCESS && rec_raw != NULL, "recovery record exists after recovery", NULL);

    /* Parse stored primary metadata and check contract */
    if (secret_raw) {
        cJSON *stored_json = cJSON_Parse(secret_raw);
        check(stored_json != NULL, "parsed stored secret JSON", NULL);
        if (stored_json) {
            cJSON *metadata = cJSON_GetObjectItem(stored_json, "metadata");
            check(metadata != NULL, "stored secret has metadata", NULL);
            if (metadata) {
                cJSON *hw = cJSON_GetObjectItem(metadata, "hardwareBacked");
                check(hw != NULL && cJSON_IsFalse(hw), "metadata hardwareBacked is false", NULL);

                /* Check camelCase keys exist */
                const char *required_keys[] = {"bundleHash", "createdAt", "hardwareBacked", "providerType"};
                for (size_t i = 0; i < 4; i++) {
                    check(cJSON_GetObjectItem(metadata, required_keys[i]) != NULL,
                          "required metadata key present", required_keys[i]);
                }

                /* Check snake_case keys are absent */
                const char *forbidden_keys[] = {"bundle_hash", "created_at", "hardware_backed", "provider_type"};
                for (size_t i = 0; i < 4; i++) {
                    check(cJSON_GetObjectItem(metadata, forbidden_keys[i]) == NULL,
                          "forbidden snake_case key absent", forbidden_keys[i]);
                }
            }
            cJSON_Delete(stored_json);
        }
        free(secret_raw);
    }
    if (rec_raw) free(rec_raw);

    knishio_secret_storage_provider_free(provider);
    knishio_storage_backend_free(backend);
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

/* ------------------------------------------------------------------ */
/* 6. AES-GCM secret storage provider contract                        */
/* ------------------------------------------------------------------ */
static void test_aes_gcm_provider_contract(void) {
    printf("\n--- 6. AES-GCM Secret Storage Provider Contract ---\n");

    knishio_storage_backend_t *backend = NULL;
    knishio_error_t err = knishio_memory_storage_backend_create(&backend);
    check(err == KNISHIO_SUCCESS && backend != NULL, "create memory backend for provider", NULL);

    knishio_secret_storage_provider_t *p = NULL;
    err = knishio_aes_gcm_secret_storage_provider_create(backend, "default-pass", &p);
    check(err == KNISHIO_SUCCESS && p != NULL, "create aes-gcm provider with default passphrase", NULL);

    check(strcmp(p->provider_type(p), "aes-gcm") == 0, "provider_type is aes-gcm", p->provider_type(p));
    check(!p->is_hardware_backed(p), "is_hardware_backed is false", NULL);

    const char *bundle = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    const char *probe_secret = "MASTER-SECRET-PROVIDER-PROBE";
    knishio_storage_options_t store_opts;
    knishio_storage_options_init(&store_opts);
    store_opts.label = "probe";
    store_opts.recovery_passphrase = "rec-pass";

    err = p->store_secret(p, bundle, probe_secret, strlen(probe_secret), &store_opts);
    check(err == KNISHIO_SUCCESS, "provider store_secret succeeded", NULL);

    bool exists = false;
    err = p->has_secret(p, bundle, &exists);
    check(err == KNISHIO_SUCCESS && exists, "provider has_secret returns true", NULL);

    /* Retrieve using default passphrase (NULL options) */
    char *plaintext = NULL;
    size_t plaintext_len = 0;
    err = p->retrieve_secret(p, bundle, NULL, &plaintext, &plaintext_len);
    check(err == KNISHIO_SUCCESS && plaintext != NULL, "provider retrieve_secret with default pass succeeded", NULL);
    if (plaintext) {
        check(strcmp(plaintext, probe_secret) == 0, "retrieved secret matches probe_secret", NULL);
        knishio_secure_free(plaintext, plaintext_len);
        plaintext = NULL;
    }

    /* Retrieve with wrong passphrase must fail */
    knishio_storage_options_t wrong_opts;
    knishio_storage_options_init(&wrong_opts);
    wrong_opts.passphrase = "wrong-pass";
    err = p->retrieve_secret(p, bundle, &wrong_opts, &plaintext, &plaintext_len);
    check(err != KNISHIO_SUCCESS, "retrieve with wrong passphrase fails closed", NULL);
    check(plaintext == NULL, "no plaintext returned on wrong passphrase", NULL);

    /* List secrets */
    knishio_secret_metadata_t *meta_list = NULL;
    size_t count = 0;
    err = p->list_secrets(p, &meta_list, &count);
    check(err == KNISHIO_SUCCESS && count == 1, "list_secrets returns 1 entry", NULL);
    if (meta_list && count > 0) {
        check(strcmp(meta_list[0].bundle_hash, bundle) == 0, "listed bundle_hash matches", meta_list[0].bundle_hash);
        check(strcmp(meta_list[0].provider_type, "aes-gcm") == 0, "listed provider_type is aes-gcm", meta_list[0].provider_type);
        check(!meta_list[0].hardware_backed, "listed hardware_backed is false", NULL);
        check(meta_list[0].label && strcmp(meta_list[0].label, "probe") == 0, "listed label matches probe", meta_list[0].label);
        knishio_secret_metadata_list_free(meta_list, count);
    }

    /* Verify recovery envelope was sealed in backend */
    char rec_key[128];
    snprintf(rec_key, sizeof(rec_key), "%s%s", KNISHIO_RECOVERY_KEY_PREFIX, bundle);
    char *rec_val = NULL;
    err = backend->get_item(backend, rec_key, &rec_val);
    check(err == KNISHIO_SUCCESS && rec_val != NULL, "recovery envelope exists in backend", NULL);
    if (rec_val) free(rec_val);

    /* Recover with recovery passphrase */
    knishio_storage_options_t new_opts;
    knishio_storage_options_init(&new_opts);
    new_opts.passphrase = "new-pass";
    err = p->recover_secret(p, bundle, "rec-pass", &new_opts);
    check(err == KNISHIO_SUCCESS, "recover_secret with recovery passphrase succeeded", NULL);

    /* Retrieve with new passphrase */
    err = p->retrieve_secret(p, bundle, &new_opts, &plaintext, &plaintext_len);
    check(err == KNISHIO_SUCCESS && plaintext != NULL, "retrieve_secret with new-pass succeeded", NULL);
    if (plaintext) {
        check(strcmp(plaintext, probe_secret) == 0, "retrieved secret after recovery matches probe_secret", NULL);
        knishio_secure_free(plaintext, plaintext_len);
        plaintext = NULL;
    }

    /* Second provider without default passphrase requires options->passphrase */
    knishio_secret_storage_provider_t *p_no_def = NULL;
    err = knishio_aes_gcm_secret_storage_provider_create(backend, NULL, &p_no_def);
    check(err == KNISHIO_SUCCESS && p_no_def != NULL, "create provider without default passphrase", NULL);
    err = p_no_def->retrieve_secret(p_no_def, bundle, NULL, &plaintext, &plaintext_len);
    check(err == KNISHIO_ERROR_INVALID_ARGS, "retrieve without passphrase returns INVALID_ARGS", NULL);
    knishio_secret_storage_provider_free(p_no_def);

    /* Delete secret */
    bool existed = false;
    err = p->delete_secret(p, bundle, &existed);
    check(err == KNISHIO_SUCCESS && existed, "delete_secret returns existed=true", NULL);
    exists = true;
    err = p->has_secret(p, bundle, &exists);
    check(err == KNISHIO_SUCCESS && !exists, "has_secret after delete returns false", NULL);

    knishio_secret_storage_provider_free(p);
    knishio_storage_backend_free(backend);
}

/* ------------------------------------------------------------------ */
/* 7. Client secret storage just-in-time unwrapping                   */
/* ------------------------------------------------------------------ */
static void test_client_secret_storage_jit_unwrap(void) {
    printf("\n--- 7. Client Secret Storage JIT Unwrapping ---\n");

    knishio_storage_backend_t *backend = NULL;
    knishio_error_t err = knishio_memory_storage_backend_create(&backend);
    check(err == KNISHIO_SUCCESS && backend != NULL, "create memory backend for client tests", NULL);

    knishio_secret_storage_provider_t *provider = NULL;
    err = knishio_aes_gcm_secret_storage_provider_create(backend, "default-pass", &provider);
    check(err == KNISHIO_SUCCESS && provider != NULL, "create provider for client tests", NULL);

    char *secret = NULL;
    bool gen_ok = knishio_generate_secret("provider-test-seed-2026", 2048, &secret);
    check(gen_ok && secret != NULL, "generate 2048-hex secret", NULL);

    knishio_client_config_t config = {
        .uri = "https://localhost:8080/graphql",
        .cell_slug = "TEST",
        .client = NULL,
        .socket = NULL,
        .server_sdk_version = 3,
        .logging = false
    };

    /* Client A: attaches provider, sets secret (which auto-stores to provider and drops cleartext) */
    knishio_client_t *clientA = NULL;
    err = knishio_client_create(&clientA, &config);
    check(err == KNISHIO_SUCCESS && clientA != NULL, "create client A", NULL);

    knishio_storage_options_t store_opts;
    knishio_storage_options_init(&store_opts);
    store_opts.passphrase = "default-pass";
    store_opts.allow_unrecoverable = true;

    err = knishio_client_set_secret_storage(clientA, provider, NULL, &store_opts);
    check(err == KNISHIO_SUCCESS, "client A set_secret_storage succeeded", NULL);
    check(knishio_client_get_secret_storage(clientA) == provider, "get_secret_storage returns provider", NULL);

    err = knishio_client_set_secret(clientA, secret);
    check(err == KNISHIO_SUCCESS, "client A set_secret succeeded", NULL);

    const char *bundleA = knishio_client_get_bundle(clientA);
    check(bundleA != NULL && strlen(bundleA) == 64, "client A get_bundle returns 64-char hash", bundleA);

    /* Verify on-disk/in-memory envelope was stored under bundleA */
    bool exists_in_provider = false;
    err = provider->has_secret(provider, bundleA, &exists_in_provider);
    check(err == KNISHIO_SUCCESS && exists_in_provider, "secret is stored in provider under bundleA", NULL);

    knishio_wallet_t *walletA = NULL;
    err = knishio_client_get_source_wallet(clientA, "USER", &walletA);
    check(err == KNISHIO_SUCCESS && walletA != NULL, "client A get_source_wallet succeeded", NULL);
    if (walletA) {
        check(walletA->secret != NULL && strcmp(walletA->secret, secret) == 0, "wallet A secret matches original", NULL);
        check(walletA->bundle_hash != NULL && strcmp(walletA->bundle_hash, bundleA) == 0, "wallet A bundle matches bundleA", NULL);
    }

    /* Client B: fresh client instance, NEVER receives the cleartext secret directly.
     * Only receives provider + bundleA. Must resolve secret just-in-time and derive matching wallet. */
    knishio_client_t *clientB = NULL;
    err = knishio_client_create(&clientB, &config);
    check(err == KNISHIO_SUCCESS && clientB != NULL, "create client B", NULL);

    err = knishio_client_set_secret_storage(clientB, provider, bundleA, &store_opts);
    check(err == KNISHIO_SUCCESS, "client B set_secret_storage with bundleA succeeded", NULL);

    knishio_wallet_t *walletB = NULL;
    err = knishio_client_get_source_wallet(clientB, "USER", &walletB);
    check(err == KNISHIO_SUCCESS && walletB != NULL, "client B get_source_wallet succeeded (JIT unwrap)", NULL);
    if (walletA && walletB) {
        check(strcmp(walletB->address, walletA->address) == 0, "wallet B address matches wallet A", walletB->address);
        check(strcmp(walletB->bundle_hash, walletA->bundle_hash) == 0, "wallet B bundle matches wallet A", walletB->bundle_hash);
        check(strcmp(walletB->secret, secret) == 0, "wallet B derived secret matches original", NULL);
    }

    /* Retrieve secret via client B */
    char *retrieved_sec = NULL;
    size_t retrieved_len = 0;
    err = knishio_client_retrieve_secret(clientB, &retrieved_sec, &retrieved_len);
    check(err == KNISHIO_SUCCESS && retrieved_sec != NULL, "client B retrieve_secret succeeded", NULL);
    if (retrieved_sec) {
        check(strcmp(retrieved_sec, secret) == 0, "client B retrieved secret matches original", NULL);
        knishio_secure_free(retrieved_sec, retrieved_len);
    }

    /* Client C: unconfigured client, has no secret and no provider → must return INVALID_STATE */
    knishio_client_t *clientC = NULL;
    err = knishio_client_create(&clientC, &config);
    check(err == KNISHIO_SUCCESS && clientC != NULL, "create client C", NULL);

    knishio_wallet_t *walletC = NULL;
    err = knishio_client_get_source_wallet(clientC, "USER", &walletC);
    check(err == KNISHIO_ERROR_INVALID_STATE, "client C get_source_wallet fails with INVALID_STATE", NULL);
    check(walletC == NULL, "wallet C remains NULL", NULL);

    /* Cleanup */
    if (walletA) knishio_wallet_free(walletA);
    if (walletB) knishio_wallet_free(walletB);
    if (clientA) knishio_client_destroy(clientA);
    if (clientB) knishio_client_destroy(clientB);
    if (clientC) knishio_client_destroy(clientC);
    if (secret) knishio_free(secret);
    knishio_secret_storage_provider_free(provider);
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
    test_cross_sdk_recovery_vector(vectors);
    test_emitted_metadata_contract();
    test_round_trip_and_wrong_passphrase();
    test_storage_backends();
    test_recovery_envelope_and_reenrollment();
    test_aes_gcm_provider_contract();
    test_client_secret_storage_jit_unwrap();
    cJSON_Delete(root);

    printf("\nResult: %s (%d failure%s)\n",
           g_failures ? "FAILED" : "ALL TESTS PASSED",
           g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
