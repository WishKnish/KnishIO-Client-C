#ifndef KNISHIO_RESPONSE_WALLET_LIST_H
#define KNISHIO_RESPONSE_WALLET_LIST_H

/**
 * @file response_wallet_list.h
 * @brief Response for wallet list queries
 * 
 * Handles responses from wallet list queries, providing utilities to
 * convert wallet data to client wallet instances compatible with the
 * JavaScript SDK's ResponseWalletList.
 */

#include "response.h"
#include "../wallet.h"
#include "../token_unit.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_wallet_list knishio_response_wallet_list_t;

/**
 * @brief Trade rate information
 */
typedef struct {
    char *token_slug;                   /**< Token slug */
    double amount;                      /**< Trade rate amount */
} knishio_trade_rate_t;

/**
 * @brief Wallet list response structure
 */
struct knishio_response_wallet_list {
    knishio_response_t base;            /**< Base response */
    knishio_wallet_t **wallets;         /**< Array of wallets */
    size_t wallet_count;                /**< Number of wallets */
    bool wallets_cached;                /**< Whether wallets are cached */
};

/**
 * @brief Convert wallet data to client wallet
 * @param wallet_data JSON wallet data
 * @param secret Secret for wallet reconstruction (can be NULL)
 * @return Client wallet or NULL on error
 */
knishio_wallet_t* knishio_response_wallet_list_to_client_wallet(knishio_json_t *wallet_data,
                                                                const char *secret);

/* Iterator functions */

/**
 * @brief Wallet iteration callback
 * @param index Wallet index
 * @param wallet Wallet instance
 * @param user_data User data
 * @return True to continue iteration, false to stop
 */
typedef bool (*knishio_wallet_iterator_fn)(size_t index,
                                           knishio_wallet_t *wallet,
                                           void *user_data);

/* Utility functions */

/* Conversion functions */

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_WALLET_LIST_H */