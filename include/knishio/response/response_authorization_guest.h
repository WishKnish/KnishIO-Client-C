#ifndef KNISHIO_RESPONSE_AUTHORIZATION_GUEST_H
#define KNISHIO_RESPONSE_AUTHORIZATION_GUEST_H

/**
 * @file response_authorization_guest.h
 * @brief Response for AuthorizationGuest query operations
 * 
 * Handles responses from AuthorizationGuest queries that retrieve existing
 * guest authorization information and status following 2025 C17 best practices.
 */

#include "response.h"
#include "../auth_token.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_authorization_guest knishio_response_authorization_guest_t;

/**
 * @brief Guest authorization status enumeration
 */
typedef enum {
    KNISHIO_GUEST_AUTH_ACTIVE = 0,              /**< Authorization active */
    KNISHIO_GUEST_AUTH_INACTIVE = 1,            /**< Authorization inactive */
    KNISHIO_GUEST_AUTH_EXPIRED = 2,             /**< Authorization expired */
    KNISHIO_GUEST_AUTH_REVOKED = 3,             /**< Authorization revoked */
    KNISHIO_GUEST_AUTH_SUSPENDED = 4            /**< Authorization suspended */
} knishio_guest_auth_status_t;

/**
 * @brief Guest authorization activity log entry
 */
typedef struct knishio_guest_auth_activity {
    char *timestamp;                            /**< Activity timestamp */
    char *action;                               /**< Action performed */
    char *resource;                             /**< Resource accessed */
    char *ip_address;                           /**< Client IP address */
    char *user_agent;                           /**< Client user agent */
    bool success;                               /**< Operation success */
    char *error_message;                        /**< Error if failed */
} knishio_guest_auth_activity_t;

/**
 * @brief AuthorizationGuest response structure
 * 
 * Represents the response from an AuthorizationGuest GraphQL query operation
 * that retrieves information about existing guest authorization sessions.
 */
struct knishio_response_authorization_guest {
    knishio_response_t base;                    /**< Base response */
    char *guest_token;                          /**< Guest authorization token */
    char *session_id;                           /**< Guest session identifier */
    knishio_guest_auth_status_t status;         /**< Current authorization status */
    int permissions;                            /**< Granted permissions bitfield */
    char *scope;                                /**< Granted access scope */
    char *issued_at;                            /**< Token issue timestamp */
    char *expires_at;                           /**< Token expiration timestamp */
    char *last_used_at;                         /**< Last usage timestamp */
    char *issued_by;                            /**< Authorization issuer */
    char *issued_to;                            /**< Authorization recipient */
    int max_operations;                         /**< Maximum operations allowed */
    int operations_used;                        /**< Operations already used */
    char *allowed_ips;                          /**< Allowed IP addresses (comma-separated) */
    char *allowed_origins;                      /**< Allowed origins (comma-separated) */
    knishio_guest_auth_activity_t *activities;  /**< Activity log entries */
    size_t activity_count;                      /**< Number of activity entries */
    bool is_renewable;                          /**< Can authorization be renewed */
    char *renewal_token;                        /**< Token for renewal (if applicable) */
};

/**
 * @brief Create AuthorizationGuest response
 * @param query Original AuthorizationGuest query
 * @param json JSON response data from GraphQL
 * @return AuthorizationGuest response or NULL on error
 */
knishio_response_authorization_guest_t* knishio_response_authorization_guest_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free AuthorizationGuest response
 * @param response AuthorizationGuest response to free
 */
void knishio_response_authorization_guest_free(
    knishio_response_authorization_guest_t *response
);

/**
 * @brief Get guest authorization token
 * @param response AuthorizationGuest response
 * @return Guest token string or NULL if not available
 */
const char* knishio_response_authorization_guest_get_token(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get guest session identifier
 * @param response AuthorizationGuest response
 * @return Session ID string or NULL if not available
 */
const char* knishio_response_authorization_guest_get_session_id(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get authorization status
 * @param response AuthorizationGuest response
 * @return Current authorization status
 */
knishio_guest_auth_status_t knishio_response_authorization_guest_get_status(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get granted permissions
 * @param response AuthorizationGuest response
 * @return Bitfield of granted permissions
 */
int knishio_response_authorization_guest_get_permissions(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Check if guest has specific permission
 * @param response AuthorizationGuest response
 * @param permission Permission to check
 * @return True if permission granted, false otherwise
 */
bool knishio_response_authorization_guest_has_permission(
    const knishio_response_authorization_guest_t *response,
    int permission
);

/**
 * @brief Get granted access scope
 * @param response AuthorizationGuest response
 * @return Scope string or NULL if not specified
 */
const char* knishio_response_authorization_guest_get_scope(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get token issue timestamp
 * @param response AuthorizationGuest response
 * @return Issue timestamp or NULL if not available
 */
const char* knishio_response_authorization_guest_get_issued_at(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get token expiration timestamp
 * @param response AuthorizationGuest response
 * @return Expiration timestamp or NULL if permanent
 */
const char* knishio_response_authorization_guest_get_expires_at(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get last usage timestamp
 * @param response AuthorizationGuest response
 * @return Last usage timestamp or NULL if never used
 */
const char* knishio_response_authorization_guest_get_last_used_at(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Check if authorization is currently active
 * @param response AuthorizationGuest response
 * @return True if active, false otherwise
 */
bool knishio_response_authorization_guest_is_active(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Check if authorization is expired
 * @param response AuthorizationGuest response
 * @return True if expired, false otherwise
 */
bool knishio_response_authorization_guest_is_expired(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get remaining operations
 * @param response AuthorizationGuest response
 * @return Remaining operations count (-1 if unlimited)
 */
int knishio_response_authorization_guest_get_remaining_operations(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get activity log entries
 * @param response AuthorizationGuest response
 * @param count Output parameter for number of activities
 * @return Array of activity entries or NULL if no activities
 */
const knishio_guest_auth_activity_t* knishio_response_authorization_guest_get_activities(
    const knishio_response_authorization_guest_t *response,
    size_t *count
);

/**
 * @brief Check if authorization can be renewed
 * @param response AuthorizationGuest response
 * @return True if renewable, false otherwise
 */
bool knishio_response_authorization_guest_is_renewable(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get renewal token if available
 * @param response AuthorizationGuest response
 * @return Renewal token string or NULL if not renewable
 */
const char* knishio_response_authorization_guest_get_renewal_token(
    const knishio_response_authorization_guest_t *response
);

/**
 * @brief Get authorization status as string
 * @param status Authorization status
 * @return Status string representation
 */
const char* knishio_guest_auth_status_to_string(knishio_guest_auth_status_t status);

/**
 * @brief Parse authorization status from string
 * @param status_str Status string
 * @return Parsed status or KNISHIO_GUEST_AUTH_INACTIVE if unknown
 */
knishio_guest_auth_status_t knishio_guest_auth_status_from_string(const char *status_str);

/**
 * @brief Get authorization summary as formatted string
 * @param response AuthorizationGuest response
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written or -1 on error
 */
int knishio_response_authorization_guest_get_summary(
    const knishio_response_authorization_guest_t *response,
    char *buffer,
    size_t buffer_size
);

/* Activity entry utilities */

/**
 * @brief Free guest authorization activity entry
 * @param activity Activity entry to free
 */
void knishio_guest_auth_activity_free(knishio_guest_auth_activity_t *activity);

/**
 * @brief Copy guest authorization activity entry
 * @param activity Activity entry to copy
 * @return Copied activity entry or NULL on error
 */
knishio_guest_auth_activity_t* knishio_guest_auth_activity_copy(
    const knishio_guest_auth_activity_t *activity
);

/* Factory function for response creation */

/**
 * @brief Factory function for AuthorizationGuest response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_authorization_guest_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for guest authorization */
extern const char* const KNISHIO_GUEST_AUTH_DEFAULT_SCOPE;
extern const int KNISHIO_GUEST_AUTH_DEFAULT_PERMISSIONS;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_authorization_guest_t) > sizeof(knishio_response_t),
    "AuthorizationGuest response must be larger than base response");
_Static_assert(sizeof(knishio_guest_auth_status_t) == sizeof(int),
    "Guest auth status must be integer-sized for ABI compatibility");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_AUTHORIZATION_GUEST_H */