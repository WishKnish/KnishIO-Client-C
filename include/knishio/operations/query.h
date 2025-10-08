#ifndef KNISHIO_OPERATIONS_QUERY_H
#define KNISHIO_OPERATIONS_QUERY_H

/**
 * @file operations/query.h
 * @brief Query operations for KnishIO C SDK
 * 
 * Provides high-level query operations matching JavaScript SDK:
 * - QueryAtom for atom queries
 * - QueryActiveSession for session queries  
 * - QueryUserActivity for activity queries
 * 
 * Full alignment with JS SDK query functionality.
 */

#include "knishio/error/context.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_client knishio_client_t;
typedef struct knishio_atom knishio_atom_t;

/**
 * @brief Parameters for atom query operation
 * Matches JavaScript SDK queryAtom() parameters
 */
typedef struct {
    const char** molecular_hashes;      /**< Array of molecular hashes */
    size_t molecular_hash_count;        /**< Number of molecular hashes */
    const char* molecular_hash;         /**< Single molecular hash */
    const char** bundle_hashes;         /**< Array of bundle hashes */
    size_t bundle_hash_count;           /**< Number of bundle hashes */
    const char* bundle_hash;            /**< Single bundle hash */
    const char** positions;             /**< Array of positions */
    size_t position_count;              /**< Number of positions */
    const char* position;               /**< Single position */
    const char** wallet_addresses;      /**< Array of wallet addresses */
    size_t wallet_address_count;        /**< Number of wallet addresses */
    const char* wallet_address;         /**< Single wallet address */
    const char** isotopes;              /**< Array of isotopes */
    size_t isotope_count;               /**< Number of isotopes */
    const char* isotope;                /**< Single isotope */
    const char** token_slugs;           /**< Array of token slugs */
    size_t token_slug_count;            /**< Number of token slugs */
    const char* token_slug;             /**< Single token slug */
    const char** cell_slugs;            /**< Array of cell slugs */
    size_t cell_slug_count;             /**< Number of cell slugs */
    const char* cell_slug;              /**< Single cell slug */
    const char** batch_ids;             /**< Array of batch IDs */
    size_t batch_id_count;              /**< Number of batch IDs */
    const char* batch_id;               /**< Single batch ID */
    const char** values;                /**< Array of values */
    size_t value_count;                 /**< Number of values */
    const char* value;                  /**< Single value */
    const char** meta_types;            /**< Array of meta types */
    size_t meta_type_count;             /**< Number of meta types */
    const char* meta_type;              /**< Single meta type */
    const char** meta_ids;              /**< Array of meta IDs */
    size_t meta_id_count;               /**< Number of meta IDs */
    const char* meta_id;                /**< Single meta ID */
    const char** indexes;               /**< Array of atom indexes */
    size_t index_count;                 /**< Number of indexes */
    const char* index;                  /**< Single atom index */
    const char* filter;                 /**< Filter object (JSON string) */
    bool latest;                        /**< Latest flag */
    int limit;                          /**< Query limit (default: 15) */
    int offset;                         /**< Query offset (default: 1) */
} knishio_query_atom_params_t;

/**
 * @brief Result of atom query operation
 */
typedef struct {
    bool success;                       /**< Query success flag */
    knishio_atom_t** atoms;             /**< Array of atoms */
    size_t atom_count;                  /**< Number of atoms */
    char* response;                     /**< Full response data */
    char* error_message;                /**< Error message if failed */
} knishio_query_atom_result_t;

/**
 * @brief Parameters for active session query
 * Matches JavaScript SDK queryActiveSession() parameters
 */
typedef struct {
    const char* bundle_hash;            /**< Bundle hash */
    const char* meta_type;              /**< Meta type */
    const char* meta_id;                /**< Meta ID */
} knishio_query_active_session_params_t;

/**
 * @brief Result of active session query
 */
typedef struct {
    bool success;                       /**< Query success flag */
    char* response;                     /**< Session data (JSON) */
    char* error_message;                /**< Error message if failed */
} knishio_query_active_session_result_t;

/**
 * @brief Parameters for user activity query
 * Matches JavaScript SDK queryUserActivity() parameters
 */
typedef struct {
    const char* bundle_hash;            /**< Bundle hash */
    const char* meta_type;              /**< Meta type */
    const char* meta_id;                /**< Meta ID */
    const char* ip_address;             /**< IP address */
    const char* browser;                /**< Browser */
    const char* os_cpu;                 /**< Operating system and CPU */
    const char* resolution;             /**< Screen resolution */
    const char* time_zone;              /**< Time zone */
    const char* count_by;               /**< Count by parameter */
    const char* interval;               /**< Interval parameter */
} knishio_query_user_activity_params_t;

/**
 * @brief Result of user activity query
 */
typedef struct {
    bool success;                       /**< Query success flag */
    char* response;                     /**< Activity data (JSON) */
    char* error_message;                /**< Error message if failed */
} knishio_query_user_activity_result_t;

/**
 * @brief Query atoms by various parameters
 * Equivalent to JavaScript: client.queryAtom({ ... })
 * 
 * @param client KnishIO client instance
 * @param params Query parameters
 * @param result Output query result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_atom(
    knishio_client_t* client,
    const knishio_query_atom_params_t* params,
    knishio_query_atom_result_t** result
);

/**
 * @brief Query active sessions
 * Equivalent to JavaScript: client.queryActiveSession({ bundleHash, metaType, metaId })
 * 
 * @param client KnishIO client instance
 * @param params Query parameters
 * @param result Output query result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_active_session(
    knishio_client_t* client,
    const knishio_query_active_session_params_t* params,
    knishio_query_active_session_result_t** result
);

/**
 * @brief Query user activity
 * Equivalent to JavaScript: client.queryUserActivity({ ... })
 * 
 * @param client KnishIO client instance
 * @param params Query parameters
 * @param result Output query result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_user_activity(
    knishio_client_t* client,
    const knishio_query_user_activity_params_t* params,
    knishio_query_user_activity_result_t** result
);

/**
 * @brief Free atom query result
 * @param result Result to free
 */
void knishio_query_atom_result_free(knishio_query_atom_result_t* result);

/**
 * @brief Free active session query result
 * @param result Result to free
 */
void knishio_query_active_session_result_free(knishio_query_active_session_result_t* result);

/**
 * @brief Free user activity query result
 * @param result Result to free
 */
void knishio_query_user_activity_result_free(knishio_query_user_activity_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_OPERATIONS_QUERY_H */