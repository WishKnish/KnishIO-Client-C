#include "knishio/storage/envelope.h"
#include "knishio/storage/backend.h"
#include "knishio/crypto/aes_gcm.h"
#include "knishio/utils/encoding.h"
#include "knishio/utils/memory.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

knishio_error_t knishio_envelope_seal(
    const char *secret,
    size_t secret_len,
    const char *passphrase,
    const knishio_secret_metadata_t *metadata,
    knishio_encrypted_payload_t *payload_out
) {
    if (!secret || !passphrase || !metadata || !payload_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    uint8_t salt[KNISHIO_STORAGE_SALT_LENGTH];
    uint8_t iv[KNISHIO_STORAGE_IV_LENGTH];
    uint8_t key[AES_256_KEY_SIZE];
    uint8_t *ciphertext_raw = NULL;
    size_t ciphertext_raw_len = 0;
    knishio_error_t result = KNISHIO_ERROR_CRYPTO;

    knishio_encrypted_payload_init(payload_out);

    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        goto cleanup;
    }

    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        goto cleanup;
    }

    if (PKCS5_PBKDF2_HMAC(
            passphrase,
            (int)strlen(passphrase),
            salt,
            sizeof(salt),
            KNISHIO_STORAGE_DEFAULT_ITERATIONS,
            EVP_sha256(),
            (int)sizeof(key),
            key
        ) != 1) {
        goto cleanup;
    }

    result = knishio_aes_gcm_encrypt_iv(
        (const uint8_t *)secret,
        secret_len,
        key,
        iv,
        sizeof(iv),
        &ciphertext_raw,
        &ciphertext_raw_len
    );
    if (result != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    if (!knishio_base64_encode(ciphertext_raw, ciphertext_raw_len, &payload_out->ciphertext) ||
        !knishio_base64_encode(iv, sizeof(iv), &payload_out->iv) ||
        !knishio_base64_encode(salt, sizeof(salt), &payload_out->salt)) {
        result = KNISHIO_ERROR_MEMORY;
        goto cleanup;
    }

    payload_out->version = 1;
    payload_out->algorithm = strdup("AES-GCM");
    payload_out->iterations = KNISHIO_STORAGE_DEFAULT_ITERATIONS;

    payload_out->metadata.bundle_hash = metadata->bundle_hash ? strdup(metadata->bundle_hash) : NULL;
    payload_out->metadata.label = metadata->label ? strdup(metadata->label) : NULL;
    payload_out->metadata.created_at = metadata->created_at;
    payload_out->metadata.hardware_backed = metadata->hardware_backed;
    payload_out->metadata.provider_type = metadata->provider_type ? strdup(metadata->provider_type) : strdup("aes-gcm");

    result = KNISHIO_SUCCESS;

cleanup:
    knishio_secure_zero(key, sizeof(key));
    if (ciphertext_raw) {
        free(ciphertext_raw);
    }
    if (result != KNISHIO_SUCCESS) {
        knishio_encrypted_payload_cleanup(payload_out);
    }
    return result;
}

knishio_error_t knishio_envelope_open(
    const knishio_encrypted_payload_t *payload,
    const char *passphrase,
    char **plaintext_out,
    size_t *plaintext_len_out
) {
    if (!payload || !passphrase || !plaintext_out || !plaintext_len_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    unsigned char *salt_raw = NULL;
    size_t salt_raw_len = 0;
    unsigned char *iv_raw = NULL;
    size_t iv_raw_len = 0;
    unsigned char *ct_raw = NULL;
    size_t ct_raw_len = 0;
    uint8_t key[AES_256_KEY_SIZE];
    uint8_t *plaintext_raw = NULL;
    size_t plaintext_raw_len = 0;
    knishio_error_t result = KNISHIO_ERROR_CRYPTO;

    if (!knishio_base64_decode(payload->salt, &salt_raw, &salt_raw_len) || salt_raw_len == 0) {
        result = KNISHIO_ERROR_INVALID_ARGS;
        goto cleanup;
    }

    if (!knishio_base64_decode(payload->iv, &iv_raw, &iv_raw_len) || iv_raw_len != KNISHIO_STORAGE_IV_LENGTH) {
        result = KNISHIO_ERROR_INVALID_ARGS;
        goto cleanup;
    }

    if (!knishio_base64_decode(payload->ciphertext, &ct_raw, &ct_raw_len) || ct_raw_len < AES_GCM_TAG_SIZE) {
        result = KNISHIO_ERROR_INVALID_ARGS;
        goto cleanup;
    }

    uint32_t iterations = payload->iterations ? payload->iterations : KNISHIO_STORAGE_DEFAULT_ITERATIONS;

    if (PKCS5_PBKDF2_HMAC(
            passphrase,
            (int)strlen(passphrase),
            salt_raw,
            (int)salt_raw_len,
            (int)iterations,
            EVP_sha256(),
            (int)sizeof(key),
            key
        ) != 1) {
        result = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }

    result = knishio_aes_gcm_decrypt_iv(
        ct_raw,
        ct_raw_len,
        key,
        iv_raw,
        iv_raw_len,
        &plaintext_raw,
        &plaintext_raw_len
    );
    if (result != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    *plaintext_out = (char *)plaintext_raw;
    *plaintext_len_out = plaintext_raw_len;
    plaintext_raw = NULL;
    result = KNISHIO_SUCCESS;

cleanup:
    knishio_secure_zero(key, sizeof(key));
    if (salt_raw) free(salt_raw);
    if (iv_raw) free(iv_raw);
    if (ct_raw) free(ct_raw);
    if (plaintext_raw) {
        knishio_secure_zero(plaintext_raw, plaintext_raw_len);
        free(plaintext_raw);
    }
    return result;
}

knishio_error_t knishio_envelope_seal_json(
    const char *secret,
    size_t secret_len,
    const char *passphrase,
    const knishio_secret_metadata_t *metadata,
    char **json_out
) {
    if (!json_out) return KNISHIO_ERROR_NULL_POINTER;
    *json_out = NULL;

    knishio_encrypted_payload_t payload;
    knishio_error_t err = knishio_envelope_seal(secret, secret_len, passphrase, metadata, &payload);
    if (err != KNISHIO_SUCCESS) {
        return err;
    }

    char *json = knishio_encrypted_payload_to_json(&payload);
    knishio_encrypted_payload_cleanup(&payload);
    if (!json) {
        return KNISHIO_ERROR_MEMORY;
    }

    *json_out = json;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_envelope_open_json(
    const char *json_str,
    const char *passphrase,
    char **plaintext_out,
    size_t *plaintext_len_out
) {
    if (!json_str || !passphrase || !plaintext_out || !plaintext_len_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_encrypted_payload_t payload;
    knishio_error_t err = knishio_encrypted_payload_from_json(json_str, &payload);
    if (err != KNISHIO_SUCCESS) {
        return err;
    }

    err = knishio_envelope_open(&payload, passphrase, plaintext_out, plaintext_len_out);
    knishio_encrypted_payload_cleanup(&payload);
    return err;
}

static int64_t get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

knishio_error_t knishio_envelope_store_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    const char *secret,
    size_t secret_len,
    const char *passphrase,
    const knishio_storage_options_t *options
) {
    if (!backend || !bundle_hash || !secret) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    if (bundle_hash[0] == '\0' || secret_len == 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!backend->set_item) {
        return KNISHIO_ERROR_NOT_IMPLEMENTED;
    }

    const char *primary_pass = passphrase;
    if ((!primary_pass || primary_pass[0] == '\0') && options && options->passphrase) {
        primary_pass = options->passphrase;
    }
    if (!primary_pass || primary_pass[0] == '\0') {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    int64_t now_ms = get_current_time_ms();

    /* Primary envelope metadata */
    knishio_secret_metadata_t meta;
    knishio_secret_metadata_init(&meta);
    meta.bundle_hash = strdup(bundle_hash);
    meta.label = (options && options->label) ? strdup(options->label) : NULL;
    meta.created_at = now_ms;
    meta.hardware_backed = false;
    meta.provider_type = strdup("aes-gcm");

    char *primary_json = NULL;
    knishio_error_t err = knishio_envelope_seal_json(secret, secret_len, primary_pass, &meta, &primary_json);
    knishio_secret_metadata_cleanup(&meta);
    if (err != KNISHIO_SUCCESS) {
        return err;
    }

    size_t hash_len = strlen(bundle_hash);
    size_t sec_prefix_len = strlen(KNISHIO_SECRET_STORAGE_PREFIX);
    char *primary_key = malloc(sec_prefix_len + hash_len + 1);
    if (!primary_key) {
        free(primary_json);
        return KNISHIO_ERROR_MEMORY;
    }
    memcpy(primary_key, KNISHIO_SECRET_STORAGE_PREFIX, sec_prefix_len);
    memcpy(primary_key + sec_prefix_len, bundle_hash, hash_len + 1);

    err = backend->set_item(backend, primary_key, primary_json);
    free(primary_json);
    free(primary_key);
    if (err != KNISHIO_SUCCESS) {
        return err;
    }

    /* Recovery envelope if recovery_passphrase provided */
    if (options && options->recovery_passphrase && options->recovery_passphrase[0] != '\0') {
        knishio_secret_metadata_t rec_meta;
        knishio_secret_metadata_init(&rec_meta);
        rec_meta.bundle_hash = strdup(bundle_hash);
        rec_meta.label = options->label ? strdup(options->label) : NULL;
        rec_meta.created_at = now_ms;
        rec_meta.hardware_backed = false;
        rec_meta.provider_type = strdup("aes-gcm");

        char *rec_json = NULL;
        err = knishio_envelope_seal_json(secret, secret_len, options->recovery_passphrase, &rec_meta, &rec_json);
        knishio_secret_metadata_cleanup(&rec_meta);
        if (err != KNISHIO_SUCCESS) {
            return err;
        }

        size_t rec_prefix_len = strlen(KNISHIO_RECOVERY_KEY_PREFIX);
        char *rec_key = malloc(rec_prefix_len + hash_len + 1);
        if (!rec_key) {
            free(rec_json);
            return KNISHIO_ERROR_MEMORY;
        }
        memcpy(rec_key, KNISHIO_RECOVERY_KEY_PREFIX, rec_prefix_len);
        memcpy(rec_key + rec_prefix_len, bundle_hash, hash_len + 1);

        err = backend->set_item(backend, rec_key, rec_json);
        free(rec_json);
        free(rec_key);
        if (err != KNISHIO_SUCCESS) {
            return err;
        }
    }

    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_envelope_retrieve_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    const char *passphrase,
    const knishio_storage_options_t *options,
    char **plaintext_out,
    size_t *plaintext_len_out
) {
    if (!backend || !bundle_hash || !plaintext_out || !plaintext_len_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *plaintext_out = NULL;
    *plaintext_len_out = 0;

    if (!backend->get_item) {
        return KNISHIO_ERROR_NOT_IMPLEMENTED;
    }

    const char *pass = passphrase;
    if ((!pass || pass[0] == '\0') && options && options->passphrase) {
        pass = options->passphrase;
    }
    if (!pass || pass[0] == '\0') {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    size_t hash_len = strlen(bundle_hash);
    size_t sec_prefix_len = strlen(KNISHIO_SECRET_STORAGE_PREFIX);
    char *key = malloc(sec_prefix_len + hash_len + 1);
    if (!key) {
        return KNISHIO_ERROR_MEMORY;
    }
    memcpy(key, KNISHIO_SECRET_STORAGE_PREFIX, sec_prefix_len);
    memcpy(key + sec_prefix_len, bundle_hash, hash_len + 1);

    char *json_str = NULL;
    knishio_error_t err = backend->get_item(backend, key, &json_str);
    free(key);
    if (err != KNISHIO_SUCCESS) {
        return err;
    }

    if (!json_str) {
        /* Secret not found: *plaintext_out remains NULL */
        return KNISHIO_SUCCESS;
    }

    err = knishio_envelope_open_json(json_str, pass, plaintext_out, plaintext_len_out);
    free(json_str);
    return err;
}

knishio_error_t knishio_envelope_delete_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    bool *existed_out
) {
    return knishio_storage_backend_delete_secret(backend, bundle_hash, existed_out);
}

knishio_error_t knishio_envelope_has_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    bool *exists_out
) {
    if (!backend || !bundle_hash || !exists_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *exists_out = false;

    if (!backend->get_item) {
        return KNISHIO_ERROR_NOT_IMPLEMENTED;
    }

    size_t hash_len = strlen(bundle_hash);
    size_t sec_prefix_len = strlen(KNISHIO_SECRET_STORAGE_PREFIX);
    char *key = malloc(sec_prefix_len + hash_len + 1);
    if (!key) return KNISHIO_ERROR_MEMORY;
    memcpy(key, KNISHIO_SECRET_STORAGE_PREFIX, sec_prefix_len);
    memcpy(key + sec_prefix_len, bundle_hash, hash_len + 1);

    char *val = NULL;
    knishio_error_t err = backend->get_item(backend, key, &val);
    free(key);
    if (err != KNISHIO_SUCCESS) return err;

    if (val) {
        *exists_out = true;
        free(val);
    }
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_envelope_list_secrets(
    knishio_storage_backend_t *backend,
    knishio_secret_metadata_t **metadata_list_out,
    size_t *count_out
) {
    if (!backend || !metadata_list_out || !count_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *metadata_list_out = NULL;
    *count_out = 0;

    if (!backend->keys || !backend->get_item) {
        return KNISHIO_ERROR_NOT_IMPLEMENTED;
    }

    char **keys = NULL;
    size_t total_keys = 0;
    knishio_error_t err = backend->keys(backend, &keys, &total_keys);
    if (err != KNISHIO_SUCCESS) return err;

    size_t sec_prefix_len = strlen(KNISHIO_SECRET_STORAGE_PREFIX);
    size_t rec_prefix_len = strlen(KNISHIO_RECOVERY_KEY_PREFIX);

    size_t capacity = 16;
    knishio_secret_metadata_t *list = malloc(capacity * sizeof(knishio_secret_metadata_t));
    if (!list && capacity > 0) {
        for (size_t i = 0; i < total_keys; i++) free(keys[i]);
        free(keys);
        return KNISHIO_ERROR_MEMORY;
    }
    size_t matched_count = 0;

    for (size_t i = 0; i < total_keys; i++) {
        const char *k = keys[i];
        if (strncmp(k, KNISHIO_SECRET_STORAGE_PREFIX, sec_prefix_len) == 0 &&
            strncmp(k, KNISHIO_RECOVERY_KEY_PREFIX, rec_prefix_len) != 0) {
            char *val = NULL;
            if (backend->get_item(backend, k, &val) == KNISHIO_SUCCESS && val) {
                knishio_encrypted_payload_t payload;
                if (knishio_encrypted_payload_from_json(val, &payload) == KNISHIO_SUCCESS) {
                    if (matched_count >= capacity) {
                        capacity *= 2;
                        knishio_secret_metadata_t *new_list = realloc(list, capacity * sizeof(knishio_secret_metadata_t));
                        if (!new_list) {
                            knishio_encrypted_payload_cleanup(&payload);
                            free(val);
                            continue;
                        }
                        list = new_list;
                    }
                    knishio_secret_metadata_t *dest = &list[matched_count];
                    knishio_secret_metadata_init(dest);
                    dest->bundle_hash = payload.metadata.bundle_hash ? strdup(payload.metadata.bundle_hash) : NULL;
                    dest->label = payload.metadata.label ? strdup(payload.metadata.label) : NULL;
                    dest->created_at = payload.metadata.created_at;
                    dest->hardware_backed = payload.metadata.hardware_backed;
                    dest->provider_type = payload.metadata.provider_type ? strdup(payload.metadata.provider_type) : NULL;
                    matched_count++;
                    knishio_encrypted_payload_cleanup(&payload);
                }
                free(val);
            }
        }
        free(keys[i]);
    }
    free(keys);

    *metadata_list_out = list;
    *count_out = matched_count;
    return KNISHIO_SUCCESS;
}

void knishio_secret_metadata_list_free(
    knishio_secret_metadata_t *metadata_list,
    size_t count
) {
    if (!metadata_list) return;
    for (size_t i = 0; i < count; i++) {
        knishio_secret_metadata_cleanup(&metadata_list[i]);
    }
    free(metadata_list);
}

knishio_error_t knishio_envelope_recover_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    const char *recovery_passphrase,
    const char *new_passphrase,
    const knishio_storage_options_t *options
) {
    if (!backend || !bundle_hash || !recovery_passphrase || !new_passphrase) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    if (bundle_hash[0] == '\0' || recovery_passphrase[0] == '\0' || new_passphrase[0] == '\0') {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!backend->get_item || !backend->set_item) {
        return KNISHIO_ERROR_NOT_IMPLEMENTED;
    }

    size_t hash_len = strlen(bundle_hash);
    size_t rec_prefix_len = strlen(KNISHIO_RECOVERY_KEY_PREFIX);
    char *rec_key = malloc(rec_prefix_len + hash_len + 1);
    if (!rec_key) return KNISHIO_ERROR_MEMORY;
    memcpy(rec_key, KNISHIO_RECOVERY_KEY_PREFIX, rec_prefix_len);
    memcpy(rec_key + rec_prefix_len, bundle_hash, hash_len + 1);

    char *rec_json = NULL;
    knishio_error_t err = backend->get_item(backend, rec_key, &rec_json);
    free(rec_key);
    if (err != KNISHIO_SUCCESS) return err;

    if (!rec_json) {
        return KNISHIO_ERROR_INVALID_STATE; /* recovery record not found */
    }

    char *plaintext = NULL;
    size_t plaintext_len = 0;
    err = knishio_envelope_open_json(rec_json, recovery_passphrase, &plaintext, &plaintext_len);
    free(rec_json);
    if (err != KNISHIO_SUCCESS) {
        return err;
    }

    /* Re-enroll under new_passphrase */
    knishio_storage_options_t store_opts;
    if (options) {
        store_opts = *options;
    } else {
        knishio_storage_options_init(&store_opts);
    }
    store_opts.passphrase = new_passphrase;

    err = knishio_envelope_store_secret(
        backend,
        bundle_hash,
        plaintext,
        plaintext_len,
        new_passphrase,
        &store_opts
    );

    /* Zeroize plaintext secret immediately; never leak or return */
    knishio_secure_zero(plaintext, plaintext_len);
    free(plaintext);

    return err;
}
