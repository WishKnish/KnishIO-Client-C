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
#include "knishio/wallet.h"
#include "knishio/http.h"
#include "knishio/storage/provider.h"
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
    knishio_secret_storage_provider_t *secret_storage;  /**< Attached provider, NOT owned */
    char *bundle_hash;                                  /**< Bundle hash of the active secret (owned) or NULL */
    char *storage_label;                                /**< Owned copy of label */
    char *storage_passphrase;                           /**< Owned copy of primary passphrase */
    char *storage_recovery_passphrase;                  /**< Owned copy of recovery passphrase */
    bool storage_allow_unrecoverable;                   /**< Allow unrecoverable flag */
} knishio_client_auth_state_t;

/**
 * @brief Client structure - internal definition shared across client modules
 */
struct knishio_client {
    char *uri;                          /**< GraphQL endpoint URI */
    char *cell_slug;                    /**< Cell identifier */
    knishio_http_client_t *http_client; /**< HTTP client for requests */
    knishio_auth_token_t *auth_token;   /**< Current authentication token (legacy) */
    bool insecure_tls;                  /**< Skip TLS cert verification (dev/self-signed validators) */
    bool initialized;                   /**< Initialization status */
    knishio_client_auth_state_t auth_state; /**< Authentication state */

    /* PQ-transport (Phase E): ML-KEM CipherHash encrypted transport context (set at auth). */
    bool cipher_enabled;                /**< Encrypt subsequent ops via CipherHash */
    char *cipher_server_pubkey;         /**< Validator's ML-KEM pubkey (base64), for encrypt */
    knishio_wallet_t *cipher_wallet;    /**< Owned copy of the auth signing wallet's identity */
    int mlkem_parameter_set;            /**< ML-KEM parameter set: 1024 or 768 */
};

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CLIENT_INTERNAL_H */