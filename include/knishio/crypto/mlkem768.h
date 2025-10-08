#ifndef KNISHIO_CRYPTO_MLKEM768_H
#define KNISHIO_CRYPTO_MLKEM768_H

/**
 * @file mlkem768.h
 * @brief ML-KEM-768 post-quantum cryptography implementation for KnishIO SDK
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Include proper error definitions */
#include "../error/context.h"

/**
 * @brief ML-KEM-768 key pair structure
 */
typedef struct {
    uint8_t public_key[1184];   /**< ML-KEM-768 public key (1184 bytes) */
    uint8_t private_key[2400];  /**< ML-KEM-768 private key (2400 bytes) */
} knishio_mlkem768_keypair_t;

/**
 * @brief ML-KEM-768 ciphertext structure
 */
typedef struct {
    uint8_t ciphertext[1088];   /**< ML-KEM-768 ciphertext (1088 bytes) */
} knishio_mlkem768_ciphertext_t;

/**
 * @brief ML-KEM-768 shared secret structure
 */
typedef struct {
    uint8_t shared_secret[32];  /**< ML-KEM-768 shared secret (32 bytes) */
} knishio_mlkem768_shared_secret_t;

/* Core ML-KEM Functions */

/**
 * @brief Generate ML-KEM-768 key pair
 * @param keypair Output key pair structure
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_mlkem768_keypair(knishio_mlkem768_keypair_t *keypair);

/**
 * @brief Generate ML-KEM-768 key pair from seed (deterministic)
 * @param keypair Output key pair structure
 * @param seed Input seed (minimum 64 bytes)
 * @param seed_len Length of seed
 * @return KNISHIO_SUCCESS on success, error code on failure
 * 
 * Compatible with JavaScript SDK: MlKEM768.keygen(seed)
 */
knishio_error_t knishio_mlkem768_keypair_from_seed(knishio_mlkem768_keypair_t *keypair, 
                                                   const uint8_t *seed, size_t seed_len);

/**
 * @brief Encapsulate shared secret using ML-KEM-768
 * @param public_key Public key for encapsulation
 * @param ciphertext Output ciphertext
 * @param shared_secret Output shared secret
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_mlkem768_encapsulate(
    const uint8_t *public_key,
    knishio_mlkem768_ciphertext_t *ciphertext,
    knishio_mlkem768_shared_secret_t *shared_secret
);

/**
 * @brief Decapsulate shared secret using ML-KEM-768
 * @param private_key Private key for decapsulation
 * @param ciphertext Input ciphertext
 * @param shared_secret Output shared secret
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_mlkem768_decapsulate(
    const uint8_t *private_key,
    const knishio_mlkem768_ciphertext_t *ciphertext,
    knishio_mlkem768_shared_secret_t *shared_secret
);

/* High-level Encryption/Decryption */

/**
 * @brief Encrypt data using ML-KEM-768 hybrid encryption
 * @param public_key Recipient's public key
 * @param plaintext Data to encrypt
 * @param plaintext_len Length of plaintext
 * @param ciphertext_out Output ciphertext (allocated, must be freed)
 * @param ciphertext_len_out Length of output ciphertext
 * @return KNISHIO_SUCCESS on success, error code on failure
 * 
 * Uses KEM + symmetric encryption for arbitrary-length data
 */
knishio_error_t knishio_mlkem768_encrypt(const uint8_t *public_key, 
                                         const uint8_t *plaintext, size_t plaintext_len,
                                         uint8_t **ciphertext_out, size_t *ciphertext_len_out);

/**
 * @brief Decrypt data using ML-KEM-768 hybrid decryption
 * @param private_key Private key for decryption
 * @param ciphertext Ciphertext to decrypt
 * @param ciphertext_len Length of ciphertext
 * @param plaintext_out Output plaintext (allocated, must be freed)
 * @param plaintext_len_out Length of output plaintext
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_mlkem768_decrypt(const uint8_t *private_key,
                                         const uint8_t *ciphertext, size_t ciphertext_len,
                                         uint8_t **plaintext_out, size_t *plaintext_len_out);

/* Key Serialization */

/**
 * @brief Convert public key to hex string
 * @param public_key Input public key
 * @param hex_output Output hex string (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_mlkem768_public_key_to_hex(
    const uint8_t *public_key,
    char **hex_output
);

/**
 * @brief Convert private key to hex string
 * @param private_key Input private key
 * @param hex_output Output hex string (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_mlkem768_private_key_to_hex(
    const uint8_t *private_key,
    char **hex_output
);

/**
 * @brief Parse public key from hex string
 * @param hex_input Input hex string
 * @param public_key Output public key
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_mlkem768_public_key_from_hex(
    const char *hex_input,
    uint8_t *public_key
);

/**
 * @brief Parse private key from hex string
 * @param hex_input Input hex string
 * @param private_key Output private key
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_mlkem768_private_key_from_hex(
    const char *hex_input,
    uint8_t *private_key
);

/* Utility Functions */

/**
 * @brief Check if ML-KEM-768 is available on this platform
 * @return true if available, false otherwise
 */
bool knishio_mlkem768_is_available(void);

/**
 * @brief Get algorithm name string
 * @return Algorithm name (e.g., "Kyber768")
 */
const char* knishio_mlkem768_get_algorithm_name(void);

/**
 * @brief Cleanup ML-KEM-768 resources
 * 
 * Call this when shutting down to clean up any global state
 */
void knishio_mlkem768_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CRYPTO_MLKEM768_H */
