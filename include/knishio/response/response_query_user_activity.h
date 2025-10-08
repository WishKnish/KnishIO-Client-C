#ifndef KNISHIO_RESPONSE_QUERY_USER_ACTIVITY_H
#define KNISHIO_RESPONSE_QUERY_USER_ACTIVITY_H

/**
 * @file response_query_user_activity.h
 * @brief Response for QueryUserActivity query operations
 * 
 * Handles responses from QueryUserActivity queries that retrieve user
 * activity logs and transaction history following 2025 C17 best practices.
 */

#include "response.h"
#include "../molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_query_user_activity knishio_response_query_user_activity_t;

/**
 * @brief User activity type enumeration
 */
typedef enum {
    KNISHIO_ACTIVITY_LOGIN = 0,                 /**< User login activity */
    KNISHIO_ACTIVITY_LOGOUT = 1,                /**< User logout activity */
    KNISHIO_ACTIVITY_TRANSACTION = 2,           /**< Transaction activity */
    KNISHIO_ACTIVITY_TRANSFER = 3,              /**< Token transfer activity */
    KNISHIO_ACTIVITY_CREATE_WALLET = 4,         /**< Wallet creation activity */
    KNISHIO_ACTIVITY_CREATE_TOKEN = 5,          /**< Token creation activity */
    KNISHIO_ACTIVITY_CREATE_META = 6,           /**< Metadata creation activity */
    KNISHIO_ACTIVITY_IDENTIFIER_OPERATION = 7,  /**< Identifier operations */
    KNISHIO_ACTIVITY_AUTHORIZATION = 8,         /**< Authorization events */
    KNISHIO_ACTIVITY_POLICY_OPERATION = 9,      /**< Policy-related operations */
    KNISHIO_ACTIVITY_SESSION_MANAGEMENT = 10,   /**< Session management */
    KNISHIO_ACTIVITY_SYSTEM_EVENT = 11,         /**< System-generated events */
    KNISHIO_ACTIVITY_ERROR = 12,                /**< Error events */
    KNISHIO_ACTIVITY_CUSTOM = 99                /**< Custom activity type */
} knishio_activity_type_t;

/**
 * @brief Activity result status enumeration
 */
typedef enum {
    KNISHIO_ACTIVITY_SUCCESS = 0,               /**< Activity completed successfully */
    KNISHIO_ACTIVITY_FAILED = 1,                /**< Activity failed */
    KNISHIO_ACTIVITY_PENDING = 2,               /**< Activity still pending */
    KNISHIO_ACTIVITY_CANCELLED = 3,             /**< Activity was cancelled */
    KNISHIO_ACTIVITY_TIMEOUT = 4,               /**< Activity timed out */
    KNISHIO_ACTIVITY_REJECTED = 5               /**< Activity was rejected */
} knishio_activity_result_t;

/**
 * @brief User activity entry structure
 */
typedef struct knishio_user_activity_entry {
    char *activity_id;                          /**< Unique activity identifier */
    knishio_activity_type_t activity_type;      /**< Type of activity */
    char *custom_type_name;                     /**< Custom type name if applicable */
    char *description;                          /**< Activity description */
    char *bundle_hash;                          /**< Associated bundle hash */
    char *wallet_address;                       /**< Associated wallet address */
    char *molecular_hash;                       /**< Associated molecular hash */
    char *session_id;                           /**< Session identifier */
    knishio_activity_result_t result;           /**< Activity result status */
    char *error_message;                        /**< Error message if failed */
    char *timestamp;                            /**< Activity timestamp */
    char *ip_address;                           /**< Client IP address */
    char *user_agent;                           /**< Client user agent */
    char *location;                             /**< Geographic location (if available) */
    char *device_fingerprint;                   /**< Device fingerprint */
    char *metadata;                             /**< Additional metadata (JSON) */
    double amount;                              /**< Associated amount (if applicable) */
    char *token_slug;                           /**< Associated token (if applicable) */
    char *target_address;                       /**< Target address (if applicable) */
    bool is_automated;                          /**< Automated activity flag */
    bool is_suspicious;                         /**< Suspicious activity flag */
    int risk_score;                             /**< Activity risk score (0-100) */
} knishio_user_activity_entry_t;

/**
 * @brief Activity summary statistics
 */
typedef struct knishio_activity_summary {
    size_t total_activities;                    /**< Total activity count */
    size_t successful_activities;               /**< Successful activity count */
    size_t failed_activities;                   /**< Failed activity count */
    size_t suspicious_activities;               /**< Suspicious activity count */
    char *first_activity_at;                    /**< First activity timestamp */
    char *last_activity_at;                     /**< Last activity timestamp */
    double total_volume;                        /**< Total transaction volume */
    int unique_sessions;                        /**< Number of unique sessions */
    int unique_ip_addresses;                    /**< Number of unique IP addresses */
    double average_risk_score;                  /**< Average risk score */
} knishio_activity_summary_t;

/**
 * @brief QueryUserActivity response structure
 * 
 * Represents the response from a QueryUserActivity GraphQL query operation
 * that retrieves comprehensive user activity information.
 */
struct knishio_response_query_user_activity {
    knishio_response_t base;                    /**< Base response */
    char *user_bundle_hash;                     /**< Queried user bundle hash */
    char *query_period_start;                   /**< Query period start timestamp */
    char *query_period_end;                     /**< Query period end timestamp */
    knishio_user_activity_entry_t *activities;  /**< Activity entries array */
    size_t activity_count;                      /**< Number of activities */
    size_t activity_capacity;                   /**< Current array capacity */
    knishio_activity_summary_t summary;         /**< Activity summary statistics */
    char **activity_types_filter;               /**< Applied activity type filters */
    size_t filter_count;                        /**< Number of applied filters */
    bool has_more_results;                      /**< Pagination flag */
    char *next_cursor;                          /**< Next page cursor */
    size_t page_size;                           /**< Current page size */
    char *sort_order;                           /**< Sort order applied */
    bool include_system_events;                 /**< System events inclusion flag */
    bool include_metadata;                      /**< Metadata inclusion flag */
};

/**
 * @brief Create QueryUserActivity response
 * @param query Original QueryUserActivity query
 * @param json JSON response data from GraphQL
 * @return QueryUserActivity response or NULL on error
 */
knishio_response_query_user_activity_t* knishio_response_query_user_activity_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free QueryUserActivity response
 * @param response QueryUserActivity response to free
 */
void knishio_response_query_user_activity_free(knishio_response_query_user_activity_t *response);

/**
 * @brief Get user bundle hash
 * @param response QueryUserActivity response
 * @return User bundle hash or NULL if not available
 */
const char* knishio_response_query_user_activity_get_user_bundle_hash(
    const knishio_response_query_user_activity_t *response
);

/**
 * @brief Get user activity entries
 * @param response QueryUserActivity response
 * @param count Output parameter for number of activities
 * @return Array of activity entries or NULL if no activities
 */
const knishio_user_activity_entry_t* knishio_response_query_user_activity_get_activities(
    const knishio_response_query_user_activity_t *response,
    size_t *count
);

/**
 * @brief Get activity entry by index
 * @param response QueryUserActivity response
 * @param index Activity index
 * @return Activity entry or NULL if index out of bounds
 */
const knishio_user_activity_entry_t* knishio_response_query_user_activity_get_activity(
    const knishio_response_query_user_activity_t *response,
    size_t index
);

/**
 * @brief Find activity by ID
 * @param response QueryUserActivity response
 * @param activity_id Activity ID to search for
 * @return First matching activity entry or NULL if not found
 */
const knishio_user_activity_entry_t* knishio_response_query_user_activity_find_activity(
    const knishio_response_query_user_activity_t *response,
    const char *activity_id
);

/**
 * @brief Get activity summary statistics
 * @param response QueryUserActivity response
 * @return Activity summary structure
 */
const knishio_activity_summary_t* knishio_response_query_user_activity_get_summary(
    const knishio_response_query_user_activity_t *response
);

/**
 * @brief Check if more results are available
 * @param response QueryUserActivity response
 * @return True if more results available, false otherwise
 */
bool knishio_response_query_user_activity_has_more_results(
    const knishio_response_query_user_activity_t *response
);

/**
 * @brief Get next page cursor for pagination
 * @param response QueryUserActivity response
 * @return Next cursor string or NULL if no more pages
 */
const char* knishio_response_query_user_activity_get_next_cursor(
    const knishio_response_query_user_activity_t *response
);

/**
 * @brief Get number of activities in response
 * @param response QueryUserActivity response
 * @return Number of activity entries
 */
size_t knishio_response_query_user_activity_get_count(
    const knishio_response_query_user_activity_t *response
);

/**
 * @brief Get activity type as string
 * @param type Activity type
 * @return Type string representation
 */
const char* knishio_activity_type_to_string(knishio_activity_type_t type);

/**
 * @brief Parse activity type from string
 * @param type_str Type string
 * @return Parsed type or KNISHIO_ACTIVITY_CUSTOM if unknown
 */
knishio_activity_type_t knishio_activity_type_from_string(const char *type_str);

/**
 * @brief Get activity result as string
 * @param result Activity result
 * @return Result string representation
 */
const char* knishio_activity_result_to_string(knishio_activity_result_t result);

/**
 * @brief Parse activity result from string
 * @param result_str Result string
 * @return Parsed result or KNISHIO_ACTIVITY_SUCCESS if unknown
 */
knishio_activity_result_t knishio_activity_result_from_string(const char *result_str);

/* Activity entry utilities */

/**
 * @brief Free user activity entry
 * @param entry Activity entry to free
 */
void knishio_user_activity_entry_free(knishio_user_activity_entry_t *entry);

/**
 * @brief Copy user activity entry
 * @param entry Activity entry to copy
 * @return Copied entry or NULL on error
 */
knishio_user_activity_entry_t* knishio_user_activity_entry_copy(
    const knishio_user_activity_entry_t *entry
);

/**
 * @brief Free activity summary
 * @param summary Activity summary to free
 */
void knishio_activity_summary_free(knishio_activity_summary_t *summary);

/* Factory function for response creation */

/**
 * @brief Factory function for QueryUserActivity response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_query_user_activity_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for user activity operations */
extern const char* const KNISHIO_USER_ACTIVITY_DEFAULT_SORT_ORDER;
extern const size_t KNISHIO_USER_ACTIVITY_DEFAULT_PAGE_SIZE;
extern const int KNISHIO_USER_ACTIVITY_MAX_RISK_SCORE;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_query_user_activity_t) > sizeof(knishio_response_t),
    "QueryUserActivity response must be larger than base response");
_Static_assert(sizeof(knishio_activity_type_t) == sizeof(int),
    "Activity type must be integer-sized for ABI compatibility");
_Static_assert(sizeof(knishio_activity_result_t) == sizeof(int),
    "Activity result must be integer-sized for ABI compatibility");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_QUERY_USER_ACTIVITY_H */