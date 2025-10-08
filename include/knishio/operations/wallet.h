#ifndef KNISHIO_OPERATIONS_WALLET_H
#define KNISHIO_OPERATIONS_WALLET_H

/**
 * @file operations/wallet.h
 * @brief Wallet operations for KnishIO C SDK
 * 
 * Provides high-level wallet operations matching JavaScript SDK:
 * - CreateWallet mutation
 * - QueryWalletList query
 * - QueryWalletBundle query
 * - QueryContinuId query
 * 
 * Full alignment with JS SDK wallet management functionality.
 */

#include "knishio/error/context.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_client knishio_client_t;
typedef struct knishio_wallet knishio_wallet_t;

/**
 * @brief Parameters for wallet creation
 * Matches JavaScript SDK createWallet() parameters
 */
typedef struct {
    const char* token;              /**< Token type for the wallet */
    const char* position;           /**< Initial position (optional) */
    const char* batch_id;           /**< Batch ID (optional) */
} knishio_create_wallet_params_t;

/**
 * @brief Result of wallet creation
 */
typedef struct {
    bool success;                   /**< Creation success flag */
    knishio_wallet_t* wallet;       /**< Created wallet */
    char* molecular_hash;           /**< Molecular hash of creation */
    char* error_message;            /**< Error message if failed */
} knishio_create_wallet_result_t;

/**
 * @brief Parameters for wallet list query
 */
typedef struct {
    const char* bundle;             /**< Bundle hash to query (optional) */
    const char* token;              /**< Filter by token type (optional) */
    bool include_shadow;            /**< Include shadow wallets */
} knishio_wallet_list_params_t;

/**
 * @brief Result of wallet list query
 */
typedef struct {
    bool success;                   /**< Query success flag */
    knishio_wallet_t** wallets;     /**< Array of wallets */
    size_t wallet_count;            /**< Number of wallets */
    char* error_message;            /**< Error message if failed */
} knishio_wallet_list_result_t;

/**
 * @brief Result of ContinuId query
 */
typedef struct {
    bool success;                   /**< Query success flag */
    char* bundle_hash;              /**< Bundle hash */
    knishio_wallet_t* wallet;       /**< Primary wallet */
    char* error_message;            /**< Error message if failed */
} knishio_continuId_result_t;

/**
 * @brief Create a new wallet
 * Equivalent to JavaScript: client.createWallet({ token })
 * 
 * @param client KnishIO client instance
 * @param params Wallet creation parameters
 * @param result Output creation result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_create_wallet(
    knishio_client_t* client,
    const knishio_create_wallet_params_t* params,
    knishio_create_wallet_result_t** result
);

/**
 * @brief Query list of wallets
 * Equivalent to JavaScript: client.queryWallets({ bundle, token })
 * 
 * @param client KnishIO client instance
 * @param params Query parameters
 * @param result Output wallet list (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_wallets(
    knishio_client_t* client,
    const knishio_wallet_list_params_t* params,
    knishio_wallet_list_result_t** result
);

/**
 * @brief Query wallet bundle
 * Equivalent to JavaScript: client.queryBundle({ bundle })
 * 
 * @param client KnishIO client instance
 * @param bundle Bundle hash (optional, uses client bundle if NULL)
 * @param result Output wallet list (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_bundle(
    knishio_client_t* client,
    const char* bundle,
    knishio_wallet_list_result_t** result
);

/**
 * @brief Query ContinuId
 * Equivalent to JavaScript: client.queryContinuId({ bundle })
 * 
 * @param client KnishIO client instance
 * @param bundle Bundle hash (optional, uses client bundle if NULL)
 * @param result Output ContinuId result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_continuId(
    knishio_client_t* client,
    const char* bundle,
    knishio_continuId_result_t** result
);

/* Result management functions */

/**
 * @brief Free wallet creation result
 * @param result Result to free
 */
void knishio_create_wallet_result_free(knishio_create_wallet_result_t* result);

/**
 * @brief Free wallet list result
 * @param result Result to free
 */
void knishio_wallet_list_result_free(knishio_wallet_list_result_t* result);

/**
 * @brief Free ContinuId result
 * @param result Result to free
 */
void knishio_continuId_result_free(knishio_continuId_result_t* result);

/* Result accessor functions */

/**
 * @brief Check if wallet creation was successful
 * @param result Creation result
 * @return True if successful
 */
bool knishio_create_wallet_result_is_success(const knishio_create_wallet_result_t* result);

/**
 * @brief Get created wallet
 * @param result Creation result
 * @return Wallet or NULL if failed (do not free)
 */
knishio_wallet_t* knishio_create_wallet_result_get_wallet(const knishio_create_wallet_result_t* result);

/**
 * @brief Get wallet count from list result
 * @param result List result
 * @return Number of wallets
 */
size_t knishio_wallet_list_result_get_count(const knishio_wallet_list_result_t* result);

/**
 * @brief Get wallet by index from list result
 * @param result List result
 * @param index Wallet index
 * @return Wallet or NULL if index out of bounds (do not free)
 */
knishio_wallet_t* knishio_wallet_list_result_get_wallet(
    const knishio_wallet_list_result_t* result,
    size_t index
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_OPERATIONS_WALLET_H */
