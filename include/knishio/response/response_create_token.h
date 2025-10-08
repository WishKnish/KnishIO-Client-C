#ifndef KNISHIO_RESPONSE_CREATE_TOKEN_H
#define KNISHIO_RESPONSE_CREATE_TOKEN_H

/**
 * @file response_create_token.h
 * @brief Response for token creation operations
 * 
 * Handles responses from token creation mutations, extending the
 * propose molecule response functionality compatible with the
 * JavaScript SDK's ResponseCreateToken.
 */

#include "response_propose_molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Include response types for the typedef */
#include "response_types.h"

/**
 * @brief Create token creation response
 * @param query Original mutation query
 * @param json JSON response data
 * @param client_molecule Original molecule that was proposed
 * @return Create token response or NULL on error
 */
knishio_response_create_token_t* knishio_response_create_token_create(knishio_query_t *query,
                                                                      knishio_json_t *json,
                                                                      knishio_molecule_t *client_molecule);

/**
 * @brief Free create token response
 * @param response Create token response to free
 */
void knishio_response_create_token_free(knishio_response_create_token_t *response);

/**
 * @brief Check if token creation was successful
 * @param response Create token response
 * @return True if token created successfully, false otherwise
 */
bool knishio_response_create_token_success(knishio_response_create_token_t *response);

/**
 * @brief Get created token slug from payload
 * @param response Create token response
 * @return Token slug or NULL if not available
 */
const char* knishio_response_create_token_get_token_slug(knishio_response_create_token_t *response);

/**
 * @brief Get token name from payload
 * @param response Create token response
 * @return Token name or NULL if not available
 */
const char* knishio_response_create_token_get_token_name(knishio_response_create_token_t *response);

/**
 * @brief Get token supply from payload
 * @param response Create token response
 * @param supply Output token supply
 * @return True if supply available, false otherwise
 */
bool knishio_response_create_token_get_supply(knishio_response_create_token_t *response,
                                              double *supply);

/**
 * @brief Get token fungibility from payload
 * @param response Create token response
 * @param fungible Output fungibility flag
 * @return True if fungibility info available, false otherwise
 */
bool knishio_response_create_token_get_fungibility(knishio_response_create_token_t *response,
                                                   bool *fungible);

/**
 * @brief Get token metadata from payload
 * @param response Create token response
 * @return Token metadata JSON or NULL if not available
 */
knishio_json_t* knishio_response_create_token_get_metadata(knishio_response_create_token_t *response);

/**
 * @brief Get created token unit information
 * @param response Create token response
 * @param unit_id Output unit ID (can be NULL)
 * @param amount Output unit amount (can be NULL)
 * @return True if unit info available, false otherwise
 */
bool knishio_response_create_token_get_unit_info(knishio_response_create_token_t *response,
                                                 const char **unit_id,
                                                 double *amount);

/**
 * @brief Get wallet address where token was created
 * @param response Create token response
 * @return Wallet address or NULL if not available
 */
const char* knishio_response_create_token_get_wallet_address(knishio_response_create_token_t *response);

/**
 * @brief Get wallet bundle hash where token was created
 * @param response Create token response
 * @return Bundle hash or NULL if not available
 */
const char* knishio_response_create_token_get_wallet_bundle(knishio_response_create_token_t *response);

/* Conversion functions */

/**
 * @brief Convert to propose molecule response
 * @param create_token_response Create token response
 * @return Propose molecule response
 */
knishio_response_propose_molecule_t* knishio_response_create_token_to_propose_molecule(knishio_response_create_token_t *create_token_response);

/**
 * @brief Convert from propose molecule response
 * @param propose_molecule_response Propose molecule response
 * @return Create token response (same pointer, different type)
 */
knishio_response_create_token_t* knishio_response_create_token_from_propose_molecule(knishio_response_propose_molecule_t *propose_molecule_response);

/**
 * @brief Convert to base response
 * @param create_token_response Create token response
 * @return Base response
 */
knishio_response_t* knishio_response_create_token_to_base(knishio_response_create_token_t *create_token_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Create token response or NULL if not a create token response
 */
knishio_response_create_token_t* knishio_response_create_token_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_CREATE_TOKEN_H */