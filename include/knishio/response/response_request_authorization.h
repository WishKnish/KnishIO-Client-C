#ifndef KNISHIO_RESPONSE_REQUEST_AUTHORIZATION_H
#define KNISHIO_RESPONSE_REQUEST_AUTHORIZATION_H

/**
 * @file response_request_authorization.h
 * @brief Response for authorization request operations
 * 
 * Handles responses from authorization request mutations, extracting authorization
 * information compatible with the JavaScript SDK's ResponseRequestAuthorization.
 */

#include "response.h"
#include "../molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_request_authorization knishio_response_request_authorization_t;

/**
 * @brief Authorization request status
 */
typedef enum {
    KNISHIO_AUTH_STATUS_UNKNOWN = 0,        /**< Unknown status */
    KNISHIO_AUTH_STATUS_PENDING,            /**< Authorization pending */
    KNISHIO_AUTH_STATUS_APPROVED,           /**< Authorization approved */
    KNISHIO_AUTH_STATUS_REJECTED,           /**< Authorization rejected */
    KNISHIO_AUTH_STATUS_EXPIRED,            /**< Authorization expired */
    KNISHIO_AUTH_STATUS_CANCELLED           /**< Authorization cancelled */
} knishio_auth_status_t;

/**
 * @brief Request authorization response structure
 */
struct knishio_response_request_authorization {
    knishio_response_t base;            /**< Base response */
    knishio_molecule_t *molecule;       /**< Authorization request molecule */
    char *auth_token;                   /**< Authorization token */
    char *auth_code;                    /**< Authorization code */
    char *auth_url;                     /**< Authorization URL */
    char *bundle_hash;                  /**< Requester bundle hash */
    char *meta_type;                    /**< Meta type for authorization */
    char *meta_id;                      /**< Meta ID for authorization */
    knishio_auth_status_t status;       /**< Authorization status */
    time_t expires_at;                  /**< Authorization expiration */
    bool requires_approval;             /**< Whether manual approval needed */
};

/**
 * @brief Create request authorization response
 * @param query Original query
 * @param json JSON response data
 * @return Request authorization response or NULL on error
 */
knishio_response_request_authorization_t* knishio_response_request_authorization_create(knishio_query_t *query,
                                                                                        knishio_json_t *json);

/**
 * @brief Free request authorization response
 * @param response Request authorization response to free
 */
void knishio_response_request_authorization_free(knishio_response_request_authorization_t *response);

/**
 * @brief Get authorization request molecule
 * @param response Request authorization response
 * @return Authorization molecule or NULL if not available
 */
knishio_molecule_t* knishio_response_request_authorization_get_molecule(knishio_response_request_authorization_t *response);

/**
 * @brief Get authorization token
 * @param response Request authorization response
 * @return Authorization token or NULL if not available
 */
const char* knishio_response_request_authorization_get_auth_token(knishio_response_request_authorization_t *response);

/**
 * @brief Get authorization code
 * @param response Request authorization response
 * @return Authorization code or NULL if not available
 */
const char* knishio_response_request_authorization_get_auth_code(knishio_response_request_authorization_t *response);

/**
 * @brief Get authorization URL
 * @param response Request authorization response
 * @return Authorization URL or NULL if not available
 */
const char* knishio_response_request_authorization_get_auth_url(knishio_response_request_authorization_t *response);

/**
 * @brief Get requester bundle hash
 * @param response Request authorization response
 * @return Bundle hash or NULL if not available
 */
const char* knishio_response_request_authorization_get_bundle_hash(knishio_response_request_authorization_t *response);

/**
 * @brief Get meta type
 * @param response Request authorization response
 * @return Meta type or NULL if not available
 */
const char* knishio_response_request_authorization_get_meta_type(knishio_response_request_authorization_t *response);

/**
 * @brief Get meta ID
 * @param response Request authorization response
 * @return Meta ID or NULL if not available
 */
const char* knishio_response_request_authorization_get_meta_id(knishio_response_request_authorization_t *response);

/**
 * @brief Get authorization status
 * @param response Request authorization response
 * @return Authorization status
 */
knishio_auth_status_t knishio_response_request_authorization_get_status(knishio_response_request_authorization_t *response);

/**
 * @brief Get authorization expiration time
 * @param response Request authorization response
 * @return Expiration timestamp or 0 if not available
 */
time_t knishio_response_request_authorization_get_expires_at(knishio_response_request_authorization_t *response);

/**
 * @brief Check if manual approval is required
 * @param response Request authorization response
 * @return True if manual approval required, false otherwise
 */
bool knishio_response_request_authorization_requires_approval(knishio_response_request_authorization_t *response);

/**
 * @brief Check if authorization is expired
 * @param response Request authorization response
 * @return True if authorization expired, false otherwise
 */
bool knishio_response_request_authorization_is_expired(knishio_response_request_authorization_t *response);

/**
 * @brief Check if authorization is approved
 * @param response Request authorization response
 * @return True if authorization approved, false otherwise
 */
bool knishio_response_request_authorization_is_approved(knishio_response_request_authorization_t *response);

/**
 * @brief Get molecular hash of authorization request
 * @param response Request authorization response
 * @return Molecular hash or NULL if not available
 */
const char* knishio_response_request_authorization_get_molecular_hash(knishio_response_request_authorization_t *response);

/**
 * @brief Check if response has valid authorization data
 * @param response Request authorization response
 * @return True if has valid authorization data, false otherwise
 */
bool knishio_response_request_authorization_has_data(knishio_response_request_authorization_t *response);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param auth_response Request authorization response
 * @return Base response
 */
knishio_response_t* knishio_response_request_authorization_to_base(knishio_response_request_authorization_t *auth_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Request authorization response or NULL if not a request authorization response
 */
knishio_response_request_authorization_t* knishio_response_request_authorization_from_base(knishio_response_t *base_response);

/* Utility functions */

/**
 * @brief Convert authorization status to string
 * @param status Authorization status
 * @return Status string
 */
const char* knishio_auth_status_to_string(knishio_auth_status_t status);

/**
 * @brief Parse authorization status from string
 * @param status_str Status string
 * @return Authorization status or KNISHIO_AUTH_STATUS_UNKNOWN if invalid
 */
knishio_auth_status_t knishio_auth_status_from_string(const char *status_str);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_REQUEST_AUTHORIZATION_H */