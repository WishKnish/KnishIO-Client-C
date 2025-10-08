#ifndef KNISHIO_CRYPTO_AES_GCM_H
#define KNISHIO_CRYPTO_AES_GCM_H

/**
 * @file aes_gcm.h
 * @brief AES-256-GCM encryption/decryption for KnishIO SDK
 *
 * Compatible with JavaScript SDK's encryptWithSharedSecret/decryptWithSharedSecret
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../error/context.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AES_GCM_IV_SIZE 12      /* 96-bit IV for GCM mode */
#define AES_GCM_TAG_SIZE 16     /* 128-bit authentication tag */
#define AES_256_KEY_SIZE 32     /* 256-bit key */

/**
 * @brief Encrypt data using AES-256-GCM (JavaScript SDK compatible)
 * @param plaintext Data to encrypt
 * @param plaintext_len Length of plaintext
 * @param key 32-byte encryption key (shared secret)
 * @param ciphertext_out Output buffer (allocated, must be freed)
 * @param ciphertext_len_out Length of output (IV + encrypted + tag)
 * @return KNISHIO_SUCCESS on success, error code on failure
 *
 * Output format: [12-byte IV][encrypted data][16-byte tag]
 * Compatible with JavaScript SDK's encryptWithSharedSecret()
 */
knishio_error_t knishio_aes_gcm_encrypt(
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t *key,
    uint8_t **ciphertext_out,
    size_t *ciphertext_len_out
);

/**
 * @brief Decrypt data using AES-256-GCM (JavaScript SDK compatible)
 * @param ciphertext Combined IV + encrypted + tag
 * @param ciphertext_len Length of ciphertext
 * @param key 32-byte decryption key (shared secret)
 * @param plaintext_out Output buffer (allocated, must be freed)
 * @param plaintext_len_out Length of output plaintext
 * @return KNISHIO_SUCCESS on success, error code on failure
 *
 * Input format: [12-byte IV][encrypted data][16-byte tag]
 * Compatible with JavaScript SDK's decryptWithSharedSecret()
 */
knishio_error_t knishio_aes_gcm_decrypt(
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *key,
    uint8_t **plaintext_out,
    size_t *plaintext_len_out
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CRYPTO_AES_GCM_H */
