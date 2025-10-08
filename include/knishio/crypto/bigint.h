#ifndef KNISHIO_CRYPTO_BIGINT_H
#define KNISHIO_CRYPTO_BIGINT_H

/**
 * @file bigint.h
 * @brief GMP-based BigInt arithmetic for KnishIO SDK
 * 
 * Provides arbitrary-precision integer arithmetic using GNU Multiple Precision
 * Arithmetic Library (GMP). Essential for wallet key derivation compatibility
 * with JavaScript BigInt, Kotlin BigInteger, and PHP BigInteger operations.
 * 
 * Critical operations:
 * - Hexadecimal string to/from BigInt conversion
 * - Large integer addition (8192+ bit numbers)
 * - Base conversion for molecular hashing
 * - Cross-platform deterministic output
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <gmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_bigint knishio_bigint_t;

/**
 * @brief KnishIO BigInt structure wrapping GMP mpz_t
 */
struct knishio_bigint {
    mpz_t value;            /**< GMP arbitrary precision integer */
    bool initialized;       /**< Initialization status */
};

/* Core BigInt operations */

/**
 * @brief Initialize a new BigInt
 * @param bigint BigInt to initialize
 * @return True on success, false on failure
 */
bool knishio_bigint_init(knishio_bigint_t *bigint);

/**
 * @brief Initialize BigInt from hexadecimal string
 * @param bigint BigInt to initialize
 * @param hex_str Hexadecimal string (without 0x prefix)
 * @return True on success, false on failure
 */
bool knishio_bigint_init_hex(knishio_bigint_t *bigint, const char *hex_str);

/**
 * @brief Initialize BigInt from decimal string
 * @param bigint BigInt to initialize  
 * @param dec_str Decimal string
 * @return True on success, false on failure
 */
bool knishio_bigint_init_dec(knishio_bigint_t *bigint, const char *dec_str);

/**
 * @brief Initialize BigInt from unsigned long
 * @param bigint BigInt to initialize
 * @param value Initial value
 * @return True on success, false on failure
 */
bool knishio_bigint_init_ui(knishio_bigint_t *bigint, unsigned long value);

/**
 * @brief Copy BigInt value
 * @param dest Destination BigInt (will be initialized)
 * @param src Source BigInt
 * @return True on success, false on failure
 */
bool knishio_bigint_copy(knishio_bigint_t *dest, const knishio_bigint_t *src);

/**
 * @brief Clean up BigInt resources
 * @param bigint BigInt to clean up
 */
void knishio_bigint_cleanup(knishio_bigint_t *bigint);

/* Arithmetic operations */

/**
 * @brief Add two BigInts (result = a + b)
 * @param result Result BigInt (must be initialized)
 * @param a First operand
 * @param b Second operand
 * @return True on success, false on failure
 */
bool knishio_bigint_add(knishio_bigint_t *result, const knishio_bigint_t *a, const knishio_bigint_t *b);

/**
 * @brief Add unsigned long to BigInt (result = a + b)
 * @param result Result BigInt (must be initialized)
 * @param a BigInt operand
 * @param b Unsigned long operand
 * @return True on success, false on failure
 */
bool knishio_bigint_add_ui(knishio_bigint_t *result, const knishio_bigint_t *a, unsigned long b);

/**
 * @brief Multiply two BigInts (result = a * b)
 * @param result Result BigInt (must be initialized)
 * @param a First operand
 * @param b Second operand
 * @return True on success, false on failure
 */
bool knishio_bigint_mul(knishio_bigint_t *result, const knishio_bigint_t *a, const knishio_bigint_t *b);

/**
 * @brief Multiply BigInt by unsigned long (result = a * b)
 * @param result Result BigInt (must be initialized)
 * @param a BigInt operand
 * @param b Unsigned long operand
 * @return True on success, false on failure
 */
bool knishio_bigint_mul_ui(knishio_bigint_t *result, const knishio_bigint_t *a, unsigned long b);

/**
 * @brief Divide BigInt by unsigned long (result = a / b, remainder = a % b)
 * @param quotient Quotient result (must be initialized, can be NULL)
 * @param remainder Remainder result (must be initialized, can be NULL)
 * @param dividend Dividend BigInt
 * @param divisor Divisor (unsigned long)
 * @return Remainder value, or ULONG_MAX on error
 */
unsigned long knishio_bigint_divmod_ui(knishio_bigint_t *quotient, knishio_bigint_t *remainder, 
                                      const knishio_bigint_t *dividend, unsigned long divisor);

/**
 * @brief Get remainder of BigInt divided by unsigned long (a % b)
 * @param a BigInt operand
 * @param b Divisor
 * @return Remainder value, or ULONG_MAX on error
 */
unsigned long knishio_bigint_mod_ui(const knishio_bigint_t *a, unsigned long b);

/* Comparison operations */

/**
 * @brief Compare two BigInts
 * @param a First BigInt
 * @param b Second BigInt
 * @return Negative if a < b, 0 if a == b, positive if a > b
 */
int knishio_bigint_cmp(const knishio_bigint_t *a, const knishio_bigint_t *b);

/**
 * @brief Compare BigInt with unsigned long
 * @param a BigInt operand
 * @param b Unsigned long operand
 * @return Negative if a < b, 0 if a == b, positive if a > b
 */
int knishio_bigint_cmp_ui(const knishio_bigint_t *a, unsigned long b);

/**
 * @brief Check if BigInt is zero
 * @param a BigInt to check
 * @return True if zero, false otherwise
 */
bool knishio_bigint_is_zero(const knishio_bigint_t *a);

/* String conversion operations */

/**
 * @brief Convert BigInt to hexadecimal string
 * @param bigint BigInt to convert
 * @param hex_str Output hex string (allocated, must be freed)
 * @return True on success, false on failure
 */
bool knishio_bigint_to_hex(const knishio_bigint_t *bigint, char **hex_str);

/**
 * @brief Convert BigInt to decimal string
 * @param bigint BigInt to convert
 * @param dec_str Output decimal string (allocated, must be freed)
 * @return True on success, false on failure
 */
bool knishio_bigint_to_dec(const knishio_bigint_t *bigint, char **dec_str);

/**
 * @brief Set BigInt from hexadecimal string
 * @param bigint BigInt to set (must be initialized)
 * @param hex_str Hexadecimal string (without 0x prefix)
 * @return True on success, false on failure
 */
bool knishio_bigint_set_hex(knishio_bigint_t *bigint, const char *hex_str);

/**
 * @brief Set BigInt from decimal string
 * @param bigint BigInt to set (must be initialized)
 * @param dec_str Decimal string
 * @return True on success, false on failure
 */
bool knishio_bigint_set_dec(knishio_bigint_t *bigint, const char *dec_str);

/**
 * @brief Set BigInt from unsigned long
 * @param bigint BigInt to set (must be initialized)
 * @param value Value to set
 * @return True on success, false on failure
 */
bool knishio_bigint_set_ui(knishio_bigint_t *bigint, unsigned long value);

/* High-level wallet operations */

/**
 * @brief Generate wallet key from secret and position (JavaScript compatible)
 * @param secret_hex Secret as hexadecimal string (typically 2048 chars)
 * @param position_hex Position as hexadecimal string (typically 64 chars)
 * @param key_hex Output key as hexadecimal string (allocated, must be freed)
 * @return True on success, false on failure
 */
bool knishio_wallet_key_from_hex(const char *secret_hex, const char *position_hex, char **key_hex);

/* Base conversion for molecular hashing */

/**
 * @brief Convert string from one base to another using BigInt arithmetic
 * @param input Input string
 * @param from_base Source base (2-36)
 * @param to_base Destination base (2-36)
 * @param from_symbols Symbol table for source base
 * @param to_symbols Symbol table for destination base
 * @param output Output string (allocated, must be freed)
 * @return True on success, false on failure
 */
bool knishio_bigint_base_convert(const char *input, int from_base, int to_base,
                                const char *from_symbols, const char *to_symbols, 
                                char **output);

/* Test and validation functions */

/**
 * @brief Test BigInt operations against known test vectors
 * @return True if all tests pass, false if any fail
 */
bool knishio_bigint_self_test(void);

/**
 * @brief Validate BigInt operation against specific test vector
 * @param secret_hex Test secret
 * @param position_hex Test position
 * @param expected_hex Expected result
 * @return True if result matches expected, false otherwise
 */
bool knishio_bigint_test_vector(const char *secret_hex, const char *position_hex, const char *expected_hex);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CRYPTO_BIGINT_H */
