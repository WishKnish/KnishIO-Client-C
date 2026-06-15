#ifndef KNISHIO_META_H
#define KNISHIO_META_H

/**
 * @file meta.h
 * @brief Metadata operations for KnishIO SDK
 * 
 * Implements JavaScript-compatible metadata handling for post-blockchain DLT.
 * Meta objects represent key-value pairs conveyed by atoms.
 * Features simple KISS design following 2025 C17 best practices.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_meta knishio_meta_t;
typedef struct knishio_meta_array knishio_meta_array_t;

/* Constants */
#define KNISHIO_MAX_META_KEY_LENGTH 256     /**< Maximum meta key length */
#define KNISHIO_MAX_META_VALUE_LENGTH 2048  /**< Maximum meta value length (fits base64 ML-KEM768 pubkey ~1580 chars) */

/* Meta structure with C17 static assertions */
struct knishio_meta {
    char* key;                  /**< Meta key (required) */
    char* value;                /**< Meta value (required) */
};

/* Meta array structure for collections */
struct knishio_meta_array {
    knishio_meta_t** items;     /**< Array of meta pointers */
    size_t count;               /**< Number of meta items */
    size_t capacity;            /**< Allocated capacity */
};

/* Static assertions for C17 safety */
_Static_assert(KNISHIO_MAX_META_KEY_LENGTH > 0 && KNISHIO_MAX_META_KEY_LENGTH <= 1024,
    "Meta key length must be reasonable");
_Static_assert(KNISHIO_MAX_META_VALUE_LENGTH > 0 && KNISHIO_MAX_META_VALUE_LENGTH <= 4096,
    "Meta value length must be reasonable");

/* Meta lifecycle */

/**
 * @brief Create a new meta object
 * @param meta Output meta pointer
 * @param key Meta key (required)
 * @param value Meta value (required)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_meta_create(
    knishio_meta_t** meta,
    const char* key,
    const char* value
);

/**
 * @brief Free meta object and all associated resources
 * @param meta Meta to free
 */
void knishio_meta_free(knishio_meta_t* meta);

/* Meta properties */

/**
 * @brief Get meta key
 * @param meta Source meta
 * @return Key string or NULL
 */
const char* knishio_meta_get_key(const knishio_meta_t* meta);

/**
 * @brief Get meta value
 * @param meta Source meta
 * @return Value string or NULL
 */
const char* knishio_meta_get_value(const knishio_meta_t* meta);

/**
 * @brief Set meta value
 * @param meta Target meta
 * @param value New value
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_meta_set_value(knishio_meta_t* meta, const char* value);

/* Meta array lifecycle */

/**
 * @brief Create a new meta array
 * @param array Output meta array pointer
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_meta_array_create(knishio_meta_array_t** array);

/**
 * @brief Free meta array and all associated resources
 * @param array Meta array to free
 * @param free_items If true, also free all meta items in the array
 */
void knishio_meta_array_free(knishio_meta_array_t* array, bool free_items);

/**
 * @brief Add meta to array
 * @param array Target array
 * @param meta Meta to add (array takes ownership)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_meta_array_add(knishio_meta_array_t* array, knishio_meta_t* meta);

/**
 * @brief Get meta from array by index
 * @param array Source array
 * @param index Meta index
 * @return Meta pointer or NULL if index invalid
 */
knishio_meta_t* knishio_meta_array_get(const knishio_meta_array_t* array, size_t index);

/**
 * @brief Get meta array size
 * @param array Source array
 * @return Number of meta items
 */
size_t knishio_meta_array_size(const knishio_meta_array_t* array);

/**
 * @brief Find meta in array by key
 * @param array Source array
 * @param key Key to search for
 * @return Meta pointer or NULL if not found
 */
knishio_meta_t* knishio_meta_array_find(const knishio_meta_array_t* array, const char* key);

/* JavaScript SDK compatibility functions */

/**
 * @brief Normalize meta from object notation to array notation
 * @param input_json JSON object in format {"key1": "value1", "key2": "value2"}
 * @param output_array Output meta array (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 * 
 * Converts object-based meta into array-based notation:
 * {"name": "Alice", "age": "25"} -> [{"key": "name", "value": "Alice"}, {"key": "age", "value": "25"}]
 */
knishio_error_t knishio_meta_normalize(
    const char* input_json,
    knishio_meta_array_t** output_array
);

/**
 * @brief Aggregate meta from array notation to object notation  
 * @param input_array Meta array
 * @param output_json Output JSON object (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 * 
 * Converts array-based meta into object-based notation:
 * [{"key": "name", "value": "Alice"}, {"key": "age", "value": "25"}] -> {"name": "Alice", "age": "25"}
 */
knishio_error_t knishio_meta_aggregate(
    const knishio_meta_array_t* input_array,
    char** output_json
);

/**
 * @brief Create meta array from JSON object
 * @param json_input JSON object string
 * @param output_array Output meta array (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_meta_array_from_json_object(
    const char* json_input,
    knishio_meta_array_t** output_array
);

/**
 * @brief Create meta array from JSON array
 * @param json_input JSON array string in format [{"key": "k1", "value": "v1"}, ...]
 * @param output_array Output meta array (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_meta_array_from_json_array(
    const char* json_input,
    knishio_meta_array_t** output_array
);

/**
 * @brief Convert meta array to JSON array string
 * @param array Source meta array
 * @param output_json Output JSON string (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_meta_array_to_json(
    const knishio_meta_array_t* array,
    char** output_json
);

/* Validation */

/**
 * @brief Validate meta object
 * @param meta Meta to validate
 * @return KNISHIO_SUCCESS if valid, error code if invalid
 */
knishio_error_t knishio_meta_validate(const knishio_meta_t* meta);

/**
 * @brief Validate meta array
 * @param array Meta array to validate
 * @return KNISHIO_SUCCESS if valid, error code if invalid
 */
knishio_error_t knishio_meta_array_validate(const knishio_meta_array_t* array);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_META_H */
