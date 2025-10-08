#ifndef KNISHIO_JSON_H
#define KNISHIO_JSON_H

/**
 * @file json.h
 * @brief Convenience header for all JSON functionality in KnishIO SDK
 * 
 * This header includes all JSON-related headers and provides
 * additional helper functions for common JSON operations.
 */

/* Include existing JSON headers */
#include "knishio/json/parser.h"
#include "knishio/json/builder.h"
#include "knishio/json/serializers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Additional helper functions for compatibility */

/**
 * @brief Get object property (convenience wrapper)
 * @param json JSON value
 * @param key Property key
 * @param value Output JSON value
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_get_object(const knishio_json_t *json, const char *key, knishio_json_t **value);

/**
 * @brief Get array from object (convenience wrapper)
 * @param json JSON value
 * @param key Property key for array
 * @param array Output JSON array
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_get_array(const knishio_json_t *json, const char *key, knishio_json_t **array);

/**
 * @brief Convert JSON to string
 * @param json JSON value to serialize
 * @param output Output string (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_to_string(const knishio_json_t *json, char **output);

/**
 * @brief Add raw JSON string to builder
 * @param builder JSON builder
 * @param key Property key
 * @param raw_json Raw JSON string to add
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_builder_add_raw(knishio_json_builder_t *builder, const char *key, const char *raw_json);

/**
 * @brief Add JSON object to builder
 * @param builder JSON builder
 * @param key Property key
 * @param json JSON object to add
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_builder_add_json(knishio_json_builder_t *builder, const char *key, const knishio_json_t *json);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_JSON_H */
