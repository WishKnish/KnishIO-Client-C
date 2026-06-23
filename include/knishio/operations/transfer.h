#ifndef KNISHIO_OPERATIONS_TRANSFER_H
#define KNISHIO_OPERATIONS_TRANSFER_H

/**
 * @file operations/transfer.h
 * @brief Token transfer operations for KnishIO C SDK
 * 
 * Provides high-level token transfer operations matching JavaScript SDK:
 * - TransferTokens mutation
 * - Balance queries  
 * - Token unit management
 * 
 * Full alignment with JS SDK transferToken() functionality.
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
typedef struct knishio_wallet knishio_wallet_t;

/**
 * @brief Parameters for token transfer operation
 * Matches JavaScript SDK transferToken() parameters
 */
typedef struct {
    const char* recipient;          /**< Recipient wallet address or bundle hash */
    const char* token;              /**< Token slug to transfer */
    double amount;                  /**< Amount to transfer (for stackable tokens) */
    const char** units;             /**< Array of unit IDs (for non-fungible tokens) */
    size_t unit_count;              /**< Number of units */
    const char* batch_id;          /**< Batch ID for grouped transfers (optional) */
    knishio_wallet_t* source_wallet;/**< Source wallet (optional, auto-selected if NULL) */
} knishio_transfer_params_t;

/**
 * @brief Result of token transfer operation
 */
typedef struct {
    bool success;                   /**< Transfer success flag */
    char* molecular_hash;           /**< Molecular hash of transfer */
    char* response;                 /**< Full response data */
    char* error_message;            /**< Error message if failed */
} knishio_transfer_result_t;

/**
 * @brief Transfer tokens to another wallet
 * Equivalent to JavaScript: client.transferToken({ recipient, token, amount, units })
 * 
 * @param client KnishIO client instance
 * @param params Transfer parameters
 * @param result Output transfer result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_transfer_tokens(
    knishio_client_t* client,
    const knishio_transfer_params_t* params,
    knishio_transfer_result_t** result
);

/**
 * @brief One destination of a multi-recipient stackable transfer.
 * Provide EITHER units (stackable/NFT: amount = unit_count) OR amount (fungible), not both.
 * batch_id makes the recipient a claimable shadow under that batch.
 */
typedef struct {
    const char* bundle_hash;        /**< Recipient bundle hash */
    const char** units;             /**< Array of unit IDs for this recipient (stackable) */
    size_t unit_count;              /**< Number of units for this recipient */
    int amount;                     /**< Fungible amount (when unit_count == 0) */
    const char* batch_id;           /**< Batch ID -> claimable shadow (optional) */
} knishio_transfer_recipient_t;

/**
 * @brief Parameters for a multi-recipient transfer (one source funds N recipients).
 */
typedef struct {
    const char* token;                              /**< Token slug to transfer */
    const knishio_transfer_recipient_t* recipients; /**< Array of recipients */
    size_t recipient_count;                         /**< Number of recipients */
    knishio_wallet_t* source_wallet;                /**< Source wallet (optional, auto-selected if NULL) */
} knishio_transfer_multi_params_t;

/**
 * @brief Transfer tokens to N recipients in a single molecule (multi-recipient sibling of
 * knishio_client_transfer_tokens). Each recipient gets its own subset of stackable units (or a
 * fungible amount); a remainder returns the rest to the sender.
 *
 * @param client KnishIO client instance
 * @param params Multi-recipient transfer parameters
 * @param result Output transfer result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_transfer_tokens_multi(
    knishio_client_t* client,
    const knishio_transfer_multi_params_t* params,
    knishio_transfer_result_t** result
);

/**
 * @brief Query balance for a wallet
 * Enhanced version matching JavaScript: client.queryBalance({ token, bundle })
 * 
 * @param client KnishIO client instance
 * @param token Token slug to query (optional, NULL for all)
 * @param bundle Bundle hash (optional, uses client bundle if NULL)
 * @param balance Output balance value (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_balance(
    knishio_client_t* client,
    const char* token,
    const char* bundle,
    char** balance
);

/**
 * @brief Query detailed balance information
 * Returns full wallet information including token units
 * 
 * @param client KnishIO client instance
 * @param token Token slug to query
 * @param wallet Output wallet with balance (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_balance_wallet(
    knishio_client_t* client,
    const char* token,
    knishio_wallet_t** wallet
);

/**
 * @brief Query source wallet for transfers with validation
 * Equivalent to JavaScript: client.querySourceWallet({ token, amount, type })
 * 
 * @param client KnishIO client instance
 * @param token Token slug to query
 * @param amount Required amount for transfer validation
 * @param type Wallet type ("regular" or "buffer")
 * @param wallet Output source wallet (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_source_wallet(
    knishio_client_t* client,
    const char* token,
    double amount,
    const char* type,
    knishio_wallet_t** wallet
);

/**
 * @brief Burn tokens (transfer to null address)
 * Equivalent to JavaScript: client.burnTokens({ token, amount, units })
 * 
 * @param client KnishIO client instance
 * @param token Token slug to burn
 * @param amount Amount to burn (for stackable tokens)
 * @param units Array of unit IDs to burn (for non-fungible)
 * @param unit_count Number of units
 * @param result Output burn result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_burn_tokens(
    knishio_client_t* client,
    const char* token,
    double amount,
    const char** units,
    size_t unit_count,
    knishio_transfer_result_t** result
);

/**
 * @brief Deposit tokens to buffer
 * Equivalent to JavaScript: client.depositBufferToken({ token, amount, bufferId })
 * 
 * @param client KnishIO client instance
 * @param token Token slug to deposit
 * @param amount Amount to deposit
 * @param buffer_id Buffer identifier
 * @param result Output deposit result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_deposit_buffer_token(
    knishio_client_t* client,
    const char* token,
    double amount,
    const char* buffer_id,
    knishio_transfer_result_t** result
);

/**
 * @brief Withdraw tokens from buffer
 * Equivalent to JavaScript: client.withdrawBufferToken({ token, amount, bufferId })
 * 
 * @param client KnishIO client instance
 * @param token Token slug to withdraw
 * @param amount Amount to withdraw
 * @param buffer_id Buffer identifier
 * @param result Output withdrawal result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_withdraw_buffer_token(
    knishio_client_t* client,
    const char* token,
    double amount,
    const char* buffer_id,
    knishio_transfer_result_t** result
);

/**
 * @brief Free transfer result
 * @param result Result to free
 */
void knishio_transfer_result_free(knishio_transfer_result_t* result);

/**
 * @brief Check if transfer was successful
 * @param result Transfer result
 * @return True if successful
 */
bool knishio_transfer_result_is_success(const knishio_transfer_result_t* result);

/**
 * @brief Get transfer molecular hash
 * @param result Transfer result
 * @return Molecular hash string (do not free)
 */
const char* knishio_transfer_result_get_hash(const knishio_transfer_result_t* result);

/**
 * @brief Get transfer error message
 * @param result Transfer result
 * @return Error message or NULL if successful (do not free)
 */
const char* knishio_transfer_result_get_error(const knishio_transfer_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_OPERATIONS_TRANSFER_H */
