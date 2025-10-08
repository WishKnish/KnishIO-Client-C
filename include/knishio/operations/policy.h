#ifndef KNISHIO_OPERATIONS_POLICY_H
#define KNISHIO_OPERATIONS_POLICY_H

/**
 * @file operations/policy.h
 * @brief Policy operations for KnishIO C SDK
 * 
 * Provides high-level policy operations matching JavaScript SDK:
 * - CreatePolicy mutation
 * - QueryPolicy query
 * - CreateRule mutation
 * - Policy enforcement integration
 * 
 * Full alignment with JS SDK policy functionality.
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
typedef struct knishio_molecule knishio_molecule_t;
typedef struct knishio_policy_result knishio_policy_result_t;

/**
 * @brief Parameters for policy creation
 * Matches JavaScript SDK createPolicy() parameters
 */
typedef struct {
    const char* meta_type;              /**< Meta type */
    const char* meta_id;                /**< Meta ID */
    const char* policy_json;            /**< Policy object as JSON string */
} knishio_create_policy_params_t;

/**
 * @brief Result of policy creation
 */
typedef struct {
    bool success;                       /**< Creation success flag */
    char* molecular_hash;               /**< Molecular hash of policy creation */
    char* response;                     /**< Full response data */
    char* error_message;                /**< Error message if failed */
} knishio_create_policy_result_t;

/**
 * @brief Parameters for policy query
 * Matches JavaScript SDK queryPolicy() parameters
 */
typedef struct {
    const char* meta_type;              /**< Meta type */
    const char* meta_id;                /**< Meta ID */
} knishio_query_policy_params_t;

/**
 * @brief Result of policy query
 */
typedef struct {
    bool success;                       /**< Query success flag */
    char* policy_data;                  /**< Policy data (JSON) */
    char* response;                     /**< Full response data */
    char* error_message;                /**< Error message if failed */
} knishio_query_policy_result_t;

/**
 * @brief Parameters for rule creation
 * Matches JavaScript SDK createRule() parameters
 */
typedef struct {
    const char* meta_type;              /**< Meta type */
    const char* meta_id;                /**< Meta ID */
    const char* rule_json;              /**< Rule object as JSON string */
    const char* policy_json;            /**< Policy object as JSON string (optional) */
} knishio_create_rule_params_t;

/**
 * @brief Result of rule creation
 */
typedef struct {
    bool success;                       /**< Creation success flag */
    char* molecular_hash;               /**< Molecular hash of rule creation */
    char* response;                     /**< Full response data */
    char* error_message;                /**< Error message if failed */
} knishio_create_rule_result_t;

/**
 * @brief Create access control policy
 * Equivalent to JavaScript: client.createPolicy({ metaType, metaId, policy })
 * 
 * @param client KnishIO client instance
 * @param params Policy parameters
 * @param result Output policy result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_create_policy(
    knishio_client_t* client,
    const knishio_create_policy_params_t* params,
    knishio_create_policy_result_t** result
);

/**
 * @brief Query existing policies
 * Equivalent to JavaScript: client.queryPolicy({ metaType, metaId })
 * 
 * @param client KnishIO client instance
 * @param params Query parameters
 * @param result Output query result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_query_policy(
    knishio_client_t* client,
    const knishio_query_policy_params_t* params,
    knishio_query_policy_result_t** result
);

/**
 * @brief Create policy rule
 * Equivalent to JavaScript: client.createRule({ metaType, metaId, rule, policy })
 * 
 * @param client KnishIO client instance
 * @param params Rule parameters
 * @param result Output rule result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_create_rule(
    knishio_client_t* client,
    const knishio_create_rule_params_t* params,
    knishio_create_rule_result_t** result
);

/**
 * @brief Enforce policies on molecule
 * 
 * @param client KnishIO client instance
 * @param molecule Molecule to evaluate
 * @param result Output policy result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_enforce_policy(
    knishio_client_t* client,
    const knishio_molecule_t* molecule,
    knishio_policy_result_t** result
);

/**
 * @brief Check meta access permissions
 * 
 * @param client KnishIO client instance
 * @param meta_type Meta type
 * @param meta_id Meta ID
 * @param wallet_address Wallet address requesting access
 * @param is_write Write access flag
 * @param result Output policy result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_check_meta_access(
    knishio_client_t* client,
    const char* meta_type,
    const char* meta_id,
    const char* wallet_address,
    bool is_write,
    knishio_policy_result_t** result
);

/**
 * @brief Load policy from query result into policy engine
 * 
 * @param client KnishIO client instance
 * @param query_result Query result containing policy data
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_client_load_policy_from_query(
    knishio_client_t* client,
    const knishio_query_policy_result_t* query_result
);

/**
 * @brief Free policy creation result
 * @param result Result to free
 */
void knishio_create_policy_result_free(knishio_create_policy_result_t* result);

/**
 * @brief Free policy query result
 * @param result Result to free
 */
void knishio_query_policy_result_free(knishio_query_policy_result_t* result);

/**
 * @brief Free rule creation result
 * @param result Result to free
 */
void knishio_create_rule_result_free(knishio_create_rule_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_OPERATIONS_POLICY_H */