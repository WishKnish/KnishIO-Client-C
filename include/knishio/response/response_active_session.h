#ifndef KNISHIO_RESPONSE_ACTIVE_SESSION_H
#define KNISHIO_RESPONSE_ACTIVE_SESSION_H

/**
 * @file response_active_session.h
 * @brief Response for active session queries
 * 
 * Handles responses from active session queries, extracting session information
 * compatible with the JavaScript SDK's ResponseActiveSession.
 */

#include "response.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_active_session knishio_response_active_session_t;

/**
 * @brief Active session information
 */
typedef struct {
    char *session_id;                   /**< Session identifier */
    char *bundle_hash;                  /**< Associated bundle hash */
    char *ip_address;                   /**< Client IP address */
    char *user_agent;                   /**< Client user agent */
    time_t created_at;                  /**< Session creation time */
    time_t updated_at;                  /**< Session last update time */
    time_t expires_at;                  /**< Session expiration time */
    bool is_active;                     /**< Whether session is active */
    char *meta_type;                    /**< Session meta type */
    char *meta_id;                      /**< Session meta ID */
} knishio_session_info_t;

/**
 * @brief Active session response structure
 */
struct knishio_response_active_session {
    knishio_response_t base;            /**< Base response */
    knishio_session_info_t *session;    /**< Session information */
    bool has_active_session;            /**< Whether active session exists */
};

/**
 * @brief Create active session response
 * @param query Original query
 * @param json JSON response data
 * @return Active session response or NULL on error
 */
knishio_response_active_session_t* knishio_response_active_session_create(knishio_query_t *query,
                                                                          knishio_json_t *json);

/**
 * @brief Free active session response
 * @param response Active session response to free
 */
void knishio_response_active_session_free(knishio_response_active_session_t *response);

/**
 * @brief Get session information
 * @param response Active session response
 * @return Session info or NULL if no active session
 */
knishio_session_info_t* knishio_response_active_session_get_session(knishio_response_active_session_t *response);

/**
 * @brief Check if session is active
 * @param response Active session response
 * @return True if has active session, false otherwise
 */
bool knishio_response_active_session_has_active_session(knishio_response_active_session_t *response);

/**
 * @brief Get session ID
 * @param response Active session response
 * @return Session ID or NULL if no active session
 */
const char* knishio_response_active_session_get_session_id(knishio_response_active_session_t *response);

/**
 * @brief Get bundle hash
 * @param response Active session response
 * @return Bundle hash or NULL if not available
 */
const char* knishio_response_active_session_get_bundle_hash(knishio_response_active_session_t *response);

/**
 * @brief Get client IP address
 * @param response Active session response
 * @return IP address or NULL if not available
 */
const char* knishio_response_active_session_get_ip_address(knishio_response_active_session_t *response);

/**
 * @brief Get client user agent
 * @param response Active session response
 * @return User agent or NULL if not available
 */
const char* knishio_response_active_session_get_user_agent(knishio_response_active_session_t *response);

/**
 * @brief Get session creation time
 * @param response Active session response
 * @return Creation timestamp or 0 if not available
 */
time_t knishio_response_active_session_get_created_at(knishio_response_active_session_t *response);

/**
 * @brief Get session expiration time
 * @param response Active session response
 * @return Expiration timestamp or 0 if not available
 */
time_t knishio_response_active_session_get_expires_at(knishio_response_active_session_t *response);

/**
 * @brief Check if session is expired
 * @param response Active session response
 * @return True if session is expired, false otherwise
 */
bool knishio_response_active_session_is_expired(knishio_response_active_session_t *response);

/**
 * @brief Get session meta type
 * @param response Active session response
 * @return Meta type or NULL if not available
 */
const char* knishio_response_active_session_get_meta_type(knishio_response_active_session_t *response);

/**
 * @brief Get session meta ID
 * @param response Active session response
 * @return Meta ID or NULL if not available
 */
const char* knishio_response_active_session_get_meta_id(knishio_response_active_session_t *response);

/**
 * @brief Check if response has valid session data
 * @param response Active session response
 * @return True if has valid session data, false otherwise
 */
bool knishio_response_active_session_has_data(knishio_response_active_session_t *response);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param active_session_response Active session response
 * @return Base response
 */
knishio_response_t* knishio_response_active_session_to_base(knishio_response_active_session_t *active_session_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Active session response or NULL if not an active session response
 */
knishio_response_active_session_t* knishio_response_active_session_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_ACTIVE_SESSION_H */