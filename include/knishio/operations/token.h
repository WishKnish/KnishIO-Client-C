#ifndef KNISHIO_OPERATIONS_TOKEN_H
#define KNISHIO_OPERATIONS_TOKEN_H

/**
 * @file operations/token.h
 * @brief Token management operations for KnishIO C SDK
 * 
 * Provides high-level token operations matching JavaScript SDK:
 * - CreateToken mutation
 * - RequestTokens mutation
 * - Token metadata management
 * 
 * Full alignment with JS SDK token creation and management.
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
typedef struct knishio_meta knishio_meta_t;

/**
 * @brief Token fungibility types
 * Matches JavaScript SDK token types
 */
typedef enum {
    KNISHIO_TOKEN_FUNGIBLE,         /**< Fungible token (default) */
    KNISHIO_TOKEN_NONFUNGIBLE,      /**< Non-fungible token (NFT) */
    KNISHIO_TOKEN_STACKABLE,        /**< Stackable token (limited supply) */
    KNISHIO_TOKEN_REPLENISHABLE     /**< Replenishable token */
} knishio_token_fungibility_t;

/**
 * @brief Parameters for token creation
 * Matches JavaScript SDK createToken() parameters
 */
typedef struct {
    const char* token;              /**< Token slug/identifier */
    const char* name;               /**< Human-readable token name */
    int amount;                     /**< Initial supply amount */
    knishio_token_fungibility_t fungibility; /**< Token fungibility type */
    const char** units;             /**< Array of unit IDs (for NFTs) */
    size_t unit_count;              /**< Number of units */
    knishio_meta_t** meta;          /**< Array of metadata */
    size_t meta_count;              /**< Number of metadata entries */
} knishio_create_token_params_t;

/**
 * @brief Result of token creation
 */
typedef struct {
    bool success;                   /**< Creation success flag */
    char* token_slug;               /**< Created token slug */
    char* molecular_hash;           /**< Molecular hash of creation */
    char* response;                 /**< Full response data */
    char* error_message;            /**< Error message if failed */
} knishio_create_token_result_t;

/**
 * @brief Parameters for requesting tokens
 * Matches JavaScript SDK requestTokens() parameters
 */
typedef struct {
    const char* token;              /**< Token slug to request */
    const char* requested_amount;   /**< Amount to request (as string for precision) */
    const char** requested_units;   /**< Unit IDs to request (for NFTs) */
    size_t unit_count;              /**< Number of units */
    const char* to;                 /**< Recipient address/bundle (optional) */
    knishio_meta_t** meta;          /**< Request metadata */
    size_t meta_count;              /**< Number of metadata entries */
} knishio_request_tokens_params_t;

/**
 * @brief Result of token request
 */
typedef struct {
    bool success;                   /**< Request success flag */
    char* molecular_hash;           /**< Molecular hash of request */
    char* response;                 /**< Full response data */
    char* error_message;            /**< Error message if failed */
} knishio_request_tokens_result_t;

/**
 * @brief Create a new token
 * Equivalent to JavaScript: client.createToken({ token, name, amount, fungibility, meta })
 * 
 * @param client KnishIO client instance
 * @param params Token creation parameters
 * @param result Output creation result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_create_token(
    knishio_client_t* client,
    const knishio_create_token_params_t* params,
    knishio_create_token_result_t** result
);

/**
 * @brief Request tokens (mint new tokens)
 * Equivalent to JavaScript: client.requestTokens({ token, amount, units, to, meta })
 * 
 * @param client KnishIO client instance
 * @param params Request parameters
 * @param result Output request result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_request_tokens(
    knishio_client_t* client,
    const knishio_request_tokens_params_t* params,
    knishio_request_tokens_result_t** result
);

/**
 * @brief Replenish token supply
 * Equivalent to JavaScript: client.replenishToken({ token, amount })
 * 
 * @param client KnishIO client instance
 * @param token Token slug to replenish
 * @param amount Amount to add to supply
 * @param result Output result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_replenish_token(
    knishio_client_t* client,
    const char* token,
    int amount,
    knishio_request_tokens_result_t** result
);

/* Result management functions */

/**
 * @brief Free token creation result
 * @param result Result to free
 */
void knishio_create_token_result_free(knishio_create_token_result_t* result);

/**
 * @brief Free token request result
 * @param result Result to free
 */
void knishio_request_tokens_result_free(knishio_request_tokens_result_t* result);

/* Result accessor functions */

/**
 * @brief Check if token creation was successful
 * @param result Creation result
 * @return True if successful
 */
bool knishio_create_token_result_is_success(const knishio_create_token_result_t* result);

/**
 * @brief Get created token slug
 * @param result Creation result
 * @return Token slug or NULL if failed (do not free)
 */
const char* knishio_create_token_result_get_slug(const knishio_create_token_result_t* result);

/**
 * @brief Get creation molecular hash
 * @param result Creation result
 * @return Molecular hash or NULL if failed (do not free)
 */
const char* knishio_create_token_result_get_hash(const knishio_create_token_result_t* result);

/**
 * @brief Check if token request was successful
 * @param result Request result
 * @return True if successful
 */
bool knishio_request_tokens_result_is_success(const knishio_request_tokens_result_t* result);

/**
 * @brief Get request molecular hash
 * @param result Request result
 * @return Molecular hash or NULL if failed (do not free)
 */
const char* knishio_request_tokens_result_get_hash(const knishio_request_tokens_result_t* result);

/* Utility functions */

/**
 * @brief Convert fungibility enum to string
 * @param fungibility Fungibility type
 * @return String representation (do not free)
 */
const char* knishio_token_fungibility_to_string(knishio_token_fungibility_t fungibility);

/**
 * @brief Convert string to fungibility enum
 * @param str String representation
 * @return Fungibility type
 */
knishio_token_fungibility_t knishio_token_fungibility_from_string(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_OPERATIONS_TOKEN_H */
