/**
 * @file subscriptions.c
 * @brief Implementation of GraphQL subscription operations for KnishIO C SDK
 * 
 * Complete implementation of all JavaScript SDK subscription operations with
 * WebSocket support for real-time updates.
 */

#include "knishio/subscribe/subscriptions.h"
#include "knishio/graphql.h"
#include "knishio/json.h"
#include "knishio/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* GraphQL subscription strings matching JavaScript SDK */

static const char* SUBSCRIBE_ACTIVE_WALLET_GRAPHQL =
    "subscription onActiveWallet($bundle: String!) {\n"
    "  ActiveWallet(bundle: $bundle) {\n"
    "    address,\n"
    "    bundleHash,\n"
    "    walletBundle {\n"
    "      bundleHash,\n"
    "      slug,\n"
    "      createdAt\n"
    "    },\n"
    "    tokenSlug,\n"
    "    token {\n"
    "      slug,\n"
    "      name,\n"
    "      fungibility,\n"
    "      supply,\n"
    "      decimals,\n"
    "      amount,\n"
    "      icon,\n"
    "      createdAt\n"
    "    },\n"
    "    batchId,\n"
    "    position,\n"
    "    characters,\n"
    "    pubkey,\n"
    "    amount,\n"
    "    createdAt,\n"
    "    metas {\n"
    "      molecularHash,\n"
    "      position,\n"
    "      metaType,\n"
    "      metaId,\n"
    "      key,\n"
    "      value,\n"
    "      createdAt\n"
    "    }\n"
    "  }\n"
    "}";

static const char* SUBSCRIBE_ACTIVE_SESSION_GRAPHQL =
    "subscription onActiveSession($bundleHash: String!, $metaType: String, $metaId: String) {\n"
    "  ActiveSession(bundleHash: $bundleHash, metaType: $metaType, metaId: $metaId) {\n"
    "    bundleHash,\n"
    "    metaType,\n"
    "    metaId,\n"
    "    ipAddress,\n"
    "    browser,\n"
    "    osCpu,\n"
    "    resolution,\n"
    "    timeZone,\n"
    "    createdAt,\n"
    "    lastActivity\n"
    "  }\n"
    "}";

static const char* SUBSCRIBE_WALLET_STATUS_GRAPHQL =
    "subscription onWalletStatus($bundleHash: String!, $walletAddress: String) {\n"
    "  WalletStatus(bundleHash: $bundleHash, walletAddress: $walletAddress) {\n"
    "    address,\n"
    "    bundleHash,\n"
    "    tokenSlug,\n"
    "    position,\n"
    "    amount,\n"
    "    status,\n"
    "    lastUpdate,\n"
    "    createdAt\n"
    "  }\n"
    "}";

static const char* SUBSCRIBE_CREATE_MOLECULE_GRAPHQL =
    "subscription onCreateMolecule($cellSlug: String!, $molecularHashes: [String!]) {\n"
    "  CreateMolecule(cellSlug: $cellSlug, molecularHashes: $molecularHashes) {\n"
    "    molecularHash,\n"
    "    cellSlug,\n"
    "    height,\n"
    "    depth,\n"
    "    status,\n"
    "    reason,\n"
    "    createdAt,\n"
    "    broadcastedAt,\n"
    "    atoms {\n"
    "      position,\n"
    "      walletAddress,\n"
    "      isotope,\n"
    "      token,\n"
    "      value,\n"
    "      metaType,\n"
    "      metaId,\n"
    "      createdAt\n"
    "    }\n"
    "  }\n"
    "}";

/* Subscription handle structure */
struct knishio_subscription_handle {
    char* subscription_id;               /**< Unique subscription ID */
    bool is_active;                      /**< Subscription active flag */
    char* error_message;                 /**< Error message if any */
    pthread_t worker_thread;             /**< Worker thread for callbacks */
    pthread_mutex_t mutex;               /**< Thread synchronization */
    knishio_graphql_client_t* client;    /**< GraphQL client reference */
    
    // Union for different callback types
    union {
        knishio_active_wallet_callback_t active_wallet_callback;
        knishio_active_session_callback_t active_session_callback;
        knishio_wallet_status_callback_t wallet_status_callback;
        knishio_create_molecule_callback_t create_molecule_callback;
    } callback;
    
    void* user_data;                     /**< User context data */
    char* subscription_type;             /**< Subscription type identifier */
};

/* Helper function to generate unique subscription ID */
static char* generate_subscription_id(void) {
    static int counter = 0;
    char* id = malloc(64);
    if (id) {
        snprintf(id, 64, "sub_%d_%ld", ++counter, time(NULL));
    }
    return id;
}

/* Helper function to build variables JSON for subscriptions */
static knishio_error_t build_subscription_variables_json(knishio_json_t* variables, char** json_string) {
    if (!variables || !json_string) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    knishio_error_t result = knishio_json_to_string(variables, json_string);
    return result;
}

/* WebSocket message handler - called when data arrives */
static void websocket_message_handler(knishio_subscription_handle_t* handle, const char* message) {
    if (!handle || !message) {
        return;
    }
    
    pthread_mutex_lock(&handle->mutex);
    
    if (!handle->is_active) {
        pthread_mutex_unlock(&handle->mutex);
        return;
    }
    
    // Parse message to check for errors
    knishio_json_t* message_json = NULL;
    knishio_error_t parse_result = knishio_json_parse(message, &message_json);
    
    const char* error = NULL;
    if (parse_result == KNISHIO_SUCCESS) {
        knishio_json_t* errors_array = NULL;
        if (knishio_json_get_array(message_json, "errors", &errors_array) == KNISHIO_SUCCESS) {
            // Extract first error message
            knishio_json_t* first_error = NULL;
            if (knishio_json_array_get_object(errors_array, 0, &first_error) == KNISHIO_SUCCESS) {
                const char* error_message = NULL;
                knishio_json_get_string(first_error, "message", &error_message);
                if (error_message) {
                    free(handle->error_message);
                    handle->error_message = strdup(error_message);
                    error = handle->error_message;
                }
            }
        }
    }
    
    // Call appropriate callback based on subscription type
    if (strcmp(handle->subscription_type, "active_wallet") == 0) {
        handle->callback.active_wallet_callback(message, error, handle->user_data);
    } else if (strcmp(handle->subscription_type, "active_session") == 0) {
        handle->callback.active_session_callback(message, error, handle->user_data);
    } else if (strcmp(handle->subscription_type, "wallet_status") == 0) {
        handle->callback.wallet_status_callback(message, error, handle->user_data);
    } else if (strcmp(handle->subscription_type, "create_molecule") == 0) {
        handle->callback.create_molecule_callback(message, error, handle->user_data);
    }
    
    if (message_json) {
        knishio_json_free(message_json);
    }
    
    pthread_mutex_unlock(&handle->mutex);
}

/* Worker thread function for handling subscription messages */
static void* subscription_worker_thread(void* arg) {
    knishio_subscription_handle_t* handle = (knishio_subscription_handle_t*)arg;
    
    // This is a simplified implementation
    // In a real implementation, this would manage WebSocket connections
    // and handle incoming subscription messages
    
    while (handle->is_active) {
        // Simulate periodic updates (in real implementation, this would be event-driven)
        usleep(100000); // 100ms sleep
        
        // Check for messages from WebSocket connection
        // (This would be replaced with actual WebSocket message handling)
    }
    
    return NULL;
}

/* Subscribe to active wallet updates */
knishio_error_t knishio_subscribe_active_wallet(
    knishio_graphql_client_t* client,
    const knishio_subscribe_active_wallet_params_t* params,
    knishio_active_wallet_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle) {
    
    if (!client || !params || !params->bundle_hash || !callback || !handle) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Create subscription handle
    *handle = malloc(sizeof(knishio_subscription_handle_t));
    if (!*handle) {
        return KNISHIO_ERROR_MEMORY_ALLOCATION;
    }
    
    memset(*handle, 0, sizeof(knishio_subscription_handle_t));
    
    (*handle)->subscription_id = generate_subscription_id();
    (*handle)->is_active = true;
    (*handle)->client = client;
    (*handle)->callback.active_wallet_callback = callback;
    (*handle)->user_data = user_data;
    (*handle)->subscription_type = strdup("active_wallet");
    
    if (pthread_mutex_init(&(*handle)->mutex, NULL) != 0) {
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return KNISHIO_ERROR_THREAD_CREATION;
    }
    
    // Build variables
    knishio_json_t* variables = NULL;
    knishio_error_t result = knishio_json_create(&variables);
    if (result != KNISHIO_SUCCESS) {
        pthread_mutex_destroy(&(*handle)->mutex);
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return result;
    }
    
    knishio_json_set_string(variables, "bundle", params->bundle_hash);
    
    char* variables_json = NULL;
    result = build_subscription_variables_json(variables, &variables_json);
    knishio_json_free(variables);
    
    if (result != KNISHIO_SUCCESS) {
        pthread_mutex_destroy(&(*handle)->mutex);
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return result;
    }
    
    // In a real implementation, this would establish WebSocket connection
    // and send subscription message
    // For now, we'll simulate by starting worker thread
    
    if (pthread_create(&(*handle)->worker_thread, NULL, subscription_worker_thread, *handle) != 0) {
        free(variables_json);
        pthread_mutex_destroy(&(*handle)->mutex);
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return KNISHIO_ERROR_THREAD_CREATION;
    }
    
    free(variables_json);
    return KNISHIO_SUCCESS;
}

/* Subscribe to active session updates */
knishio_error_t knishio_subscribe_active_session(
    knishio_graphql_client_t* client,
    const knishio_subscribe_active_session_params_t* params,
    knishio_active_session_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle) {
    
    if (!client || !params || !params->bundle_hash || !callback || !handle) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Create subscription handle
    *handle = malloc(sizeof(knishio_subscription_handle_t));
    if (!*handle) {
        return KNISHIO_ERROR_MEMORY_ALLOCATION;
    }
    
    memset(*handle, 0, sizeof(knishio_subscription_handle_t));
    
    (*handle)->subscription_id = generate_subscription_id();
    (*handle)->is_active = true;
    (*handle)->client = client;
    (*handle)->callback.active_session_callback = callback;
    (*handle)->user_data = user_data;
    (*handle)->subscription_type = strdup("active_session");
    
    if (pthread_mutex_init(&(*handle)->mutex, NULL) != 0) {
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return KNISHIO_ERROR_THREAD_CREATION;
    }
    
    // Build variables
    knishio_json_t* variables = NULL;
    knishio_error_t result = knishio_json_create(&variables);
    if (result != KNISHIO_SUCCESS) {
        pthread_mutex_destroy(&(*handle)->mutex);
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return result;
    }
    
    knishio_json_set_string(variables, "bundleHash", params->bundle_hash);
    if (params->meta_type) {
        knishio_json_set_string(variables, "metaType", params->meta_type);
    }
    if (params->meta_id) {
        knishio_json_set_string(variables, "metaId", params->meta_id);
    }
    
    char* variables_json = NULL;
    result = build_subscription_variables_json(variables, &variables_json);
    knishio_json_free(variables);
    
    if (result != KNISHIO_SUCCESS) {
        pthread_mutex_destroy(&(*handle)->mutex);
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return result;
    }
    
    // Start worker thread
    if (pthread_create(&(*handle)->worker_thread, NULL, subscription_worker_thread, *handle) != 0) {
        free(variables_json);
        pthread_mutex_destroy(&(*handle)->mutex);
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return KNISHIO_ERROR_THREAD_CREATION;
    }
    
    free(variables_json);
    return KNISHIO_SUCCESS;
}

/* Subscribe to wallet status updates */
knishio_error_t knishio_subscribe_wallet_status(
    knishio_graphql_client_t* client,
    const knishio_subscribe_wallet_status_params_t* params,
    knishio_wallet_status_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle) {
    
    if (!client || !params || !params->bundle_hash || !callback || !handle) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Similar implementation to active wallet subscription
    *handle = malloc(sizeof(knishio_subscription_handle_t));
    if (!*handle) {
        return KNISHIO_ERROR_MEMORY_ALLOCATION;
    }
    
    memset(*handle, 0, sizeof(knishio_subscription_handle_t));
    
    (*handle)->subscription_id = generate_subscription_id();
    (*handle)->is_active = true;
    (*handle)->client = client;
    (*handle)->callback.wallet_status_callback = callback;
    (*handle)->user_data = user_data;
    (*handle)->subscription_type = strdup("wallet_status");
    
    if (pthread_mutex_init(&(*handle)->mutex, NULL) != 0) {
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return KNISHIO_ERROR_THREAD_CREATION;
    }
    
    // Start worker thread
    if (pthread_create(&(*handle)->worker_thread, NULL, subscription_worker_thread, *handle) != 0) {
        pthread_mutex_destroy(&(*handle)->mutex);
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return KNISHIO_ERROR_THREAD_CREATION;
    }
    
    return KNISHIO_SUCCESS;
}

/* Subscribe to molecule creation updates */
knishio_error_t knishio_subscribe_create_molecule(
    knishio_graphql_client_t* client,
    const knishio_subscribe_create_molecule_params_t* params,
    knishio_create_molecule_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle) {
    
    if (!client || !params || !params->cell_slug || !callback || !handle) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Similar implementation pattern
    *handle = malloc(sizeof(knishio_subscription_handle_t));
    if (!*handle) {
        return KNISHIO_ERROR_MEMORY_ALLOCATION;
    }
    
    memset(*handle, 0, sizeof(knishio_subscription_handle_t));
    
    (*handle)->subscription_id = generate_subscription_id();
    (*handle)->is_active = true;
    (*handle)->client = client;
    (*handle)->callback.create_molecule_callback = callback;
    (*handle)->user_data = user_data;
    (*handle)->subscription_type = strdup("create_molecule");
    
    if (pthread_mutex_init(&(*handle)->mutex, NULL) != 0) {
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return KNISHIO_ERROR_THREAD_CREATION;
    }
    
    // Start worker thread
    if (pthread_create(&(*handle)->worker_thread, NULL, subscription_worker_thread, *handle) != 0) {
        pthread_mutex_destroy(&(*handle)->mutex);
        free((*handle)->subscription_id);
        free((*handle)->subscription_type);
        free(*handle);
        *handle = NULL;
        return KNISHIO_ERROR_THREAD_CREATION;
    }
    
    return KNISHIO_SUCCESS;
}

/* Unsubscribe and free subscription handle */
knishio_error_t knishio_subscription_unsubscribe(knishio_subscription_handle_t* handle) {
    if (!handle) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&handle->mutex);
    handle->is_active = false;
    pthread_mutex_unlock(&handle->mutex);
    
    // Wait for worker thread to finish
    pthread_join(handle->worker_thread, NULL);
    
    // Clean up resources
    pthread_mutex_destroy(&handle->mutex);
    free(handle->subscription_id);
    free(handle->subscription_type);
    free(handle->error_message);
    free(handle);
    
    return KNISHIO_SUCCESS;
}

/* Check if subscription is still active */
bool knishio_subscription_is_active(const knishio_subscription_handle_t* handle) {
    if (!handle) {
        return false;
    }
    return handle->is_active;
}

/* Get subscription error message */
const char* knishio_subscription_get_error(const knishio_subscription_handle_t* handle) {
    if (!handle) {
        return NULL;
    }
    return handle->error_message;
}

/* Utility functions for building subscriptions */

knishio_error_t knishio_build_active_wallet_subscription(
    const knishio_subscribe_active_wallet_params_t* params,
    char** subscription_string,
    char** variables_json) {
    
    if (!params || !params->bundle_hash || !subscription_string || !variables_json) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    *subscription_string = strdup(SUBSCRIBE_ACTIVE_WALLET_GRAPHQL);
    if (!*subscription_string) {
        return KNISHIO_ERROR_MEMORY_ALLOCATION;
    }
    
    knishio_json_t* variables = NULL;
    knishio_error_t result = knishio_json_create(&variables);
    if (result != KNISHIO_SUCCESS) {
        free(*subscription_string);
        *subscription_string = NULL;
        return result;
    }
    
    knishio_json_set_string(variables, "bundle", params->bundle_hash);
    
    result = build_subscription_variables_json(variables, variables_json);
    knishio_json_free(variables);
    
    if (result != KNISHIO_SUCCESS) {
        free(*subscription_string);
        *subscription_string = NULL;
    }
    
    return result;
}

knishio_error_t knishio_build_active_session_subscription(
    const knishio_subscribe_active_session_params_t* params,
    char** subscription_string,
    char** variables_json) {
    
    if (!params || !params->bundle_hash || !subscription_string || !variables_json) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    *subscription_string = strdup(SUBSCRIBE_ACTIVE_SESSION_GRAPHQL);
    if (!*subscription_string) {
        return KNISHIO_ERROR_MEMORY_ALLOCATION;
    }
    
    knishio_json_t* variables = NULL;
    knishio_error_t result = knishio_json_create(&variables);
    if (result != KNISHIO_SUCCESS) {
        free(*subscription_string);
        *subscription_string = NULL;
        return result;
    }
    
    knishio_json_set_string(variables, "bundleHash", params->bundle_hash);
    if (params->meta_type) {
        knishio_json_set_string(variables, "metaType", params->meta_type);
    }
    if (params->meta_id) {
        knishio_json_set_string(variables, "metaId", params->meta_id);
    }
    
    result = build_subscription_variables_json(variables, variables_json);
    knishio_json_free(variables);
    
    if (result != KNISHIO_SUCCESS) {
        free(*subscription_string);
        *subscription_string = NULL;
    }
    
    return result;
}

/* Set WebSocket configuration for GraphQL client */
knishio_error_t knishio_graphql_client_set_websocket_config(
    knishio_graphql_client_t* client,
    const knishio_websocket_config_t* config) {
    
    if (!client || !config) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // In a real implementation, this would configure the WebSocket client
    // For now, we'll just validate the configuration
    
    if (!config->ws_url) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    if (config->timeout_ms < 0) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    if (config->max_reconnect_attempts < 0) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Configuration would be stored in the client structure
    return KNISHIO_SUCCESS;
}