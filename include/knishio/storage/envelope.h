#ifndef KNISHIO_STORAGE_ENVELOPE_H
#define KNISHIO_STORAGE_ENVELOPE_H

#include "knishio/storage/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Seal a secret into an encrypted secret payload using AES-256-GCM and PBKDF2-HMAC-SHA256.
 *
 * Matches the cross-SDK envelope specification:
 * - 16-byte random salt
 * - 12-byte random IV
 * - PBKDF2-HMAC-SHA256 with 100,000 iterations to derive 32-byte key
 * - AES-256-GCM encryption with 16-byte tag appended to ciphertext
 * - Base64 encoding of ciphertext+tag, iv, salt
 * - camelCase metadata with bundleHash, createdAt, hardwareBacked (false), providerType ("aes-gcm")
 *
 * @param secret Plaintext secret string or bytes
 * @param secret_len Length of secret
 * @param passphrase Passphrase for key derivation
 * @param metadata Metadata describing the secret (bundleHash, etc.)
 * @param payload_out Output payload structure (caller frees with knishio_encrypted_payload_cleanup)
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_envelope_seal(
    const char *secret,
    size_t secret_len,
    const char *passphrase,
    const knishio_secret_metadata_t *metadata,
    knishio_encrypted_payload_t *payload_out
);

/**
 * @brief Open an encrypted secret payload using passphrase.
 *
 * Decodes salt, iv, and ciphertext from base64.
 * Derives key using PBKDF2-HMAC-SHA256 with payload->iterations.
 * Decrypts with AES-256-GCM and verifies 16-byte authentication tag.
 *
 * @param payload Encrypted payload to open
 * @param passphrase Passphrase for decryption
 * @param plaintext_out Output buffer containing decrypted plaintext (caller zeroizes and frees)
 * @param plaintext_len_out Output length of plaintext
 * @return KNISHIO_SUCCESS or error code (KNISHIO_ERROR_DECRYPTION_KEY / KNISHIO_ERROR_CRYPTO on tag failure)
 */
knishio_error_t knishio_envelope_open(
    const knishio_encrypted_payload_t *payload,
    const char *passphrase,
    char **plaintext_out,
    size_t *plaintext_len_out
);

/**
 * @brief Convenience helper: seal directly to JSON string
 * @param secret Plaintext secret
 * @param secret_len Secret length
 * @param passphrase Passphrase
 * @param metadata Metadata
 * @param json_out Output JSON string (caller frees with free())
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_envelope_seal_json(
    const char *secret,
    size_t secret_len,
    const char *passphrase,
    const knishio_secret_metadata_t *metadata,
    char **json_out
);

/**
 * @brief Convenience helper: open directly from JSON string
 * @param json_str Payload JSON string
 * @param passphrase Passphrase
 * @param plaintext_out Output buffer (caller zeroizes and frees)
 * @param plaintext_len_out Output length
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_envelope_open_json(
    const char *json_str,
    const char *passphrase,
    char **plaintext_out,
    size_t *plaintext_len_out
);

/**
 * @brief Store a master secret into storage backend with optional recovery envelope.
 *
 * Primary envelope is sealed under passphrase (or options->passphrase) and saved at
 * "knishio:secret:<bundle_hash>".
 *
 * When options != NULL and options->recovery_passphrase != NULL:
 * A secondary recovery envelope is sealed with software AES-GCM under recovery_passphrase
 * (hardwareBacked = false, providerType = "aes-gcm") and stored at "knishio:recovery:<bundle_hash>".
 *
 * @param backend Storage backend instance
 * @param bundle_hash Bundle hash identifying account/wallet
 * @param secret Plaintext secret
 * @param secret_len Length of secret
 * @param passphrase Primary encryption passphrase (optional if options->passphrase is set)
 * @param options Storage options (label, recovery_passphrase, allow_unrecoverable)
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_envelope_store_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    const char *secret,
    size_t secret_len,
    const char *passphrase,
    const knishio_storage_options_t *options
);

/**
 * @brief Retrieve and decrypt a master secret from storage backend.
 *
 * Looks up "knishio:secret:<bundle_hash>" in backend and decrypts it using passphrase
 * (or options->passphrase).
 *
 * @param backend Storage backend instance
 * @param bundle_hash Bundle hash identifying account/wallet
 * @param passphrase Decryption passphrase (optional if options->passphrase is set)
 * @param options Storage options
 * @param plaintext_out Output buffer containing decrypted plaintext (caller zeroizes and frees)
 * @param plaintext_len_out Output length of plaintext
 * @return KNISHIO_SUCCESS, or error code
 */
knishio_error_t knishio_envelope_retrieve_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    const char *passphrase,
    const knishio_storage_options_t *options,
    char **plaintext_out,
    size_t *plaintext_len_out
);

/**
 * @brief Delete a stored secret from a storage backend.
 *
 * Removes both primary envelope ("knishio:secret:<bundle_hash>") and
 * recovery envelope ("knishio:recovery:<bundle_hash>").
 *
 * @param backend Storage backend instance
 * @param bundle_hash Bundle hash
 * @param existed_out Optional pointer set to true if either record existed
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_envelope_delete_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    bool *existed_out
);

/**
 * @brief Check if a stored secret exists for bundle_hash.
 *
 * @param backend Storage backend instance
 * @param bundle_hash Bundle hash
 * @param exists_out Output boolean set to true if primary secret exists
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_envelope_has_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    bool *exists_out
);

/**
 * @brief List all stored secret metadata without exposing plaintext secrets.
 *
 * Scans backend keys, filtering for "knishio:secret:" and ignoring "knishio:recovery:".
 *
 * @param backend Storage backend instance
 * @param metadata_list_out Output array of metadata structures (caller frees with knishio_secret_metadata_list_free)
 * @param count_out Output count of items
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_envelope_list_secrets(
    knishio_storage_backend_t *backend,
    knishio_secret_metadata_t **metadata_list_out,
    size_t *count_out
);

/**
 * @brief Free metadata list returned by knishio_envelope_list_secrets.
 *
 * @param metadata_list Array of metadata structs to free
 * @param count Number of elements in array
 */
void knishio_secret_metadata_list_free(
    knishio_secret_metadata_t *metadata_list,
    size_t count
);

/**
 * @brief Recover a secret from its recovery envelope and re-enroll under new_passphrase.
 *
 * Opens recovery payload from "knishio:recovery:<bundle_hash>" using recovery_passphrase,
 * then re-encrypts under new_passphrase and stores as primary envelope.
 * Plaintext is zeroized immediately after re-encryption and never returned to caller.
 *
 * @param backend Storage backend instance
 * @param bundle_hash Bundle hash
 * @param recovery_passphrase Passphrase used when recovery envelope was sealed
 * @param new_passphrase New primary passphrase for re-encryption
 * @param options Storage options (optional label, etc.)
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_envelope_recover_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    const char *recovery_passphrase,
    const char *new_passphrase,
    const knishio_storage_options_t *options
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_STORAGE_ENVELOPE_H */
