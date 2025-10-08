#ifndef KNISHIO_POLICY_ENGINE_H
#define KNISHIO_POLICY_ENGINE_H

/**
 * @file policy/engine.h
 * @brief Policy enforcement engine for KnishIO C SDK
 * 
 * Provides policy enforcement, rule management, and validation
 * integration with the broader KnishIO system.
 */

#include "knishio/policy/rule.h"
#include "knishio/error/context.h"
#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_policy_engine knishio_policy_engine_t;
typedef struct knishio_policy knishio_policy_t;
typedef struct knishio_molecule knishio_molecule_t;
typedef struct knishio_atom knishio_atom_t;

/**
 * @brief Policy structure
 */
struct knishio_policy {
    char* meta_type;                  /**< Meta type this policy applies to */
    char* meta_id;                    /**< Meta ID this policy applies to */
    knishio_rule_t** rules;           /**< Array of rules */
    size_t rule_count;                /**< Number of rules */
    cJSON* read_policy;               /**< Read access policy */
    cJSON* write_policy;              /**< Write access policy */
    int64_t created_at;               /**< Creation timestamp */
};

/**
 * @brief Policy engine structure
 */
struct knishio_policy_engine {
    knishio_policy_t** policies;      /**< Array of loaded policies */
    size_t policy_count;              /**< Number of loaded policies */
    size_t policy_capacity;           /**< Policy array capacity */
    bool strict_mode;                 /**< Strict enforcement mode */
};

/**
 * @brief Policy evaluation result
 */
typedef struct knishio_policy_result {
    bool allowed;                     /**< Operation allowed flag */
    char* reason;                     /**< Reason if not allowed */
    knishio_callback_t** actions;     /**< Actions to execute */
    size_t action_count;              /**< Number of actions */
} knishio_policy_result_t;

/**
 * @brief Policy enforcement points
 */
typedef enum {
    KNISHIO_POLICY_MOLECULE_VALIDATION, /**< Molecule validation */
    KNISHIO_POLICY_ATOM_VALIDATION,     /**< Atom validation */
    KNISHIO_POLICY_META_READ,           /**< Meta read access */
    KNISHIO_POLICY_META_WRITE,          /**< Meta write access */
    KNISHIO_POLICY_TOKEN_TRANSFER,      /**< Token transfer */
    KNISHIO_POLICY_WALLET_OPERATION     /**< Wallet operation */
} knishio_policy_enforcement_point_t;

/* Policy engine operations */

/**
 * @brief Create new policy engine
 * @return New policy engine or NULL on error
 */
knishio_policy_engine_t* knishio_policy_engine_create(void);

/**
 * @brief Set strict enforcement mode
 * @param engine Policy engine
 * @param strict_mode Strict mode flag
 */
void knishio_policy_engine_set_strict_mode(
    knishio_policy_engine_t* engine,
    bool strict_mode
);

/**
 * @brief Load policy from JSON
 * @param engine Policy engine
 * @param policy_json Policy JSON data
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_engine_load_policy(
    knishio_policy_engine_t* engine,
    cJSON* policy_json
);

/**
 * @brief Load policy from rule data
 * @param engine Policy engine
 * @param meta_type Meta type
 * @param meta_id Meta ID
 * @param rules_json Rules JSON array
 * @param policy_json Policy JSON object (optional)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_engine_add_policy(
    knishio_policy_engine_t* engine,
    const char* meta_type,
    const char* meta_id,
    cJSON* rules_json,
    cJSON* policy_json
);

/**
 * @brief Find policy by meta type and ID
 * @param engine Policy engine
 * @param meta_type Meta type
 * @param meta_id Meta ID
 * @return Policy or NULL if not found
 */
knishio_policy_t* knishio_policy_engine_find_policy(
    const knishio_policy_engine_t* engine,
    const char* meta_type,
    const char* meta_id
);

/**
 * @brief Evaluate molecule against policies
 * @param engine Policy engine
 * @param molecule Molecule to evaluate
 * @param result Output evaluation result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_engine_evaluate_molecule(
    const knishio_policy_engine_t* engine,
    const knishio_molecule_t* molecule,
    knishio_policy_result_t** result
);

/**
 * @brief Evaluate atom against policies
 * @param engine Policy engine
 * @param atom Atom to evaluate
 * @param result Output evaluation result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_engine_evaluate_atom(
    const knishio_policy_engine_t* engine,
    const knishio_atom_t* atom,
    knishio_policy_result_t** result
);

/**
 * @brief Check meta access permissions
 * @param engine Policy engine
 * @param meta_type Meta type
 * @param meta_id Meta ID
 * @param wallet_address Requesting wallet address
 * @param is_write Write access flag
 * @param result Output evaluation result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_engine_check_meta_access(
    const knishio_policy_engine_t* engine,
    const char* meta_type,
    const char* meta_id,
    const char* wallet_address,
    bool is_write,
    knishio_policy_result_t** result
);

/**
 * @brief Enforce policy at specific point
 * @param engine Policy engine
 * @param enforcement_point Enforcement point
 * @param context Rule context
 * @param result Output evaluation result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_engine_enforce(
    const knishio_policy_engine_t* engine,
    knishio_policy_enforcement_point_t enforcement_point,
    const knishio_rule_context_t* context,
    knishio_policy_result_t** result
);

/**
 * @brief Clear all policies
 * @param engine Policy engine
 */
void knishio_policy_engine_clear(knishio_policy_engine_t* engine);

/**
 * @brief Free policy engine
 * @param engine Policy engine to free
 */
void knishio_policy_engine_free(knishio_policy_engine_t* engine);

/* Policy operations */

/**
 * @brief Create new policy
 * @param meta_type Meta type
 * @param meta_id Meta ID
 * @return New policy or NULL on error
 */
knishio_policy_t* knishio_policy_create(
    const char* meta_type,
    const char* meta_id
);

/**
 * @brief Create policy from JSON
 * @param json Policy JSON data
 * @return New policy or NULL on error
 */
knishio_policy_t* knishio_policy_from_json(cJSON* json);

/**
 * @brief Add rule to policy
 * @param policy Policy
 * @param rule Rule to add
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_add_rule(
    knishio_policy_t* policy,
    knishio_rule_t* rule
);

/**
 * @brief Set read access policy
 * @param policy Policy
 * @param read_policy Read policy JSON
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_set_read_policy(
    knishio_policy_t* policy,
    cJSON* read_policy
);

/**
 * @brief Set write access policy
 * @param policy Policy
 * @param write_policy Write policy JSON
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_set_write_policy(
    knishio_policy_t* policy,
    cJSON* write_policy
);

/**
 * @brief Evaluate policy against context
 * @param policy Policy to evaluate
 * @param context Evaluation context
 * @param result Output evaluation result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_evaluate(
    const knishio_policy_t* policy,
    const knishio_rule_context_t* context,
    knishio_policy_result_t** result
);

/**
 * @brief Convert policy to JSON
 * @param policy Policy to convert
 * @return JSON object or NULL on error
 */
cJSON* knishio_policy_to_json(const knishio_policy_t* policy);

/**
 * @brief Free policy
 * @param policy Policy to free
 */
void knishio_policy_free(knishio_policy_t* policy);

/**
 * @brief Free policy evaluation result
 * @param result Result to free
 */
void knishio_policy_result_free(knishio_policy_result_t* result);

/* Utility functions */

/**
 * @brief Normalize policy JSON structure
 * Equivalent to JavaScript PolicyMeta.normalizePolicy()
 * @param policy_json Policy JSON object
 * @return Normalized policy JSON or NULL on error
 */
cJSON* knishio_policy_normalize_json(cJSON* policy_json);

/**
 * @brief Fill default policy values
 * Equivalent to JavaScript PolicyMeta.fillDefault()
 * @param policy_json Policy JSON object
 * @param meta_keys Available meta keys
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_policy_fill_defaults(
    cJSON* policy_json,
    const char** meta_keys,
    size_t meta_key_count
);

/**
 * @brief Create rule context from molecule
 * @param molecule Source molecule
 * @return Rule context or NULL on error
 */
knishio_rule_context_t* knishio_rule_context_from_molecule(
    const knishio_molecule_t* molecule
);

/**
 * @brief Create rule context from atom
 * @param atom Source atom
 * @return Rule context or NULL on error
 */
knishio_rule_context_t* knishio_rule_context_from_atom(
    const knishio_atom_t* atom
);

/**
 * @brief Free rule context
 * @param context Context to free
 */
void knishio_rule_context_free(knishio_rule_context_t* context);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_POLICY_ENGINE_H */