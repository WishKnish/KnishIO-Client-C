/**
 * @file mlkem768.c
 * @brief ML-KEM-768 post-quantum cryptography implementation for KnishIO SDK
 * 
 * This module implements ML-KEM-768 post-quantum key encapsulation 
 * mechanism using mlkem-native. It provides compatibility with the JavaScript SDK's
 * @noble/post-quantum ML-KEM768 implementation.
 * 
 * Key Features:
 * - NIST FIPS 203 standard-compliant ML-KEM-768 
 * - Compatible with @noble/post-quantum outputs
 * - Constant-time operations for side-channel resistance
 * - Secure memory management with key zeroization
 * - Deterministic key generation from seeds
 * - JavaScript cross-platform compatibility
 * 
 * Security Level: ~192-bit classical / ~128-bit quantum
 */

#include "knishio/crypto/mlkem768.h"
#include "knishio/crypto/shake256.h"
#include "knishio/crypto/aes_gcm.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/error/context.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_MLKEM_NATIVE
/* mlkem-native configuration for ML-KEM768 */
#define MLK_CONFIG_API_PARAMETER_SET 768
#define MLK_CONFIG_API_NAMESPACE_PREFIX mlkem
#include "mlkem_native.h"
#endif

#ifdef HAVE_LIBOQS
#include <oqs/oqs.h>
#endif

/* ML-KEM-768 parameter constants (NIST standard) */
#define MLKEM768_PUBLIC_KEY_BYTES   1184
#define MLKEM768_PRIVATE_KEY_BYTES  2400
#define MLKEM768_CIPHERTEXT_BYTES   1088
#define MLKEM768_SHARED_SECRET_BYTES   32

/* Internal structures for ML-KEM state */
typedef struct {
    bool initialized;
    char algorithm_name[32];
} mlkem768_state_t;

static mlkem768_state_t g_mlkem_state = { false, "ML-KEM-768" };

/**
 * @brief Initialize ML-KEM-768 algorithm
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
static knishio_error_t mlkem768_init(void) {
    if (g_mlkem_state.initialized) {
        return KNISHIO_SUCCESS;
    }

#ifdef HAVE_MLKEM_NATIVE
    /* mlkem-native is always available once compiled in */
    strcpy(g_mlkem_state.algorithm_name, "ML-KEM-768-Native");
    g_mlkem_state.initialized = true;
    return KNISHIO_SUCCESS;
#elif defined(HAVE_LIBOQS)
    /* Initialize liboqs as fallback */
    OQS_init();
    printf("DEBUG INIT: Using liboqs fallback\n");
    strcpy(g_mlkem_state.algorithm_name, "Kyber768-Fallback");
    g_mlkem_state.initialized = true;
    return KNISHIO_SUCCESS;
#else
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief Secure memory clearing utility
 * @param ptr Pointer to memory to clear
 * @param size Size of memory to clear
 */
static void secure_zero(void *ptr, size_t size) {
    volatile unsigned char *p = ptr;
    while (size--) {
        *p++ = 0;
    }
}

/**
 * @brief Convert binary data to hex string
 * @param data Input binary data
 * @param data_len Length of input data
 * @param hex_out Output hex string (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
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
 * @param hex_str Input hex string
 * @param data_out Output binary data
 * @param expected_len Expected length of binary data
 * @return KNISHIO_SUCCESS on success, error code on failure
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

/* Public API Implementation */

knishio_error_t knishio_mlkem768_keypair(knishio_mlkem768_keypair_t *keypair) {
    if (!keypair) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    knishio_error_t ret = mlkem768_init();
    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

#ifdef HAVE_LIBOQS
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (!kem) {
        return KNISHIO_ERROR_CRYPTO;
    }

    /* Generate key pair */
    OQS_STATUS status = OQS_KEM_keypair(kem, keypair->public_key, keypair->private_key);
    
    OQS_KEM_free(kem);
    
    if (status != OQS_SUCCESS) {
        /* Clear sensitive data on failure */
        secure_zero(keypair->private_key, MLKEM768_PRIVATE_KEY_BYTES);
        return KNISHIO_ERROR_CRYPTO;
    }
    
    return KNISHIO_SUCCESS;
#else
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#endif
}

knishio_error_t knishio_mlkem768_keypair_from_seed(knishio_mlkem768_keypair_t *keypair, 
                                                   const uint8_t *seed, size_t seed_len) {
    if (!keypair || !seed) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (seed_len < 64) {  /* Minimum 64 bytes for security */
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    knishio_error_t ret = mlkem768_init();
    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

#ifdef HAVE_MLKEM_NATIVE
    /* mlkem-native implementation - perfect JavaScript compatibility */
    
    /* Validate seed length - mlkem-native expects 64 bytes (2*MLKEM_SYMBYTES) */
    if (seed_len != 64) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Call mlkem-native deterministic key generation */
    int result = crypto_kem_keypair_derand(
        keypair->public_key,
        keypair->private_key,
        seed  /* Use JavaScript 64-byte seed directly */
    );
    
    if (result != 0) {
        secure_zero(keypair->private_key, MLKEM768_PRIVATE_KEY_BYTES);
        return KNISHIO_ERROR_CRYPTO;
    }
    return KNISHIO_SUCCESS;

#elif defined(HAVE_LIBOQS)
    /* liboqs fallback implementation */
    printf("DEBUG ML-KEM768: Using liboqs fallback (limited compatibility)\n");
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#else
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#endif
}

knishio_error_t knishio_mlkem768_encapsulate(const uint8_t *public_key,
                                             knishio_mlkem768_ciphertext_t *ciphertext,
                                             knishio_mlkem768_shared_secret_t *shared_secret) {
    if (!public_key || !ciphertext || !shared_secret) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    knishio_error_t ret = mlkem768_init();
    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

#ifdef HAVE_MLKEM_NATIVE
    /* mlkem-native encapsulation implementation */
    int result = crypto_kem_enc(
        ciphertext->ciphertext,
        shared_secret->shared_secret,
        public_key
    );
    
    if (result != 0) {
        secure_zero(shared_secret->shared_secret, MLKEM768_SHARED_SECRET_BYTES);
        return KNISHIO_ERROR_CRYPTO;
    }
    
    return KNISHIO_SUCCESS;

#elif defined(HAVE_LIBOQS)
    /* liboqs fallback implementation */
    printf("DEBUG ML-KEM768: Using liboqs fallback for encapsulation\n");
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#else
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#endif
}

knishio_error_t knishio_mlkem768_decapsulate(const uint8_t *private_key,
                                             const knishio_mlkem768_ciphertext_t *ciphertext,
                                             knishio_mlkem768_shared_secret_t *shared_secret) {
    if (!private_key || !ciphertext || !shared_secret) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    knishio_error_t ret = mlkem768_init();
    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

#ifdef HAVE_MLKEM_NATIVE
    /* mlkem-native decapsulation implementation */
    int result = crypto_kem_dec(
        shared_secret->shared_secret,
        ciphertext->ciphertext,
        private_key
    );
    
    if (result != 0) {
        secure_zero(shared_secret->shared_secret, MLKEM768_SHARED_SECRET_BYTES);
        return KNISHIO_ERROR_CRYPTO;
    }
    
    return KNISHIO_SUCCESS;

#elif defined(HAVE_LIBOQS)
    /* liboqs fallback implementation */
    printf("DEBUG ML-KEM768: Using liboqs fallback for decapsulation\n");
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#else
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
#endif
}

/* Key serialization functions */

knishio_error_t knishio_mlkem768_public_key_to_hex(const uint8_t *public_key, char **hex_output) {
    if (!public_key || !hex_output) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    return bin_to_hex(public_key, MLKEM768_PUBLIC_KEY_BYTES, hex_output);
}

knishio_error_t knishio_mlkem768_private_key_to_hex(const uint8_t *private_key, char **hex_output) {
    if (!private_key || !hex_output) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    return bin_to_hex(private_key, MLKEM768_PRIVATE_KEY_BYTES, hex_output);
}

knishio_error_t knishio_mlkem768_public_key_from_hex(const char *hex_input, uint8_t *public_key) {
    if (!hex_input || !public_key) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    return hex_to_bin(hex_input, public_key, MLKEM768_PUBLIC_KEY_BYTES);
}

knishio_error_t knishio_mlkem768_private_key_from_hex(const char *hex_input, uint8_t *private_key) {
    if (!hex_input || !private_key) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    return hex_to_bin(hex_input, private_key, MLKEM768_PRIVATE_KEY_BYTES);
}

/* High-level encryption/decryption using hybrid approach */

knishio_error_t knishio_mlkem768_encrypt(const uint8_t *public_key, 
                                         const uint8_t *plaintext, size_t plaintext_len,
                                         uint8_t **ciphertext_out, size_t *ciphertext_len_out) {
    if (!public_key || !plaintext || !ciphertext_out || !ciphertext_len_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    knishio_mlkem768_ciphertext_t kem_ciphertext;
    knishio_mlkem768_shared_secret_t shared_secret;
    
    /* Step 1: Encapsulate to get shared secret */
    knishio_error_t ret = knishio_mlkem768_encapsulate(public_key, &kem_ciphertext, &shared_secret);
    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

    /* Step 2: Encrypt plaintext with AES-256-GCM using shared secret (JavaScript SDK compatible) */
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

    /* Step 3: Combine KEM ciphertext + encrypted message */
    size_t total_len = MLKEM768_CIPHERTEXT_BYTES + encrypted_message_len;
    uint8_t *result = malloc(total_len);
    if (!result) {
        free(encrypted_message);
        secure_zero(&shared_secret, sizeof(shared_secret));
        return KNISHIO_ERROR_MEMORY;
    }

    /* Copy KEM ciphertext */
    memcpy(result, kem_ciphertext.ciphertext, MLKEM768_CIPHERTEXT_BYTES);

    /* Copy encrypted message (IV + ciphertext + tag) */
    memcpy(result + MLKEM768_CIPHERTEXT_BYTES, encrypted_message, encrypted_message_len);

    /* Clear sensitive data */
    free(encrypted_message);
    secure_zero(&shared_secret, sizeof(shared_secret));

    *ciphertext_out = result;
    *ciphertext_len_out = total_len;

    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_mlkem768_decrypt(const uint8_t *private_key,
                                         const uint8_t *ciphertext, size_t ciphertext_len,
                                         uint8_t **plaintext_out, size_t *plaintext_len_out) {
    if (!private_key || !ciphertext || !plaintext_out || !plaintext_len_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (ciphertext_len < MLKEM768_CIPHERTEXT_BYTES) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Step 1: Extract KEM ciphertext */
    knishio_mlkem768_ciphertext_t kem_ciphertext;
    memcpy(kem_ciphertext.ciphertext, ciphertext, MLKEM768_CIPHERTEXT_BYTES);

    /* Step 2: Decapsulate to get shared secret */
    knishio_mlkem768_shared_secret_t shared_secret;
    knishio_error_t ret = knishio_mlkem768_decapsulate(private_key, &kem_ciphertext, &shared_secret);
    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

    /* Step 3: Decrypt message with AES-256-GCM using shared secret (JavaScript SDK compatible) */
    const uint8_t *encrypted_message = ciphertext + MLKEM768_CIPHERTEXT_BYTES;
    size_t encrypted_message_len = ciphertext_len - MLKEM768_CIPHERTEXT_BYTES;

    uint8_t *plaintext = NULL;
    size_t plaintext_len = 0;

    ret = knishio_aes_gcm_decrypt(
        encrypted_message, encrypted_message_len,
        shared_secret.shared_secret,
        &plaintext, &plaintext_len
    );

    /* Clear sensitive data */
    secure_zero(&shared_secret, sizeof(shared_secret));

    if (ret != KNISHIO_SUCCESS) {
        return ret;
    }

    *plaintext_out = plaintext;
    *plaintext_len_out = plaintext_len;

    return KNISHIO_SUCCESS;
}

/* Utility functions */

bool knishio_mlkem768_is_available(void) {
#ifdef HAVE_MLKEM_NATIVE
    return true;  /* mlkem-native is always available when compiled in */
#elif defined(HAVE_LIBOQS)
    return OQS_KEM_alg_is_enabled(OQS_KEM_alg_ml_kem_768);
#else
    return false;
#endif
}

const char* knishio_mlkem768_get_algorithm_name(void) {
    return g_mlkem_state.algorithm_name;
}

void knishio_mlkem768_cleanup(void) {
    g_mlkem_state.initialized = false;
#ifdef HAVE_MLKEM_NATIVE
    /* mlkem-native doesn't require explicit cleanup */
#elif defined(HAVE_LIBOQS)
    OQS_destroy();
#endif
}
