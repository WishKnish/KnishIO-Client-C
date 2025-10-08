#ifndef KNISHIO_RESPONSE_CREATE_IDENTIFIER_H
#define KNISHIO_RESPONSE_CREATE_IDENTIFIER_H

/**
 * @file response_create_identifier.h
 * @brief Response for CreateIdentifier mutation operations
 * 
 * Handles responses from CreateIdentifier mutations that create new
 * identity entries in the KnishIO DLT following 2025 C17 best practices.
 */

#include "response.h"
#include "../molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_create_identifier knishio_response_create_identifier_t;

/**
 * @brief Identifier type enumeration
 */
typedef enum {
    KNISHIO_IDENTIFIER_TYPE_EMAIL = 0,          /**< Email address identifier */
    KNISHIO_IDENTIFIER_TYPE_PHONE = 1,          /**< Phone number identifier */
    KNISHIO_IDENTIFIER_TYPE_USERNAME = 2,       /**< Username identifier */
    KNISHIO_IDENTIFIER_TYPE_DOMAIN = 3,         /**< Domain name identifier */
    KNISHIO_IDENTIFIER_TYPE_SSN = 4,            /**< Social Security Number */
    KNISHIO_IDENTIFIER_TYPE_PASSPORT = 5,       /**< Passport number */
    KNISHIO_IDENTIFIER_TYPE_LICENSE = 6,        /**< License identifier */
    KNISHIO_IDENTIFIER_TYPE_CUSTOM = 99         /**< Custom identifier type */
} knishio_identifier_type_t;

/**
 * @brief Identifier verification status enumeration
 */
typedef enum {
    KNISHIO_IDENTIFIER_UNVERIFIED = 0,          /**< Not verified */
    KNISHIO_IDENTIFIER_PENDING = 1,             /**< Verification pending */
    KNISHIO_IDENTIFIER_VERIFIED = 2,            /**< Successfully verified */
    KNISHIO_IDENTIFIER_FAILED = 3,              /**< Verification failed */
    KNISHIO_IDENTIFIER_EXPIRED = 4              /**< Verification expired */
} knishio_identifier_verification_status_t;

/**
 * @brief CreateIdentifier response structure
 * 
 * Represents the response from a CreateIdentifier GraphQL mutation operation
 * that creates a new identity entry linked to a wallet or bundle.
 */
struct knishio_response_create_identifier {
    knishio_response_t base;                    /**< Base response */
    knishio_molecule_t *molecule;               /**< Created identifier molecule */
    char *molecular_hash;                       /**< Hash of identifier molecule */
    char *identifier_hash;                      /**< Unique identifier hash */
    char *identifier_value;                     /**< Identifier value/content */
    knishio_identifier_type_t identifier_type;  /**< Type of identifier */
    char *custom_type_name;                     /**< Custom type name if applicable */
    char *bundle_hash;                          /**< Associated bundle hash */
    char *wallet_address;                       /**< Associated wallet address */
    knishio_identifier_verification_status_t verification_status; /**< Verification status */
    char *verification_token;                   /**< Verification token */
    char *verification_expires_at;              /**< Verification expiration */
    char *created_at;                           /**< Creation timestamp */
    char *verified_at;                          /**< Verification timestamp */
    char *metadata;                             /**< Additional metadata (JSON) */
    bool is_primary;                            /**< Primary identifier flag */
    bool is_public;                             /**< Public visibility flag */
    bool success;                               /**< Operation success flag */
};

/**
 * @brief Create CreateIdentifier response
 * @param query Original CreateIdentifier mutation query
 * @param json JSON response data from GraphQL
 * @return CreateIdentifier response or NULL on error
 */
knishio_response_create_identifier_t* knishio_response_create_identifier_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free CreateIdentifier response
 * @param response CreateIdentifier response to free
 */
void knishio_response_create_identifier_free(knishio_response_create_identifier_t *response);

/**
 * @brief Get identifier molecule
 * @param response CreateIdentifier response
 * @return Identifier molecule or NULL if operation failed
 */
knishio_molecule_t* knishio_response_create_identifier_get_molecule(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get molecular hash of identifier
 * @param response CreateIdentifier response
 * @return Molecular hash or NULL if operation failed
 */
const char* knishio_response_create_identifier_get_molecular_hash(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get unique identifier hash
 * @param response CreateIdentifier response
 * @return Identifier hash string or NULL if not available
 */
const char* knishio_response_create_identifier_get_identifier_hash(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get identifier value
 * @param response CreateIdentifier response
 * @return Identifier value string or NULL if not available
 */
const char* knishio_response_create_identifier_get_identifier_value(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get identifier type
 * @param response CreateIdentifier response
 * @return Identifier type
 */
knishio_identifier_type_t knishio_response_create_identifier_get_identifier_type(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get custom type name (for custom identifiers)
 * @param response CreateIdentifier response
 * @return Custom type name or NULL if not custom type
 */
const char* knishio_response_create_identifier_get_custom_type_name(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get associated bundle hash
 * @param response CreateIdentifier response
 * @return Bundle hash or NULL if not associated
 */
const char* knishio_response_create_identifier_get_bundle_hash(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get associated wallet address
 * @param response CreateIdentifier response
 * @return Wallet address or NULL if not associated
 */
const char* knishio_response_create_identifier_get_wallet_address(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get verification status
 * @param response CreateIdentifier response
 * @return Current verification status
 */
knishio_identifier_verification_status_t knishio_response_create_identifier_get_verification_status(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get verification token
 * @param response CreateIdentifier response
 * @return Verification token or NULL if not available
 */
const char* knishio_response_create_identifier_get_verification_token(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Check if identifier is verified
 * @param response CreateIdentifier response
 * @return True if verified, false otherwise
 */
bool knishio_response_create_identifier_is_verified(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Check if identifier is primary
 * @param response CreateIdentifier response
 * @return True if primary identifier, false otherwise
 */
bool knishio_response_create_identifier_is_primary(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Check if identifier is public
 * @param response CreateIdentifier response
 * @return True if publicly visible, false otherwise
 */
bool knishio_response_create_identifier_is_public(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Check if identifier creation was successful
 * @param response CreateIdentifier response
 * @return True if successful, false otherwise
 */
bool knishio_response_create_identifier_is_successful(
    const knishio_response_create_identifier_t *response
);

/**
 * @brief Get identifier type as string
 * @param type Identifier type
 * @return Type string representation
 */
const char* knishio_identifier_type_to_string(knishio_identifier_type_t type);

/**
 * @brief Parse identifier type from string
 * @param type_str Type string
 * @return Parsed type or KNISHIO_IDENTIFIER_TYPE_CUSTOM if unknown
 */
knishio_identifier_type_t knishio_identifier_type_from_string(const char *type_str);

/**
 * @brief Get verification status as string
 * @param status Verification status
 * @return Status string representation
 */
const char* knishio_identifier_verification_status_to_string(
    knishio_identifier_verification_status_t status
);

/**
 * @brief Parse verification status from string
 * @param status_str Status string
 * @return Parsed status or KNISHIO_IDENTIFIER_UNVERIFIED if unknown
 */
knishio_identifier_verification_status_t knishio_identifier_verification_status_from_string(
    const char *status_str
);

/**
 * @brief Get identifier information as formatted string
 * @param response CreateIdentifier response
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written or -1 on error
 */
int knishio_response_create_identifier_get_info(
    const knishio_response_create_identifier_t *response,
    char *buffer,
    size_t buffer_size
);

/* Factory function for response creation */

/**
 * @brief Factory function for CreateIdentifier response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_create_identifier_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for identifier operations */
extern const char* const KNISHIO_CREATE_IDENTIFIER_SUCCESS_MESSAGE;
extern const char* const KNISHIO_IDENTIFIER_DEFAULT_METADATA;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_create_identifier_t) > sizeof(knishio_response_t),
    "CreateIdentifier response must be larger than base response");
_Static_assert(sizeof(knishio_identifier_type_t) == sizeof(int),
    "Identifier type must be integer-sized for ABI compatibility");
_Static_assert(sizeof(knishio_identifier_verification_status_t) == sizeof(int),
    "Verification status must be integer-sized for ABI compatibility");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_CREATE_IDENTIFIER_H */