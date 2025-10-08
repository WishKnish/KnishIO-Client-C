#ifndef KNISHIO_SUBSCRIPTIONS_H
#define KNISHIO_SUBSCRIPTIONS_H

/**
 * @file subscriptions.h
 * @brief GraphQL subscription operations for KnishIO C SDK
 * 
 * Implements all subscription operations from JavaScript SDK for full compatibility:
 * - ActiveWalletSubscribe - Real-time wallet monitoring
 * - ActiveSessionSubscribe - Session state changes
 * - WalletStatusSubscribe - Wallet status updates
 * - CreateMoleculeSubscribe - Real-time molecule updates
 * 
 * Uses WebSocket integration for real-time subscriptions.
 */

#include "knishio/graphql.h"
#include "knishio/response/responses.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_graphql_client knishio_graphql_client_t;

/* Subscription callback types */

/**
 * @brief Callback for active wallet subscription updates
 * @param data Wallet data (JSON string)
 * @param error Error message (NULL if no error)
 * @param user_data User-provided context data
 */
typedef void (*knishio_active_wallet_callback_t)(
    const char* data, 
    const char* error, 
    void* user_data
);

/**
 * @brief Callback for active session subscription updates
 * @param data Session data (JSON string)
 * @param error Error message (NULL if no error)
 * @param user_data User-provided context data
 */
typedef void (*knishio_active_session_callback_t)(
    const char* data, 
    const char* error, 
    void* user_data
);

/**
 * @brief Callback for wallet status subscription updates
 * @param data Wallet status data (JSON string)
 * @param error Error message (NULL if no error)
 * @param user_data User-provided context data
 */
typedef void (*knishio_wallet_status_callback_t)(
    const char* data, 
    const char* error, 
    void* user_data
);

/**
 * @brief Callback for molecule creation subscription updates
 * @param data Molecule data (JSON string)
 * @param error Error message (NULL if no error)
 * @param user_data User-provided context data
 */
typedef void (*knishio_create_molecule_callback_t)(
    const char* data, 
    const char* error, 
    void* user_data
);

/* Subscription parameter structures */

/**
 * @brief Parameters for active wallet subscription
 * Matches JavaScript SDK ActiveWalletSubscribe
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash to monitor */
} knishio_subscribe_active_wallet_params_t;

/**
 * @brief Parameters for active session subscription
 * Matches JavaScript SDK ActiveSessionSubscribe
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash for session */
    const char* meta_type;           /**< Session metadata type */
    const char* meta_id;             /**< Session metadata ID */
} knishio_subscribe_active_session_params_t;

/**
 * @brief Parameters for wallet status subscription
 * Matches JavaScript SDK WalletStatusSubscribe
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash to monitor */
    const char* wallet_address;      /**< Specific wallet address (optional) */
} knishio_subscribe_wallet_status_params_t;

/**
 * @brief Parameters for molecule creation subscription
 * Matches JavaScript SDK CreateMoleculeSubscribe
 */
typedef struct {
    const char* cell_slug;           /**< Cell to monitor */
    const char** molecular_hashes;   /**< Specific molecular hashes (optional) */
    size_t molecular_hash_count;     /**< Number of molecular hashes */
} knishio_subscribe_create_molecule_params_t;

/* Subscription handle for managing active subscriptions */
typedef struct knishio_subscription_handle knishio_subscription_handle_t;

/* Subscription execution functions */

/**
 * @brief Subscribe to active wallet updates
 * Equivalent to JavaScript: client.activeWalletSubscribe({ bundle })
 * 
 * @param client GraphQL client
 * @param params Subscription parameters
 * @param callback Callback function for updates
 * @param user_data User context data
 * @param handle Output subscription handle
 * @return Success or error code
 */
knishio_error_t knishio_subscribe_active_wallet(
    knishio_graphql_client_t* client,
    const knishio_subscribe_active_wallet_params_t* params,
    knishio_active_wallet_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle
);

/**
 * @brief Subscribe to active session updates
 * Equivalent to JavaScript: client.activeSessionSubscribe({ ... })
 * 
 * @param client GraphQL client
 * @param params Subscription parameters
 * @param callback Callback function for updates
 * @param user_data User context data
 * @param handle Output subscription handle
 * @return Success or error code
 */
knishio_error_t knishio_subscribe_active_session(
    knishio_graphql_client_t* client,
    const knishio_subscribe_active_session_params_t* params,
    knishio_active_session_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle
);

/**
 * @brief Subscribe to wallet status updates
 * Equivalent to JavaScript: client.walletStatusSubscribe({ ... })
 * 
 * @param client GraphQL client
 * @param params Subscription parameters
 * @param callback Callback function for updates
 * @param user_data User context data
 * @param handle Output subscription handle
 * @return Success or error code
 */
knishio_error_t knishio_subscribe_wallet_status(
    knishio_graphql_client_t* client,
    const knishio_subscribe_wallet_status_params_t* params,
    knishio_wallet_status_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle
);

/**
 * @brief Subscribe to molecule creation updates
 * Equivalent to JavaScript: client.createMoleculeSubscribe({ ... })
 * 
 * @param client GraphQL client
 * @param params Subscription parameters
 * @param callback Callback function for updates
 * @param user_data User context data
 * @param handle Output subscription handle
 * @return Success or error code
 */
knishio_error_t knishio_subscribe_create_molecule(
    knishio_graphql_client_t* client,
    const knishio_subscribe_create_molecule_params_t* params,
    knishio_create_molecule_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle
);

/* Subscription management functions */

/**
 * @brief Unsubscribe and free subscription handle
 * @param handle Subscription handle to close
 * @return Success or error code
 */
knishio_error_t knishio_subscription_unsubscribe(
    knishio_subscription_handle_t* handle
);

/**
 * @brief Check if subscription is still active
 * @param handle Subscription handle
 * @return True if active, false if closed
 */
bool knishio_subscription_is_active(
    const knishio_subscription_handle_t* handle
);

/**
 * @brief Get subscription error message
 * @param handle Subscription handle
 * @return Error message or NULL if no error
 */
const char* knishio_subscription_get_error(
    const knishio_subscription_handle_t* handle
);

/* Utility functions for building subscriptions */

/**
 * @brief Build active wallet subscription GraphQL string
 */
knishio_error_t knishio_build_active_wallet_subscription(
    const knishio_subscribe_active_wallet_params_t* params,
    char** subscription_string,
    char** variables_json
);

/**
 * @brief Build active session subscription GraphQL string
 */
knishio_error_t knishio_build_active_session_subscription(
    const knishio_subscribe_active_session_params_t* params,
    char** subscription_string,
    char** variables_json
);

/**
 * @brief Build wallet status subscription GraphQL string
 */
knishio_error_t knishio_build_wallet_status_subscription(
    const knishio_subscribe_wallet_status_params_t* params,
    char** subscription_string,
    char** variables_json
);

/**
 * @brief Build molecule creation subscription GraphQL string
 */
knishio_error_t knishio_build_create_molecule_subscription(
    const knishio_subscribe_create_molecule_params_t* params,
    char** subscription_string,
    char** variables_json
);

/* WebSocket configuration for subscriptions */

/**
 * @brief WebSocket configuration for subscriptions
 */
typedef struct {
    const char* ws_url;              /**< WebSocket URL */
    const char* protocol;            /**< WebSocket protocol */
    long timeout_ms;                 /**< Connection timeout */
    long heartbeat_interval_ms;      /**< Heartbeat interval */
    bool auto_reconnect;             /**< Auto-reconnect on disconnect */
    int max_reconnect_attempts;      /**< Maximum reconnect attempts */
} knishio_websocket_config_t;

/**
 * @brief Set WebSocket configuration for GraphQL client
 * @param client GraphQL client
 * @param config WebSocket configuration
 * @return Success or error code
 */
knishio_error_t knishio_graphql_client_set_websocket_config(
    knishio_graphql_client_t* client,
    const knishio_websocket_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_SUBSCRIPTIONS_H */