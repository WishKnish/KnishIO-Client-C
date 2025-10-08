/**
 * @file aes_gcm.c
 * @brief AES-256-GCM encryption/decryption implementation using OpenSSL
 *
 * Compatible with JavaScript SDK's Web Crypto API AES-GCM operations
 */

#include "knishio/crypto/aes_gcm.h"
#include "knishio/error/context.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Encrypt data using AES-256-GCM (JavaScript SDK compatible)
 *
 * Format matches JavaScript SDK's encryptWithSharedSecret():
 * - Generates random 12-byte IV
 * - Encrypts with AES-256-GCM
 * - Returns: [IV (12 bytes)][ciphertext][tag (16 bytes)]
 */
knishio_error_t knishio_aes_gcm_encrypt(
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t *key,
    uint8_t **ciphertext_out,
    size_t *ciphertext_len_out
) {
    if (!plaintext || !key || !ciphertext_out || !ciphertext_len_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    EVP_CIPHER_CTX *ctx = NULL;
    uint8_t iv[AES_GCM_IV_SIZE];
    uint8_t tag[AES_GCM_TAG_SIZE];
    uint8_t *output = NULL;
    size_t output_len = AES_GCM_IV_SIZE + plaintext_len + AES_GCM_TAG_SIZE;
    int len = 0;
    int ciphertext_len = 0;
    knishio_error_t result = KNISHIO_ERROR_CRYPTO;

    /* Generate random IV */
    if (RAND_bytes(iv, AES_GCM_IV_SIZE) != 1) {
        goto cleanup;
    }

    /* Allocate output buffer: IV + ciphertext + tag */
    output = malloc(output_len);
    if (!output) {
        result = KNISHIO_ERROR_MEMORY;
        goto cleanup;
    }

    /* Copy IV to start of output */
    memcpy(output, iv, AES_GCM_IV_SIZE);

    /* Create and initialize cipher context */
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        goto cleanup;
    }

    /* Initialize encryption operation with AES-256-GCM */
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        goto cleanup;
    }

    /* Set IV length (12 bytes for GCM mode) */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_GCM_IV_SIZE, NULL) != 1) {
        goto cleanup;
    }

    /* Initialize key and IV */
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        goto cleanup;
    }

    /* Encrypt plaintext */
    if (EVP_EncryptUpdate(ctx, output + AES_GCM_IV_SIZE, &len, plaintext, plaintext_len) != 1) {
        goto cleanup;
    }
    ciphertext_len = len;

    /* Finalize encryption */
    if (EVP_EncryptFinal_ex(ctx, output + AES_GCM_IV_SIZE + len, &len) != 1) {
        goto cleanup;
    }
    ciphertext_len += len;

    /* Get authentication tag */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_TAG_SIZE, tag) != 1) {
        goto cleanup;
    }

    /* Append tag to output */
    memcpy(output + AES_GCM_IV_SIZE + ciphertext_len, tag, AES_GCM_TAG_SIZE);

    /* Success */
    *ciphertext_out = output;
    *ciphertext_len_out = AES_GCM_IV_SIZE + ciphertext_len + AES_GCM_TAG_SIZE;
    result = KNISHIO_SUCCESS;
    output = NULL;  /* Don't free on cleanup */

cleanup:
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }
    if (output) {
        free(output);
    }
    return result;
}

/**
 * @brief Decrypt data using AES-256-GCM (JavaScript SDK compatible)
 *
 * Format matches JavaScript SDK's decryptWithSharedSecret():
 * - Expects: [IV (12 bytes)][ciphertext][tag (16 bytes)]
 * - Decrypts with AES-256-GCM
 * - Verifies authentication tag
 */
knishio_error_t knishio_aes_gcm_decrypt(
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *key,
    uint8_t **plaintext_out,
    size_t *plaintext_len_out
) {
    if (!ciphertext || !key || !plaintext_out || !plaintext_len_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    /* Validate minimum length: IV + tag */
    if (ciphertext_len < AES_GCM_IV_SIZE + AES_GCM_TAG_SIZE) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    EVP_CIPHER_CTX *ctx = NULL;
    uint8_t *plaintext = NULL;
    size_t encrypted_len = ciphertext_len - AES_GCM_IV_SIZE - AES_GCM_TAG_SIZE;
    int len = 0;
    int plaintext_len = 0;
    knishio_error_t result = KNISHIO_ERROR_CRYPTO;

    /* Extract components */
    const uint8_t *iv = ciphertext;
    const uint8_t *encrypted_data = ciphertext + AES_GCM_IV_SIZE;
    const uint8_t *tag = ciphertext + ciphertext_len - AES_GCM_TAG_SIZE;

    /* Allocate output buffer */
    plaintext = malloc(encrypted_len + 1);  /* +1 for null terminator */
    if (!plaintext) {
        result = KNISHIO_ERROR_MEMORY;
        goto cleanup;
    }

    /* Create and initialize cipher context */
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        goto cleanup;
    }

    /* Initialize decryption operation with AES-256-GCM */
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        goto cleanup;
    }

    /* Set IV length */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_GCM_IV_SIZE, NULL) != 1) {
        goto cleanup;
    }

    /* Initialize key and IV */
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        goto cleanup;
    }

    /* Decrypt ciphertext */
    if (EVP_DecryptUpdate(ctx, plaintext, &len, encrypted_data, encrypted_len) != 1) {
        goto cleanup;
    }
    plaintext_len = len;

    /* Set expected tag value */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_TAG_SIZE, (void*)tag) != 1) {
        goto cleanup;
    }

    /* Finalize decryption and verify tag */
    int ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    if (ret <= 0) {
        /* Tag verification failed */
        result = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }
    plaintext_len += len;

    /* Null-terminate for convenience (text messages) */
    plaintext[plaintext_len] = '\0';

    /* Success */
    *plaintext_out = plaintext;
    *plaintext_len_out = plaintext_len;
    result = KNISHIO_SUCCESS;
    plaintext = NULL;  /* Don't free on cleanup */

cleanup:
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }
    if (plaintext) {
        free(plaintext);
    }
    return result;
}