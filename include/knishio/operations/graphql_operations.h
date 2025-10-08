#ifndef KNISHIO_GRAPHQL_OPERATIONS_H
#define KNISHIO_GRAPHQL_OPERATIONS_H

/**
 * @file graphql_operations.h
 * @brief Complete GraphQL operations integration for KnishIO C SDK
 * 
 * Unified interface for all GraphQL operations matching JavaScript SDK functionality:
 * - Complete query operations (15+ types)
 * - Complete mutation operations (20+ types)  
 * - Complete subscription operations (WebSocket-based)
 * - High-level client operations
 * - Complex transaction patterns
 * - Batch operations for efficiency
 * 
 * This header provides a single point of access to all GraphQL operations
 * with full JavaScript SDK compatibility.
 */

#include "knishio/query/queries.h"
#include "knishio/mutation/mutations.h"
#include "knishio/subscribe/subscriptions.h"
#include "knishio/graphql.h"
#include "knishio/client.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_client knishio_client_t;
typedef struct knishio_graphql_client knishio_graphql_client_t;

/**
 * @brief Complete GraphQL operations manager
 * Encapsulates all GraphQL functionality in a single structure
 */
typedef struct knishio_graphql_operations {
    knishio_graphql_client_t* client;    /**< Underlying GraphQL client */
    knishio_client_t* knishio_client;    /**< Main KnishIO client */
    bool debug_mode;                     /**< Debug mode flag */
    char* endpoint_url;                  /**< GraphQL endpoint */
    char* cell_slug;                     /**< Current cell */
} knishio_graphql_operations_t;

/* High-level operations manager lifecycle */

/**
 * @brief Create GraphQL operations manager
 * @param ops Output operations manager
 * @param knishio_client Main KnishIO client
 * @param endpoint_url GraphQL endpoint URL
 * @param cell_slug Cell identifier
 * @return Success or error code
 */
knishio_error_t knishio_graphql_operations_create(
    knishio_graphql_operations_t** ops,
    knishio_client_t* knishio_client,
    const char* endpoint_url,
    const char* cell_slug
);

/**
 * @brief Free GraphQL operations manager
 * @param ops Operations manager to free
 */
void knishio_graphql_operations_free(knishio_graphql_operations_t* ops);

/**
 * @brief Set debug mode for all operations
 * @param ops Operations manager
 * @param debug_mode Enable/disable debug mode
 * @return Success or error code
 */
knishio_error_t knishio_graphql_operations_set_debug(
    knishio_graphql_operations_t* ops,
    bool debug_mode
);

/**
 * @brief Set authentication token for all operations
 * @param ops Operations manager
 * @param auth_token Authentication token
 * @return Success or error code
 */
knishio_error_t knishio_graphql_operations_set_auth_token(
    knishio_graphql_operations_t* ops,
    const char* auth_token
);

/* High-level client operations matching JavaScript SDK */

/**
 * @brief Complete balance query with automatic response handling
 * Equivalent to JavaScript: client.queryBalance({ address, token })
 * 
 * @param ops Operations manager
 * @param wallet_address Wallet address to query
 * @param token_slug Token to query (NULL for all tokens)
 * @param response Output balance response
 * @return Success or error code
 */
knishio_error_t knishio_operations_query_balance_simple(
    knishio_graphql_operations_t* ops,
    const char* wallet_address,
    const char* token_slug,
    knishio_response_balance_t** response
);

/**
 * @brief Complete token transfer with automatic molecule composition
 * Equivalent to JavaScript: client.transferTokens({ recipient, amount, token })
 * 
 * @param ops Operations manager
 * @param source_wallet Source wallet
 * @param recipient_address Recipient wallet address
 * @param token_slug Token to transfer
 * @param amount Amount to transfer
 * @param response Output transfer response
 * @return Success or error code
 */
knishio_error_t knishio_operations_transfer_tokens_simple(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* source_wallet,
    const char* recipient_address,
    const char* token_slug,
    const char* amount,
    knishio_response_transfer_tokens_t** response
);

/**
 * @brief Complete token creation with automatic molecule composition
 * Equivalent to JavaScript: client.createToken({ recipient, token, amount })
 * 
 * @param ops Operations manager
 * @param recipient_wallet Recipient wallet
 * @param token_slug Token identifier
 * @param amount Initial token amount
 * @param meta_json Additional metadata (JSON string)
 * @param response Output creation response
 * @return Success or error code
 */
knishio_error_t knishio_operations_create_token_simple(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* recipient_wallet,
    const char* token_slug,
    const char* amount,
    const char* meta_json,
    knishio_response_create_token_t** response
);

/**
 * @brief Request tokens from faucet with automatic composition
 * Equivalent to JavaScript: client.requestTokens({ token, amount })
 * 
 * @param ops Operations manager
 * @param wallet Requesting wallet
 * @param token_slug Token to request
 * @param amount Amount to request
 * @param response Output request response
 * @return Success or error code
 */
knishio_error_t knishio_operations_request_tokens_simple(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* wallet,
    const char* token_slug,
    const char* amount,
    knishio_response_request_tokens_t** response
);

/* Complex transaction patterns */

/**
 * @brief Multi-party token transfer
 * Transfers tokens to multiple recipients in a single molecule
 * 
 * @param ops Operations manager
 * @param source_wallet Source wallet
 * @param recipients Array of recipient structures
 * @param recipient_count Number of recipients
 * @param token_slug Token to transfer
 * @param response Output transfer response
 * @return Success or error code
 */
typedef struct {
    const char* wallet_address;      /**< Recipient address */
    const char* amount;              /**< Amount to transfer */
    const char* memo;                /**< Optional memo */
} knishio_transfer_recipient_t;

knishio_error_t knishio_operations_transfer_tokens_multi(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* source_wallet,
    const knishio_transfer_recipient_t* recipients,
    size_t recipient_count,
    const char* token_slug,
    knishio_response_transfer_tokens_t** response
);

/**
 * @brief Token swap operation
 * Atomic swap of two different tokens between parties
 * 
 * @param ops Operations manager
 * @param party1_wallet First party wallet
 * @param party2_wallet Second party wallet
 * @param token1_slug First token type
 * @param amount1 Amount of first token
 * @param token2_slug Second token type
 * @param amount2 Amount of second token
 * @param response Output swap response
 * @return Success or error code
 */
knishio_error_t knishio_operations_swap_tokens(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* party1_wallet,
    knishio_wallet_t* party2_wallet,
    const char* token1_slug,
    const char* amount1,
    const char* token2_slug,
    const char* amount2,
    knishio_response_transfer_tokens_t** response
);

/**
 * @brief Create metadata with automatic molecule composition
 * Equivalent to JavaScript: client.createMeta({ metaType, metaId, meta })
 * 
 * @param ops Operations manager
 * @param wallet Source wallet
 * @param meta_type Metadata type
 * @param meta_id Metadata identifier
 * @param meta_json Metadata content (JSON string)
 * @param response Output metadata response
 * @return Success or error code
 */
knishio_error_t knishio_operations_create_meta_simple(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* wallet,
    const char* meta_type,
    const char* meta_id,
    const char* meta_json,
    knishio_response_create_meta_t** response
);

/* Batch operations for efficiency */

/**
 * @brief Batch query operation
 * Execute multiple queries in parallel for efficiency
 * 
 * @param ops Operations manager
 * @param query_types Array of query type identifiers
 * @param query_params Array of query parameter structures
 * @param query_count Number of queries
 * @param responses Output array of responses
 * @return Success or error code
 */
typedef enum {
    KNISHIO_QUERY_BALANCE,
    KNISHIO_QUERY_WALLET_LIST,
    KNISHIO_QUERY_TOKEN,
    KNISHIO_QUERY_ATOM,
    KNISHIO_QUERY_ACTIVE_SESSION
} knishio_query_type_t;

knishio_error_t knishio_operations_batch_query(
    knishio_graphql_operations_t* ops,
    const knishio_query_type_t* query_types,
    const void** query_params,
    size_t query_count,
    void** responses
);

/**
 * @brief Batch mutation operation
 * Execute multiple mutations in sequence with dependency handling
 * 
 * @param ops Operations manager
 * @param mutation_types Array of mutation type identifiers
 * @param mutation_params Array of mutation parameter structures
 * @param mutation_count Number of mutations
 * @param responses Output array of responses
 * @return Success or error code
 */
typedef enum {
    KNISHIO_MUTATION_CREATE_TOKEN,
    KNISHIO_MUTATION_TRANSFER_TOKENS,
    KNISHIO_MUTATION_REQUEST_TOKENS,
    KNISHIO_MUTATION_CREATE_META
} knishio_mutation_type_t;

knishio_error_t knishio_operations_batch_mutation(
    knishio_graphql_operations_t* ops,
    const knishio_mutation_type_t* mutation_types,
    const void** mutation_params,
    size_t mutation_count,
    void** responses
);

/* Authentication operations */

/**
 * @brief Authenticate with profile credentials
 * Equivalent to JavaScript: client.requestAuthorization({ secret, cell })
 * 
 * @param ops Operations manager
 * @param secret User secret
 * @param cell_slug Cell identifier
 * @param response Output authorization response
 * @return Success or error code
 */
knishio_error_t knishio_operations_authenticate_profile(
    knishio_graphql_operations_t* ops,
    const char* secret,
    const char* cell_slug,
    knishio_response_request_authorization_t** response
);

/**
 * @brief Authenticate as guest user
 * Equivalent to JavaScript: client.requestAuthorizationGuest({ cell })
 * 
 * @param ops Operations manager
 * @param cell_slug Cell identifier
 * @param response Output authorization response
 * @return Success or error code
 */
knishio_error_t knishio_operations_authenticate_guest(
    knishio_graphql_operations_t* ops,
    const char* cell_slug,
    knishio_response_request_authorization_guest_t** response
);

/* Session management operations */

/**
 * @brief Start active session with metadata
 * Equivalent to JavaScript: client.activeSession({ meta, ipAddress, browser })
 * 
 * @param ops Operations manager
 * @param wallet Session wallet
 * @param meta_type Session metadata type
 * @param meta_id Session metadata ID
 * @param ip_address Client IP address
 * @param browser_info Browser information
 * @param response Output session response
 * @return Success or error code
 */
knishio_error_t knishio_operations_start_session(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* wallet,
    const char* meta_type,
    const char* meta_id,
    const char* ip_address,
    const char* browser_info,
    knishio_response_active_session_t** response
);

/* Subscription management */

/**
 * @brief Subscribe to wallet balance changes
 * Monitor wallet for balance updates in real-time
 * 
 * @param ops Operations manager
 * @param bundle_hash Bundle to monitor
 * @param callback Callback for balance updates
 * @param user_data User context data
 * @param handle Output subscription handle
 * @return Success or error code
 */
knishio_error_t knishio_operations_subscribe_wallet_balance(
    knishio_graphql_operations_t* ops,
    const char* bundle_hash,
    knishio_active_wallet_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle
);

/**
 * @brief Subscribe to molecule proposals
 * Monitor cell for new molecule proposals
 * 
 * @param ops Operations manager
 * @param cell_slug Cell to monitor
 * @param callback Callback for molecule updates
 * @param user_data User context data
 * @param handle Output subscription handle
 * @return Success or error code
 */
knishio_error_t knishio_operations_subscribe_molecules(
    knishio_graphql_operations_t* ops,
    const char* cell_slug,
    knishio_create_molecule_callback_t callback,
    void* user_data,
    knishio_subscription_handle_t** handle
);

/* Utility and helper functions */

/**
 * @brief Get wallet balance using simple interface
 * @param ops Operations manager
 * @param wallet_address Wallet address
 * @param token_slug Token type
 * @param balance_out Output balance string
 * @return Success or error code
 */
knishio_error_t knishio_operations_get_balance_string(
    knishio_graphql_operations_t* ops,
    const char* wallet_address,
    const char* token_slug,
    char** balance_out
);

/**
 * @brief Check if wallet has sufficient balance
 * @param ops Operations manager
 * @param wallet_address Wallet address
 * @param token_slug Token type
 * @param required_amount Required amount
 * @param sufficient_out Output flag
 * @return Success or error code
 */
knishio_error_t knishio_operations_check_sufficient_balance(
    knishio_graphql_operations_t* ops,
    const char* wallet_address,
    const char* token_slug,
    const char* required_amount,
    bool* sufficient_out
);

/**
 * @brief Get transaction status by molecular hash
 * @param ops Operations manager
 * @param molecular_hash Molecular hash to check
 * @param status_out Output status string
 * @return Success or error code
 */
knishio_error_t knishio_operations_get_transaction_status(
    knishio_graphql_operations_t* ops,
    const char* molecular_hash,
    char** status_out
);

/**
 * @brief Wait for transaction confirmation
 * Polls transaction status until confirmed or timeout
 * 
 * @param ops Operations manager
 * @param molecular_hash Molecular hash to monitor
 * @param timeout_ms Timeout in milliseconds
 * @param confirmed_out Output confirmation flag
 * @return Success or error code
 */
knishio_error_t knishio_operations_wait_for_confirmation(
    knishio_graphql_operations_t* ops,
    const char* molecular_hash,
    long timeout_ms,
    bool* confirmed_out
);

/* Direct access to underlying GraphQL client */

/**
 * @brief Get underlying GraphQL client for advanced operations
 * @param ops Operations manager
 * @return GraphQL client (do not free)
 */
knishio_graphql_client_t* knishio_operations_get_graphql_client(
    knishio_graphql_operations_t* ops
);

/**
 * @brief Execute raw GraphQL query
 * For advanced users who need direct GraphQL access
 * 
 * @param ops Operations manager
 * @param query_string GraphQL query
 * @param variables_json Variables JSON
 * @param response_out Output response
 * @return Success or error code
 */
knishio_error_t knishio_operations_execute_raw_query(
    knishio_graphql_operations_t* ops,
    const char* query_string,
    const char* variables_json,
    char** response_out
);

/**
 * @brief Execute raw GraphQL mutation
 * For advanced users who need direct GraphQL access
 * 
 * @param ops Operations manager
 * @param mutation_string GraphQL mutation
 * @param variables_json Variables JSON
 * @param response_out Output response
 * @return Success or error code
 */
knishio_error_t knishio_operations_execute_raw_mutation(
    knishio_graphql_operations_t* ops,
    const char* mutation_string,
    const char* variables_json,
    char** response_out
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_GRAPHQL_OPERATIONS_H */