#ifndef KNISHIO_RESPONSES_H
#define KNISHIO_RESPONSES_H

/**
 * @file responses.h
 * @brief Central include for all KnishIO response types
 * 
 * This file provides a single include point for all response types in the
 * KnishIO SDK, matching the complete JavaScript SDK response architecture.
 */

/* Core response infrastructure */
#include "response.h"
#include "response_types.h"

/* Main response types matching JavaScript SDK */
#include "response_balance.h"
#include "response_wallet_list.h"
#include "response_propose_molecule.h"
#include "response_atom.h"
#include "response_create_token.h"
#include "response_transfer_tokens.h"
#include "response_continu_id.h"

/* Additional implemented response types */
#include "response_wallet_bundle.h"
#include "response_create_wallet.h"
#include "response_active_session.h"
#include "response_request_authorization.h"
#include "response_meta_type.h"

/* Newly implemented response types (2025) */
#include "response_create_meta.h"
#include "response_meta_batch.h"
#include "response_request_tokens.h"
#include "response_request_authorization_guest.h"
#include "response_authorization_guest.h"
#include "response_create_identifier.h"
#include "response_link_identifier.h"
#include "response_claim_shadow_wallet.h"
#include "response_create_rule.h"
#include "response_policy.h"
#include "response_query_user_activity.h"
#include "response_query_active_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Additional response type forward declarations */
typedef struct knishio_response_wallet_bundle knishio_response_wallet_bundle_t;
typedef struct knishio_response_active_session knishio_response_active_session_t;
typedef struct knishio_response_query_active_session knishio_response_query_active_session_t;
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
typedef struct knishio_response_query_user_activity knishio_response_query_user_activity_t;

/**
 * @brief Complete response system initialization
 * @return True on success, false on error
 */
bool knishio_responses_init(void);

/**
 * @brief Complete response system cleanup
 */
void knishio_responses_cleanup(void);

/* Factory functions for all response types */

/**
 * @brief Create balance response factory wrapper
 * @param query Query object
 * @param json JSON response data
 * @return Response as base type
 */
knishio_response_t* knishio_response_balance_factory(knishio_query_t *query, knishio_json_t *json);

/**
 * @brief Create wallet list response factory wrapper
 * @param query Query object
 * @param json JSON response data
 * @return Response as base type
 */
knishio_response_t* knishio_response_wallet_list_factory(knishio_query_t *query, knishio_json_t *json);

/**
 * @brief Create propose molecule response factory wrapper
 * @param query Query object
 * @param json JSON response data
 * @return Response as base type
 */
knishio_response_t* knishio_response_propose_molecule_factory(knishio_query_t *query, knishio_json_t *json);

/**
 * @brief Create atom response factory wrapper
 * @param query Query object
 * @param json JSON response data
 * @return Response as base type
 */
knishio_response_t* knishio_response_atom_factory(knishio_query_t *query, knishio_json_t *json);

/**
 * @brief Create create token response factory wrapper
 * @param query Query object
 * @param json JSON response data
 * @return Response as base type
 */
knishio_response_t* knishio_response_create_token_factory(knishio_query_t *query, knishio_json_t *json);

/**
 * @brief Create transfer tokens response factory wrapper
 * @param query Query object
 * @param json JSON response data
 * @return Response as base type
 */
knishio_response_t* knishio_response_transfer_tokens_factory(knishio_query_t *query, knishio_json_t *json);

/**
 * @brief Create ContinuID response factory wrapper
 * @param query Query object
 * @param json JSON response data
 * @return Response as base type
 */
knishio_response_t* knishio_response_continu_id_factory(knishio_query_t *query, knishio_json_t *json);

/* Utility macros for common response operations */

/**
 * @brief Generic response success check
 * @param response Any response type
 * @return True if response indicates success, false otherwise
 */
#define KNISHIO_RESPONSE_SUCCESS(response) \
    (!(response) ? false : !knishio_response_has_errors((knishio_response_t*)(response)))

/**
 * @brief Generic response error message
 * @param response Any response type
 * @return Error message or NULL
 */
#define KNISHIO_RESPONSE_ERROR_MESSAGE(response) \
    (!(response) ? NULL : knishio_response_error_message((knishio_response_t*)(response)))

/**
 * @brief Generic response data access
 * @param response Any response type
 * @return Response data JSON or NULL
 */
#define KNISHIO_RESPONSE_DATA(response) \
    (!(response) ? NULL : knishio_response_data((knishio_response_t*)(response)))

/* Response validation helpers */

/**
 * @brief Validate any response type
 * @param response Response to validate
 * @return True if response is valid, false otherwise
 */
bool knishio_response_validate_any(void *response);

/**
 * @brief Get response type name as string
 * @param response Any response type
 * @return Response type name or "unknown"
 */
const char* knishio_response_get_type_name(void *response);

/**
 * @brief Check if response is of expected type
 * @param response Response to check
 * @param expected_type Expected response type
 * @return True if types match, false otherwise
 */
bool knishio_response_check_type(void *response, knishio_response_type_t expected_type);

/* Response debugging and logging */

/**
 * @brief Get response debug information
 * @param response Any response type
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written or -1 on error
 */
int knishio_response_debug_info(void *response, char *buffer, size_t buffer_size);

/**
 * @brief Log response information (if logging enabled)
 * @param response Any response type
 * @param level Log level
 * @param message Custom message prefix
 */
void knishio_response_log(void *response, int level, const char *message);

/* Constants for all response data keys */
extern const char* const KNISHIO_RESPONSE_KEY_BALANCE;
extern const char* const KNISHIO_RESPONSE_KEY_WALLET;
extern const char* const KNISHIO_RESPONSE_KEY_WALLET_BUNDLE;
extern const char* const KNISHIO_RESPONSE_KEY_PROPOSE_MOLECULE;
extern const char* const KNISHIO_RESPONSE_KEY_ATOM;
extern const char* const KNISHIO_RESPONSE_KEY_ACTIVE_SESSION;
extern const char* const KNISHIO_RESPONSE_KEY_QUERY_ACTIVE_SESSION;
extern const char* const KNISHIO_RESPONSE_KEY_CONTINU_ID;
extern const char* const KNISHIO_RESPONSE_KEY_META_TYPE;
extern const char* const KNISHIO_RESPONSE_KEY_META_TYPE_VIA_ATOM;
extern const char* const KNISHIO_RESPONSE_KEY_CREATE_META;
extern const char* const KNISHIO_RESPONSE_KEY_META_BATCH;
extern const char* const KNISHIO_RESPONSE_KEY_CREATE_WALLET;
extern const char* const KNISHIO_RESPONSE_KEY_REQUEST_TOKENS;
extern const char* const KNISHIO_RESPONSE_KEY_REQUEST_AUTHORIZATION;
extern const char* const KNISHIO_RESPONSE_KEY_REQUEST_AUTHORIZATION_GUEST;
extern const char* const KNISHIO_RESPONSE_KEY_AUTHORIZATION_GUEST;
extern const char* const KNISHIO_RESPONSE_KEY_CREATE_IDENTIFIER;
extern const char* const KNISHIO_RESPONSE_KEY_LINK_IDENTIFIER;
extern const char* const KNISHIO_RESPONSE_KEY_CLAIM_SHADOW_WALLET;
extern const char* const KNISHIO_RESPONSE_KEY_CREATE_RULE;
extern const char* const KNISHIO_RESPONSE_KEY_POLICY;
extern const char* const KNISHIO_RESPONSE_KEY_QUERY_USER_ACTIVITY;

/* Response system statistics */

/**
 * @brief Response system statistics
 */
typedef struct {
    size_t total_responses_created;     /**< Total responses created */
    size_t total_responses_freed;       /**< Total responses freed */
    size_t active_responses;            /**< Currently active responses */
    size_t total_errors;                /**< Total errors encountered */
    size_t type_counts[KNISHIO_RESPONSE_TYPE_COUNT]; /**< Count per response type */
} knishio_response_stats_t;

/**
 * @brief Get response system statistics
 * @param stats Output statistics structure
 * @return True on success, false on error
 */
bool knishio_response_get_stats(knishio_response_stats_t *stats);

/**
 * @brief Reset response system statistics
 */
void knishio_response_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSES_H */