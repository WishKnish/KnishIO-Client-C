#ifndef KNISHIO_RESPONSE_CREATE_WALLET_H
#define KNISHIO_RESPONSE_CREATE_WALLET_H

/**
 * @file response_create_wallet.h
 * @brief Response for create wallet operations
 * 
 * Handles responses from create wallet mutations, extracting wallet creation
 * results compatible with the JavaScript SDK's ResponseCreateWallet.
 */

#include "response.h"
#include "../molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_create_wallet knishio_response_create_wallet_t;

/**
 * @brief Create wallet response structure
 */
struct knishio_response_create_wallet {
    knishio_response_t base;            /**< Base response */
    knishio_molecule_t *molecule;       /**< Created molecule */
    char *bundle_hash;                  /**< New wallet bundle hash */
    char *wallet_address;               /**< New wallet address */
    char *token_slug;                   /**< Token for created wallet */
    bool success;                       /**< Whether creation succeeded */
};

/**
 * @brief Create create wallet response
 * @param query Original query
 * @param json JSON response data
 * @return Create wallet response or NULL on error
 */
knishio_response_create_wallet_t* knishio_response_create_wallet_create(knishio_query_t *query,
                                                                        knishio_json_t *json);

/**
 * @brief Free create wallet response
 * @param response Create wallet response to free
 */
void knishio_response_create_wallet_free(knishio_response_create_wallet_t *response);

/**
 * @brief Get created molecule
 * @param response Create wallet response
 * @return Created molecule or NULL if creation failed
 */
knishio_molecule_t* knishio_response_create_wallet_get_molecule(knishio_response_create_wallet_t *response);

/**
 * @brief Get new wallet bundle hash
 * @param response Create wallet response
 * @return Bundle hash or NULL if not available
 */
const char* knishio_response_create_wallet_get_bundle_hash(knishio_response_create_wallet_t *response);

/**
 * @brief Get new wallet address
 * @param response Create wallet response
 * @return Wallet address or NULL if not available
 */
const char* knishio_response_create_wallet_get_wallet_address(knishio_response_create_wallet_t *response);

/**
 * @brief Get token slug for created wallet
 * @param response Create wallet response
 * @return Token slug or NULL if not available
 */
const char* knishio_response_create_wallet_get_token_slug(knishio_response_create_wallet_t *response);

/**
 * @brief Check if wallet creation was successful
 * @param response Create wallet response
 * @return True if creation succeeded, false otherwise
 */
bool knishio_response_create_wallet_is_success(knishio_response_create_wallet_t *response);

/**
 * @brief Get molecular hash of created wallet
 * @param response Create wallet response
 * @return Molecular hash or NULL if not available
 */
const char* knishio_response_create_wallet_get_molecular_hash(knishio_response_create_wallet_t *response);

/**
 * @brief Check if response has valid wallet creation data
 * @param response Create wallet response
 * @return True if has valid creation data, false otherwise
 */
bool knishio_response_create_wallet_has_data(knishio_response_create_wallet_t *response);

/**
 * @brief Get molecule status
 * @param response Create wallet response
 * @return Molecule status or KNISHIO_MOLECULE_STATUS_UNKNOWN if not available
 */
knishio_molecule_status_t knishio_response_create_wallet_get_molecule_status(knishio_response_create_wallet_t *response);

/**
 * @brief Get creation timestamp
 * @param response Create wallet response
 * @return Creation timestamp or NULL if not available
 */
const char* knishio_response_create_wallet_get_created_at(knishio_response_create_wallet_t *response);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param create_wallet_response Create wallet response
 * @return Base response
 */
knishio_response_t* knishio_response_create_wallet_to_base(knishio_response_create_wallet_t *create_wallet_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Create wallet response or NULL if not a create wallet response
 */
knishio_response_create_wallet_t* knishio_response_create_wallet_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_CREATE_WALLET_H */