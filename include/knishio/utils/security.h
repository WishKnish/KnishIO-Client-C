#ifndef KNISHIO_UTILS_SECURITY_H
#define KNISHIO_UTILS_SECURITY_H

/**
 * @file security.h
 * @brief Security utilities for memory safety and input validation
 * @version 2025 Security Hardened
 * 
 * Provides comprehensive security utilities including:
 * - Safe string operations with bounds checking
 * - Integer overflow protection
 * - Secure memory handling
 * - Input validation helpers
 * - Timing attack protection
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Safe String Operations ===== */

/**
 * @brief Safe string copy with guaranteed null termination
 * @param dest Destination buffer
 * @param src Source string
 * @param size Size of destination buffer
 * @return true on success, false if truncation occurred
 */
static inline bool knishio_safe_strcpy(char *dest, const char *src, size_t size) {
    if (!dest || !src || size == 0) return false;
    
    size_t src_len = strlen(src);
    if (src_len >= size) {
        // Truncation will occur
        memcpy(dest, src, size - 1);
        dest[size - 1] = '\0';
        return false;
    }
    
    memcpy(dest, src, src_len + 1);
    return true;
}

/**
 * @brief Safe string concatenation with bounds checking
 * @param dest Destination buffer
 * @param src Source string to append
 * @param size Total size of destination buffer
 * @return true on success, false if truncation occurred
 */
static inline bool knishio_safe_strcat(char *dest, const char *src, size_t size) {
    if (!dest || !src || size == 0) return false;
    
    size_t dest_len = strlen(dest);
    if (dest_len >= size - 1) return false;
    
    size_t remaining = size - dest_len - 1;
    size_t src_len = strlen(src);
    
    if (src_len > remaining) {
        // Truncation will occur
        memcpy(dest + dest_len, src, remaining);
        dest[size - 1] = '\0';
        return false;
    }
    
    memcpy(dest + dest_len, src, src_len + 1);
    return true;
}

/**
 * @brief Safe formatted string writing
 * @param dest Destination buffer
 * @param size Size of destination buffer
 * @param format Format string
 * @param ... Format arguments
 * @return Number of characters written (excluding null terminator)
 */
#define knishio_safe_snprintf(dest, size, format, ...) \
    snprintf((dest), (size), (format), ##__VA_ARGS__)

/* ===== Integer Overflow Protection ===== */

/**
 * @brief Safe addition with overflow detection
 * @param a First operand
 * @param b Second operand
 * @param result Pointer to store result
 * @return true if safe, false if overflow would occur
 */
static inline bool knishio_safe_add_size_t(size_t a, size_t b, size_t *result) {
    if (a > SIZE_MAX - b) return false;
    *result = a + b;
    return true;
}

/**
 * @brief Safe multiplication with overflow detection
 * @param a First operand
 * @param b Second operand
 * @param result Pointer to store result
 * @return true if safe, false if overflow would occur
 */
static inline bool knishio_safe_mul_size_t(size_t a, size_t b, size_t *result) {
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }
    if (a > SIZE_MAX / b) return false;
    *result = a * b;
    return true;
}

/**
 * @brief Safe signed addition with overflow detection
 * @param a First operand
 * @param b Second operand
 * @param result Pointer to store result
 * @return true if safe, false if overflow would occur
 */
static inline bool knishio_safe_add_int(int a, int b, int *result) {
    if (b > 0 && a > INT_MAX - b) return false;
    if (b < 0 && a < INT_MIN - b) return false;
    *result = a + b;
    return true;
}

/* ===== Memory Safety ===== */

/**
 * @brief Safe memory allocation with overflow checking
 * @param count Number of items
 * @param size Size of each item
 * @return Allocated memory or NULL on failure
 */
void* knishio_safe_calloc(size_t count, size_t size);

/**
 * @brief Safe memory reallocation with overflow checking
 * @param ptr Original pointer
 * @param new_size New size in bytes
 * @return Reallocated memory or NULL on failure
 */
void* knishio_safe_realloc(void *ptr, size_t new_size);

/**
 * @brief Secure memory wiping (prevents compiler optimization)
 * @param ptr Memory to wipe
 * @param size Size in bytes
 */
void knishio_secure_wipe(void *ptr, size_t size);

/**
 * @brief Secure free - wipes memory before freeing
 * @param ptr Pointer to free
 * @param size Size of memory block
 */
void knishio_secure_free(void *ptr, size_t size);

/* ===== Input Validation ===== */

/**
 * @brief Validate hexadecimal string
 * @param hex String to validate
 * @param expected_len Expected length (0 for any length)
 * @return true if valid hex string
 */
bool knishio_validate_hex(const char *hex, size_t expected_len);

/**
 * @brief Validate base64 string
 * @param base64 String to validate
 * @return true if valid base64 string
 */
bool knishio_validate_base64(const char *base64);

/**
 * @brief Validate wallet address format
 * @param address Address to validate
 * @return true if valid address format
 */
bool knishio_validate_address(const char *address);

/**
 * @brief Validate position string format
 * @param position Position to validate
 * @return true if valid position format
 */
bool knishio_validate_position(const char *position);

/**
 * @brief Validate token slug format
 * @param token Token slug to validate
 * @return true if valid token format
 */
bool knishio_validate_token(const char *token);

/**
 * @brief Validate isotope character
 * @param isotope Isotope character
 * @return true if valid isotope
 */
bool knishio_validate_isotope(char isotope);

/**
 * @brief Sanitize string for safe usage
 * @param str String to sanitize
 * @param max_len Maximum allowed length
 * @return Sanitized string (caller must free)
 */
char* knishio_sanitize_string(const char *str, size_t max_len);

/* ===== Timing Attack Protection ===== */

/**
 * @brief Constant-time memory comparison
 * @param a First buffer
 * @param b Second buffer
 * @param size Number of bytes to compare
 * @return 0 if equal, non-zero if different
 */
int knishio_secure_compare(const void *a, const void *b, size_t size);

/**
 * @brief Constant-time string comparison
 * @param a First string
 * @param b Second string
 * @return 0 if equal, non-zero if different
 */
int knishio_secure_strcmp(const char *a, const char *b);

/* ===== Security Assertions ===== */

#ifdef KNISHIO_DEBUG
    #define KNISHIO_SECURITY_ASSERT(cond, msg) do { \
        if (!(cond)) { \
            fprintf(stderr, "SECURITY ASSERTION FAILED: %s\n", (msg)); \
            fprintf(stderr, "  at %s:%d in %s\n", __FILE__, __LINE__, __func__); \
            abort(); \
        } \
    } while(0)
#else
    #define KNISHIO_SECURITY_ASSERT(cond, msg) ((void)0)
#endif

/**
 * @brief Check if pointer is valid (non-NULL)
 */
#define KNISHIO_CHECK_PTR(ptr) do { \
    if (!(ptr)) { \
        return KNISHIO_ERROR_NULL_POINTER; \
    } \
} while(0)

/**
 * @brief Check if size is within bounds
 */
#define KNISHIO_CHECK_SIZE(size, max) do { \
    if ((size) > (max)) { \
        return KNISHIO_ERROR_INVALID_ARGS; \
    } \
} while(0)

/**
 * @brief Safe array access with bounds checking
 */
#define KNISHIO_SAFE_ARRAY_ACCESS(arr, index, size) \
    (((index) < (size)) ? (arr)[(index)] : (abort(), (arr)[0]))

/* ===== Secure Random Number Generation ===== */

/**
 * @brief Generate cryptographically secure random bytes
 * @param buffer Buffer to fill with random data
 * @param size Number of bytes to generate
 * @return true on success, false on failure
 */
bool knishio_secure_random(void *buffer, size_t size);

/**
 * @brief Generate secure random integer in range [min, max]
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @param result Pointer to store result
 * @return true on success, false on failure
 */
bool knishio_secure_random_range(uint64_t min, uint64_t max, uint64_t *result);

/* ===== Memory Limits ===== */

#define KNISHIO_MAX_STRING_LENGTH   (1024 * 1024)     // 1MB max string
#define KNISHIO_MAX_HEX_LENGTH      (256 * 1024)      // 256KB max hex
#define KNISHIO_MAX_ADDRESS_LENGTH  256               // Max address length
#define KNISHIO_MAX_TOKEN_LENGTH    64                // Max token slug length
#define KNISHIO_MAX_META_KEY_LENGTH 256               // Max meta key length
#define KNISHIO_MAX_META_VAL_LENGTH (64 * 1024)       // Max meta value length
#define KNISHIO_MAX_ATOMS_PER_MOLECULE 1000           // Max atoms in molecule
#define KNISHIO_MAX_ALLOCATIONS     10000             // Max concurrent allocations

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_UTILS_SECURITY_H */
