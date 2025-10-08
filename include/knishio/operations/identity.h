#ifndef KNISHIO_OPERATIONS_IDENTITY_H
#define KNISHIO_OPERATIONS_IDENTITY_H

/**
 * @file operations/identity.h
 * @brief Identity operations for KnishIO C SDK
 * 
 * Provides high-level identity operations matching JavaScript SDK:
 * - CreateIdentifier mutation
 * - ClaimShadowWallet mutation
 * - ClaimShadowWallets batch operation
 * 
 * Full alignment with JS SDK identity functionality.
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
 * @brief Parameters for identifier creation
 * Matches JavaScript SDK createIdentifier() parameters
 */
typedef struct {
    const char* type;                   /**< Identifier type */
    const char* contact;                /**< Contact information */
    const char* code;                   /**< Identifier code */
} knishio_create_identifier_params_t;

/**
 * @brief Result of identifier creation
 */
typedef struct {
    bool success;                       /**< Creation success flag */
    char* molecular_hash;               /**< Molecular hash of identifier creation */
    char* response;                     /**< Full response data */
    char* error_message;                /**< Error message if failed */
} knishio_create_identifier_result_t;

/**
 * @brief Parameters for shadow wallet claim
 * Matches JavaScript SDK claimShadowWallet() parameters
 */
typedef struct {
    const char* token;                  /**< Token slug */
    const char* batch_id;               /**< Batch ID (optional) */
} knishio_claim_shadow_wallet_params_t;

/**
 * @brief Result of shadow wallet claim
 */
typedef struct {
    bool success;                       /**< Claim success flag */
    char* molecular_hash;               /**< Molecular hash of claim */
    char* response;                     /**< Full response data */
    char* error_message;                /**< Error message if failed */
} knishio_claim_shadow_wallet_result_t;

/**
 * @brief Parameters for claiming all shadow wallets
 * Matches JavaScript SDK claimShadowWallets() parameters
 */
typedef struct {
    const char* token;                  /**< Token slug */
} knishio_claim_shadow_wallets_params_t;

/**
 * @brief Result of claiming all shadow wallets
 */
typedef struct {
    bool success;                       /**< Overall success flag */
    knishio_claim_shadow_wallet_result_t** results; /**< Array of individual results */
    size_t result_count;                /**< Number of results */
    char* error_message;                /**< Error message if failed */
} knishio_claim_shadow_wallets_result_t;

/**
 * @brief Create identity identifier
 * Equivalent to JavaScript: client.createIdentifier({ type, contact, code })
 * 
 * @param client KnishIO client instance
 * @param params Identifier parameters
 * @param result Output identifier result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_create_identifier(
    knishio_client_t* client,
    const knishio_create_identifier_params_t* params,
    knishio_create_identifier_result_t** result
);

/**
 * @brief Claim individual shadow wallet
 * Equivalent to JavaScript: client.claimShadowWallet({ token, batchId })
 * 
 * @param client KnishIO client instance
 * @param params Claim parameters
 * @param result Output claim result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_claim_shadow_wallet(
    knishio_client_t* client,
    const knishio_claim_shadow_wallet_params_t* params,
    knishio_claim_shadow_wallet_result_t** result
);

/**
 * @brief Claim all shadow wallets for token
 * Equivalent to JavaScript: client.claimShadowWallets({ token })
 * 
 * @param client KnishIO client instance
 * @param params Claim parameters
 * @param result Output claim results (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_claim_shadow_wallets(
    knishio_client_t* client,
    const knishio_claim_shadow_wallets_params_t* params,
    knishio_claim_shadow_wallets_result_t** result
);

/**
 * @brief Free identifier creation result
 * @param result Result to free
 */
void knishio_create_identifier_result_free(knishio_create_identifier_result_t* result);

/**
 * @brief Free shadow wallet claim result
 * @param result Result to free
 */
void knishio_claim_shadow_wallet_result_free(knishio_claim_shadow_wallet_result_t* result);

/**
 * @brief Free shadow wallets claim result
 * @param result Result to free
 */
void knishio_claim_shadow_wallets_result_free(knishio_claim_shadow_wallets_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_OPERATIONS_IDENTITY_H */