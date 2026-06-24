#ifndef KNISHIO_RESPONSE_TYPES_H
#define KNISHIO_RESPONSE_TYPES_H

/**
 * @file response_types.h
 * @brief Central registry of all response types in KnishIO SDK
 * 
 * This file provides a comprehensive listing of all response types matching
 * the JavaScript SDK's response architecture, including factory functions
 * and type identification.
 */

#include "response.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for all response types */
typedef struct knishio_response_balance knishio_response_balance_t;
typedef struct knishio_response_wallet_list knishio_response_wallet_list_t;
typedef struct knishio_response_wallet_bundle knishio_response_wallet_bundle_t;
typedef struct knishio_response_propose_molecule knishio_response_propose_molecule_t;
typedef struct knishio_response_atom knishio_response_atom_t;
typedef struct knishio_response_create_token knishio_response_create_token_t;
typedef struct knishio_response_transfer_tokens knishio_response_transfer_tokens_t;
typedef struct knishio_response_active_session knishio_response_active_session_t;
typedef struct knishio_response_query_active_session knishio_response_query_active_session_t;
typedef struct knishio_response_continu_id knishio_response_continu_id_t;
typedef struct knishio_response_meta_type knishio_response_meta_type_t;
typedef struct knishio_response_meta_type_via_atom knishio_response_meta_type_via_atom_t;
typedef struct knishio_response_create_meta knishio_response_create_meta_t;
typedef struct knishio_response_meta_batch knishio_response_meta_batch_t;
typedef struct knishio_response_create_wallet knishio_response_create_wallet_t;
typedef struct knishio_response_request_tokens knishio_response_request_tokens_t;
typedef struct knishio_response_request_authorization knishio_response_request_authorization_t;
typedef struct knishio_response_request_authorization_guest knishio_response_request_authorization_guest_t;
typedef struct knishio_response_authorization_guest knishio_response_authorization_guest_t;
typedef struct knishio_response_create_identifier knishio_response_create_identifier_t;
typedef struct knishio_response_link_identifier knishio_response_link_identifier_t;
typedef struct knishio_response_claim_shadow_wallet knishio_response_claim_shadow_wallet_t;
typedef struct knishio_response_create_rule knishio_response_create_rule_t;
typedef struct knishio_response_policy knishio_response_policy_t;
typedef struct knishio_response_token knishio_response_token_t;
typedef struct knishio_response_batch knishio_response_batch_t;
typedef struct knishio_response_batch_history knishio_response_batch_history_t;

/**
 * @brief Response type enumeration
 */
typedef enum {
    KNISHIO_RESPONSE_TYPE_BASE,                     /**< Base response */
    KNISHIO_RESPONSE_TYPE_BALANCE,                  /**< Balance query response */
    KNISHIO_RESPONSE_TYPE_WALLET_LIST,              /**< Wallet list response */
    KNISHIO_RESPONSE_TYPE_WALLET_BUNDLE,            /**< Wallet bundle response */
    KNISHIO_RESPONSE_TYPE_PROPOSE_MOLECULE,         /**< Propose molecule response */
    KNISHIO_RESPONSE_TYPE_ATOM,                     /**< Atom query response */
    KNISHIO_RESPONSE_TYPE_CREATE_TOKEN,             /**< Create token response */
    KNISHIO_RESPONSE_TYPE_TRANSFER_TOKENS,          /**< Transfer tokens response */
    KNISHIO_RESPONSE_TYPE_ACTIVE_SESSION,           /**< Active session response */
    KNISHIO_RESPONSE_TYPE_QUERY_ACTIVE_SESSION,     /**< Query active session response */
    KNISHIO_RESPONSE_TYPE_CONTINU_ID,               /**< ContinuID response */
    KNISHIO_RESPONSE_TYPE_META_TYPE,                /**< Meta type response */
    KNISHIO_RESPONSE_TYPE_META_TYPE_VIA_ATOM,       /**< Meta type via atom response */
    KNISHIO_RESPONSE_TYPE_CREATE_META,              /**< Create meta response */
    KNISHIO_RESPONSE_TYPE_META_BATCH,               /**< Meta batch response */
    KNISHIO_RESPONSE_TYPE_CREATE_WALLET,            /**< Create wallet response */
    KNISHIO_RESPONSE_TYPE_REQUEST_TOKENS,           /**< Request tokens response */
    KNISHIO_RESPONSE_TYPE_REQUEST_AUTHORIZATION,    /**< Request authorization response */
    KNISHIO_RESPONSE_TYPE_REQUEST_AUTHORIZATION_GUEST, /**< Request guest auth response */
    KNISHIO_RESPONSE_TYPE_AUTHORIZATION_GUEST,      /**< Guest authorization response */
    KNISHIO_RESPONSE_TYPE_CREATE_IDENTIFIER,        /**< Create identifier response */
    KNISHIO_RESPONSE_TYPE_LINK_IDENTIFIER,          /**< Link identifier response */
    KNISHIO_RESPONSE_TYPE_CLAIM_SHADOW_WALLET,      /**< Claim shadow wallet response */
    KNISHIO_RESPONSE_TYPE_CREATE_RULE,              /**< Create rule response */
    KNISHIO_RESPONSE_TYPE_POLICY,                   /**< Policy response */
    KNISHIO_RESPONSE_TYPE_QUERY_USER_ACTIVITY,      /**< Query user activity response */
    KNISHIO_RESPONSE_TYPE_TOKEN,                    /**< Token query response */
    KNISHIO_RESPONSE_TYPE_BATCH,                    /**< Batch query response */
    KNISHIO_RESPONSE_TYPE_BATCH_HISTORY,            /**< Batch history response */
    KNISHIO_RESPONSE_TYPE_COUNT                     /**< Total number of types */
} knishio_response_type_t;

/**
 * @brief Response factory function type
 * @param query The query that generated this response
 * @param json The JSON response data
 * @return Created response or NULL on error
 */
typedef knishio_response_t* (*knishio_response_factory_fn)(knishio_query_t *query,
                                                           knishio_json_t *json);

/**
 * @brief Response type registry entry
 */
typedef struct {
    knishio_response_type_t type;           /**< Response type */
    const char *name;                       /**< Response type name */
    const char *data_key;                   /**< Default data key */
    knishio_response_factory_fn factory;    /**< Factory function */
} knishio_response_type_info_t;

/* Response type identification */

/**
 * @brief Get response type from response object
 * @param response Response object
 * @return Response type
 */
knishio_response_type_t knishio_response_get_type(knishio_response_t *response);

/**
 * @brief Get response type name
 * @param type Response type
 * @return Type name string
 */
const char* knishio_response_type_name(knishio_response_type_t type);

/**
 * @brief Get response type by name
 * @param name Type name string
 * @return Response type or KNISHIO_RESPONSE_TYPE_BASE if not found
 */
knishio_response_type_t knishio_response_type_from_name(const char *name);

/* Response factory registry */

/**
 * @brief Register response factory
 * @param type Response type
 * @param name Type name
 * @param data_key Default data key
 * @param factory Factory function
 * @return True on success, false on error
 */
bool knishio_response_register_type(knishio_response_type_t type,
                                    const char *name,
                                    const char *data_key,
                                    knishio_response_factory_fn factory);

/**
 * @brief Create response using factory
 * @param type Response type
 * @param query Query object
 * @param json JSON response data
 * @return Created response or NULL on error
 */
knishio_response_t* knishio_response_create_by_type(knishio_response_type_t type,
                                                    knishio_query_t *query,
                                                    knishio_json_t *json);

/**
 * @brief Create response by name
 * @param type_name Response type name
 * @param query Query object
 * @param json JSON response data
 * @return Created response or NULL on error
 */
knishio_response_t* knishio_response_create_by_name(const char *type_name,
                                                    knishio_query_t *query,
                                                    knishio_json_t *json);

/* Auto-detection */

/**
 * @brief Auto-detect response type from JSON structure
 * @param json JSON response data
 * @return Detected response type or KNISHIO_RESPONSE_TYPE_BASE
 */
knishio_response_type_t knishio_response_detect_type(knishio_json_t *json);

/**
 * @brief Create response with auto-detection
 * @param query Query object
 * @param json JSON response data
 * @return Created response with appropriate type or NULL on error
 */
knishio_response_t* knishio_response_create_auto(knishio_query_t *query,
                                                 knishio_json_t *json);

/* Utility macros for type checking */

/**
 * @brief Check if response is of specific type
 * @param response Response object
 * @param expected_type Expected response type
 * @return True if types match, false otherwise
 */
#define KNISHIO_RESPONSE_IS_TYPE(response, expected_type) \
    (knishio_response_get_type(response) == (expected_type))

/**
 * @brief Cast response to specific type (unsafe - use with type checking)
 * @param response Response object
 * @param target_type Target type (e.g., knishio_response_balance_t)
 * @return Casted response pointer
 */
#define KNISHIO_RESPONSE_CAST(response, target_type) \
    ((target_type*)(response))

/**
 * @brief Safe cast response to specific type
 * @param response Response object
 * @param expected_type Expected response type
 * @param target_type Target type (e.g., knishio_response_balance_t)
 * @return Casted response pointer or NULL if types don't match
 */
#define KNISHIO_RESPONSE_SAFE_CAST(response, expected_type, target_type) \
    (KNISHIO_RESPONSE_IS_TYPE(response, expected_type) ? \
     KNISHIO_RESPONSE_CAST(response, target_type) : NULL)

/* Standard data keys for each response type */
extern const char* const KNISHIO_DATA_KEY_BALANCE;                 /**< "data.Balance" */
extern const char* const KNISHIO_DATA_KEY_WALLET;                  /**< "data.Wallet" */
extern const char* const KNISHIO_DATA_KEY_WALLET_BUNDLE;           /**< "data.WalletBundle" */
extern const char* const KNISHIO_DATA_KEY_PROPOSE_MOLECULE;        /**< "data.ProposeMolecule" */
extern const char* const KNISHIO_DATA_KEY_ATOM;                    /**< "data.Atom" */
extern const char* const KNISHIO_DATA_KEY_ACTIVE_SESSION;          /**< "data.ActiveSession" */
extern const char* const KNISHIO_DATA_KEY_CONTINU_ID;              /**< "data.ContinuId" */
extern const char* const KNISHIO_DATA_KEY_META_TYPE;               /**< "data.MetaType" */
extern const char* const KNISHIO_DATA_KEY_CREATE_META;             /**< "data.CreateMeta" */
extern const char* const KNISHIO_DATA_KEY_REQUEST_TOKENS;          /**< "data.RequestTokens" */
extern const char* const KNISHIO_DATA_KEY_CREATE_IDENTIFIER;       /**< "data.CreateIdentifier" */
extern const char* const KNISHIO_DATA_KEY_POLICY;                  /**< "data.Policy" */

/* Initialization */

/**
 * @brief Initialize response type system
 * @return True on success, false on error
 */
bool knishio_response_types_init(void);

/**
 * @brief Cleanup response type system
 */
void knishio_response_types_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_TYPES_H */