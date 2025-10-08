/**
 * @file security.c
 * @brief Security utilities implementation
 * @version 2025 Security Hardened
 */

#include "knishio/utils/security.h"
#include "knishio/utils/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/random.h>
#endif

/* ===== Memory Safety Implementation ===== */

void* knishio_safe_calloc(size_t count, size_t size) {
    size_t total_size;
    
    // Check for overflow
    if (!knishio_safe_mul_size_t(count, size, &total_size)) {
        return NULL;
    }
    
    // Check against maximum allocation size
    if (total_size > KNISHIO_MAX_STRING_LENGTH * 10) {
        return NULL;
    }
    
    return calloc(count, size);
}

void* knishio_safe_realloc(void *ptr, size_t new_size) {
    // Check against maximum allocation size
    if (new_size > KNISHIO_MAX_STRING_LENGTH * 10) {
        return NULL;
    }
    
    return realloc(ptr, new_size);
}

void knishio_secure_wipe(void *ptr, size_t size) {
    if (!ptr || size == 0) return;
    
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (size--) {
        *p++ = 0;
    }
    
    // Prevent compiler optimization
#ifdef _WIN32
    SecureZeroMemory(ptr, size);
#elif defined(__GNUC__)
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}

void knishio_secure_free(void *ptr, size_t size) {
    if (!ptr) return;
    
    knishio_secure_wipe(ptr, size);
    free(ptr);
}

/* ===== Input Validation Implementation ===== */

bool knishio_validate_hex(const char *hex, size_t expected_len) {
    if (!hex) return false;
    
    size_t len = strlen(hex);
    if (len > KNISHIO_MAX_HEX_LENGTH) return false;
    if (expected_len > 0 && len != expected_len) return false;
    
    for (size_t i = 0; i < len; i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            return false;
        }
    }
    
    return true;
}

bool knishio_validate_base64(const char *base64) {
    if (!base64) return false;
    
    size_t len = strlen(base64);
    if (len > KNISHIO_MAX_STRING_LENGTH) return false;
    
    // Simple base64 validation
    for (size_t i = 0; i < len; i++) {
        char c = base64[i];
        if (!isalnum((unsigned char)c) && c != '+' && c != '/' && c != '=') {
            return false;
        }
    }
    
    // Check padding
    size_t padding = 0;
    for (size_t i = len; i > 0 && base64[i-1] == '='; i--) {
        padding++;
        if (padding > 2) return false;
    }
    
    return true;
}

bool knishio_validate_address(const char *address) {
    if (!address) return false;
    
    size_t len = strlen(address);
    if (len == 0 || len > KNISHIO_MAX_ADDRESS_LENGTH) return false;
    
    // KnishIO addresses are hexadecimal strings of specific lengths
    // Common lengths: 64 characters (256 bits)
    if (len != 64 && len != 128) return false;
    
    return knishio_validate_hex(address, len);
}

bool knishio_validate_position(const char *position) {
    if (!position) return false;
    
    size_t len = strlen(position);
    if (len == 0 || len > 64) return false;
    
    // Positions are typically 64-character hex strings
    return knishio_validate_hex(position, 0);
}

bool knishio_validate_token(const char *token) {
    if (!token) return false;
    
    size_t len = strlen(token);
    if (len == 0 || len > KNISHIO_MAX_TOKEN_LENGTH) return false;
    
    // Token slugs are alphanumeric with possible underscores
    for (size_t i = 0; i < len; i++) {
        char c = token[i];
        if (!isalnum((unsigned char)c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    return true;
}

bool knishio_validate_isotope(char isotope) {
    // Valid isotopes from SDK Implementation Guide
    const char *valid_isotopes = "VBMCFRPITUA";
    return strchr(valid_isotopes, isotope) != NULL;
}

char* knishio_sanitize_string(const char *str, size_t max_len) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    if (len > max_len) len = max_len;
    
    char *sanitized = knishio_malloc(len + 1);
    if (!sanitized) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len && str[i]; i++) {
        unsigned char c = (unsigned char)str[i];
        // Allow printable characters and common whitespace
        if (isprint(c) || c == '\n' || c == '\r' || c == '\t') {
            sanitized[j++] = str[i];
        }
    }
    sanitized[j] = '\0';
    
    return sanitized;
}

/* ===== Timing Attack Protection Implementation ===== */

int knishio_secure_compare(const void *a, const void *b, size_t size) {
    if (!a || !b) return -1;
    if (size == 0) return 0;
    
    const volatile unsigned char *pa = (const volatile unsigned char *)a;
    const volatile unsigned char *pb = (const volatile unsigned char *)b;
    volatile unsigned char result = 0;
    
    for (size_t i = 0; i < size; i++) {
        result |= pa[i] ^ pb[i];
    }
    
    return result;
}

int knishio_secure_strcmp(const char *a, const char *b) {
    if (!a || !b) return -1;
    
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    
    // Compare lengths in constant time
    volatile size_t len_diff = len_a ^ len_b;
    
    // Use the longer length for comparison
    size_t max_len = (len_a > len_b) ? len_a : len_b;
    
    const volatile unsigned char *pa = (const volatile unsigned char *)a;
    const volatile unsigned char *pb = (const volatile unsigned char *)b;
    volatile unsigned char result = 0;
    
    for (size_t i = 0; i < max_len; i++) {
        unsigned char ca = (i < len_a) ? pa[i] : 0;
        unsigned char cb = (i < len_b) ? pb[i] : 0;
        result |= ca ^ cb;
    }
    
    return result | len_diff;
}

/* ===== Secure Random Number Generation Implementation ===== */

bool knishio_secure_random(void *buffer, size_t size) {
    if (!buffer || size == 0) return false;
    
#ifdef _WIN32
    // Windows: Use BCryptGenRandom
    NTSTATUS status = BCryptGenRandom(NULL, (PUCHAR)buffer, (ULONG)size, 
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return NT_SUCCESS(status);
    
#elif defined(__linux__) || defined(__APPLE__)
    // Linux/macOS: Try getrandom first, fall back to /dev/urandom
    #ifdef __linux__
        // Linux has getrandom() system call
        ssize_t ret = getrandom(buffer, size, 0);
        if (ret == (ssize_t)size) {
            return true;
        }
    #endif
    
    // Fall back to /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return false;
    
    ssize_t bytes_read = read(fd, buffer, size);
    close(fd);
    
    return bytes_read == (ssize_t)size;
    
#else
    // Unsupported platform - use less secure method as fallback
    #warning "Secure random not available on this platform, using less secure fallback"
    
    static int initialized = 0;
    if (!initialized) {
        srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
        initialized = 1;
    }
    
    unsigned char *p = (unsigned char *)buffer;
    for (size_t i = 0; i < size; i++) {
        p[i] = (unsigned char)(rand() & 0xFF);
    }
    return true;
#endif
}

bool knishio_secure_random_range(uint64_t min, uint64_t max, uint64_t *result) {
    if (!result || max < min) return false;
    
    uint64_t range = max - min;
    if (range == 0) {
        *result = min;
        return true;
    }
    
    // Find the number of bits needed
    uint64_t mask = 0;
    uint64_t tmp = range;
    while (tmp) {
        mask = (mask << 1) | 1;
        tmp >>= 1;
    }
    
    // Generate random values until we get one in range
    uint64_t value;
    do {
        if (!knishio_secure_random(&value, sizeof(value))) {
            return false;
        }
        value &= mask;
    } while (value > range);
    
    *result = min + value;
    return true;
}
