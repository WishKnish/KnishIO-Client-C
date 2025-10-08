#ifndef KNISHIO_JSON_PARSER_H
#define KNISHIO_JSON_PARSER_H

/**
 * @file parser.h
 * @brief JSON parsing utilities for KnishIO SDK
 * 
 * Provides JSON parsing functionality that's compatible with the JavaScript SDK's
 * JSON handling, supporting GraphQL responses and complex nested structures.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_json knishio_json_t;
typedef struct knishio_json_array knishio_json_array_t;
typedef struct knishio_json_object knishio_json_object_t;

/* JSON value types */
typedef enum {
    KNISHIO_JSON_NULL,
    KNISHIO_JSON_BOOL,
    KNISHIO_JSON_NUMBER,
    KNISHIO_JSON_STRING,
    KNISHIO_JSON_ARRAY,
    KNISHIO_JSON_OBJECT
} knishio_json_type_t;

/**
 * @brief JSON value structure
 */
typedef struct knishio_json {
    knishio_json_type_t type;
    union {
        bool bool_value;
        double number_value;
        char *string_value;
        knishio_json_array_t *array_value;
        knishio_json_object_t *object_value;
    } data;
} knishio_json_t;

/**
 * @brief Parse JSON string
 * @param json_string JSON string to parse
 * @param error_msg Optional error message output
 * @return Parsed JSON object or NULL on error
 */
knishio_json_t* knishio_json_parse(const char *json_string, char **error_msg);

/**
 * @brief Parse JSON with length
 * @param json_string JSON string to parse
 * @param length Length of JSON string
 * @param error_msg Optional error message output
 * @return Parsed JSON object or NULL on error
 */
knishio_json_t* knishio_json_parse_n(const char *json_string, size_t length, char **error_msg);

/**
 * @brief Free JSON object
 * @param json JSON object to free
 */
void knishio_json_free(knishio_json_t *json);

/**
 * @brief Get JSON value type
 * @param json JSON value
 * @return Type of JSON value
 */
knishio_json_type_t knishio_json_get_type(const knishio_json_t *json);

/**
 * @brief Check if JSON is null
 * @param json JSON value
 * @return True if null, false otherwise
 */
bool knishio_json_is_null(const knishio_json_t *json);

/**
 * @brief Get boolean value
 * @param json JSON value
 * @param value Output boolean value
 * @return True if value is boolean, false otherwise
 */
bool knishio_json_get_bool(const knishio_json_t *json, bool *value);

/**
 * @brief Get number value
 * @param json JSON value
 * @param value Output number value
 * @return True if value is number, false otherwise
 */
bool knishio_json_get_number(const knishio_json_t *json, double *value);

/**
 * @brief Get integer value
 * @param json JSON value
 * @param value Output integer value
 * @return True if value is integer, false otherwise
 */
bool knishio_json_get_int(const knishio_json_t *json, int64_t *value);

/**
 * @brief Get string value
 * @param json JSON value
 * @return String value or NULL if not a string
 */
const char* knishio_json_get_string(const knishio_json_t *json);

/**
 * @brief Get string value with length
 * @param json JSON value
 * @param length Output string length
 * @return String value or NULL if not a string
 */
const char* knishio_json_get_string_n(const knishio_json_t *json, size_t *length);

/* Array operations */

/**
 * @brief Get array size
 * @param json JSON array
 * @return Array size or 0 if not an array
 */
size_t knishio_json_array_size(const knishio_json_t *json);

/**
 * @brief Get array item
 * @param json JSON array
 * @param index Array index
 * @return Array item or NULL if invalid
 */
knishio_json_t* knishio_json_array_get(const knishio_json_t *json, size_t index);

/**
 * @brief Iterate over array
 * @param json JSON array
 * @param callback Callback function for each item
 * @param user_data User data for callback
 * @return True if all iterations succeeded, false otherwise
 */
bool knishio_json_array_foreach(const knishio_json_t *json,
                                bool (*callback)(size_t index, knishio_json_t *item, void *user_data),
                                void *user_data);

/* Object operations */

/**
 * @brief Get object size
 * @param json JSON object
 * @return Object size or 0 if not an object
 */
size_t knishio_json_object_size(const knishio_json_t *json);

/**
 * @brief Get object property
 * @param json JSON object
 * @param key Property key
 * @return Property value or NULL if not found
 */
knishio_json_t* knishio_json_object_get(const knishio_json_t *json, const char *key);

/**
 * @brief Check if object has property
 * @param json JSON object
 * @param key Property key
 * @return True if property exists, false otherwise
 */
bool knishio_json_object_has(const knishio_json_t *json, const char *key);

/**
 * @brief Get object property keys
 * @param json JSON object
 * @param keys Output array for keys (caller must free)
 * @param count Output key count
 * @return True on success, false on error
 */
bool knishio_json_object_keys(const knishio_json_t *json, char ***keys, size_t *count);

/**
 * @brief Iterate over object properties
 * @param json JSON object
 * @param callback Callback function for each property
 * @param user_data User data for callback
 * @return True if all iterations succeeded, false otherwise
 */
bool knishio_json_object_foreach(const knishio_json_t *json,
                                 bool (*callback)(const char *key, knishio_json_t *value, void *user_data),
                                 void *user_data);

/* Path navigation */

/**
 * @brief Get value by path (e.g., "data.user.name")
 * @param json Root JSON object
 * @param path Dot-separated path
 * @return Value at path or NULL if not found
 */
knishio_json_t* knishio_json_get_path(const knishio_json_t *json, const char *path);

/**
 * @brief Get string by path
 * @param json Root JSON object
 * @param path Dot-separated path
 * @return String value at path or NULL if not found
 */
const char* knishio_json_get_string_path(const knishio_json_t *json, const char *path);

/**
 * @brief Get number by path
 * @param json Root JSON object
 * @param path Dot-separated path
 * @param value Output number value
 * @return True if path contains number, false otherwise
 */
bool knishio_json_get_number_path(const knishio_json_t *json, const char *path, double *value);

/**
 * @brief Get boolean by path
 * @param json Root JSON object
 * @param path Dot-separated path
 * @param value Output boolean value
 * @return True if path contains boolean, false otherwise
 */
bool knishio_json_get_bool_path(const knishio_json_t *json, const char *path, bool *value);

/* Serialization */

/**
 * @brief Serialize JSON to string
 * @param json JSON object to serialize
 * @param pretty Pretty print if true
 * @return Serialized JSON string (caller must free) or NULL on error
 */
char* knishio_json_serialize(const knishio_json_t *json, bool pretty);

/**
 * @brief Serialize JSON to string with custom options
 * @param json JSON object to serialize
 * @param indent Number of spaces for indentation (0 for compact)
 * @param ensure_ascii Escape non-ASCII characters if true
 * @return Serialized JSON string (caller must free) or NULL on error
 */
char* knishio_json_serialize_ex(const knishio_json_t *json, int indent, bool ensure_ascii);

/* Validation */

/**
 * @brief Validate JSON string
 * @param json_string JSON string to validate
 * @param error_msg Optional error message output
 * @return True if valid JSON, false otherwise
 */
bool knishio_json_validate(const char *json_string, char **error_msg);

/**
 * @brief Deep clone JSON object
 * @param json JSON object to clone
 * @return Cloned JSON object (caller must free) or NULL on error
 */
knishio_json_t* knishio_json_clone(const knishio_json_t *json);

/**
 * @brief Compare two JSON objects for equality
 * @param a First JSON object
 * @param b Second JSON object
 * @return True if equal, false otherwise
 */
bool knishio_json_equal(const knishio_json_t *a, const knishio_json_t *b);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_JSON_PARSER_H */
