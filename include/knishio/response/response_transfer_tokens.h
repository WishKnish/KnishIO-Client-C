#ifndef KNISHIO_RESPONSE_TRANSFER_TOKENS_H
#define KNISHIO_RESPONSE_TRANSFER_TOKENS_H

/**
 * @file response_transfer_tokens.h
 * @brief Response for token transfer operations
 * 
 * Handles responses from token transfer mutations, extending the
 * propose molecule response functionality compatible with the
 * JavaScript SDK's ResponseTransferTokens.
 */

#include "response_propose_molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Include response types for the typedef */
#include "response_types.h"

/**
 * @brief Create transfer tokens response
 * @param query Original mutation query
 * @param json JSON response data
 * @param client_molecule Original molecule that was proposed
 * @return Transfer tokens response or NULL on error
 */
knishio_response_transfer_tokens_t* knishio_response_transfer_tokens_create(knishio_query_t *query,
                                                                            knishio_json_t *json,
                                                                            knishio_molecule_t *client_molecule);

/**
 * @brief Free transfer tokens response
 * @param response Transfer tokens response to free
 */
void knishio_response_transfer_tokens_free(knishio_response_transfer_tokens_t *response);

/**
 * @brief Check if token transfer was successful
 * @param response Transfer tokens response
 * @return True if transfer completed successfully, false otherwise
 */
bool knishio_response_transfer_tokens_success(knishio_response_transfer_tokens_t *response);

/**
 * @brief Get transferred token slug from payload
 * @param response Transfer tokens response
 * @return Token slug or NULL if not available
 */
const char* knishio_response_transfer_tokens_get_token_slug(knishio_response_transfer_tokens_t *response);

/**
 * @brief Get transferred amount from payload
 * @param response Transfer tokens response
 * @param amount Output transferred amount
 * @return True if amount available, false otherwise
 */
bool knishio_response_transfer_tokens_get_amount(knishio_response_transfer_tokens_t *response,
                                                 double *amount);

/**
 * @brief Get source wallet information
 * @param response Transfer tokens response
 * @param address Output source address (can be NULL)
 * @param bundle Output source bundle (can be NULL)
 * @return True if source info available, false otherwise
 */
bool knishio_response_transfer_tokens_get_source_wallet(knishio_response_transfer_tokens_t *response,
                                                        const char **address,
                                                        const char **bundle);

/**
 * @brief Get destination wallet information
 * @param response Transfer tokens response
 * @param address Output destination address (can be NULL)
 * @param bundle Output destination bundle (can be NULL)
 * @return True if destination info available, false otherwise
 */
bool knishio_response_transfer_tokens_get_destination_wallet(knishio_response_transfer_tokens_t *response,
                                                             const char **address,
                                                             const char **bundle);

/**
 * @brief Get transfer metadata from payload
 * @param response Transfer tokens response
 * @return Transfer metadata JSON or NULL if not available
 */
knishio_json_t* knishio_response_transfer_tokens_get_metadata(knishio_response_transfer_tokens_t *response);

/**
 * @brief Get transfer fee information
 * @param response Transfer tokens response
 * @param fee_amount Output fee amount (can be NULL)
 * @param fee_token Output fee token slug (can be NULL)
 * @return True if fee info available, false otherwise
 */
bool knishio_response_transfer_tokens_get_fee_info(knishio_response_transfer_tokens_t *response,
                                                   double *fee_amount,
                                                   const char **fee_token);

/**
 * @brief Get remaining balance in source wallet
 * @param response Transfer tokens response
 * @param remaining_balance Output remaining balance
 * @return True if balance info available, false otherwise
 */
bool knishio_response_transfer_tokens_get_remaining_balance(knishio_response_transfer_tokens_t *response,
                                                            double *remaining_balance);

/**
 * @brief Get new balance in destination wallet
 * @param response Transfer tokens response
 * @param new_balance Output new balance
 * @return True if balance info available, false otherwise
 */
bool knishio_response_transfer_tokens_get_new_balance(knishio_response_transfer_tokens_t *response,
                                                      double *new_balance);

/**
 * @brief Get transfer ID or hash
 * @param response Transfer tokens response
 * @return Transfer ID/hash or NULL if not available
 */
const char* knishio_response_transfer_tokens_get_transfer_id(knishio_response_transfer_tokens_t *response);

/**
 * @brief Get transfer memo/note
 * @param response Transfer tokens response
 * @return Transfer memo or NULL if not available
 */
const char* knishio_response_transfer_tokens_get_memo(knishio_response_transfer_tokens_t *response);

/**
 * @brief Check if transfer required wallet creation
 * @param response Transfer tokens response
 * @return True if new wallets were created during transfer, false otherwise
 */
bool knishio_response_transfer_tokens_created_wallets(knishio_response_transfer_tokens_t *response);

/**
 * @brief Get created wallet addresses (if any)
 * @param response Transfer tokens response
 * @param addresses Output array of created addresses (caller must free)
 * @param count Output address count
 * @return True if created addresses available, false otherwise
 */
bool knishio_response_transfer_tokens_get_created_addresses(knishio_response_transfer_tokens_t *response,
                                                            char ***addresses,
                                                            size_t *count);

/* Conversion functions */

/**
 * @brief Convert to propose molecule response
 * @param transfer_response Transfer tokens response
 * @return Propose molecule response
 */
knishio_response_propose_molecule_t* knishio_response_transfer_tokens_to_propose_molecule(knishio_response_transfer_tokens_t *transfer_response);

/**
 * @brief Convert from propose molecule response
 * @param propose_molecule_response Propose molecule response
 * @return Transfer tokens response (same pointer, different type)
 */
knishio_response_transfer_tokens_t* knishio_response_transfer_tokens_from_propose_molecule(knishio_response_propose_molecule_t *propose_molecule_response);

/**
 * @brief Convert to base response
 * @param transfer_response Transfer tokens response
 * @return Base response
 */
knishio_response_t* knishio_response_transfer_tokens_to_base(knishio_response_transfer_tokens_t *transfer_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Transfer tokens response or NULL if not a transfer tokens response
 */
knishio_response_transfer_tokens_t* knishio_response_transfer_tokens_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_TRANSFER_TOKENS_H */