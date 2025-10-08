#ifndef KNISHIO_POLICY_RULE_H
#define KNISHIO_POLICY_RULE_H

/**
 * @file policy/rule.h
 * @brief Rule system structures and operations for KnishIO policy engine
 * 
 * Provides rule definition, condition evaluation, and callback execution
 * matching JavaScript SDK Rule, Condition, and Callback classes.
 */

#include "knishio/error/context.h"
#include <stdbool.h>
#include <stdint.h>
#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_rule knishio_rule_t;
typedef struct knishio_condition knishio_condition_t;
typedef struct knishio_callback knishio_callback_t;
typedef struct knishio_rule_meta knishio_rule_meta_t;

/**
 * @brief Condition comparison operators
 */
typedef enum {
    KNISHIO_CONDITION_EQUAL,          /**< == equality */
    KNISHIO_CONDITION_NOT_EQUAL,      /**< != inequality */
    KNISHIO_CONDITION_LESS_THAN,      /**< < less than */
    KNISHIO_CONDITION_LESS_EQUAL,     /**< <= less than or equal */
    KNISHIO_CONDITION_GREATER_THAN,   /**< > greater than */
    KNISHIO_CONDITION_GREATER_EQUAL,  /**< >= greater than or equal */
    KNISHIO_CONDITION_CONTAINS,       /**< string contains */
    KNISHIO_CONDITION_REGEX,          /**< regular expression */
    KNISHIO_CONDITION_IN,             /**< value in array */
    KNISHIO_CONDITION_NOT_IN          /**< value not in array */
} knishio_condition_operator_t;

/**
 * @brief Callback action types
 */
typedef enum {
    KNISHIO_CALLBACK_REJECT,          /**< Reject transaction */
    KNISHIO_CALLBACK_META,            /**< Create meta atom */
    KNISHIO_CALLBACK_COLLECT,         /**< Collect tokens */
    KNISHIO_CALLBACK_BUFFER,          /**< Buffer tokens */
    KNISHIO_CALLBACK_REMIT,           /**< Remit tokens */
    KNISHIO_CALLBACK_BURN             /**< Burn tokens */
} knishio_callback_action_t;

/**
 * @brief Rule condition structure
 * Equivalent to JavaScript Condition class
 */
struct knishio_condition {
    char* key;                        /**< Key to evaluate */
    char* value;                      /**< Value to compare against */
    knishio_condition_operator_t comparison; /**< Comparison operator */
};

/**
 * @brief Rule meta structure for callback meta actions
 * Equivalent to JavaScript Meta class
 */
struct knishio_rule_meta {
    cJSON* data;                      /**< Meta data as JSON */
};

/**
 * @brief Rule callback structure
 * Equivalent to JavaScript Callback class
 */
struct knishio_callback {
    knishio_callback_action_t action; /**< Action type */
    char* meta_type;                  /**< Meta type (optional) */
    char* meta_id;                    /**< Meta ID (optional) */
    knishio_rule_meta_t* meta;        /**< Meta data (optional) */
    char* address;                    /**< Target address (optional) */
    char* token;                      /**< Token type (optional) */
    char* amount;                     /**< Token amount (optional) */
    char* comparison;                 /**< Comparison value (optional) */
};

/**
 * @brief Rule structure
 * Equivalent to JavaScript Rule class
 */
struct knishio_rule {
    knishio_condition_t** conditions; /**< Array of conditions */
    size_t condition_count;           /**< Number of conditions */
    knishio_callback_t** callbacks;   /**< Array of callbacks */
    size_t callback_count;            /**< Number of callbacks */
};

/**
 * @brief Rule evaluation context
 */
typedef struct {
    const char* wallet_address;       /**< Current wallet address */
    const char* token;                /**< Current token */
    const char* amount;               /**< Current amount */
    const char* meta_type;            /**< Current meta type */
    const char* meta_id;              /**< Current meta ID */
    cJSON* meta_data;                 /**< Current meta data */
    cJSON* transaction_data;          /**< Full transaction data */
} knishio_rule_context_t;

/**
 * @brief Rule evaluation result
 */
typedef struct {
    bool passed;                      /**< Rule evaluation result */
    char* error_message;              /**< Error message if failed */
    knishio_callback_t** triggered_callbacks; /**< Triggered callbacks */
    size_t callback_count;            /**< Number of triggered callbacks */
} knishio_rule_result_t;

/* Condition operations */

/**
 * @brief Create new condition
 * @param key Key to evaluate
 * @param value Value to compare against
 * @param comparison Comparison operator
 * @return New condition or NULL on error
 */
knishio_condition_t* knishio_condition_create(
    const char* key,
    const char* value,
    knishio_condition_operator_t comparison
);

/**
 * @brief Create condition from JSON object
 * @param json JSON object
 * @return New condition or NULL on error
 */
knishio_condition_t* knishio_condition_from_json(cJSON* json);

/**
 * @brief Evaluate condition against context
 * @param condition Condition to evaluate
 * @param context Evaluation context
 * @return true if condition passes, false otherwise
 */
bool knishio_condition_evaluate(
    const knishio_condition_t* condition,
    const knishio_rule_context_t* context
);

/**
 * @brief Convert condition to JSON
 * @param condition Condition to convert
 * @return JSON object or NULL on error
 */
cJSON* knishio_condition_to_json(const knishio_condition_t* condition);

/**
 * @brief Free condition
 * @param condition Condition to free
 */
void knishio_condition_free(knishio_condition_t* condition);

/* Rule meta operations */

/**
 * @brief Create new rule meta
 * @param data JSON data
 * @return New rule meta or NULL on error
 */
knishio_rule_meta_t* knishio_rule_meta_create(cJSON* data);

/**
 * @brief Create rule meta from JSON object
 * @param json JSON object
 * @return New rule meta or NULL on error
 */
knishio_rule_meta_t* knishio_rule_meta_from_json(cJSON* json);

/**
 * @brief Convert rule meta to JSON
 * @param meta Rule meta to convert
 * @return JSON object or NULL on error
 */
cJSON* knishio_rule_meta_to_json(const knishio_rule_meta_t* meta);

/**
 * @brief Free rule meta
 * @param meta Rule meta to free
 */
void knishio_rule_meta_free(knishio_rule_meta_t* meta);

/* Callback operations */

/**
 * @brief Create new callback
 * @param action Action type
 * @return New callback or NULL on error
 */
knishio_callback_t* knishio_callback_create(knishio_callback_action_t action);

/**
 * @brief Create callback from JSON object
 * @param json JSON object
 * @return New callback or NULL on error
 */
knishio_callback_t* knishio_callback_from_json(cJSON* json);

/**
 * @brief Check if callback is reject type
 * @param callback Callback to check
 * @return true if reject callback
 */
bool knishio_callback_is_reject(const knishio_callback_t* callback);

/**
 * @brief Check if callback is meta type
 * @param callback Callback to check
 * @return true if meta callback
 */
bool knishio_callback_is_meta(const knishio_callback_t* callback);

/**
 * @brief Check if callback is collect type
 * @param callback Callback to check
 * @return true if collect callback
 */
bool knishio_callback_is_collect(const knishio_callback_t* callback);

/**
 * @brief Check if callback is buffer type
 * @param callback Callback to check
 * @return true if buffer callback
 */
bool knishio_callback_is_buffer(const knishio_callback_t* callback);

/**
 * @brief Check if callback is remit type
 * @param callback Callback to check
 * @return true if remit callback
 */
bool knishio_callback_is_remit(const knishio_callback_t* callback);

/**
 * @brief Check if callback is burn type
 * @param callback Callback to check
 * @return true if burn callback
 */
bool knishio_callback_is_burn(const knishio_callback_t* callback);

/**
 * @brief Convert callback to JSON
 * @param callback Callback to convert
 * @return JSON object or NULL on error
 */
cJSON* knishio_callback_to_json(const knishio_callback_t* callback);

/**
 * @brief Free callback
 * @param callback Callback to free
 */
void knishio_callback_free(knishio_callback_t* callback);

/* Rule operations */

/**
 * @brief Create new rule
 * @return New rule or NULL on error
 */
knishio_rule_t* knishio_rule_create(void);

/**
 * @brief Create rule from JSON object
 * @param json JSON object
 * @return New rule or NULL on error
 */
knishio_rule_t* knishio_rule_from_json(cJSON* json);

/**
 * @brief Add condition to rule
 * @param rule Rule to modify
 * @param condition Condition to add
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_rule_add_condition(
    knishio_rule_t* rule,
    knishio_condition_t* condition
);

/**
 * @brief Add callback to rule
 * @param rule Rule to modify
 * @param callback Callback to add
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_rule_add_callback(
    knishio_rule_t* rule,
    knishio_callback_t* callback
);

/**
 * @brief Evaluate rule against context
 * @param rule Rule to evaluate
 * @param context Evaluation context
 * @param result Output evaluation result (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_rule_evaluate(
    const knishio_rule_t* rule,
    const knishio_rule_context_t* context,
    knishio_rule_result_t** result
);

/**
 * @brief Convert rule to JSON
 * @param rule Rule to convert
 * @return JSON object or NULL on error
 */
cJSON* knishio_rule_to_json(const knishio_rule_t* rule);

/**
 * @brief Free rule
 * @param rule Rule to free
 */
void knishio_rule_free(knishio_rule_t* rule);

/**
 * @brief Free rule evaluation result
 * @param result Result to free
 */
void knishio_rule_result_free(knishio_rule_result_t* result);

/* Utility functions */

/**
 * @brief Parse condition operator from string
 * @param operator_str Operator string
 * @return Condition operator or -1 on error
 */
knishio_condition_operator_t knishio_condition_parse_operator(const char* operator_str);

/**
 * @brief Convert condition operator to string
 * @param operator Condition operator
 * @return Operator string or NULL on error
 */
const char* knishio_condition_operator_to_string(knishio_condition_operator_t operator);

/**
 * @brief Parse callback action from string
 * @param action_str Action string
 * @return Callback action or -1 on error
 */
knishio_callback_action_t knishio_callback_parse_action(const char* action_str);

/**
 * @brief Convert callback action to string
 * @param action Callback action
 * @return Action string or NULL on error
 */
const char* knishio_callback_action_to_string(knishio_callback_action_t action);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_POLICY_RULE_H */