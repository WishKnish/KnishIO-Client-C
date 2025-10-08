#ifndef KNISHIO_RESPONSE_LINK_IDENTIFIER_H
#define KNISHIO_RESPONSE_LINK_IDENTIFIER_H

/**
 * @file response_link_identifier.h
 * @brief Response for LinkIdentifier mutation operations
 * 
 * Handles responses from LinkIdentifier mutations that link existing
 * identifiers to wallets or bundles following 2025 C17 best practices.
 */

#include "response.h"
#include "../molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_link_identifier knishio_response_link_identifier_t;

/**
 * @brief Identifier link type enumeration
 */
typedef enum {
    KNISHIO_LINK_TYPE_WALLET = 0,               /**< Link to wallet address */
    KNISHIO_LINK_TYPE_BUNDLE = 1,               /**< Link to bundle hash */
    KNISHIO_LINK_TYPE_IDENTITY = 2,             /**< Link to identity bundle */
    KNISHIO_LINK_TYPE_ASSET = 3,                /**< Link to asset/token */
    KNISHIO_LINK_TYPE_CONTRACT = 4,             /**< Link to smart contract */
    KNISHIO_LINK_TYPE_DOMAIN = 5                /**< Link to domain/namespace */
} knishio_link_type_t;

/**
 * @brief Link status enumeration
 */
typedef enum {
    KNISHIO_LINK_STATUS_PENDING = 0,            /**< Link pending confirmation */
    KNISHIO_LINK_STATUS_ACTIVE = 1,             /**< Link active and confirmed */
    KNISHIO_LINK_STATUS_SUSPENDED = 2,          /**< Link temporarily suspended */
    KNISHIO_LINK_STATUS_REVOKED = 3,            /**< Link permanently revoked */
    KNISHIO_LINK_STATUS_EXPIRED = 4             /**< Link expired */
} knishio_link_status_t;

/**
 * @brief Linked identifier relationship information
 */
typedef struct knishio_identifier_link {
    char *identifier_hash;                      /**< Linked identifier hash */
    char *identifier_value;                     /**< Identifier value */
    char *target_hash;                          /**< Target wallet/bundle hash */
    knishio_link_type_t link_type;              /**< Type of link */
    knishio_link_status_t status;               /**< Link status */
    char *linked_at;                            /**< Link creation timestamp */
    char *confirmed_at;                         /**< Link confirmation timestamp */
    char *expires_at;                           /**< Link expiration (optional) */
    char *metadata;                             /**< Link metadata (JSON) */
    bool is_bidirectional;                      /**< Bidirectional link flag */
    bool is_primary;                            /**< Primary link flag */
} knishio_identifier_link_t;

/**
 * @brief LinkIdentifier response structure
 * 
 * Represents the response from a LinkIdentifier GraphQL mutation operation
 * that creates relationships between identifiers and other entities.
 */
struct knishio_response_link_identifier {
    knishio_response_t base;                    /**< Base response */
    knishio_molecule_t *molecule;               /**< Created link molecule */
    char *molecular_hash;                       /**< Hash of link molecule */
    knishio_identifier_link_t *links;           /**< Array of created links */
    size_t link_count;                          /**< Number of links created */
    size_t link_capacity;                       /**< Current array capacity */
    char *transaction_id;                       /**< Link transaction ID */
    char *batch_id;                             /**< Batch ID for multiple links */
    bool requires_confirmation;                 /**< Confirmation required flag */
    char *confirmation_token;                   /**< Confirmation token */
    char *confirmation_expires_at;              /**< Confirmation expiration */
    bool success;                               /**< Operation success flag */
};

/**
 * @brief Create LinkIdentifier response
 * @param query Original LinkIdentifier mutation query
 * @param json JSON response data from GraphQL
 * @return LinkIdentifier response or NULL on error
 */
knishio_response_link_identifier_t* knishio_response_link_identifier_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free LinkIdentifier response
 * @param response LinkIdentifier response to free
 */
void knishio_response_link_identifier_free(knishio_response_link_identifier_t *response);

/**
 * @brief Get link molecule
 * @param response LinkIdentifier response
 * @return Link molecule or NULL if operation failed
 */
knishio_molecule_t* knishio_response_link_identifier_get_molecule(
    const knishio_response_link_identifier_t *response
);

/**
 * @brief Get molecular hash of link transaction
 * @param response LinkIdentifier response
 * @return Molecular hash or NULL if operation failed
 */
const char* knishio_response_link_identifier_get_molecular_hash(
    const knishio_response_link_identifier_t *response
);

/**
 * @brief Get created identifier links
 * @param response LinkIdentifier response
 * @param count Output parameter for number of links
 * @return Array of identifier links or NULL if no links
 */
const knishio_identifier_link_t* knishio_response_link_identifier_get_links(
    const knishio_response_link_identifier_t *response,
    size_t *count
);

/**
 * @brief Get identifier link by index
 * @param response LinkIdentifier response
 * @param index Link index
 * @return Identifier link or NULL if index out of bounds
 */
const knishio_identifier_link_t* knishio_response_link_identifier_get_link(
    const knishio_response_link_identifier_t *response,
    size_t index
);

/**
 * @brief Find identifier link by identifier hash
 * @param response LinkIdentifier response
 * @param identifier_hash Identifier hash to search for
 * @return First matching identifier link or NULL if not found
 */
const knishio_identifier_link_t* knishio_response_link_identifier_find_link(
    const knishio_response_link_identifier_t *response,
    const char *identifier_hash
);

/**
 * @brief Get transaction identifier
 * @param response LinkIdentifier response
 * @return Transaction ID or NULL if not available
 */
const char* knishio_response_link_identifier_get_transaction_id(
    const knishio_response_link_identifier_t *response
);

/**
 * @brief Get batch identifier
 * @param response LinkIdentifier response
 * @return Batch ID or NULL if single operation
 */
const char* knishio_response_link_identifier_get_batch_id(
    const knishio_response_link_identifier_t *response
);

/**
 * @brief Check if confirmation is required
 * @param response LinkIdentifier response
 * @return True if confirmation required, false otherwise
 */
bool knishio_response_link_identifier_requires_confirmation(
    const knishio_response_link_identifier_t *response
);

/**
 * @brief Get confirmation token
 * @param response LinkIdentifier response
 * @return Confirmation token or NULL if no confirmation needed
 */
const char* knishio_response_link_identifier_get_confirmation_token(
    const knishio_response_link_identifier_t *response
);

/**
 * @brief Get number of links created
 * @param response LinkIdentifier response
 * @return Number of identifier links
 */
size_t knishio_response_link_identifier_get_count(
    const knishio_response_link_identifier_t *response
);

/**
 * @brief Check if link operation was successful
 * @param response LinkIdentifier response
 * @return True if successful, false otherwise
 */
bool knishio_response_link_identifier_is_successful(
    const knishio_response_link_identifier_t *response
);

/**
 * @brief Get link type as string
 * @param type Link type
 * @return Type string representation
 */
const char* knishio_link_type_to_string(knishio_link_type_t type);

/**
 * @brief Parse link type from string
 * @param type_str Type string
 * @return Parsed type or KNISHIO_LINK_TYPE_WALLET if unknown
 */
knishio_link_type_t knishio_link_type_from_string(const char *type_str);

/**
 * @brief Get link status as string
 * @param status Link status
 * @return Status string representation
 */
const char* knishio_link_status_to_string(knishio_link_status_t status);

/**
 * @brief Parse link status from string
 * @param status_str Status string
 * @return Parsed status or KNISHIO_LINK_STATUS_PENDING if unknown
 */
knishio_link_status_t knishio_link_status_from_string(const char *status_str);

/**
 * @brief Get link summary as formatted string
 * @param response LinkIdentifier response
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written or -1 on error
 */
int knishio_response_link_identifier_get_summary(
    const knishio_response_link_identifier_t *response,
    char *buffer,
    size_t buffer_size
);

/* Link manipulation functions */

/**
 * @brief Free identifier link
 * @param link Link to free
 */
void knishio_identifier_link_free(knishio_identifier_link_t *link);

/**
 * @brief Copy identifier link
 * @param link Link to copy
 * @return Copied link or NULL on error
 */
knishio_identifier_link_t* knishio_identifier_link_copy(
    const knishio_identifier_link_t *link
);

/* Factory function for response creation */

/**
 * @brief Factory function for LinkIdentifier response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_link_identifier_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for link operations */
extern const char* const KNISHIO_LINK_IDENTIFIER_SUCCESS_MESSAGE;
extern const char* const KNISHIO_LINK_IDENTIFIER_DEFAULT_METADATA;
extern const size_t KNISHIO_LINK_IDENTIFIER_DEFAULT_CAPACITY;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_link_identifier_t) > sizeof(knishio_response_t),
    "LinkIdentifier response must be larger than base response");
_Static_assert(sizeof(knishio_link_type_t) == sizeof(int),
    "Link type must be integer-sized for ABI compatibility");
_Static_assert(sizeof(knishio_link_status_t) == sizeof(int),
    "Link status must be integer-sized for ABI compatibility");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_LINK_IDENTIFIER_H */