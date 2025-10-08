#ifndef KNISHIO_RESPONSE_REQUEST_TOKENS_H
#define KNISHIO_RESPONSE_REQUEST_TOKENS_H

/**
 * @file response_request_tokens.h
 * @brief Response for RequestTokens mutation operations
 * 
 * Handles responses from RequestTokens mutations that request tokens
 * from other wallets or the system following 2025 C17 best practices.
 */

#include "response.h"
#include "../molecule.h"
#include "../wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_request_tokens knishio_response_request_tokens_t;

/**
 * @brief Token request status enumeration
 */
typedef enum {
    KNISHIO_TOKEN_REQUEST_PENDING = 0,      /**< Request pending approval */
    KNISHIO_TOKEN_REQUEST_APPROVED = 1,     /**< Request approved */
    KNISHIO_TOKEN_REQUEST_REJECTED = 2,     /**< Request rejected */
    KNISHIO_TOKEN_REQUEST_EXPIRED = 3,      /**< Request expired */
    KNISHIO_TOKEN_REQUEST_CANCELLED = 4     /**< Request cancelled */
} knishio_token_request_status_t;

/**
 * @brief RequestTokens response structure
 * 
 * Represents the response from a RequestTokens GraphQL mutation operation
 * that creates a token request between wallets.
 */
struct knishio_response_request_tokens {
    knishio_response_t base;                /**< Base response */
    knishio_molecule_t *molecule;           /**< Created request molecule */
    char *molecular_hash;                   /**< Hash of request molecule */
    char *request_id;                       /**< Unique request identifier */
    char *token_slug;                       /**< Requested token type */
    double amount;                          /**< Requested amount */
    char *from_wallet;                      /**< Source wallet address */
    char *to_wallet;                        /**< Target wallet address */
    char *reason;                           /**< Request reason/message */
    knishio_token_request_status_t status;  /**< Request status */
    char *created_at;                       /**< Request creation timestamp */
    char *expires_at;                       /**< Request expiration timestamp */
    bool success;                           /**< Operation success flag */
};

/**
 * @brief Create RequestTokens response
 * @param query Original RequestTokens mutation query
 * @param json JSON response data from GraphQL
 * @return RequestTokens response or NULL on error
 */
knishio_response_request_tokens_t* knishio_response_request_tokens_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free RequestTokens response
 * @param response RequestTokens response to free
 */
void knishio_response_request_tokens_free(knishio_response_request_tokens_t *response);

/**
 * @brief Get request molecule
 * @param response RequestTokens response
 * @return Request molecule or NULL if operation failed
 */
knishio_molecule_t* knishio_response_request_tokens_get_molecule(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get molecular hash of token request
 * @param response RequestTokens response
 * @return Molecular hash or NULL if operation failed
 */
const char* knishio_response_request_tokens_get_molecular_hash(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get request identifier
 * @param response RequestTokens response
 * @return Request ID string or NULL if not available
 */
const char* knishio_response_request_tokens_get_request_id(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get requested token type
 * @param response RequestTokens response
 * @return Token slug string or NULL if not available
 */
const char* knishio_response_request_tokens_get_token_slug(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get requested amount
 * @param response RequestTokens response
 * @return Requested token amount
 */
double knishio_response_request_tokens_get_amount(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get source wallet address
 * @param response RequestTokens response
 * @return Source wallet address or NULL if not available
 */
const char* knishio_response_request_tokens_get_from_wallet(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get target wallet address
 * @param response RequestTokens response
 * @return Target wallet address or NULL if not available
 */
const char* knishio_response_request_tokens_get_to_wallet(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get request reason
 * @param response RequestTokens response
 * @return Request reason string or NULL if not provided
 */
const char* knishio_response_request_tokens_get_reason(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get request status
 * @param response RequestTokens response
 * @return Current request status
 */
knishio_token_request_status_t knishio_response_request_tokens_get_status(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get request creation timestamp
 * @param response RequestTokens response
 * @return Creation timestamp or NULL if not available
 */
const char* knishio_response_request_tokens_get_created_at(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get request expiration timestamp
 * @param response RequestTokens response
 * @return Expiration timestamp or NULL if not set
 */
const char* knishio_response_request_tokens_get_expires_at(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Check if token request was successful
 * @param response RequestTokens response
 * @return True if successful, false otherwise
 */
bool knishio_response_request_tokens_is_successful(
    const knishio_response_request_tokens_t *response
);

/**
 * @brief Get token request status as string
 * @param status Request status
 * @return Status string representation
 */
const char* knishio_token_request_status_to_string(knishio_token_request_status_t status);

/**
 * @brief Parse token request status from string
 * @param status_str Status string
 * @return Parsed status or KNISHIO_TOKEN_REQUEST_PENDING if unknown
 */
knishio_token_request_status_t knishio_token_request_status_from_string(const char *status_str);

/**
 * @brief Get token request information as formatted string
 * @param response RequestTokens response
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written or -1 on error
 */
int knishio_response_request_tokens_get_info(
    const knishio_response_request_tokens_t *response,
    char *buffer,
    size_t buffer_size
);

/* Factory function for response creation */

/**
 * @brief Factory function for RequestTokens response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_request_tokens_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for RequestTokens operations */
extern const char* const KNISHIO_REQUEST_TOKENS_SUCCESS_MESSAGE;
extern const char* const KNISHIO_REQUEST_TOKENS_DEFAULT_REASON;
extern const double KNISHIO_REQUEST_TOKENS_MIN_AMOUNT;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_request_tokens_t) > sizeof(knishio_response_t),
    "RequestTokens response must be larger than base response");
_Static_assert(sizeof(knishio_token_request_status_t) == sizeof(int),
    "Token request status must be integer-sized for ABI compatibility");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_REQUEST_TOKENS_H */