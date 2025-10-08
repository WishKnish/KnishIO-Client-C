#ifndef KNISHIO_OPERATIONS_AUTH_H
#define KNISHIO_OPERATIONS_AUTH_H

/**
 * @file operations/auth.h
 * @brief Authentication and session operations for KnishIO C SDK
 * 
 * Provides high-level auth operations matching JavaScript SDK:
 * - RequestGuestAuthToken mutation
 * - RequestProfileAuthToken mutation  
 * - ActiveSession mutation
 * - WebSocket subscription management
 * 
 * Full alignment with JS SDK authentication functionality.
 */

#include "knishio/error/context.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_client knishio_client_t;

/**
 * @brief Parameters for guest auth token request
 * Matches JavaScript SDK requestGuestAuthToken() parameters
 */
typedef struct {
    const char* cell_slug;              /**< Cell slug */
    bool encrypt;                       /**< Encryption flag */
} knishio_request_guest_auth_token_params_t;

/**
 * @brief Result of guest auth token request
 */
typedef struct {
    bool success;                       /**< Request success flag */
    char* token;                        /**< Auth token string */
    char* response;                     /**< Full response data */
    char* error_message;                /**< Error message if failed */
} knishio_request_guest_auth_token_result_t;

/**
 * @brief Parameters for profile auth token request
 * Matches JavaScript SDK requestProfileAuthToken() parameters
 */
typedef struct {
    const char* secret;                 /**< User secret */
    bool encrypt;                       /**< Encryption flag */
} knishio_request_profile_auth_token_params_t;

/**
 * @brief Result of profile auth token request
 */
typedef struct {
    bool success;                       /**< Request success flag */
    char* token;                        /**< Auth token string */
    char* response;                     /**< Full response data */
    char* error_message;                /**< Error message if failed */
} knishio_request_profile_auth_token_result_t;

/**
 * @brief Parameters for active session declaration
 * Matches JavaScript SDK activeSession() parameters
 */
typedef struct {
    const char* bundle;                 /**< Bundle hash */
    const char* meta_type;              /**< Meta type */
    const char* meta_id;                /**< Meta ID */
    const char* ip_address;             /**< IP address */
    const char* browser;                /**< Browser string */
    const char* os_cpu;                 /**< OS and CPU */
    const char* resolution;             /**< Screen resolution */
    const char* time_zone;              /**< Time zone */
    const char* json_data;              /**< Additional JSON data */
} knishio_active_session_params_t;

/**
 * @brief Result of active session declaration
 */
typedef struct {
    bool success;                       /**< Declaration success flag */
    char* response;                     /**< Full response data */
    char* error_message;                /**< Error message if failed */
} knishio_active_session_result_t;

/**
 * @brief Request guest authentication token
 * Equivalent to JavaScript: client.requestGuestAuthToken({ cellSlug, encrypt })
 * 
 * @param client KnishIO client instance
 * @param params Request parameters
 * @param result Output auth result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_request_guest_auth_token(
    knishio_client_t* client,
    const knishio_request_guest_auth_token_params_t* params,
    knishio_request_guest_auth_token_result_t** result
);

/**
 * @brief Request profile authentication token
 * Equivalent to JavaScript: client.requestProfileAuthToken({ secret, encrypt })
 * 
 * @param client KnishIO client instance
 * @param params Request parameters
 * @param result Output auth result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_request_profile_auth_token(
    knishio_client_t* client,
    const knishio_request_profile_auth_token_params_t* params,
    knishio_request_profile_auth_token_result_t** result
);

/**
 * @brief Declare active session
 * Equivalent to JavaScript: client.activeSession({ bundle, metaType, metaId, ... })
 * 
 * @param client KnishIO client instance
 * @param params Session parameters
 * @param result Output session result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_active_session(
    knishio_client_t* client,
    const knishio_active_session_params_t* params,
    knishio_active_session_result_t** result
);

/**
 * @brief Unsubscribe from WebSocket subscription
 * Equivalent to JavaScript: client.unsubscribe(operationName)
 * 
 * @param client KnishIO client instance
 * @param operation_name Subscription operation name
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_unsubscribe(
    knishio_client_t* client,
    const char* operation_name
);

/**
 * @brief Unsubscribe from all WebSocket subscriptions
 * Equivalent to JavaScript: client.unsubscribeAll()
 * 
 * @param client KnishIO client instance
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_unsubscribe_all(
    knishio_client_t* client
);

/**
 * @brief Free guest auth token result
 * @param result Result to free
 */
void knishio_request_guest_auth_token_result_free(knishio_request_guest_auth_token_result_t* result);

/**
 * @brief Free profile auth token result
 * @param result Result to free
 */
void knishio_request_profile_auth_token_result_free(knishio_request_profile_auth_token_result_t* result);

/**
 * @brief Free active session result
 * @param result Result to free
 */
void knishio_active_session_result_free(knishio_active_session_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_OPERATIONS_AUTH_H */