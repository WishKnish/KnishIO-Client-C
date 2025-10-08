#ifndef KNISHIO_RESPONSE_REQUEST_AUTHORIZATION_GUEST_H
#define KNISHIO_RESPONSE_REQUEST_AUTHORIZATION_GUEST_H

/**
 * @file response_request_authorization_guest.h
 * @brief Response for RequestAuthorizationGuest mutation operations
 * 
 * Handles responses from RequestAuthorizationGuest mutations that request
 * temporary guest authorization for limited access following 2025 C17 best practices.
 */

#include "response.h"
#include "../molecule.h"
#include "../auth_token.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_request_authorization_guest knishio_response_request_authorization_guest_t;

/**
 * @brief Guest authorization request status enumeration
 */
typedef enum {
    KNISHIO_GUEST_AUTH_REQUEST_PENDING = 0,     /**< Request pending approval */
    KNISHIO_GUEST_AUTH_REQUEST_APPROVED = 1,    /**< Request approved */
    KNISHIO_GUEST_AUTH_REQUEST_REJECTED = 2,    /**< Request rejected */
    KNISHIO_GUEST_AUTH_REQUEST_EXPIRED = 3,     /**< Request expired */
    KNISHIO_GUEST_AUTH_REQUEST_REVOKED = 4      /**< Authorization revoked */
} knishio_guest_auth_request_status_t;

/**
 * @brief Guest permission level enumeration
 */
typedef enum {
    KNISHIO_GUEST_PERMISSION_READ = 1,          /**< Read-only access */
    KNISHIO_GUEST_PERMISSION_WRITE = 2,         /**< Limited write access */
    KNISHIO_GUEST_PERMISSION_METADATA = 4,      /**< Metadata operations */
    KNISHIO_GUEST_PERMISSION_QUERY = 8          /**< Query operations */
} knishio_guest_permission_t;

/**
 * @brief RequestAuthorizationGuest response structure
 * 
 * Represents the response from a RequestAuthorizationGuest GraphQL mutation
 * operation that requests temporary guest access with limited permissions.
 */
struct knishio_response_request_authorization_guest {
    knishio_response_t base;                    /**< Base response */
    knishio_molecule_t *molecule;               /**< Created request molecule */
    char *molecular_hash;                       /**< Hash of request molecule */
    char *guest_token;                          /**< Guest authorization token */
    char *request_id;                           /**< Unique request identifier */
    char *session_id;                           /**< Guest session identifier */
    knishio_guest_auth_request_status_t status; /**< Request status */
    int permissions;                            /**< Bitfield of permissions */
    char *requested_scope;                      /**< Requested access scope */
    char *granted_scope;                        /**< Actually granted scope */
    char *expires_at;                           /**< Token expiration timestamp */
    char *created_at;                           /**< Request creation timestamp */
    char *approved_by;                          /**< Approver identifier */
    char *reason;                               /**< Request reason/justification */
    int max_operations;                         /**< Maximum operations allowed */
    int operations_used;                        /**< Operations already used */
    bool success;                               /**< Operation success flag */
};

/**
 * @brief Create RequestAuthorizationGuest response
 * @param query Original RequestAuthorizationGuest mutation query
 * @param json JSON response data from GraphQL
 * @return RequestAuthorizationGuest response or NULL on error
 */
knishio_response_request_authorization_guest_t* knishio_response_request_authorization_guest_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free RequestAuthorizationGuest response
 * @param response RequestAuthorizationGuest response to free
 */
void knishio_response_request_authorization_guest_free(
    knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get request molecule
 * @param response RequestAuthorizationGuest response
 * @return Request molecule or NULL if operation failed
 */
knishio_molecule_t* knishio_response_request_authorization_guest_get_molecule(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get guest authorization token
 * @param response RequestAuthorizationGuest response
 * @return Guest token string or NULL if not granted
 */
const char* knishio_response_request_authorization_guest_get_token(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get request identifier
 * @param response RequestAuthorizationGuest response
 * @return Request ID string or NULL if not available
 */
const char* knishio_response_request_authorization_guest_get_request_id(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get guest session identifier
 * @param response RequestAuthorizationGuest response
 * @return Session ID string or NULL if not available
 */
const char* knishio_response_request_authorization_guest_get_session_id(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get authorization request status
 * @param response RequestAuthorizationGuest response
 * @return Current request status
 */
knishio_guest_auth_request_status_t knishio_response_request_authorization_guest_get_status(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get granted permissions
 * @param response RequestAuthorizationGuest response
 * @return Bitfield of granted permissions
 */
int knishio_response_request_authorization_guest_get_permissions(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Check if guest has specific permission
 * @param response RequestAuthorizationGuest response
 * @param permission Permission to check
 * @return True if permission granted, false otherwise
 */
bool knishio_response_request_authorization_guest_has_permission(
    const knishio_response_request_authorization_guest_t *response,
    knishio_guest_permission_t permission
);

/**
 * @brief Get requested access scope
 * @param response RequestAuthorizationGuest response
 * @return Requested scope string or NULL if not specified
 */
const char* knishio_response_request_authorization_guest_get_requested_scope(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get granted access scope
 * @param response RequestAuthorizationGuest response
 * @return Granted scope string or NULL if not granted
 */
const char* knishio_response_request_authorization_guest_get_granted_scope(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get token expiration timestamp
 * @param response RequestAuthorizationGuest response
 * @return Expiration timestamp or NULL if permanent
 */
const char* knishio_response_request_authorization_guest_get_expires_at(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get maximum operations allowed
 * @param response RequestAuthorizationGuest response
 * @return Maximum operations count
 */
int knishio_response_request_authorization_guest_get_max_operations(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get operations already used
 * @param response RequestAuthorizationGuest response
 * @return Used operations count
 */
int knishio_response_request_authorization_guest_get_operations_used(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get remaining operations
 * @param response RequestAuthorizationGuest response
 * @return Remaining operations count (-1 if unlimited)
 */
int knishio_response_request_authorization_guest_get_remaining_operations(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Check if guest authorization request was successful
 * @param response RequestAuthorizationGuest response
 * @return True if successful, false otherwise
 */
bool knishio_response_request_authorization_guest_is_successful(
    const knishio_response_request_authorization_guest_t *response
);

/**
 * @brief Get guest authorization status as string
 * @param status Authorization status
 * @return Status string representation
 */
const char* knishio_guest_auth_request_status_to_string(
    knishio_guest_auth_request_status_t status
);

/**
 * @brief Parse guest authorization status from string
 * @param status_str Status string
 * @return Parsed status or KNISHIO_GUEST_AUTH_REQUEST_PENDING if unknown
 */
knishio_guest_auth_request_status_t knishio_guest_auth_request_status_from_string(
    const char *status_str
);

/**
 * @brief Get permissions as formatted string
 * @param permissions Permission bitfield
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written or -1 on error
 */
int knishio_guest_permissions_to_string(
    int permissions,
    char *buffer,
    size_t buffer_size
);

/* Factory function for response creation */

/**
 * @brief Factory function for RequestAuthorizationGuest response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_request_authorization_guest_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for guest authorization operations */
extern const char* const KNISHIO_GUEST_AUTH_SUCCESS_MESSAGE;
extern const char* const KNISHIO_GUEST_AUTH_DEFAULT_SCOPE;
extern const int KNISHIO_GUEST_AUTH_DEFAULT_MAX_OPERATIONS;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_request_authorization_guest_t) > sizeof(knishio_response_t),
    "RequestAuthorizationGuest response must be larger than base response");
_Static_assert(sizeof(knishio_guest_auth_request_status_t) == sizeof(int),
    "Guest auth status must be integer-sized for ABI compatibility");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_REQUEST_AUTHORIZATION_GUEST_H */