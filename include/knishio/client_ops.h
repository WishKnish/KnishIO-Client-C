#ifndef KNISHIO_CLIENT_OPS_H
#define KNISHIO_CLIENT_OPS_H

/**
 * @file client_ops.h
 * @brief Unified client operations API for KnishIO C SDK
 * 
 * Provides complete functional alignment with JavaScript SDK.
 * All high-level operations matching JS KnishIOClient methods.
 * 
 * This is the primary header for client operations, providing:
 * - Token transfers and balance queries
 * - Wallet creation and management
 * - Token creation and minting
 * - Meta operations
 * - Full DLT functionality
 */

/* Include all operation headers for complete API */
#include "knishio/operations/transfer.h"
#include "knishio/operations/wallet.h"
#include "knishio/operations/token.h"

/* Core includes */
#include "knishio/client.h"
#include "knishio/wallet.h"
#include "knishio/molecule.h"
#include "knishio/atom.h"
#include "knishio/auth_token.h"
#include "knishio/meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Extended client structure with GraphQL support
 * Internal use only - users should use knishio_client_t
 */
typedef struct knishio_client_extended {
    knishio_client_t* base;                   /**< Base client pointer */
    struct knishio_graphql_client* graphql;   /**< GraphQL client */
    char* bundle_hash;                        /**< Client bundle hash */
    char* secret;                              /**< Client secret */
} knishio_client_extended_t;

/* High-level client operations for full JS SDK alignment */

/**
 * @brief Initialize client with authentication
 * Combines client creation with auth token request
 * Equivalent to JS: new KnishIOClient() + requestAuthToken()
 * 
 * @param client Output client pointer
 * @param uri GraphQL endpoint URI
 * @param secret User secret for authentication
 * @param cell_slug Cell identifier (optional)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_init_authenticated(
    knishio_client_t** client,
    const char* uri,
    const char* secret,
    const char* cell_slug
);

/**
 * @brief Get client bundle hash
 * Equivalent to JavaScript: client.getBundle()
 * 
 * @param client KnishIO client instance
 * @return Bundle hash or NULL if not set (do not free)
 */
const char* knishio_client_get_bundle(knishio_client_t* client);

/**
 * @brief Set client secret
 * Equivalent to JavaScript: client.setSecret(secret)
 * 
 * @param client KnishIO client instance
 * @param secret Secret to set
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_set_secret(
    knishio_client_t* client,
    const char* secret
);

/**
 * @brief Get source wallet for operations
 * Equivalent to JavaScript: client.getSourceWallet()
 * 
 * @param client KnishIO client instance
 * @param token Token type (optional)
 * @param wallet Output wallet (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_get_source_wallet(
    knishio_client_t* client,
    const char* token,
    knishio_wallet_t** wallet
);

/**
 * @brief Create and sign a molecule
 * Equivalent to JavaScript: client.createMolecule()
 * 
 * @param client KnishIO client instance
 * @param source_wallet Source wallet for signing
 * @param cell_slug Cell identifier (optional)
 * @param molecule Output molecule (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_create_molecule(
    knishio_client_t* client,
    knishio_wallet_t* source_wallet,
    const char* cell_slug,
    knishio_molecule_t** molecule
);

/**
 * @brief Propose a molecule to the network
 * Equivalent to JavaScript: client.proposeMolecule(molecule)
 * 
 * @param client KnishIO client instance
 * @param molecule Molecule to propose
 * @param molecular_hash Output molecular hash (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_propose_molecule(
    knishio_client_t* client,
    knishio_molecule_t* molecule,
    char** molecular_hash
);

/* Batch operations */

/**
 * @brief Query batch information
 * Equivalent to JavaScript: client.queryBatch({ batchId })
 * 
 * @param client KnishIO client instance
 * @param batch_id Batch ID to query
 * @param result Output batch data as JSON (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_batch(
    knishio_client_t* client,
    const char* batch_id,
    char** result
);

/**
 * @brief Query batch history
 * Equivalent to JavaScript: client.queryBatchHistory({ batchId })
 * 
 * @param client KnishIO client instance
 * @param batch_id Batch ID to query
 * @param result Output history as JSON (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_batch_history(
    knishio_client_t* client,
    const char* batch_id,
    char** result
);

/* Meta operations */

/**
 * @brief Create metadata
 * Equivalent to JavaScript: client.createMeta({ metaType, metaId, meta })
 * 
 * @param client KnishIO client instance
 * @param meta_type Metadata type
 * @param meta_id Metadata ID
 * @param meta Array of meta key-value pairs
 * @param meta_count Number of meta entries
 * @param molecular_hash Output molecular hash (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_create_meta(
    knishio_client_t* client,
    const char* meta_type,
    const char* meta_id,
    knishio_meta_t** meta,
    size_t meta_count,
    char** molecular_hash
);

/**
 * @brief Query metadata
 * Equivalent to JavaScript: client.queryMeta({ metaType, metaId })
 * 
 * @param client KnishIO client instance
 * @param meta_type Metadata type
 * @param meta_id Metadata ID (optional)
 * @param result Output meta as JSON (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_meta(
    knishio_client_t* client,
    const char* meta_type,
    const char* meta_id,
    char** result
);

/* Session operations */

/* Utility functions */

/**
 * @brief Execute raw GraphQL query
 * For advanced users needing custom queries
 * 
 * @param client KnishIO client instance
 * @param query GraphQL query string
 * @param variables Variables as JSON string (optional)
 * @param result Output result as JSON (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_execute_query(
    knishio_client_t* client,
    const char* query,
    const char* variables,
    char** result
);

/**
 * @brief Execute raw GraphQL mutation
 * For advanced users needing custom mutations
 * 
 * @param client KnishIO client instance
 * @param mutation GraphQL mutation string
 * @param variables Variables as JSON string (optional)
 * @param result Output result as JSON (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_execute_mutation(
    knishio_client_t* client,
    const char* mutation,
    const char* variables,
    char** result
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CLIENT_OPS_H */
