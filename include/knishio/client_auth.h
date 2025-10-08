#ifndef KNISHIO_CLIENT_AUTH_H
#define KNISHIO_CLIENT_AUTH_H

/**
 * @file client_auth.h
 * @brief Client-level authentication integration for KnishIO C SDK
 * 
 * Provides automatic authentication management similar to JavaScript SDK
 * including token refresh, session management, and auth header injection.
 */

#include "knishio/error/context.h"
#include "knishio/auth_token.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_client knishio_client_t;

/**
 * @brief Authentication configuration for client
 */
typedef struct {
    char* secret;                   /**< User secret for profile auth */
    char* cell_slug;               /**< Default cell slug */
    bool encrypt;                  /**< Enable encryption */
    bool auto_refresh;             /**< Auto-refresh expired tokens */
    int64_t refresh_threshold_ms;  /**< Refresh threshold in milliseconds */
} knishio_client_auth_config_t;

/**
 * @brief Configure client authentication
 * Sets up automatic token management for the client
 * 
 * @param client KnishIO client instance
 * @param config Authentication configuration
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_configure_auth(
    knishio_client_t* client,
    const knishio_client_auth_config_t* config
);

/**
 * @brief Authenticate client with profile credentials
 * Equivalent to JavaScript: client.authenticate({ secret, encrypt })
 * 
 * @param client KnishIO client instance
 * @param secret User secret
 * @param encrypt Enable encryption flag
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_authenticate(
    knishio_client_t* client,
    const char* secret,
    bool encrypt
);

/**
 * @brief Authenticate client as guest
 * Equivalent to JavaScript: client.authenticateGuest({ cellSlug, encrypt })
 * 
 * @param client KnishIO client instance  
 * @param cell_slug Cell slug
 * @param encrypt Enable encryption flag
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_authenticate_guest(
    knishio_client_t* client,
    const char* cell_slug,
    bool encrypt
);

/**
 * @brief Check if client is authenticated
 * Returns true if client has valid (non-expired) auth token
 * 
 * @param client KnishIO client instance
 * @return True if authenticated, false otherwise
 */
bool knishio_client_is_authenticated(const knishio_client_t* client);

/**
 * @brief Get current authentication token
 * Returns current AuthToken instance or NULL if not authenticated
 * 
 * @param client KnishIO client instance
 * @return AuthToken instance or NULL
 */
knishio_auth_token_t* knishio_client_get_auth_token(const knishio_client_t* client);

/**
 * @brief Set authentication token
 * Manually set authentication token for client
 * 
 * @param client KnishIO client instance
 * @param auth_token AuthToken to set
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_set_auth_token(
    knishio_client_t* client,
    knishio_auth_token_t* auth_token
);

/**
 * @brief Clear authentication token
 * Removes current authentication and sets client to unauthenticated state
 * 
 * @param client KnishIO client instance
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_clear_auth(knishio_client_t* client);

/**
 * @brief Refresh authentication token
 * Refreshes current token if it's expired or about to expire
 * 
 * @param client KnishIO client instance
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_refresh_auth(knishio_client_t* client);

/**
 * @brief Check and refresh auth token if needed
 * Internal function called before GraphQL operations
 * Automatically refreshes token if expired and auto-refresh is enabled
 * 
 * @param client KnishIO client instance
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_ensure_auth(knishio_client_t* client);

/**
 * @brief Get authentication headers for GraphQL request
 * Creates authentication headers from current auth token
 * 
 * @param client KnishIO client instance
 * @param headers Output headers string (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_get_auth_headers(
    const knishio_client_t* client,
    char** headers
);

/**
 * @brief Save authentication token to snapshot
 * Creates a snapshot of current auth token for persistence
 * 
 * @param client KnishIO client instance
 * @param snapshot_json Output JSON snapshot (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_save_auth_snapshot(
    const knishio_client_t* client,
    char** snapshot_json
);

/**
 * @brief Restore authentication token from snapshot
 * Restores auth token from previously saved snapshot
 * 
 * @param client KnishIO client instance
 * @param snapshot_json JSON snapshot string
 * @param secret User secret for token restoration
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_restore_auth_snapshot(
    knishio_client_t* client,
    const char* snapshot_json,
    const char* secret
);

/**
 * @brief Get current user secret
 * Returns the secret used for profile authentication
 * 
 * @param client KnishIO client instance
 * @return User secret or NULL if not available
 */
const char* knishio_client_get_secret(const knishio_client_t* client);

/**
 * @brief Check if secret is available
 * Returns true if client has stored user secret
 * 
 * @param client KnishIO client instance
 * @return True if secret available, false otherwise
 */
bool knishio_client_has_secret(const knishio_client_t* client);

/* Authentication event callbacks */

/**
 * @brief Authentication event callback function type
 * Called when authentication events occur
 * 
 * @param client KnishIO client instance
 * @param event_type Event type string ("authenticated", "expired", "refreshed", "failed")
 * @param user_data User data passed during callback registration
 */
typedef void (*knishio_auth_event_callback_t)(
    knishio_client_t* client,
    const char* event_type,
    void* user_data
);

/**
 * @brief Register authentication event callback
 * Registers callback to be called on auth events
 * 
 * @param client KnishIO client instance
 * @param callback Callback function
 * @param user_data User data to pass to callback
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_register_auth_callback(
    knishio_client_t* client,
    knishio_auth_event_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister authentication event callback
 * Removes previously registered auth event callback
 * 
 * @param client KnishIO client instance
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_unregister_auth_callback(knishio_client_t* client);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CLIENT_AUTH_H */