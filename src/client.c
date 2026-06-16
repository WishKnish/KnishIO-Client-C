#include "knishio/knishio.h"
#include "knishio/auth_token.h"
#include "knishio/client_auth.h"
#include "knishio/client_ops.h"
#include "knishio/wallet.h"
#include "knishio/http.h"
#include "knishio/graphql.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"
#include "client_internal.h"
#include <string.h>
#include <time.h>

/* Client structure - expanded for authentication support */
struct knishio_client {
    char *uri;                          /**< GraphQL endpoint URI */
    char *cell_slug;                    /**< Cell identifier */
    knishio_http_client_t *http_client; /**< HTTP client for requests */
    knishio_auth_token_t *auth_token;   /**< Current authentication token (legacy) */
    bool initialized;                   /**< Initialization status */
    knishio_client_auth_state_t auth_state; /**< Authentication state */
};

/* Client management implementations */
knishio_error_t knishio_client_create(knishio_client_t **client, const knishio_client_config_t *config) {
    if (client == NULL || config == NULL) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    knishio_client_t *new_client = knishio_malloc(sizeof(knishio_client_t));
    if (new_client == NULL) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    // Initialize fields
    new_client->uri = config->uri ? knishio_strdup(config->uri) : NULL;
    new_client->cell_slug = config->cell_slug ? knishio_strdup(config->cell_slug) : NULL;
    new_client->auth_token = NULL;
    new_client->initialized = false;
    
    // Initialize authentication state
    memset(&new_client->auth_state, 0, sizeof(knishio_client_auth_state_t));
    new_client->auth_state.refresh_threshold_ms = 300000; // 5 minutes default
    
    // Create HTTP client
    knishio_error_t http_result = knishio_http_client_create(&new_client->http_client, config->uri);
    if (http_result != KNISHIO_SUCCESS) {
        knishio_client_destroy(new_client);
        return http_result;
    }
    
    new_client->initialized = true;
    *client = new_client;
    return KNISHIO_SUCCESS;
}

void knishio_client_destroy(knishio_client_t *client) {
    if (client == NULL) {
        return;
    }
    
    // Clean up HTTP client
    if (client->http_client) {
        knishio_http_client_free(client->http_client);
    }
    
    // Clean up auth token
    if (client->auth_token) {
        knishio_auth_token_cleanup(client->auth_token);
    }
    
    // Clean up authentication state
    if (client->auth_state.current_token) {
        knishio_auth_token_cleanup(client->auth_state.current_token);
    }
    if (client->auth_state.secret) {
        knishio_free(client->auth_state.secret);
    }
    if (client->auth_state.cell_slug) {
        knishio_free(client->auth_state.cell_slug);
    }
    
    // Clean up strings
    knishio_free(client->uri);
    knishio_free(client->cell_slug);
    
    // Free the client structure
    knishio_free(client);
}

/* Authentication function implementations */

/* 
 * Legacy authentication function - DEPRECATED
 * Use knishio_client_authenticate() or knishio_client_authenticate_guest() instead
 */
knishio_error_t knishio_client_request_auth_token(knishio_client_t *client,
                                                  knishio_wallet_t *wallet,
                                                  knishio_auth_token_t **auth_token) {
    if (!client || !wallet || !auth_token) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!client->initialized || !client->http_client) {
        return KNISHIO_ERROR_INVALID_STATE; // Client not properly initialized
    }
    
    /* This function is deprecated and should not be used.
     * Use the new authentication system:
     * - knishio_client_authenticate() for profile authentication
     * - knishio_client_authenticate_guest() for guest authentication
     */
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
}

knishio_error_t knishio_client_set_auth_token(knishio_client_t *client,
                                              knishio_auth_token_t *auth_token) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!client->initialized || !client->http_client) {
        return KNISHIO_ERROR_INVALID_STATE; // Client not properly initialized
    }
    
    // Clear existing auth token
    if (client->auth_token) {
        knishio_auth_token_cleanup(client->auth_token);
        client->auth_token = NULL;
    }
    
    // Set new auth token
    client->auth_token = auth_token;
    
    // Update HTTP client with auth token
    if (auth_token) {
        const char *token = knishio_auth_token_get_token(auth_token);
        const char *pubkey = knishio_auth_token_get_pubkey(auth_token);
        
        if (token) {
            knishio_error_t result = knishio_http_client_set_auth_token(client->http_client, token);
            if (result != KNISHIO_SUCCESS) {
                return result;
            }
        }
        
        if (pubkey) {
            knishio_error_t result = knishio_http_client_set_auth_pubkey(client->http_client, pubkey);
            if (result != KNISHIO_SUCCESS) {
                return result;
            }
        }
    }
    
    return KNISHIO_SUCCESS;
}

knishio_auth_token_t* knishio_client_get_auth_token(const knishio_client_t* client) {
    if (!client) {
        return NULL;
    }
    
    return client->auth_token;
}

knishio_error_t knishio_client_clear_auth(knishio_client_t *client) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    // Clear auth token
    if (client->auth_token) {
        knishio_auth_token_cleanup(client->auth_token);
        client->auth_token = NULL;
    }
    
    // Clear HTTP client auth
    if (client->http_client) {
        knishio_http_client_clear_auth(client->http_client);
    }

    return KNISHIO_SUCCESS;
}

/* Build a proper graphql client from this client (its own http_client to client->uri + the client's
 * auth token), submit the operation, and free the graphql client. Replaces the prior
 * (knishio_graphql_client_t*)client cast in the create ops — knishio_client_t and
 * knishio_graphql_client_t are different structs, so the cast never propagated the auth token to
 * the X-Auth-Token header (and was UB). knishio_graphql_client_create makes its OWN http_client to
 * client->uri, so client->http_client is untouched and graphql_client_free is safe. */
knishio_error_t knishio_client_execute_graphql(
    knishio_client_t* client,
    const knishio_graphql_operation_t* operation,
    knishio_graphql_response_t** response
) {
    if (!client || !operation || !response) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!client->uri) {
        return KNISHIO_ERROR_INVALID_STATE;
    }

    knishio_graphql_client_t* gql = NULL;
    knishio_error_t error = knishio_graphql_client_create(&gql, client->uri, client->cell_slug);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Propagate the client's auth token (slice 2a: manually-set; the full JWT flow is slice 2b).
     * graphql_client_set_auth_token pushes it into the http client's X-Auth-Token header. */
    if (client->auth_token) {
        knishio_graphql_client_set_auth_token(gql, client->auth_token);
    }

    error = knishio_graphql_execute(gql, operation, response);

    knishio_graphql_client_free(gql);
    return error;
}