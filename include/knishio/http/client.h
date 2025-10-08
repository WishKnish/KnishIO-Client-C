#ifndef KNISHIO_HTTP_CLIENT_H
#define KNISHIO_HTTP_CLIENT_H

/**
 * @file client.h
 * @brief HTTP client for KnishIO SDK GraphQL communication
 * 
 * Provides HTTP client functionality for GraphQL queries and mutations,
 * compatible with the JavaScript SDK's network layer.
 */

#include <stddef.h>
#include <stdbool.h>
#include "knishio/http.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_json knishio_json_t;
typedef struct knishio_http_client knishio_http_client_t;
typedef struct knishio_http_request knishio_http_request_t;
typedef struct knishio_http_response knishio_http_response_t;

/* HTTP methods */
typedef enum {
    KNISHIO_HTTP_GET,
    KNISHIO_HTTP_POST,
    KNISHIO_HTTP_PUT,
    KNISHIO_HTTP_DELETE,
    KNISHIO_HTTP_PATCH
} knishio_http_method_t;

/* Request configuration */
typedef struct {
    const char *url;                   /**< Request URL */
    knishio_http_method_t method;      /**< HTTP method */
    const char *body;                   /**< Request body (can be NULL) */
    size_t body_length;                 /**< Body length (0 for auto) */
    const char **headers;               /**< Array of header strings */
    size_t header_count;                /**< Number of headers */
    long timeout_ms;                    /**< Request timeout in milliseconds */
    bool follow_redirects;              /**< Follow redirects */
    const char *user_agent;             /**< User agent string */
} knishio_http_config_t;

/* Response structure defined in knishio/http.h - don't redefine */
/* Use the definition from knishio/http.h */

/* Client lifecycle */

/* HTTP client functions are defined in knishio/http.h
 * The signatures there use error returns rather than NULL checks */

/**
 * @brief Set default headers for all requests
 * @param client HTTP client
 * @param headers Array of header strings
 * @param count Number of headers
 * @return True on success, false on error
 */
bool knishio_http_client_set_headers(knishio_http_client_t *client,
                                     const char **headers, size_t count);

/**
 * @brief Set authentication token
 * @param client HTTP client
 * @param token Authentication token
 * @return True on success, false on error
 */
bool knishio_http_client_set_auth(knishio_http_client_t *client, const char *token);

/**
 * @brief Set request timeout
 * @param client HTTP client
 * @param timeout_ms Timeout in milliseconds
 * @return True on success, false on error
 */
/* Function defined in knishio/http.h with different signature 
bool knishio_http_client_set_timeout(knishio_http_client_t *client, long timeout_ms); */

/* Request execution */

/**
 * @brief Execute HTTP request
 * @param client HTTP client
 * @param config Request configuration
 * @return Response or NULL on error
 */
knishio_http_response_t* knishio_http_request(knishio_http_client_t *client,
                                              const knishio_http_config_t *config);

/* Functions defined in knishio/http.h with different signatures 
knishio_http_response_t* knishio_http_get(knishio_http_client_t *client, const char *url);
knishio_http_response_t* knishio_http_post(knishio_http_client_t *client,
                                           const char *url, const char *body); */

/**
 * @brief Execute POST request with JSON
 * @param client HTTP client
 * @param url Request URL
 * @param json JSON object to send
 * @return Response or NULL on error
 */
knishio_http_response_t* knishio_http_post_json(knishio_http_client_t *client,
                                                const char *url, knishio_json_t *json);

/* GraphQL operations */

/**
 * @brief Execute GraphQL query
 * @param client HTTP client
 * @param endpoint GraphQL endpoint
 * @param query Query string
 * @param variables Variables object (can be NULL)
 * @return Response or NULL on error
 */
knishio_http_response_t* knishio_http_graphql_query(knishio_http_client_t *client,
                                                    const char *endpoint,
                                                    const char *query,
                                                    knishio_json_t *variables);

/**
 * @brief Execute GraphQL mutation
 * @param client HTTP client
 * @param endpoint GraphQL endpoint
 * @param mutation Mutation string
 * @param variables Variables object (can be NULL)
 * @return Response or NULL on error
 */
knishio_http_response_t* knishio_http_graphql_mutation(knishio_http_client_t *client,
                                                       const char *endpoint,
                                                       const char *mutation,
                                                       knishio_json_t *variables);

/* Response handling */

/**
 * @brief Parse response body as JSON
 * @param response HTTP response
 * @return Parsed JSON or NULL on error
 */
knishio_json_t* knishio_http_response_json(knishio_http_response_t *response);

/**
 * @brief Get response header value
 * @param response HTTP response
 * @param name Header name
 * @return Header value or NULL if not found
 */
const char* knishio_http_response_header(knishio_http_response_t *response, const char *name);

/**
 * @brief Free HTTP response
 * @param response Response to free
 */
void knishio_http_response_free(knishio_http_response_t *response);

/* Utility functions */

/**
 * @brief URL encode string
 * @param str String to encode
 * @return Encoded string (caller must free) or NULL on error
 */
char* knishio_http_url_encode(const char *str);

/**
 * @brief URL decode string
 * @param str String to decode
 * @return Decoded string (caller must free) or NULL on error
 */
char* knishio_http_url_decode(const char *str);

/**
 * @brief Build URL with query parameters
 * @param base_url Base URL
 * @param params Parameter names
 * @param values Parameter values
 * @param count Number of parameters
 * @return Complete URL (caller must free) or NULL on error
 */
char* knishio_http_build_url(const char *base_url,
                             const char **params,
                             const char **values,
                             size_t count);

/* Error handling */

/**
 * @brief Get last error message
 * @param client HTTP client
 * @return Error message or NULL
 */
const char* knishio_http_client_error(knishio_http_client_t *client);

/**
 * @brief Clear error state
 * @param client HTTP client
 */
void knishio_http_client_clear_error(knishio_http_client_t *client);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_HTTP_CLIENT_H */