#ifndef KNISHIO_HTTP_H
#define KNISHIO_HTTP_H

/**
 * @file http.h
 * @brief HTTP client for KnishIO C SDK
 * 
 * Simple HTTP client using libcurl for GraphQL POST operations.
 * JavaScript SDK compatible request/response handling.
 * KISS design focused on essential GraphQL communication.
 */

#include "knishio/knishio.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP response structure */
typedef struct knishio_http_response {
    char* data;                    /**< Response body (allocated) */
    size_t size;                   /**< Response body size */
    long status_code;              /**< HTTP status code */
    char* content_type;            /**< Content-Type header (allocated) */
    double total_time;             /**< Total request time in seconds */
} knishio_http_response_t;

/* HTTP client structure */
typedef struct knishio_http_client {
    char* base_url;                /**< Base URL for requests */
    char* user_agent;              /**< User agent string */
    char* auth_token;              /**< Authentication token for X-Auth-Token header */
    char* auth_pubkey;             /**< Public key for X-KnishIO-Pubkey header */
    long timeout_seconds;          /**< Request timeout */
    bool verify_ssl;               /**< SSL certificate verification */
} knishio_http_client_t;

/* HTTP client lifecycle */

/**
 * @brief Create HTTP client
 * @param client Output client pointer
 * @param base_url Base URL for GraphQL endpoint
 * @return Success or error code
 */
knishio_error_t knishio_http_client_create(
    knishio_http_client_t** client,
    const char* base_url
);

/**
 * @brief Free HTTP client
 * @param client Client to free
 */
void knishio_http_client_free(knishio_http_client_t* client);

/**
 * @brief Set client timeout
 * @param client HTTP client
 * @param timeout_seconds Timeout in seconds
 * @return Success or error code
 */
knishio_error_t knishio_http_client_set_timeout(
    knishio_http_client_t* client,
    long timeout_seconds
);

/**
 * @brief Set SSL verification
 * @param client HTTP client
 * @param verify_ssl Enable/disable SSL verification
 * @return Success or error code
 */
knishio_error_t knishio_http_client_set_ssl_verify(
    knishio_http_client_t* client,
    bool verify_ssl
);

/**
 * @brief Set authentication token header
 * @param client HTTP client
 * @param token Authentication token (sets X-Auth-Token: {token})
 * @return Success or error code
 */
knishio_error_t knishio_http_client_set_auth_token(
    knishio_http_client_t* client,
    const char* token
);

/**
 * @brief Set authentication public key header  
 * @param client HTTP client
 * @param pubkey Public key (sets X-KnishIO-Pubkey: {pubkey})
 * @return Success or error code
 */
knishio_error_t knishio_http_client_set_auth_pubkey(
    knishio_http_client_t* client,
    const char* pubkey
);

/**
 * @brief Clear authentication headers
 * @param client HTTP client
 * @return Success or error code
 */
knishio_error_t knishio_http_client_clear_auth(
    knishio_http_client_t* client
);

/**
 * @brief Get last HTTP client error message
 * @param client HTTP client
 * @return Error message string or NULL
 */
const char* knishio_http_client_error(knishio_http_client_t* client);

/**
 * @brief Set custom headers for HTTP client
 * @param client HTTP client
 * @param headers Array of header strings  
 * @param count Number of headers
 * @return True on success, false on error
 */
bool knishio_http_client_set_headers(
    knishio_http_client_t* client,
    const char** headers,
    size_t count
);

/* HTTP response lifecycle */

/**
 * @brief Create empty HTTP response
 * @param response Output response pointer
 * @return Success or error code
 */
knishio_error_t knishio_http_response_create(knishio_http_response_t** response);

/**
 * @brief Free HTTP response
 * @param response Response to free
 */
void knishio_http_response_free(knishio_http_response_t* response);

/* Core HTTP operations */

/**
 * @brief Perform GraphQL POST request
 * @param client HTTP client
 * @param graphql_query GraphQL query/mutation string
 * @param variables GraphQL variables JSON (optional)
 * @param response Output response
 * @return Success or error code
 */
knishio_error_t knishio_http_post_graphql(
    knishio_http_client_t* client,
    const char* graphql_query,
    const char* variables,
    knishio_http_response_t** response
);

/**
 * @brief Perform simple POST request
 * @param client HTTP client
 * @param path Request path (relative to base_url)
 * @param data POST data
 * @param content_type Content type header
 * @param response Output response
 * @return Success or error code
 */
knishio_error_t knishio_http_post(
    knishio_http_client_t* client,
    const char* path,
    const char* data,
    const char* content_type,
    knishio_http_response_t** response
);

/**
 * @brief Perform GET request
 * @param client HTTP client
 * @param path Request path (relative to base_url)
 * @param response Output response
 * @return Success or error code
 */
knishio_error_t knishio_http_get(
    knishio_http_client_t* client,
    const char* path,
    knishio_http_response_t** response
);

/* Response access functions */

/**
 * @brief Get response data
 * @param response HTTP response
 * @return Response body string (do not free)
 */
const char* knishio_http_response_get_data(const knishio_http_response_t* response);

/**
 * @brief Get response size
 * @param response HTTP response
 * @return Response body size
 */
size_t knishio_http_response_get_size(const knishio_http_response_t* response);

/**
 * @brief Get response status code
 * @param response HTTP response
 * @return HTTP status code
 */
long knishio_http_response_get_status(const knishio_http_response_t* response);

/**
 * @brief Get response content type
 * @param response HTTP response
 * @return Content-Type header (do not free)
 */
const char* knishio_http_response_get_content_type(const knishio_http_response_t* response);

/**
 * @brief Get response time
 * @param response HTTP response
 * @return Total request time in seconds
 */
double knishio_http_response_get_time(const knishio_http_response_t* response);

/* Utility functions */

/**
 * @brief Check if response indicates success (2xx status)
 * @param response HTTP response
 * @return True if successful status code
 */
bool knishio_http_response_is_success(const knishio_http_response_t* response);

/**
 * @brief Check if response is JSON
 * @param response HTTP response
 * @return True if content type indicates JSON
 */
bool knishio_http_response_is_json(const knishio_http_response_t* response);

/**
 * @brief Initialize libcurl (call once per application)
 * @return Success or error code
 */
knishio_error_t knishio_http_global_init(void);

/**
 * @brief Cleanup libcurl (call once per application)
 */
void knishio_http_global_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_HTTP_H */
