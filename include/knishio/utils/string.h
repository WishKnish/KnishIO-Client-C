#ifndef KNISHIO_UTILS_STRING_H
#define KNISHIO_UTILS_STRING_H

/**
 * @file string.h
 * @brief Dynamic string utilities for KnishIO SDK
 * 
 * Provides JavaScript-like string manipulation with automatic memory management,
 * UTF-8 support, and safe operations that prevent buffer overflows.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dynamic string structure
 */
typedef struct knishio_string {
    char *data;                 /**< String data (null-terminated) */
    size_t length;              /**< String length (excluding null terminator) */
    size_t capacity;            /**< Allocated capacity */
    bool is_static;             /**< True if data points to static memory */
} knishio_string_t;

/* String creation and destruction */

/**
 * @brief Create a new empty string
 * @return String object or NULL on failure
 */
knishio_string_t* knishio_string_create(void);

/**
 * @brief Create a string with initial content
 * @param initial Initial string content (copied)
 * @return String object or NULL on failure
 */
knishio_string_t* knishio_string_create_with_cstr(const char *initial);

/**
 * @brief Create a string with initial content and length
 * @param initial Initial string content (copied)
 * @param length Length of initial content
 * @return String object or NULL on failure
 */
knishio_string_t* knishio_string_create_with_length(const char *initial, size_t length);

/**
 * @brief Create a string with pre-allocated capacity
 * @param capacity Initial capacity
 * @return String object or NULL on failure
 */
knishio_string_t* knishio_string_create_with_capacity(size_t capacity);

/**
 * @brief Create a string wrapping static memory (no copy)
 * @param static_str Static string (must remain valid)
 * @return String object or NULL on failure
 */
knishio_string_t* knishio_string_create_static(const char *static_str);

/**
 * @brief Clone a string
 * @param str String to clone
 * @return Cloned string or NULL on failure
 */
knishio_string_t* knishio_string_clone(const knishio_string_t *str);

/**
 * @brief Destroy a string
 * @param str String to destroy
 */
void knishio_string_destroy(knishio_string_t *str);

/* String properties */

/**
 * @brief Get string length
 * @param str String object
 * @return String length
 */
size_t knishio_string_length(const knishio_string_t *str);

/**
 * @brief Get string capacity
 * @param str String object
 * @return String capacity
 */
size_t knishio_string_capacity(const knishio_string_t *str);

/**
 * @brief Check if string is empty
 * @param str String object
 * @return True if empty, false otherwise
 */
bool knishio_string_is_empty(const knishio_string_t *str);

/**
 * @brief Get C string representation
 * @param str String object
 * @return C string (null-terminated) or NULL if invalid
 */
const char* knishio_string_cstr(const knishio_string_t *str);

/* String modification */

/**
 * @brief Set string content
 * @param str String object
 * @param content New content (copied)
 * @return True on success, false on failure
 */
bool knishio_string_set(knishio_string_t *str, const char *content);

/**
 * @brief Set string content with length
 * @param str String object
 * @param content New content (copied)
 * @param length Length of content
 * @return True on success, false on failure
 */
bool knishio_string_set_length(knishio_string_t *str, const char *content, size_t length);

/**
 * @brief Append content to string
 * @param str String object
 * @param content Content to append
 * @return True on success, false on failure
 */
bool knishio_string_append(knishio_string_t *str, const char *content);

/**
 * @brief Append content with length to string
 * @param str String object
 * @param content Content to append
 * @param length Length of content
 * @return True on success, false on failure
 */
bool knishio_string_append_length(knishio_string_t *str, const char *content, size_t length);

/**
 * @brief Append character to string
 * @param str String object
 * @param ch Character to append
 * @return True on success, false on failure
 */
bool knishio_string_append_char(knishio_string_t *str, char ch);

/**
 * @brief Append another string
 * @param str String object
 * @param other String to append
 * @return True on success, false on failure
 */
bool knishio_string_append_string(knishio_string_t *str, const knishio_string_t *other);

/**
 * @brief Prepend content to string
 * @param str String object
 * @param content Content to prepend
 * @return True on success, false on failure
 */
bool knishio_string_prepend(knishio_string_t *str, const char *content);

/**
 * @brief Insert content at position
 * @param str String object
 * @param pos Position to insert at
 * @param content Content to insert
 * @return True on success, false on failure
 */
bool knishio_string_insert(knishio_string_t *str, size_t pos, const char *content);

/**
 * @brief Clear string content
 * @param str String object
 */
void knishio_string_clear(knishio_string_t *str);

/**
 * @brief Reserve capacity
 * @param str String object
 * @param capacity Minimum capacity to reserve
 * @return True on success, false on failure
 */
bool knishio_string_reserve(knishio_string_t *str, size_t capacity);

/**
 * @brief Shrink capacity to fit content
 * @param str String object
 * @return True on success, false on failure
 */
bool knishio_string_shrink_to_fit(knishio_string_t *str);

/* String manipulation */

/**
 * @brief Get substring
 * @param str String object
 * @param start Start position
 * @param length Length of substring (or -1 for rest of string)
 * @return New string with substring or NULL on failure
 */
knishio_string_t* knishio_string_substr(const knishio_string_t *str, size_t start, size_t length);

/**
 * @brief Find substring
 * @param str String object
 * @param needle Substring to find
 * @param start Start position for search
 * @return Position of substring or SIZE_MAX if not found
 */
size_t knishio_string_find(const knishio_string_t *str, const char *needle, size_t start);

/**
 * @brief Find last occurrence of substring
 * @param str String object
 * @param needle Substring to find
 * @return Position of last occurrence or SIZE_MAX if not found
 */
size_t knishio_string_rfind(const knishio_string_t *str, const char *needle);

/**
 * @brief Replace all occurrences of substring
 * @param str String object
 * @param search String to search for
 * @param replace String to replace with
 * @return Number of replacements made
 */
size_t knishio_string_replace(knishio_string_t *str, const char *search, const char *replace);

/**
 * @brief Split string by delimiter
 * @param str String object
 * @param delimiter Delimiter string
 * @param parts Array of resulting parts (output)
 * @param count Number of parts (output)
 * @return True on success, false on failure
 */
bool knishio_string_split(const knishio_string_t *str, const char *delimiter, 
                         knishio_string_t ***parts, size_t *count);

/**
 * @brief Join array of strings with delimiter
 * @param parts Array of strings to join
 * @param count Number of strings
 * @param delimiter Delimiter string
 * @return New joined string or NULL on failure
 */
knishio_string_t* knishio_string_join(knishio_string_t **parts, size_t count, const char *delimiter);

/**
 * @brief Trim whitespace from beginning and end
 * @param str String object
 */
void knishio_string_trim(knishio_string_t *str);

/**
 * @brief Trim whitespace from beginning
 * @param str String object
 */
void knishio_string_ltrim(knishio_string_t *str);

/**
 * @brief Trim whitespace from end
 * @param str String object
 */
void knishio_string_rtrim(knishio_string_t *str);

/**
 * @brief Convert to lowercase
 * @param str String object
 */
void knishio_string_to_lower(knishio_string_t *str);

/**
 * @brief Convert to uppercase
 * @param str String object
 */
void knishio_string_to_upper(knishio_string_t *str);

/* String comparison */

/**
 * @brief Compare two strings
 * @param str1 First string
 * @param str2 Second string
 * @return 0 if equal, <0 if str1 < str2, >0 if str1 > str2
 */
int knishio_string_compare(const knishio_string_t *str1, const knishio_string_t *str2);

/**
 * @brief Compare string with C string
 * @param str String object
 * @param cstr C string
 * @return 0 if equal, <0 if str < cstr, >0 if str > cstr
 */
int knishio_string_compare_cstr(const knishio_string_t *str, const char *cstr);

/**
 * @brief Case-insensitive comparison
 * @param str1 First string
 * @param str2 Second string
 * @return 0 if equal, <0 if str1 < str2, >0 if str1 > str2
 */
int knishio_string_compare_nocase(const knishio_string_t *str1, const knishio_string_t *str2);

/**
 * @brief Check if strings are equal
 * @param str1 First string
 * @param str2 Second string
 * @return True if equal, false otherwise
 */
bool knishio_string_equals(const knishio_string_t *str1, const knishio_string_t *str2);

/**
 * @brief Check if string equals C string
 * @param str String object
 * @param cstr C string
 * @return True if equal, false otherwise
 */
bool knishio_string_equals_cstr(const knishio_string_t *str, const char *cstr);

/**
 * @brief Check if string starts with prefix
 * @param str String object
 * @param prefix Prefix to check
 * @return True if starts with prefix, false otherwise
 */
bool knishio_string_starts_with(const knishio_string_t *str, const char *prefix);

/**
 * @brief Check if string ends with suffix
 * @param str String object
 * @param suffix Suffix to check
 * @return True if ends with suffix, false otherwise
 */
bool knishio_string_ends_with(const knishio_string_t *str, const char *suffix);

/* String formatting */

/**
 * @brief Format string using printf-style format
 * @param format Format string
 * @param ... Arguments
 * @return New formatted string or NULL on failure
 */
knishio_string_t* knishio_string_sprintf(const char *format, ...);

/**
 * @brief Append formatted content to string
 * @param str String object
 * @param format Format string
 * @param ... Arguments
 * @return True on success, false on failure
 */
bool knishio_string_append_sprintf(knishio_string_t *str, const char *format, ...);

/* Utility functions for common operations */

/**
 * @brief Check if string contains only hexadecimal characters
 * @param str String object
 * @return True if hex, false otherwise
 */
bool knishio_string_is_hex(const knishio_string_t *str);

/**
 * @brief Check if string contains only numeric characters
 * @param str String object
 * @return True if numeric, false otherwise
 */
bool knishio_string_is_numeric(const knishio_string_t *str);

/**
 * @brief Check if string is valid Base64
 * @param str String object
 * @return True if valid Base64, false otherwise
 */
bool knishio_string_is_base64(const knishio_string_t *str);

/**
 * @brief Chunk string into smaller pieces
 * @param str String object
 * @param chunk_size Size of each chunk
 * @param chunks Array of chunk strings (output)
 * @param count Number of chunks (output)
 * @return True on success, false on failure
 */
bool knishio_string_chunk(const knishio_string_t *str, size_t chunk_size,
                         knishio_string_t ***chunks, size_t *count);

/**
 * @brief Free array of strings (from split or chunk operations)
 * @param strings Array of strings
 * @param count Number of strings
 */
void knishio_string_array_free(knishio_string_t **strings, size_t count);

/* Additional utility functions for ContinuID support */

/**
 * @brief String duplication (malloc-based copy)
 * @param str Source string
 * @return Allocated copy or NULL on failure
 */
char* knishio_strdup(const char *str);

/**
 * @brief Check if C string contains only hexadecimal characters
 * @param str C string to check
 * @return True if hex, false otherwise
 */
bool knishio_is_hex_string(const char *str);

/**
 * @brief Secure zero memory (prevents compiler optimization)
 * @param ptr Memory to zero
 * @param size Size of memory
 */
void knishio_secure_zero(void *ptr, size_t size);

/**
 * @brief Chunk C string into array of strings
 * @param input Input string
 * @param chunk_size Size of each chunk
 * @param chunks Output array of strings (allocated)
 * @param chunk_count Output number of chunks
 * @return True on success, false on failure
 */
bool knishio_chunk_string(const char *input, size_t chunk_size, 
                         char ***chunks, size_t *chunk_count);

/**
 * @brief Generate random hex string
 * @param length Length of string
 * @param charset Character set (optional, defaults to hex)
 * @param output Output string (allocated)
 * @return True on success, false on failure
 */
bool knishio_random_hex_string(size_t length, const char *charset, char **output);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_UTILS_STRING_H */
