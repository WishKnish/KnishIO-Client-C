#ifndef KNISHIO_RESPONSE_WALLET_BUNDLE_H
#define KNISHIO_RESPONSE_WALLET_BUNDLE_H

/**
 * @file response_wallet_bundle.h
 * @brief Response for wallet bundle queries
 * 
 * Handles responses from wallet bundle queries, extracting wallet bundle
 * information compatible with the JavaScript SDK's ResponseWalletBundle.
 */

#include "response.h"
#include "../wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_wallet_bundle knishio_response_wallet_bundle_t;

/**
 * @brief Wallet bundle response structure
 */
struct knishio_response_wallet_bundle {
    knishio_response_t base;            /**< Base response */
    char *bundle_hash;                  /**< Bundle hash */
    knishio_wallet_t **wallets;         /**< Array of wallets in bundle */
    size_t wallet_count;                /**< Number of wallets */
    size_t wallet_capacity;             /**< Allocated wallet capacity */
};

/**
 * @brief Create wallet bundle response
 * @param query Original query
 * @param json JSON response data
 * @return Wallet bundle response or NULL on error
 */
knishio_response_wallet_bundle_t* knishio_response_wallet_bundle_create(knishio_query_t *query,
                                                                        knishio_json_t *json);

/**
 * @brief Free wallet bundle response
 * @param response Wallet bundle response to free
 */
void knishio_response_wallet_bundle_free(knishio_response_wallet_bundle_t *response);

/**
 * @brief Get bundle hash
 * @param response Wallet bundle response
 * @return Bundle hash or NULL if not found
 */
const char* knishio_response_wallet_bundle_get_bundle_hash(knishio_response_wallet_bundle_t *response);

/**
 * @brief Get wallets in bundle
 * @param response Wallet bundle response
 * @param wallet_count Output wallet count (can be NULL)
 * @return Array of wallets or NULL if no wallets
 */
knishio_wallet_t** knishio_response_wallet_bundle_get_wallets(knishio_response_wallet_bundle_t *response,
                                                              size_t *wallet_count);

/**
 * @brief Get wallet by index
 * @param response Wallet bundle response
 * @param index Wallet index
 * @return Wallet or NULL if index invalid
 */
knishio_wallet_t* knishio_response_wallet_bundle_get_wallet_at(knishio_response_wallet_bundle_t *response,
                                                               size_t index);

/**
 * @brief Get wallet by token
 * @param response Wallet bundle response
 * @param token Token slug to search for
 * @return First wallet with matching token or NULL if not found
 */
knishio_wallet_t* knishio_response_wallet_bundle_get_wallet_by_token(knishio_response_wallet_bundle_t *response,
                                                                     const char *token);

/**
 * @brief Get wallet count
 * @param response Wallet bundle response
 * @return Number of wallets in bundle
 */
size_t knishio_response_wallet_bundle_get_wallet_count(knishio_response_wallet_bundle_t *response);

/**
 * @brief Check if response has valid wallet bundle data
 * @param response Wallet bundle response
 * @return True if has valid bundle data, false otherwise
 */
bool knishio_response_wallet_bundle_has_data(knishio_response_wallet_bundle_t *response);

/**
 * @brief Get total balance across all wallets for a token
 * @param response Wallet bundle response  
 * @param token Token slug
 * @param total_balance Output total balance
 * @return True if balance calculated, false otherwise
 */
bool knishio_response_wallet_bundle_get_total_balance(knishio_response_wallet_bundle_t *response,
                                                      const char *token,
                                                      double *total_balance);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param bundle_response Wallet bundle response
 * @return Base response
 */
knishio_response_t* knishio_response_wallet_bundle_to_base(knishio_response_wallet_bundle_t *bundle_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Wallet bundle response or NULL if not a wallet bundle response
 */
knishio_response_wallet_bundle_t* knishio_response_wallet_bundle_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_WALLET_BUNDLE_H */