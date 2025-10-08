#ifndef KNISHIO_RESPONSE_BALANCE_H
#define KNISHIO_RESPONSE_BALANCE_H

/**
 * @file response_balance.h
 * @brief Response for balance queries
 * 
 * Handles responses from balance queries, extracting wallet information
 * with balance data compatible with the JavaScript SDK's ResponseBalance.
 */

#include "response.h"
#include "../wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_balance knishio_response_balance_t;

/**
 * @brief Balance response structure
 */
struct knishio_response_balance {
    knishio_response_t base;            /**< Base response */
    knishio_wallet_t *wallet;           /**< Wallet with balance */
    bool wallet_cached;                 /**< Whether wallet is cached */
};

/**
 * @brief Create balance response
 * @param query Original query
 * @param json JSON response data
 * @return Balance response or NULL on error
 */
knishio_response_balance_t* knishio_response_balance_create(knishio_query_t *query,
                                                            knishio_json_t *json);

/**
 * @brief Free balance response
 * @param response Balance response to free
 */
void knishio_response_balance_free(knishio_response_balance_t *response);

/**
 * @brief Get wallet with balance
 * @param response Balance response
 * @return Wallet or NULL if no balance data
 */
knishio_wallet_t* knishio_response_balance_get_wallet(knishio_response_balance_t *response);

/**
 * @brief Get balance amount
 * @param response Balance response
 * @param amount Output balance amount
 * @return True if balance found, false otherwise
 */
bool knishio_response_balance_get_amount(knishio_response_balance_t *response, double *amount);

/**
 * @brief Get token slug
 * @param response Balance response
 * @return Token slug or NULL if not found
 */
const char* knishio_response_balance_get_token_slug(knishio_response_balance_t *response);

/**
 * @brief Get wallet bundle hash
 * @param response Balance response
 * @return Bundle hash or NULL if not found
 */
const char* knishio_response_balance_get_bundle_hash(knishio_response_balance_t *response);

/**
 * @brief Get wallet address
 * @param response Balance response
 * @return Wallet address or NULL if not found
 */
const char* knishio_response_balance_get_address(knishio_response_balance_t *response);

/**
 * @brief Get wallet position
 * @param response Balance response
 * @return Wallet position or NULL if not found
 */
const char* knishio_response_balance_get_position(knishio_response_balance_t *response);

/**
 * @brief Get wallet public key
 * @param response Balance response
 * @return Public key or NULL if not found
 */
const char* knishio_response_balance_get_pubkey(knishio_response_balance_t *response);

/**
 * @brief Get wallet creation timestamp
 * @param response Balance response
 * @return Creation timestamp or NULL if not found
 */
const char* knishio_response_balance_get_created_at(knishio_response_balance_t *response);

/**
 * @brief Check if response has valid balance data
 * @param response Balance response
 * @return True if has valid balance data, false otherwise
 */
bool knishio_response_balance_has_data(knishio_response_balance_t *response);

/**
 * @brief Get token information if available
 * @param response Balance response
 * @param token_name Output token name (can be NULL)
 * @param token_amount Output token total amount (can be NULL)
 * @param token_supply Output token supply (can be NULL)
 * @param fungibility Output token fungibility (can be NULL)
 * @return True if token info available, false otherwise
 */
bool knishio_response_balance_get_token_info(knishio_response_balance_t *response,
                                             const char **token_name,
                                             double *token_amount,
                                             double *token_supply,
                                             bool *fungibility);

/**
 * @brief Get batch ID if available
 * @param response Balance response
 * @return Batch ID or NULL if not found
 */
const char* knishio_response_balance_get_batch_id(knishio_response_balance_t *response);

/**
 * @brief Get wallet characters if available
 * @param response Balance response
 * @return Characters string or NULL if not found
 */
const char* knishio_response_balance_get_characters(knishio_response_balance_t *response);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param balance_response Balance response
 * @return Base response
 */
knishio_response_t* knishio_response_balance_to_base(knishio_response_balance_t *balance_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Balance response or NULL if not a balance response
 */
knishio_response_balance_t* knishio_response_balance_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_BALANCE_H */