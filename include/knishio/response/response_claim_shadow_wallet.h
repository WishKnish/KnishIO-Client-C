#ifndef KNISHIO_RESPONSE_CLAIM_SHADOW_WALLET_H
#define KNISHIO_RESPONSE_CLAIM_SHADOW_WALLET_H

/**
 * @file response_claim_shadow_wallet.h
 * @brief Response for ClaimShadowWallet mutation operations
 * 
 * Handles responses from ClaimShadowWallet mutations that claim shadow wallets
 * and convert them to owned wallets following 2025 C17 best practices.
 */

#include "response.h"
#include "../molecule.h"
#include "../wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_claim_shadow_wallet knishio_response_claim_shadow_wallet_t;

/**
 * @brief Shadow wallet claim status enumeration
 */
typedef enum {
    KNISHIO_SHADOW_CLAIM_PENDING = 0,           /**< Claim pending verification */
    KNISHIO_SHADOW_CLAIM_VERIFIED = 1,          /**< Claim verified and processed */
    KNISHIO_SHADOW_CLAIM_COMPLETED = 2,         /**< Claim successfully completed */
    KNISHIO_SHADOW_CLAIM_REJECTED = 3,          /**< Claim rejected */
    KNISHIO_SHADOW_CLAIM_EXPIRED = 4,           /**< Claim expired */
    KNISHIO_SHADOW_CLAIM_FAILED = 5             /**< Claim failed due to error */
} knishio_shadow_claim_status_t;

/**
 * @brief Shadow wallet information before claim
 */
typedef struct knishio_shadow_wallet_info {
    char *shadow_address;                       /**< Original shadow address */
    char *token_slug;                           /**< Token type in shadow wallet */
    double balance;                             /**< Shadow wallet balance */
    char *batch_id;                             /**< Batch ID that created shadow */
    char *created_at;                           /**< Shadow creation timestamp */
    char *expires_at;                           /**< Shadow expiration timestamp */
    char *created_by;                           /**< Creator bundle hash */
    char *memo;                                 /**< Shadow wallet memo */
    bool was_expired;                           /**< Was shadow expired at claim */
} knishio_shadow_wallet_info_t;

/**
 * @brief ClaimShadowWallet response structure
 * 
 * Represents the response from a ClaimShadowWallet GraphQL mutation operation
 * that claims ownership of a shadow wallet and transfers its contents.
 */
struct knishio_response_claim_shadow_wallet {
    knishio_response_t base;                    /**< Base response */
    knishio_molecule_t *molecule;               /**< Created claim molecule */
    char *molecular_hash;                       /**< Hash of claim molecule */
    char *claim_id;                             /**< Unique claim identifier */
    knishio_shadow_claim_status_t status;       /**< Claim status */
    knishio_shadow_wallet_info_t shadow_info;   /**< Original shadow wallet info */
    knishio_wallet_t *claimed_wallet;           /**< Newly claimed wallet */
    char *new_wallet_address;                   /**< New wallet address */
    char *transfer_molecular_hash;              /**< Hash of transfer molecule */
    double transferred_amount;                  /**< Amount transferred to new wallet */
    char *claim_secret;                         /**< Secret used for claiming */
    char *verification_token;                   /**< Verification token */
    char *claimed_at;                           /**< Claim completion timestamp */
    char *processed_at;                         /**< Processing completion timestamp */
    bool requires_verification;                 /**< Verification requirement flag */
    bool shadow_destroyed;                      /**< Shadow wallet destroyed flag */
    bool success;                               /**< Operation success flag */
};

/**
 * @brief Create ClaimShadowWallet response
 * @param query Original ClaimShadowWallet mutation query
 * @param json JSON response data from GraphQL
 * @return ClaimShadowWallet response or NULL on error
 */
knishio_response_claim_shadow_wallet_t* knishio_response_claim_shadow_wallet_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free ClaimShadowWallet response
 * @param response ClaimShadowWallet response to free
 */
void knishio_response_claim_shadow_wallet_free(knishio_response_claim_shadow_wallet_t *response);

/**
 * @brief Get claim molecule
 * @param response ClaimShadowWallet response
 * @return Claim molecule or NULL if operation failed
 */
knishio_molecule_t* knishio_response_claim_shadow_wallet_get_molecule(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get molecular hash of claim
 * @param response ClaimShadowWallet response
 * @return Molecular hash or NULL if operation failed
 */
const char* knishio_response_claim_shadow_wallet_get_molecular_hash(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get claim identifier
 * @param response ClaimShadowWallet response
 * @return Claim ID string or NULL if not available
 */
const char* knishio_response_claim_shadow_wallet_get_claim_id(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get claim status
 * @param response ClaimShadowWallet response
 * @return Current claim status
 */
knishio_shadow_claim_status_t knishio_response_claim_shadow_wallet_get_status(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get shadow wallet information
 * @param response ClaimShadowWallet response
 * @return Shadow wallet info structure
 */
const knishio_shadow_wallet_info_t* knishio_response_claim_shadow_wallet_get_shadow_info(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get claimed wallet
 * @param response ClaimShadowWallet response
 * @return Claimed wallet or NULL if not yet claimed
 */
knishio_wallet_t* knishio_response_claim_shadow_wallet_get_claimed_wallet(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get new wallet address
 * @param response ClaimShadowWallet response
 * @return New wallet address or NULL if not yet generated
 */
const char* knishio_response_claim_shadow_wallet_get_new_wallet_address(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get transferred amount
 * @param response ClaimShadowWallet response
 * @return Amount transferred from shadow wallet
 */
double knishio_response_claim_shadow_wallet_get_transferred_amount(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get transfer molecular hash
 * @param response ClaimShadowWallet response
 * @return Transfer molecule hash or NULL if transfer not completed
 */
const char* knishio_response_claim_shadow_wallet_get_transfer_molecular_hash(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get verification token
 * @param response ClaimShadowWallet response
 * @return Verification token or NULL if no verification needed
 */
const char* knishio_response_claim_shadow_wallet_get_verification_token(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Check if verification is required
 * @param response ClaimShadowWallet response
 * @return True if verification required, false otherwise
 */
bool knishio_response_claim_shadow_wallet_requires_verification(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Check if shadow wallet was destroyed
 * @param response ClaimShadowWallet response
 * @return True if shadow destroyed, false otherwise
 */
bool knishio_response_claim_shadow_wallet_is_shadow_destroyed(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Check if claim was successful
 * @param response ClaimShadowWallet response
 * @return True if successful, false otherwise
 */
bool knishio_response_claim_shadow_wallet_is_successful(
    const knishio_response_claim_shadow_wallet_t *response
);

/**
 * @brief Get claim status as string
 * @param status Claim status
 * @return Status string representation
 */
const char* knishio_shadow_claim_status_to_string(knishio_shadow_claim_status_t status);

/**
 * @brief Parse claim status from string
 * @param status_str Status string
 * @return Parsed status or KNISHIO_SHADOW_CLAIM_PENDING if unknown
 */
knishio_shadow_claim_status_t knishio_shadow_claim_status_from_string(const char *status_str);

/**
 * @brief Get claim summary as formatted string
 * @param response ClaimShadowWallet response
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written or -1 on error
 */
int knishio_response_claim_shadow_wallet_get_summary(
    const knishio_response_claim_shadow_wallet_t *response,
    char *buffer,
    size_t buffer_size
);

/* Shadow wallet info utilities */

/**
 * @brief Free shadow wallet info
 * @param info Shadow wallet info to free
 */
void knishio_shadow_wallet_info_free(knishio_shadow_wallet_info_t *info);

/**
 * @brief Copy shadow wallet info
 * @param info Shadow wallet info to copy
 * @return Copied info or NULL on error
 */
knishio_shadow_wallet_info_t* knishio_shadow_wallet_info_copy(
    const knishio_shadow_wallet_info_t *info
);

/**
 * @brief Initialize shadow wallet info with defaults
 * @param info Shadow wallet info to initialize
 */
void knishio_shadow_wallet_info_init(knishio_shadow_wallet_info_t *info);

/* Factory function for response creation */

/**
 * @brief Factory function for ClaimShadowWallet response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_claim_shadow_wallet_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for shadow wallet claim operations */
extern const char* const KNISHIO_CLAIM_SHADOW_SUCCESS_MESSAGE;
extern const char* const KNISHIO_SHADOW_DEFAULT_MEMO;
extern const double KNISHIO_SHADOW_MIN_CLAIMABLE_AMOUNT;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_claim_shadow_wallet_t) > sizeof(knishio_response_t),
    "ClaimShadowWallet response must be larger than base response");
_Static_assert(sizeof(knishio_shadow_claim_status_t) == sizeof(int),
    "Shadow claim status must be integer-sized for ABI compatibility");
_Static_assert(sizeof(knishio_shadow_wallet_info_t) >= sizeof(char*) * 8,
    "Shadow wallet info must have sufficient space for all fields");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_CLAIM_SHADOW_WALLET_H */