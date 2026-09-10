#ifndef KNISHIO_CRYPTO_MLKEM_H
#define KNISHIO_CRYPTO_MLKEM_H

/**
 * @file mlkem.h
 * @brief ML-KEM post-quantum cryptography implementation (FIPS 203) for KnishIO SDK.
 * Supports both ML-KEM-1024 (default, CNSA 2.0 compliant) and ML-KEM-768 (opt-in step-back).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../error/context.h"

typedef enum {
    KNISHIO_MLKEM_1024 = 1024,
    KNISHIO_MLKEM_768 = 768
} knishio_mlkem_param_t;

#define KNISHIO_MLKEM_MAX_PUBLIC_KEY_BYTES  1568
#define KNISHIO_MLKEM_MAX_PRIVATE_KEY_BYTES 3168
#define KNISHIO_MLKEM_MAX_CIPHERTEXT_BYTES  1568
#define KNISHIO_MLKEM_SHARED_SECRET_BYTES   32

#define KNISHIO_MLKEM1024_PUBLIC_KEY_BYTES   1568
#define KNISHIO_MLKEM1024_PRIVATE_KEY_BYTES  3168
#define KNISHIO_MLKEM1024_CIPHERTEXT_BYTES   1568

#define KNISHIO_MLKEM768_PUBLIC_KEY_BYTES    1184
#define KNISHIO_MLKEM768_PRIVATE_KEY_BYTES   2400
#define KNISHIO_MLKEM768_CIPHERTEXT_BYTES    1088

typedef struct {
    uint8_t public_key[KNISHIO_MLKEM_MAX_PUBLIC_KEY_BYTES];
    uint8_t private_key[KNISHIO_MLKEM_MAX_PRIVATE_KEY_BYTES];
    size_t public_key_len;
    size_t private_key_len;
    knishio_mlkem_param_t param;
} knishio_mlkem_keypair_t;

typedef struct {
    uint8_t ciphertext[KNISHIO_MLKEM_MAX_CIPHERTEXT_BYTES];
    size_t ciphertext_len;
} knishio_mlkem_ciphertext_t;

typedef struct {
    uint8_t shared_secret[KNISHIO_MLKEM_SHARED_SECRET_BYTES];
} knishio_mlkem_shared_secret_t;


/* Core ML-KEM Functions */
knishio_error_t knishio_mlkem_keypair(knishio_mlkem_keypair_t *keypair, knishio_mlkem_param_t param);
knishio_error_t knishio_mlkem_keypair_from_seed(knishio_mlkem_keypair_t *keypair, const uint8_t *seed, size_t seed_len, knishio_mlkem_param_t param);
knishio_error_t knishio_mlkem_encapsulate(const uint8_t *public_key, size_t public_key_len, knishio_mlkem_ciphertext_t *ciphertext, knishio_mlkem_shared_secret_t *shared_secret);
knishio_error_t knishio_mlkem_decapsulate(const uint8_t *private_key, size_t private_key_len, const knishio_mlkem_ciphertext_t *ciphertext, knishio_mlkem_shared_secret_t *shared_secret);

/* High-level encryption/decryption using hybrid approach */
knishio_error_t knishio_mlkem_encrypt(const uint8_t *public_key, size_t public_key_len, const uint8_t *plaintext, size_t plaintext_len, uint8_t **ciphertext_out, size_t *ciphertext_len_out);
knishio_error_t knishio_mlkem_decrypt(const uint8_t *private_key, size_t private_key_len, const uint8_t *ciphertext, size_t ciphertext_len, uint8_t **plaintext_out, size_t *plaintext_len_out);

/* Key serialization */
knishio_error_t knishio_mlkem_public_key_to_hex(const uint8_t *public_key, size_t public_key_len, char **hex_output);
knishio_error_t knishio_mlkem_private_key_to_hex(const uint8_t *private_key, size_t private_key_len, char **hex_output);
knishio_error_t knishio_mlkem_public_key_from_hex(const char *hex_input, uint8_t *public_key, size_t *public_key_len);
knishio_error_t knishio_mlkem_private_key_from_hex(const char *hex_input, uint8_t *private_key, size_t *private_key_len);

/* Utility functions */
bool knishio_mlkem_is_available(void);
const char* knishio_mlkem_get_algorithm_name(void);
void knishio_mlkem_cleanup(void);


#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CRYPTO_MLKEM_H */
