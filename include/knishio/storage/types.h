#ifndef KNISHIO_STORAGE_TYPES_H
#define KNISHIO_STORAGE_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "knishio/error/context.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KNISHIO_SECRET_STORAGE_PREFIX "knishio:secret:"
#define KNISHIO_RECOVERY_STORAGE_PREFIX "knishio:recovery:"
#define KNISHIO_RECOVERY_KEY_PREFIX "knishio:recovery:"
#define KNISHIO_STORAGE_DEFAULT_ITERATIONS 100000
#define KNISHIO_STORAGE_SALT_LENGTH 16
#define KNISHIO_STORAGE_IV_LENGTH 12

/**
 * @brief Metadata associated with an encrypted secret in storage
 * 
 * Wire format is camelCase (cross-SDK contract):
 * - bundleHash (required)
 * - createdAt (required, timestamp in ms)
 * - hardwareBacked (required, boolean)
 * - providerType (required, string e.g. "aes-gcm")
 * - label (optional, string; omitted from JSON when NULL, NEVER null)
 */
typedef struct knishio_secret_metadata {
    char *bundle_hash;       /**< Bundle hash identifying account/wallet */
    char *label;             /**< Optional human-readable label (NULL if absent) */
    int64_t created_at;      /**< Creation timestamp in milliseconds */
    bool hardware_backed;    /**< True if key lives in platform hardware */
    char *provider_type;     /**< Provider type identifier (e.g. "aes-gcm") */
} knishio_secret_metadata_t;

/**
 * @brief Versioned envelope encryption payload
 *
 * Wire format (JSON):
 * - version: integer (always 1)
 * - ciphertext: base64 of (ciphertext + tag)
 * - iv: base64 of 12-byte IV
 * - salt: base64 of 16-byte salt
 * - algorithm: string ("AES-GCM")
 * - iterations: integer (100000)
 * - metadata: knishio_secret_metadata_t object
 */
typedef struct knishio_encrypted_payload {
    uint32_t version;
    char *ciphertext;        /**< Base64 encoded ciphertext + 16-byte auth tag */
    char *iv;                /**< Base64 encoded 12-byte IV */
    char *salt;              /**< Base64 encoded 16-byte salt */
    char *algorithm;         /**< "AES-GCM" */
    uint32_t iterations;     /**< PBKDF2 iteration count (100000) */
    knishio_secret_metadata_t metadata;
} knishio_encrypted_payload_t;

/**
 * @brief Pluggable key-value storage backend interface
 */
typedef struct knishio_storage_backend knishio_storage_backend_t;

struct knishio_storage_backend {
    void *user_data;         /**< Implementation-specific state */

    /**
     * @brief Retrieve item by key
     * @param backend Storage backend instance
     * @param key Key to look up
     * @param value_out Output value (allocated string, caller must free with free()), or NULL if not found
     * @return KNISHIO_SUCCESS on success (even if not found, *value_out = NULL), or error code
     */
    knishio_error_t (*get_item)(knishio_storage_backend_t *backend, const char *key, char **value_out);

    /**
     * @brief Store item by key
     * @param backend Storage backend instance
     * @param key Key to store
     * @param value Value string to store
     * @return KNISHIO_SUCCESS or error code
     */
    knishio_error_t (*set_item)(knishio_storage_backend_t *backend, const char *key, const char *value);

    /**
     * @brief Remove item by key
     * @param backend Storage backend instance
     * @param key Key to remove
     * @param existed_out Optional pointer to receive true if item existed, false otherwise
     * @return KNISHIO_SUCCESS or error code
     */
    knishio_error_t (*remove_item)(knishio_storage_backend_t *backend, const char *key, bool *existed_out);

    /**
     * @brief List all keys in storage
     * @param backend Storage backend instance
     * @param keys_out Output array of string pointers (allocated, caller frees each string and the array)
     * @param count_out Output count of keys
     * @return KNISHIO_SUCCESS or error code
     */
    knishio_error_t (*keys)(knishio_storage_backend_t *backend, char ***keys_out, size_t *count_out);

    /**
     * @brief Destroy / free backend instance and internal state
     */
    void (*free_backend)(knishio_storage_backend_t *backend);
};

/**
 * @brief Options for secret storage operations
 */
typedef struct knishio_storage_options {
    const char *label;                  /**< Optional human-readable label (NULL if absent) */
    const char *passphrase;             /**< Optional primary operation passphrase */
    const char *recovery_passphrase;    /**< Optional recovery passphrase for secondary recovery envelope */
    bool allow_unrecoverable;           /**< If true, allows storage without recovery passphrase */
} knishio_storage_options_t;

void knishio_storage_options_init(knishio_storage_options_t *options);

/* Lifecycle and serialization */
void knishio_secret_metadata_init(knishio_secret_metadata_t *meta);
void knishio_secret_metadata_cleanup(knishio_secret_metadata_t *meta);

void knishio_encrypted_payload_init(knishio_encrypted_payload_t *payload);
void knishio_encrypted_payload_cleanup(knishio_encrypted_payload_t *payload);

/**
 * @brief Serialize payload to JSON string
 * @param payload Payload to serialize
 * @return Allocated JSON string (caller frees with free()), or NULL on error
 */
char* knishio_encrypted_payload_to_json(const knishio_encrypted_payload_t *payload);

/**
 * @brief Parse payload from JSON string
 * @param json_str JSON string to parse
 * @param payload Output payload (caller frees with knishio_encrypted_payload_cleanup)
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_encrypted_payload_from_json(const char *json_str, knishio_encrypted_payload_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_STORAGE_TYPES_H */
