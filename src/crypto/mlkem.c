/**
 * @file mlkem.c
 * @brief ML-KEM post-quantum cryptography implementation (FIPS 203) for KnishIO SDK.
 * Supports both ML-KEM-1024 (default) and ML-KEM-768 (opt-in step-back).
 */

#include "knishio/crypto/mlkem.h"
#include "knishio/crypto/shake256.h"
#include "knishio/crypto/aes_gcm.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/utils/security.h"
#include "knishio/error/context.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_MLKEM_NATIVE
#include "mlkem_multilevel.h"
#endif

/* Parameter constants */
#define MLKEM1024_PUBLIC_KEY_BYTES   1568
#define MLKEM1024_PRIVATE_KEY_BYTES  3168
#define MLKEM1024_CIPHERTEXT_BYTES   1568

#define MLKEM768_PUBLIC_KEY_BYTES    1184
#define MLKEM768_PRIVATE_KEY_BYTES   2400
#define MLKEM768_CIPHERTEXT_BYTES    1088

#define MLKEM_SHARED_SECRET_BYTES    32

/**
 * @brief Secure memory clearing utility
 */
static void secure_zero(void *ptr, size_t size) {
    volatile unsigned char *p = ptr;
    while (size--) {
        *p++ = 0;
    }
}

/**
 * @brief Convert binary data to hex string
 */
static knishio_error_t bin_to_hex(const uint8_t *data, size_t data_len, char **hex_out) {
    if (!data || !hex_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    char *hex = malloc(data_len * 2 + 1);
    if (!hex) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    for (size_t i = 0; i < data_len; i++) {
        sprintf(&hex[i * 2], "%02x", data[i]);
    }
    hex[data_len * 2] = '\0';
    
    *hex_out = hex;
    return KNISHIO_SUCCESS;
}

/**
 * @brief Convert hex string to binary data
 */
static knishio_error_t hex_to_bin(const char *hex_str, uint8_t *data_out, size_t expected_len) {
    if (!hex_str || !data_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    size_t hex_len = strlen(hex_str);
    if (hex_len != expected_len * 2) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    for (size_t i = 0; i < expected_len; i++) {
        unsigned int byte;
        if (sscanf(&hex_str[i * 2], "%02x", &byte) != 1) {
            return KNISHIO_ERROR_INVALID_ARGS;
        }
        data_out[i] = (uint8_t)byte;
    }
    
    return KNISHIO_SUCCESS;
}

/* Core ML-KEM Functions */

knishio_error_t knishio_mlkem_keypair(knishio_mlkem_keypair_t *keypair, knishio_mlkem_param_t param) {
    if (!keypair) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    uint8_t seed[64];
    if (!knishio_secure_random(seed, sizeof(seed))) {
        return KNISHIO_ERROR_CRYPTO;
    }

    knishio_error_t ret = knishio_mlkem_keypair_from_seed(keypair, seed, sizeof(seed), param);
    secure_zero(seed, sizeof(seed));
    return ret;
}

knishio_error_t knishio_mlkem_keypair_from_seed(knishio_mlkem_keypair_t *keypair, 
                                                const uint8_t *seed, size_t seed_len, 
                                                knishio_mlkem_param_t param) {
    if (!keypair || !seed) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    if (seed_len != 64) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

#ifdef HAVE_MLKEM_NATIVE
    int result = 0;
    if (param == KNISHIO_MLKEM_1024) {
        result = mlkem1024_keypair_derand(keypair->public_key, keypair->private_key, seed);
        keypair->public_key_len = MLKEM1024_PUBLIC_KEY_BYTES;
        keypair->private_key_len = MLKEM1024_PRIVATE_KEY_BYTES;
        keypair->param = KNISHIO_MLKEM_1024;
    } else if (param == KNISHIO_MLKEM_768) {
        result = mlkem768_keypair_derand(keypair->public_key, keypair->private_key, seed);
        keypair->public_key_len = MLKEM768_PUBLIC_KEY_BYTES;
        keypair->private_key_len = MLKEM768_PRIVATE_KEY_BYTES;
        keypair->param = KNISHIO_MLKEM_768;
    } else {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    if (result != 0) {
        secure_zero(keypair->private_key, sizeof(keypair->private_key));
        return KNISHIO_ERROR_CRYPTO;
    }

    return KNISHIO_SUCCESS;
#else
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#endif
}

knishio_error_t knishio_mlkem_encapsulate(
    const uint8_t *public_key,
    size_t public_key_len,
    knishio_mlkem_ciphertext_t *ciphertext,
    knishio_mlkem_shared_secret_t *shared_secret
) {
    if (!public_key || !ciphertext || !shared_secret) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

#ifdef HAVE_MLKEM_NATIVE
    uint8_t coins[32];
    if (!knishio_secure_random(coins, sizeof(coins))) {
        secure_zero(coins, sizeof(coins));
        secure_zero(shared_secret->shared_secret, MLKEM_SHARED_SECRET_BYTES);
        return KNISHIO_ERROR_CRYPTO;
    }

    int result = 0;
    if (public_key_len == MLKEM1024_PUBLIC_KEY_BYTES) {
        result = mlkem1024_enc_derand(ciphertext->ciphertext, shared_secret->shared_secret, public_key, coins);
        ciphertext->ciphertext_len = MLKEM1024_CIPHERTEXT_BYTES;
    } else if (public_key_len == MLKEM768_PUBLIC_KEY_BYTES) {
        result = mlkem768_enc_derand(ciphertext->ciphertext, shared_secret->shared_secret, public_key, coins);
        ciphertext->ciphertext_len = MLKEM768_CIPHERTEXT_BYTES;
    } else {
        secure_zero(coins, sizeof(coins));
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    secure_zero(coins, sizeof(coins));

    if (result != 0) {
        secure_zero(shared_secret->shared_secret, MLKEM_SHARED_SECRET_BYTES);
        return KNISHIO_ERROR_CRYPTO;
    }

    return KNISHIO_SUCCESS;
#else
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#endif
}

knishio_error_t knishio_mlkem_decapsulate(
    const uint8_t *private_key,
    size_t private_key_len,
    const knishio_mlkem_ciphertext_t *ciphertext,
    knishio_mlkem_shared_secret_t *shared_secret
) {
    if (!private_key || !ciphertext || !shared_secret) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

#ifdef HAVE_MLKEM_NATIVE
    int result = 0;
    if (ciphertext->ciphertext_len == MLKEM1024_CIPHERTEXT_BYTES && private_key_len == MLKEM1024_PRIVATE_KEY_BYTES) {
        result = mlkem1024_dec(shared_secret->shared_secret, ciphertext->ciphertext, private_key);
    } else if (ciphertext->ciphertext_len == MLKEM768_CIPHERTEXT_BYTES && private_key_len == MLKEM768_PRIVATE_KEY_BYTES) {
        result = mlkem768_dec(shared_secret->shared_secret, ciphertext->ciphertext, private_key);
    } else {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    if (result != 0) {
        secure_zero(shared_secret->shared_secret, MLKEM_SHARED_SECRET_BYTES);
        return KNISHIO_ERROR_CRYPTO;
    }

    return KNISHIO_SUCCESS;
#else
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#endif
}

knishio_error_t knishio_mlkem_encrypt(
    const uint8_t *public_key,
    size_t public_key_len,
    const uint8_t *plaintext,
    size_t plaintext_len,
    uint8_t **ciphertext_out,
    size_t *ciphertext_len_out
) {
    if (!public_key || !plaintext || !ciphertext_out || !ciphertext_len_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_mlkem_ciphertext_t kem_ciphertext;
    knishio_mlkem_shared_secret_t shared_secret;

    knishio_error_t ret = knishio_mlkem_encapsulate(public_key, public_key_len, &kem_ciphertext, &shared_secret);
    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

    uint8_t *encrypted_message = NULL;
    size_t encrypted_message_len = 0;

    ret = knishio_aes_gcm_encrypt(
        plaintext, plaintext_len,
        shared_secret.shared_secret,
        &encrypted_message, &encrypted_message_len
    );

    if (ret != KNISHIO_SUCCESS) {
        secure_zero(&shared_secret, sizeof(shared_secret));
        return ret;
    }

    size_t total_len = kem_ciphertext.ciphertext_len + encrypted_message_len;
    uint8_t *result = malloc(total_len);
    if (!result) {
        free(encrypted_message);
        secure_zero(&shared_secret, sizeof(shared_secret));
        return KNISHIO_ERROR_MEMORY;
    }

    memcpy(result, kem_ciphertext.ciphertext, kem_ciphertext.ciphertext_len);
    memcpy(result + kem_ciphertext.ciphertext_len, encrypted_message, encrypted_message_len);

    free(encrypted_message);
    secure_zero(&shared_secret, sizeof(shared_secret));

    *ciphertext_out = result;
    *ciphertext_len_out = total_len;

    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_mlkem_decrypt(
    const uint8_t *private_key,
    size_t private_key_len,
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    uint8_t **plaintext_out,
    size_t *plaintext_len_out
) {
    if (!private_key || !ciphertext || !plaintext_out || !plaintext_len_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    size_t kem_ct_len = (private_key_len == MLKEM1024_PRIVATE_KEY_BYTES) ? MLKEM1024_CIPHERTEXT_BYTES : MLKEM768_CIPHERTEXT_BYTES;
    if (ciphertext_len < kem_ct_len) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    knishio_mlkem_ciphertext_t kem_ciphertext;
    memcpy(kem_ciphertext.ciphertext, ciphertext, kem_ct_len);
    kem_ciphertext.ciphertext_len = kem_ct_len;

    knishio_mlkem_shared_secret_t shared_secret;
    knishio_error_t ret = knishio_mlkem_decapsulate(private_key, private_key_len, &kem_ciphertext, &shared_secret);
    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

    const uint8_t *encrypted_message = ciphertext + kem_ct_len;
    size_t encrypted_message_len = ciphertext_len - kem_ct_len;

    uint8_t *plaintext = NULL;
    size_t plaintext_len = 0;

    ret = knishio_aes_gcm_decrypt(
        encrypted_message, encrypted_message_len,
        shared_secret.shared_secret,
        &plaintext, &plaintext_len
    );

    secure_zero(&shared_secret, sizeof(shared_secret));

    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

    *plaintext_out = plaintext;
    *plaintext_len_out = plaintext_len;

    return KNISHIO_SUCCESS;
}

/* Key serialization functions */

knishio_error_t knishio_mlkem_public_key_to_hex(const uint8_t *public_key, size_t public_key_len, char **hex_output) {
    if (!public_key || !hex_output) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    return bin_to_hex(public_key, public_key_len, hex_output);
}

knishio_error_t knishio_mlkem_private_key_to_hex(const uint8_t *private_key, size_t private_key_len, char **hex_output) {
    if (!private_key || !hex_output) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    return bin_to_hex(private_key, private_key_len, hex_output);
}

knishio_error_t knishio_mlkem_public_key_from_hex(const char *hex_input, uint8_t *public_key, size_t *public_key_len) {
    if (!hex_input || !public_key || !public_key_len) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    size_t hex_len = strlen(hex_input);
    size_t bytes = hex_len / 2;
    *public_key_len = bytes;
    return hex_to_bin(hex_input, public_key, bytes);
}

knishio_error_t knishio_mlkem_private_key_from_hex(const char *hex_input, uint8_t *private_key, size_t *private_key_len) {
    if (!hex_input || !private_key || !private_key_len) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    size_t hex_len = strlen(hex_input);
    size_t bytes = hex_len / 2;
    *private_key_len = bytes;
    return hex_to_bin(hex_input, private_key, bytes);
}

bool knishio_mlkem_is_available(void) {
#ifdef HAVE_MLKEM_NATIVE
    return true;
#else
    return false;
#endif
}

const char* knishio_mlkem_get_algorithm_name(void) {
    return "ML-KEM-Native (1024/768)";
}

void knishio_mlkem_cleanup(void) {
    /* No cleanup needed for native */
}

