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
 * @brief Create wallet list response
 * @param query Original query
 * @param json JSON response data
 * @return Wallet list response or NULL on error
 */
knishio_response_wallet_list_t* knishio_response_wallet_list_create(knishio_query_t *query,
                                                                    knishio_json_t *json);

/**
 * @brief Free wallet list response
 * @param response Wallet list response to free
 */
void knishio_response_wallet_list_free(knishio_response_wallet_list_t *response);

/**
 * @brief Get wallets array
 * @param response Wallet list response
 * @param secret Secret for wallet reconstruction (can be NULL)
 * @param count Output wallet count
 * @return Array of wallets or NULL
 */
knishio_wallet_t** knishio_response_wallet_list_get_wallets(knishio_response_wallet_list_t *response,
                                                            const char *secret,
                                                            size_t *count);

/**
 * @brief Get wallet at specific index
 * @param response Wallet list response
 * @param index Wallet index
 * @param secret Secret for wallet reconstruction (can be NULL)
 * @return Wallet or NULL if index out of bounds
 */
knishio_wallet_t* knishio_response_wallet_list_get_wallet(knishio_response_wallet_list_t *response,
                                                          size_t index,
                                                          const char *secret);

/**
 * @brief Get number of wallets
 * @param response Wallet list response
 * @return Number of wallets
 */
size_t knishio_response_wallet_list_count(knishio_response_wallet_list_t *response);

/**
 * @brief Convert wallet data to client wallet
 * @param wallet_data JSON wallet data
 * @param secret Secret for wallet reconstruction (can be NULL)
 * @return Client wallet or NULL on error
 */
knishio_wallet_t* knishio_response_wallet_list_to_client_wallet(knishio_json_t *wallet_data,
                                                                const char *secret);

/**
 * @brief Extract token units from wallet data
 * @param wallet_data JSON wallet data
 * @param units Output array of token units (caller must free)
 * @param count Output unit count
 * @return True on success, false on error
 */
bool knishio_response_wallet_list_extract_token_units(knishio_json_t *wallet_data,
                                                      knishio_token_unit_t **units,
                                                      size_t *count);

/**
 * @brief Extract trade rates from wallet data
 * @param wallet_data JSON wallet data
 * @param rates Output array of trade rates (caller must free)
 * @param count Output rate count
 * @return True on success, false on error
 */
bool knishio_response_wallet_list_extract_trade_rates(knishio_json_t *wallet_data,
                                                      knishio_trade_rate_t **rates,
                                                      size_t *count);

/**
 * @brief Extract token information from wallet data
 * @param wallet_data JSON wallet data
 * @param token_name Output token name (can be NULL)
 * @param token_amount Output token amount (can be NULL)
 * @param token_supply Output token supply (can be NULL)
 * @param fungibility Output fungibility (can be NULL)
 * @return True if token info found, false otherwise
 */
bool knishio_response_wallet_list_extract_token_info(knishio_json_t *wallet_data,
                                                     const char **token_name,
                                                     double *token_amount,
                                                     double *token_supply,
                                                     bool *fungibility);

/**
 * @brief Check if response has wallet data
 * @param response Wallet list response
 * @return True if has wallet data, false otherwise
 */
bool knishio_response_wallet_list_has_data(knishio_response_wallet_list_t *response);

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

/**
 * @brief Iterate over wallets
 * @param response Wallet list response
 * @param secret Secret for wallet reconstruction (can be NULL)
 * @param callback Iterator callback
 * @param user_data User data for callback
 * @return True if all iterations completed, false if stopped early
 */
bool knishio_response_wallet_list_foreach(knishio_response_wallet_list_t *response,
                                          const char *secret,
                                          knishio_wallet_iterator_fn callback,
                                          void *user_data);

/* Utility functions */

/**
 * @brief Filter wallets by token slug
 * @param response Wallet list response
 * @param token_slug Token slug to filter by
 * @param secret Secret for wallet reconstruction (can be NULL)
 * @param filtered_wallets Output filtered wallets array (caller must free)
 * @param count Output filtered count
 * @return True on success, false on error
 */
bool knishio_response_wallet_list_filter_by_token(knishio_response_wallet_list_t *response,
                                                  const char *token_slug,
                                                  const char *secret,
                                                  knishio_wallet_t ***filtered_wallets,
                                                  size_t *count);

/**
 * @brief Find wallet by bundle hash
 * @param response Wallet list response
 * @param bundle_hash Bundle hash to search for
 * @param secret Secret for wallet reconstruction (can be NULL)
 * @return Wallet or NULL if not found
 */
knishio_wallet_t* knishio_response_wallet_list_find_by_bundle(knishio_response_wallet_list_t *response,
                                                              const char *bundle_hash,
                                                              const char *secret);

/**
 * @brief Free trade rate
 * @param rate Trade rate to free
 */
void knishio_trade_rate_free(knishio_trade_rate_t *rate);

/**
 * @brief Free array of trade rates
 * @param rates Array of trade rates
 * @param count Number of rates
 */
void knishio_trade_rates_free(knishio_trade_rate_t *rates, size_t count);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param wallet_list_response Wallet list response
 * @return Base response
 */
knishio_response_t* knishio_response_wallet_list_to_base(knishio_response_wallet_list_t *wallet_list_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Wallet list response or NULL if not a wallet list response
 */
knishio_response_wallet_list_t* knishio_response_wallet_list_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_WALLET_LIST_H */