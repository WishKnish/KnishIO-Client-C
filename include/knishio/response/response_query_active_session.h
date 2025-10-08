#ifndef KNISHIO_RESPONSE_QUERY_ACTIVE_SESSION_H
#define KNISHIO_RESPONSE_QUERY_ACTIVE_SESSION_H

/**
 * @file response_query_active_session.h
 * @brief Response for QueryActiveSession query operations
 * 
 * Handles responses from QueryActiveSession queries that retrieve active
 * user session information and status following 2025 C17 best practices.
 */

#include "response.h"
#include "../auth_token.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_query_active_session knishio_response_query_active_session_t;

/**
 * @brief Session state enumeration
 */
typedef enum {
    KNISHIO_SESSION_STATE_ACTIVE = 0,           /**< Session is active */
    KNISHIO_SESSION_STATE_IDLE = 1,             /**< Session is idle */
    KNISHIO_SESSION_STATE_EXPIRING = 2,         /**< Session expiring soon */
    KNISHIO_SESSION_STATE_EXPIRED = 3,          /**< Session has expired */
    KNISHIO_SESSION_STATE_TERMINATED = 4,       /**< Session terminated */
    KNISHIO_SESSION_STATE_SUSPENDED = 5         /**< Session suspended */
} knishio_session_state_t;

/**
 * @brief Session type enumeration
 */
typedef enum {
    KNISHIO_SESSION_TYPE_REGULAR = 0,           /**< Regular user session */
    KNISHIO_SESSION_TYPE_GUEST = 1,             /**< Guest session */
    KNISHIO_SESSION_TYPE_SERVICE = 2,           /**< Service/API session */
    KNISHIO_SESSION_TYPE_ADMIN = 3,             /**< Administrative session */
    KNISHIO_SESSION_TYPE_SYSTEM = 4             /**< System session */
} knishio_session_type_t;

/**
 * @brief Session activity information
 */
typedef struct knishio_session_activity {
    char *last_activity_at;                     /**< Last activity timestamp */
    char *last_ip_address;                      /**< Last known IP address */
    char *last_user_agent;                      /**< Last user agent string */
    char *last_location;                        /**< Last geographic location */
    int activity_count;                         /**< Total activities in session */
    int transaction_count;                      /**< Total transactions in session */
    double total_volume;                        /**< Total transaction volume */
    char *most_recent_action;                   /**< Most recent action performed */
} knishio_session_activity_t;

/**
 * @brief Session security information
 */
typedef struct knishio_session_security {
    char *authentication_method;                /**< Authentication method used */
    char *device_fingerprint;                   /**< Device fingerprint */
    bool is_multi_factor;                       /**< Multi-factor authentication used */
    bool is_secure_connection;                  /**< Secure connection flag */
    int risk_score;                             /**< Session risk score (0-100) */
    char **security_flags;                      /**< Array of security flags */
    size_t security_flag_count;                 /**< Number of security flags */
    char *last_security_check;                  /**< Last security check timestamp */
} knishio_session_security_t;

/**
 * @brief Active session information structure
 */
typedef struct knishio_active_session_info {
    char *session_id;                           /**< Unique session identifier */
    char *bundle_hash;                          /**< Associated bundle hash */
    char *wallet_address;                       /**< Primary wallet address */
    knishio_session_state_t state;              /**< Current session state */
    knishio_session_type_t type;                /**< Session type */
    char *started_at;                           /**< Session start timestamp */
    char *expires_at;                           /**< Session expiration timestamp */
    char *last_renewed_at;                      /**< Last renewal timestamp */
    int idle_timeout_minutes;                   /**< Idle timeout in minutes */
    int max_duration_minutes;                   /**< Maximum duration in minutes */
    knishio_session_activity_t activity;        /**< Session activity information */
    knishio_session_security_t security;        /**< Session security information */
    char **permissions;                         /**< Array of session permissions */
    size_t permission_count;                    /**< Number of permissions */
    char *metadata;                             /**< Additional metadata (JSON) */
    bool is_renewable;                          /**< Session renewal capability */
    bool requires_reauth;                       /**< Re-authentication required flag */
} knishio_active_session_info_t;

/**
 * @brief QueryActiveSession response structure
 * 
 * Represents the response from a QueryActiveSession GraphQL query operation
 * that retrieves active session information for a user or bundle.
 */
struct knishio_response_query_active_session {
    knishio_response_t base;                    /**< Base response */
    knishio_active_session_info_t *sessions;    /**< Array of active sessions */
    size_t session_count;                       /**< Number of active sessions */
    size_t session_capacity;                    /**< Current array capacity */
    char *query_bundle_hash;                    /**< Queried bundle hash */
    char *query_session_id;                     /**< Specific session ID queried */
    char *query_timestamp;                      /**< Query execution timestamp */
    bool include_expired;                       /**< Include expired sessions flag */
    bool include_guest_sessions;                /**< Include guest sessions flag */
    char *sort_order;                           /**< Sort order for results */
    bool has_more_results;                      /**< Pagination flag */
    char *next_cursor;                          /**< Next page cursor */
    size_t total_active_sessions;               /**< Total active sessions for user */
    size_t max_concurrent_sessions;             /**< Maximum allowed concurrent sessions */
    bool session_limit_reached;                 /**< Session limit reached flag */
};

/**
 * @brief Create QueryActiveSession response
 * @param query Original QueryActiveSession query
 * @param json JSON response data from GraphQL
 * @return QueryActiveSession response or NULL on error
 */
knishio_response_query_active_session_t* knishio_response_query_active_session_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free QueryActiveSession response
 * @param response QueryActiveSession response to free
 */
void knishio_response_query_active_session_free(knishio_response_query_active_session_t *response);

/**
 * @brief Get active session information
 * @param response QueryActiveSession response
 * @param count Output parameter for number of sessions
 * @return Array of session info structures or NULL if no sessions
 */
const knishio_active_session_info_t* knishio_response_query_active_session_get_sessions(
    const knishio_response_query_active_session_t *response,
    size_t *count
);

/**
 * @brief Get session info by index
 * @param response QueryActiveSession response
 * @param index Session index
 * @return Session info or NULL if index out of bounds
 */
const knishio_active_session_info_t* knishio_response_query_active_session_get_session(
    const knishio_response_query_active_session_t *response,
    size_t index
);

/**
 * @brief Find session by session ID
 * @param response QueryActiveSession response
 * @param session_id Session ID to search for
 * @return First matching session info or NULL if not found
 */
const knishio_active_session_info_t* knishio_response_query_active_session_find_session(
    const knishio_response_query_active_session_t *response,
    const char *session_id
);

/**
 * @brief Get number of active sessions
 * @param response QueryActiveSession response
 * @return Number of active sessions
 */
size_t knishio_response_query_active_session_get_count(
    const knishio_response_query_active_session_t *response
);

/**
 * @brief Get total active sessions for user
 * @param response QueryActiveSession response
 * @return Total active sessions count
 */
size_t knishio_response_query_active_session_get_total_active_sessions(
    const knishio_response_query_active_session_t *response
);

/**
 * @brief Get maximum concurrent sessions allowed
 * @param response QueryActiveSession response
 * @return Maximum concurrent sessions
 */
size_t knishio_response_query_active_session_get_max_concurrent_sessions(
    const knishio_response_query_active_session_t *response
);

/**
 * @brief Check if session limit has been reached
 * @param response QueryActiveSession response
 * @return True if session limit reached, false otherwise
 */
bool knishio_response_query_active_session_is_session_limit_reached(
    const knishio_response_query_active_session_t *response
);

/**
 * @brief Check if more results are available
 * @param response QueryActiveSession response
 * @return True if more results available, false otherwise
 */
bool knishio_response_query_active_session_has_more_results(
    const knishio_response_query_active_session_t *response
);

/**
 * @brief Get next page cursor for pagination
 * @param response QueryActiveSession response
 * @return Next cursor string or NULL if no more pages
 */
const char* knishio_response_query_active_session_get_next_cursor(
    const knishio_response_query_active_session_t *response
);

/**
 * @brief Get session state as string
 * @param state Session state
 * @return State string representation
 */
const char* knishio_session_state_to_string(knishio_session_state_t state);

/**
 * @brief Parse session state from string
 * @param state_str State string
 * @return Parsed state or KNISHIO_SESSION_STATE_ACTIVE if unknown
 */
knishio_session_state_t knishio_session_state_from_string(const char *state_str);

/**
 * @brief Get session type as string
 * @param type Session type
 * @return Type string representation
 */
const char* knishio_session_type_to_string(knishio_session_type_t type);

/**
 * @brief Parse session type from string
 * @param type_str Type string
 * @return Parsed type or KNISHIO_SESSION_TYPE_REGULAR if unknown
 */
knishio_session_type_t knishio_session_type_from_string(const char *type_str);

/* Session component utilities */

/**
 * @brief Free active session info
 * @param session Session info to free
 */
void knishio_active_session_info_free(knishio_active_session_info_t *session);

/**
 * @brief Copy active session info
 * @param session Session info to copy
 * @return Copied session info or NULL on error
 */
knishio_active_session_info_t* knishio_active_session_info_copy(
    const knishio_active_session_info_t *session
);

/**
 * @brief Free session activity info
 * @param activity Activity info to free
 */
void knishio_session_activity_free(knishio_session_activity_t *activity);

/**
 * @brief Free session security info
 * @param security Security info to free
 */
void knishio_session_security_free(knishio_session_security_t *security);

/* Factory function for response creation */

/**
 * @brief Factory function for QueryActiveSession response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_query_active_session_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for active session operations */
extern const char* const KNISHIO_ACTIVE_SESSION_DEFAULT_SORT_ORDER;
extern const int KNISHIO_ACTIVE_SESSION_DEFAULT_IDLE_TIMEOUT;
extern const int KNISHIO_ACTIVE_SESSION_MAX_RISK_SCORE;
extern const size_t KNISHIO_ACTIVE_SESSION_DEFAULT_MAX_CONCURRENT;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_query_active_session_t) > sizeof(knishio_response_t),
    "QueryActiveSession response must be larger than base response");
_Static_assert(sizeof(knishio_session_state_t) == sizeof(int),
    "Session state must be integer-sized for ABI compatibility");
_Static_assert(sizeof(knishio_session_type_t) == sizeof(int),
    "Session type must be integer-sized for ABI compatibility");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_QUERY_ACTIVE_SESSION_H */