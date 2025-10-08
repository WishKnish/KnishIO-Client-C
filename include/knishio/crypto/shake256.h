#ifndef KNISHIO_CRYPTO_SHAKE256_H
#define KNISHIO_CRYPTO_SHAKE256_H

/**
 * @file shake256.h
 * @brief SHAKE256 cryptographic hash implementation for KnishIO SDK
 * 
 * Provides SHAKE256 hashing functionality with exact compatibility to the
 * JavaScript SDK implementation using JsSHA library. All operations must
 * produce identical outputs for cross-SDK compatibility.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_shake256_ctx knishio_shake256_ctx_t;

/**
 * @brief SHAKE256 context structure
 */
struct knishio_shake256_ctx {
    uint64_t state[25];         /**< Keccak state array */
    uint8_t buffer[200];        /**< Input buffer */
    size_t buffer_len;          /**< Current buffer length */
    size_t rate;                /**< Rate (136 bytes for SHAKE256) */
    size_t squeeze_offset;      /**< Current squeeze position in state */
    bool finalized;             /**< True if absorb phase is finished */
    bool initialized;           /**< True if context is initialized */
};

/* High-level SHAKE256 functions */

/**
 * @brief Compute SHAKE256 hash from string input
 * @param input Input string (null-terminated)
 * @param output_bits Number of output bits
 * @param hex_output Hex-encoded output string (allocated, must be freed)
 * @return True on success, false on failure
 */
bool knishio_shake256_hash(const char *input, size_t output_bits, char **hex_output);

/**
 * @brief Compute SHAKE256 hash from binary input
 * @param input Input data
 * @param input_len Input data length
 * @param output_bits Number of output bits
 * @param hex_output Hex-encoded output string (allocated, must be freed)
 * @return True on success, false on failure
 */
bool knishio_shake256_hash_binary(const uint8_t *input, size_t input_len, 
                                 size_t output_bits, char **hex_output);

/**
 * @brief Compute SHAKE256 hash with binary output
 * @param input Input data
 * @param input_len Input data length
 * @param output Output buffer
 * @param output_len Output buffer length (in bytes)
 * @return True on success, false on failure
 */
bool knishio_shake256_hash_raw(const uint8_t *input, size_t input_len,
                              uint8_t *output, size_t output_len);

/* Low-level SHAKE256 context functions */

/**
 * @brief Initialize SHAKE256 context
 * @param ctx Context to initialize
 * @return True on success, false on failure
 */
bool knishio_shake256_init(knishio_shake256_ctx_t *ctx);

/**
 * @brief Update SHAKE256 context with input data (absorb phase)
 * @param ctx SHAKE256 context
 * @param data Input data
 * @param len Input data length
 * @return True on success, false on failure
 */
bool knishio_shake256_update(knishio_shake256_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief Update SHAKE256 context with string input
 * @param ctx SHAKE256 context
 * @param str Input string (null-terminated)
 * @return True on success, false on failure
 */
bool knishio_shake256_update_string(knishio_shake256_ctx_t *ctx, const char *str);

/**
 * @brief Finalize absorb phase and prepare for squeeze phase
 * @param ctx SHAKE256 context
 * @return True on success, false on failure
 */
bool knishio_shake256_final(knishio_shake256_ctx_t *ctx);

/**
 * @brief Squeeze output from SHAKE256 context
 * @param ctx SHAKE256 context (must be finalized)
 * @param output Output buffer
 * @param output_len Number of bytes to squeeze
 * @return True on success, false on failure
 */
bool knishio_shake256_squeeze(knishio_shake256_ctx_t *ctx, uint8_t *output, size_t output_len);

/**
 * @brief Squeeze hex-encoded output from SHAKE256 context
 * @param ctx SHAKE256 context (must be finalized)
 * @param output_bits Number of output bits
 * @param hex_output Hex-encoded output string (allocated, must be freed)
 * @return True on success, false on failure
 */
bool knishio_shake256_squeeze_hex(knishio_shake256_ctx_t *ctx, size_t output_bits, char **hex_output);

/**
 * @brief Reset SHAKE256 context for reuse
 * @param ctx SHAKE256 context
 */
void knishio_shake256_reset(knishio_shake256_ctx_t *ctx);

/**
 * @brief Clean up SHAKE256 context (secure zero)
 * @param ctx SHAKE256 context
 */
void knishio_shake256_cleanup(knishio_shake256_ctx_t *ctx);

/* Utility functions */

/**
 * @brief Convert binary data to hex string
 * @param data Binary data
 * @param data_len Data length
 * @param hex_str Hex string output (allocated, must be freed)
 * @return True on success, false on failure
 */
bool knishio_binary_to_hex(const uint8_t *data, size_t data_len, char **hex_str);

/**
 * @brief Convert hex string to binary data
 * @param hex_str Hex string (null-terminated)
 * @param data Binary data output (allocated, must be freed)
 * @param data_len Data length output
 * @return True on success, false on failure
 */
bool knishio_hex_to_binary(const char *hex_str, uint8_t **data, size_t *data_len);

/**
 * @brief Validate hex string format
 * @param hex_str Hex string to validate
 * @return True if valid hex, false otherwise
 */
bool knishio_is_valid_hex(const char *hex_str);

/* Test vector validation functions */

/**
 * @brief Test SHAKE256 implementation against known test vectors
 * @return True if all tests pass, false if any fail
 */
bool knishio_shake256_self_test(void);

/**
 * @brief Validate SHAKE256 output against specific test vector
 * @param input Test input string
 * @param output_bits Expected output bits
 * @param expected_hex Expected hex output
 * @return True if output matches expected, false otherwise
 */
bool knishio_shake256_test_vector(const char *input, size_t output_bits, const char *expected_hex);

/* JavaScript SDK compatibility functions */

/**
 * @brief SHAKE256 hash exactly matching JavaScript JsSHA implementation
 * @param input Input string (same as JS)
 * @param output_length Output length in bits (same as JS outputLen parameter)
 * @param hex_output Hex string output (must be freed)
 * @return True on success, false on failure
 */
bool knishio_shake256_js_compatible(const char *input, size_t output_length, char **hex_output);

/**
 * @brief Verify this implementation produces same output as JavaScript SDK
 * @param js_test_vectors Array of test vectors from JS SDK
 * @param vector_count Number of test vectors
 * @return True if all vectors match, false if any differ
 */
bool knishio_shake256_verify_js_compatibility(const char **js_test_vectors, size_t vector_count);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CRYPTO_SHAKE256_H */
