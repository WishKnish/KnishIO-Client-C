#ifndef KNISHIO_CLIENT_INTERNAL_H
#define KNISHIO_CLIENT_INTERNAL_H

/**
 * @file client_internal.h
 * @brief Internal structures and declarations for KnishIO client implementation
 * 
 * This header contains internal definitions shared between client modules
 * but not exposed in the public API.
 */

#include "knishio/client_auth.h"
#include "knishio/auth_token.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal client authentication state structure
 * Manages authentication state including tokens, credentials, and callbacks.
 */
typedef struct {
    knishio_auth_token_t* current_token;     /**< Current authentication token */
    char* secret;                            /**< Stored user secret */
    char* cell_slug;                         /**< Default cell slug */
    bool encrypt;                            /**< Encryption flag */
    bool auto_refresh;                       /**< Auto-refresh enabled */
    int64_t refresh_threshold_ms;            /**< Refresh threshold */
    knishio_auth_event_callback_t callback;  /**< Auth event callback */
    void* callback_user_data;                /**< Callback user data */
} knishio_client_auth_state_t;

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CLIENT_INTERNAL_H */