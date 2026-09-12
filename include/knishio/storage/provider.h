#ifndef KNISHIO_STORAGE_PROVIDER_H
#define KNISHIO_STORAGE_PROVIDER_H

/**
 * @file provider.h
 * @brief High-level secret storage provider interface and implementations
 */

#include "knishio/storage/types.h"
#include "knishio/storage/backend.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct knishio_secret_storage_provider knishio_secret_storage_provider_t;

/**
 * @brief Virtual method table and state for a secret storage provider.
 */
struct knishio_secret_storage_provider {
    void *user_data;

    /**
     * @brief Return the provider type identifier (e.g. "aes-gcm")
     */
    const char *(*provider_type)(const knishio_secret_storage_provider_t *provider);

    /**
     * @brief True if provider holds keys in hardware
     */
    bool (*is_hardware_backed)(const knishio_secret_storage_provider_t *provider);

    /**
     * @brief Store a master secret under a bundle hash
     */
    knishio_error_t (*store_secret)(
        knishio_secret_storage_provider_t *provider,
        const char *bundle_hash,
        const char *secret,
        size_t secret_len,
        const knishio_storage_options_t *options
    );

    /**
     * @brief Retrieve a master secret by bundle hash.
     * On success, if secret is not found, returns KNISHIO_SUCCESS with *plaintext_out = NULL.
     * Caller frees plaintext with knishio_secure_free(p, len).
     */
    knishio_error_t (*retrieve_secret)(
        knishio_secret_storage_provider_t *provider,
        const char *bundle_hash,
        const knishio_storage_options_t *options,
        char **plaintext_out,
        size_t *plaintext_len_out
    );

    /**
     * @brief Delete a secret and its recovery record
     */
    knishio_error_t (*delete_secret)(
        knishio_secret_storage_provider_t *provider,
        const char *bundle_hash,
        bool *existed_out
    );

    /**
     * @brief Check whether a primary secret exists
     */
    knishio_error_t (*has_secret)(
        knishio_secret_storage_provider_t *provider,
        const char *bundle_hash,
        bool *exists_out
    );

    /**
     * @brief List metadata of all stored secrets.
     * Output array is freed with knishio_secret_metadata_list_free.
     */
    knishio_error_t (*list_secrets)(
        knishio_secret_storage_provider_t *provider,
        knishio_secret_metadata_t **metadata_list_out,
        size_t *count_out
    );

    /**
     * @brief Recover a secret from its recovery envelope and re-enroll under the active key
     */
    knishio_error_t (*recover_secret)(
        knishio_secret_storage_provider_t *provider,
        const char *bundle_hash,
        const char *recovery_passphrase,
        const knishio_storage_options_t *options
    );

    /**
     * @brief Destroy provider and free internal resources
     */
    void (*free_provider)(knishio_secret_storage_provider_t *provider);
};

/**
 * @brief Create an AES-256-GCM software secret storage provider over a storage backend.
 *
 * @param backend Storage backend to persist envelopes in (borrowed; caller frees after provider)
 * @param default_passphrase Default passphrase used when options->passphrase is omitted (may be NULL)
 * @param provider_out Pointer to receive created provider instance
 * @return KNISHIO_SUCCESS on success, or error code
 */
knishio_error_t knishio_aes_gcm_secret_storage_provider_create(
    knishio_storage_backend_t *backend,
    const char *default_passphrase,
    knishio_secret_storage_provider_t **provider_out
);

/**
 * @brief Free a secret storage provider.
 */
void knishio_secret_storage_provider_free(knishio_secret_storage_provider_t *provider);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_STORAGE_PROVIDER_H */
