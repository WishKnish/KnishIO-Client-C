#ifndef KNISHIO_RESPONSE_H
#define KNISHIO_RESPONSE_H

/**
 * @file response.h
 * @brief Base Response infrastructure for KnishIO SDK
 * 
 * Provides the foundational response handling system that matches the
 * JavaScript SDK's Response class architecture, including GraphQL
 * response parsing, error handling, and memory management.
 */

#include <stddef.h>
#include <stdbool.h>
#include "../json/parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response knishio_response_t;
typedef struct knishio_query knishio_query_t;

/**
 * @brief Response error information
 */
typedef struct {
    char *message;                      /**< Error message */
    char *code;                         /**< Error code */
    char *path;                         /**< GraphQL path where error occurred */
    knishio_json_t *extensions;         /**< Error extensions */
} knishio_response_error_t;

/**
 * @brief Base response structure
 */
struct knishio_response {
    knishio_query_t *query;             /**< Original query */
    knishio_json_t *origin_response;    /**< Original JSON response */
    knishio_json_t *response;           /**< Processed response */
    knishio_json_t *payload;            /**< Cached payload */
    const char *data_key;               /**< Key for extracting data */
    const char *error_key;              /**< Key for extracting errors */
    knishio_response_error_t *errors;   /**< Response errors */
    size_t error_count;                 /**< Number of errors */
    bool is_authenticated;              /**< Authentication status */
    char *error_message;                /**< General error message */
};

/**
 * @brief Response creation callback function type
 * @param query The query that generated this response
 * @param json The JSON response data
 * @param data_key The key for extracting specific data
 * @return Newly created response or NULL on error
 */
typedef knishio_response_t* (*knishio_response_create_fn)(knishio_query_t *query,
                                                          knishio_json_t *json,
                                                          const char *data_key);

/**
 * @brief Payload extraction callback function type
 * @param response The response to extract payload from
 * @return Payload data or NULL
 */
typedef knishio_json_t* (*knishio_response_payload_fn)(knishio_response_t *response);

/**
 * @brief Response cleanup callback function type
 * @param response The response to clean up
 */
typedef void (*knishio_response_cleanup_fn)(knishio_response_t *response);

/**
 * @brief Response virtual function table
 */
typedef struct {
    knishio_response_payload_fn payload;    /**< Extract payload */
    knishio_response_cleanup_fn cleanup;    /**< Custom cleanup */
} knishio_response_vtable_t;

/* Base response functions */

/**
 * @brief Create base response
 * @param query Original query
 * @param json JSON response data
 * @param data_key Key for extracting data (can be NULL)
 * @return Response or NULL on error
 */
knishio_response_t* knishio_response_create(knishio_query_t *query,
                                            knishio_json_t *json,
                                            const char *data_key);

/**
 * @brief Free response
 * @param response Response to free
 */
void knishio_response_free(knishio_response_t *response);

/**
 * @brief Initialize response (called after creation)
 * @param response Response to initialize
 * @return True on success, false on error
 */
bool knishio_response_init(knishio_response_t *response);

/**
 * @brief Get response data using data_key
 * @param response Response
 * @return Data JSON or NULL if not found
 */
knishio_json_t* knishio_response_data(knishio_response_t *response);

/**
 * @brief Get raw response JSON
 * @param response Response
 * @return Response JSON
 */
knishio_json_t* knishio_response_raw(knishio_response_t *response);

/**
 * @brief Get response payload
 * @param response Response
 * @return Payload JSON or NULL
 */
knishio_json_t* knishio_response_payload(knishio_response_t *response);

/**
 * @brief Get original query
 * @param response Response
 * @return Query that generated this response
 */
knishio_query_t* knishio_response_query(knishio_response_t *response);

/**
 * @brief Get response status
 * @param response Response
 * @return Status string or NULL
 */
const char* knishio_response_status(knishio_response_t *response);

/* Error handling */

/**
 * @brief Check if response has errors
 * @param response Response
 * @return True if errors present, false otherwise
 */
bool knishio_response_has_errors(knishio_response_t *response);

/**
 * @brief Get response errors
 * @param response Response
 * @param count Output error count
 * @return Array of errors or NULL
 */
knishio_response_error_t* knishio_response_get_errors(knishio_response_t *response,
                                                      size_t *count);

/**
 * @brief Get error message
 * @param response Response
 * @return Error message or NULL
 */
const char* knishio_response_error_message(knishio_response_t *response);

/**
 * @brief Check if error is authentication related
 * @param response Response
 * @return True if unauthenticated error, false otherwise
 */
bool knishio_response_is_unauthenticated(knishio_response_t *response);

/* GraphQL response handling */

/**
 * @brief Parse GraphQL response format
 * @param response Response
 * @return True if valid GraphQL response, false otherwise
 */
bool knishio_response_parse_graphql(knishio_response_t *response);

/**
 * @brief Extract GraphQL data section
 * @param response Response
 * @return Data section JSON or NULL
 */
knishio_json_t* knishio_response_graphql_data(knishio_response_t *response);

/**
 * @brief Extract GraphQL errors section
 * @param response Response
 * @return Errors array JSON or NULL
 */
knishio_json_t* knishio_response_graphql_errors(knishio_response_t *response);

/* Utility functions */

/**
 * @brief Get string value from response data path
 * @param response Response
 * @param path Dot-separated path (e.g., "data.user.name")
 * @return String value or NULL if not found
 */
const char* knishio_response_get_string(knishio_response_t *response, const char *path);

/**
 * @brief Get number value from response data path
 * @param response Response
 * @param path Dot-separated path
 * @param value Output number value
 * @return True if path contains number, false otherwise
 */
bool knishio_response_get_number(knishio_response_t *response, const char *path, double *value);

/**
 * @brief Get boolean value from response data path
 * @param response Response
 * @param path Dot-separated path
 * @param value Output boolean value
 * @return True if path contains boolean, false otherwise
 */
bool knishio_response_get_bool(knishio_response_t *response, const char *path, bool *value);

/**
 * @brief Get array from response data path
 * @param response Response
 * @param path Dot-separated path
 * @return Array JSON or NULL if not found
 */
knishio_json_t* knishio_response_get_array(knishio_response_t *response, const char *path);

/**
 * @brief Get object from response data path
 * @param response Response
 * @param path Dot-separated path
 * @return Object JSON or NULL if not found
 */
knishio_json_t* knishio_response_get_object(knishio_response_t *response, const char *path);

/* Validation */

/**
 * @brief Validate response structure
 * @param response Response
 * @return True if valid, false otherwise
 */
bool knishio_response_validate(knishio_response_t *response);

/**
 * @brief Clone response
 * @param response Response to clone
 * @return Cloned response or NULL on error
 */
knishio_response_t* knishio_response_clone(knishio_response_t *response);

/* Error creation helpers */

/**
 * @brief Create response error
 * @param message Error message
 * @param code Error code (can be NULL)
 * @param path Error path (can be NULL)
 * @return Response error or NULL on error
 */
knishio_response_error_t* knishio_response_error_create(const char *message,
                                                        const char *code,
                                                        const char *path);

/**
 * @brief Free response error
 * @param error Error to free
 */
void knishio_response_error_free(knishio_response_error_t *error);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_H */