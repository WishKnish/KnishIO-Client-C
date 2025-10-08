#ifndef KNISHIO_CLIENT_H
#define KNISHIO_CLIENT_H

/**
 * @file client.h
 * @brief Main KnishIO client interface
 * 
 * Provides the primary client API for interacting with KnishIO distributed ledger
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_client knishio_client_t;

/* knishio_error_t and knishio_client_config_t will be available when included via knishio.h */

/* Forward declarations for auth integration */
typedef struct knishio_auth_token knishio_auth_token_t;
typedef struct knishio_wallet knishio_wallet_t;

/* Client management functions */

/**
 * @brief Create a new KnishIO client
 * @param client Client pointer (output)
 * @param config Client configuration
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_create(knishio_client_t **client, const knishio_client_config_t *config);

/**
 * @brief Destroy KnishIO client
 * @param client Client to destroy
 */
void knishio_client_destroy(knishio_client_t *client);

/* Authentication functions */

/**
 * @brief Request authentication token from server
 * Equivalent to JavaScript SDK authentication request functionality
 * 
 * @param client KnishIO client instance
 * @param wallet Wallet to authenticate with
 * @param auth_token Output authentication token (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_request_auth_token(knishio_client_t *client,
                                                  knishio_wallet_t *wallet,
                                                  knishio_auth_token_t **auth_token);

/**
 * @brief Set authentication token for client
 * All subsequent requests will use this token for authentication
 * 
 * @param client KnishIO client instance
 * @param auth_token Authentication token to use
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_set_auth_token(knishio_client_t *client,
                                              knishio_auth_token_t *auth_token);

/**
 * @brief Get current authentication token from client
 * 
 * @param client KnishIO client instance
 * @return Current auth token or NULL if not authenticated
 */
knishio_auth_token_t* knishio_client_get_auth_token(const knishio_client_t* client);

/**
 * @brief Clear authentication from client
 * Removes authentication token and clears auth headers
 * 
 * @param client KnishIO client instance
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_clear_auth(knishio_client_t *client);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CLIENT_H */
