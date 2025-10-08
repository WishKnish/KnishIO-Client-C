#ifndef KNISHIO_RESPONSE_CREATE_RULE_H
#define KNISHIO_RESPONSE_CREATE_RULE_H

/**
 * @file response_create_rule.h
 * @brief Response for CreateRule mutation operations
 * 
 * Handles responses from CreateRule mutations that create policy rules
 * for governance and access control following 2025 C17 best practices.
 */

#include "response.h"
#include "../molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_create_rule knishio_response_create_rule_t;

/**
 * @brief Rule type enumeration
 */
typedef enum {
    KNISHIO_RULE_TYPE_ACCESS = 0,               /**< Access control rule */
    KNISHIO_RULE_TYPE_TRANSFER = 1,             /**< Token transfer rule */
    KNISHIO_RULE_TYPE_CREATION = 2,             /**< Asset creation rule */
    KNISHIO_RULE_TYPE_METADATA = 3,             /**< Metadata validation rule */
    KNISHIO_RULE_TYPE_IDENTIFIER = 4,           /**< Identifier validation rule */
    KNISHIO_RULE_TYPE_AUTHORIZATION = 5,        /**< Authorization rule */
    KNISHIO_RULE_TYPE_GOVERNANCE = 6,           /**< Governance rule */
    KNISHIO_RULE_TYPE_CUSTOM = 99               /**< Custom rule type */
} knishio_rule_type_t;

/**
 * @brief Rule enforcement level enumeration
 */
typedef enum {
    KNISHIO_RULE_ENFORCEMENT_ADVISORY = 0,      /**< Advisory only (warning) */
    KNISHIO_RULE_ENFORCEMENT_BLOCKING = 1,      /**< Blocks non-compliant operations */
    KNISHIO_RULE_ENFORCEMENT_QUARANTINE = 2,    /**< Quarantines violating transactions */
    KNISHIO_RULE_ENFORCEMENT_IMMEDIATE = 3      /**< Immediate enforcement and rejection */
} knishio_rule_enforcement_t;

/**
 * @brief Rule condition structure
 */
typedef struct knishio_rule_condition {
    char *field;                                /**< Field to evaluate */
    char *operator;                             /**< Comparison operator */
    char *value;                                /**< Expected value */
    char *data_type;                            /**< Data type hint */
    bool is_required;                           /**< Required condition flag */
} knishio_rule_condition_t;

/**
 * @brief Rule action structure
 */
typedef struct knishio_rule_action {
    char *action_type;                          /**< Type of action */
    char *parameters;                           /**< Action parameters (JSON) */
    int priority;                               /**< Action priority */
    bool is_blocking;                           /**< Blocking action flag */
} knishio_rule_action_t;

/**
 * @brief CreateRule response structure
 * 
 * Represents the response from a CreateRule GraphQL mutation operation
 * that creates new policy rules for system governance.
 */
struct knishio_response_create_rule {
    knishio_response_t base;                    /**< Base response */
    knishio_molecule_t *molecule;               /**< Created rule molecule */
    char *molecular_hash;                       /**< Hash of rule molecule */
    char *rule_id;                              /**< Unique rule identifier */
    char *rule_name;                            /**< Human-readable rule name */
    char *rule_description;                     /**< Rule description */
    knishio_rule_type_t rule_type;              /**< Type of rule */
    char *custom_type_name;                     /**< Custom type name if applicable */
    knishio_rule_enforcement_t enforcement;     /**< Enforcement level */
    char *scope;                                /**< Rule scope (JSON) */
    knishio_rule_condition_t *conditions;       /**< Rule conditions array */
    size_t condition_count;                     /**< Number of conditions */
    knishio_rule_action_t *actions;             /**< Rule actions array */
    size_t action_count;                        /**< Number of actions */
    char *created_by;                           /**< Rule creator bundle hash */
    char *created_at;                           /**< Rule creation timestamp */
    char *effective_from;                       /**< Rule effective start time */
    char *effective_until;                      /**< Rule effective end time */
    int priority;                               /**< Rule priority (higher = higher priority) */
    bool is_active;                             /**< Rule active status */
    bool is_system_rule;                        /**< System rule flag */
    bool requires_approval;                     /**< Approval required flag */
    char *approval_token;                       /**< Approval token if required */
    bool success;                               /**< Operation success flag */
};

/**
 * @brief Create CreateRule response
 * @param query Original CreateRule mutation query
 * @param json JSON response data from GraphQL
 * @return CreateRule response or NULL on error
 */
knishio_response_create_rule_t* knishio_response_create_rule_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free CreateRule response
 * @param response CreateRule response to free
 */
void knishio_response_create_rule_free(knishio_response_create_rule_t *response);

/**
 * @brief Get rule molecule
 * @param response CreateRule response
 * @return Rule molecule or NULL if operation failed
 */
knishio_molecule_t* knishio_response_create_rule_get_molecule(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get molecular hash of rule
 * @param response CreateRule response
 * @return Molecular hash or NULL if operation failed
 */
const char* knishio_response_create_rule_get_molecular_hash(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get rule identifier
 * @param response CreateRule response
 * @return Rule ID string or NULL if not available
 */
const char* knishio_response_create_rule_get_rule_id(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get rule name
 * @param response CreateRule response
 * @return Rule name string or NULL if not available
 */
const char* knishio_response_create_rule_get_rule_name(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get rule description
 * @param response CreateRule response
 * @return Rule description string or NULL if not provided
 */
const char* knishio_response_create_rule_get_rule_description(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get rule type
 * @param response CreateRule response
 * @return Rule type
 */
knishio_rule_type_t knishio_response_create_rule_get_rule_type(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get enforcement level
 * @param response CreateRule response
 * @return Enforcement level
 */
knishio_rule_enforcement_t knishio_response_create_rule_get_enforcement(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get rule scope
 * @param response CreateRule response
 * @return Scope JSON string or NULL if not specified
 */
const char* knishio_response_create_rule_get_scope(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get rule conditions
 * @param response CreateRule response
 * @param count Output parameter for number of conditions
 * @return Array of rule conditions or NULL if no conditions
 */
const knishio_rule_condition_t* knishio_response_create_rule_get_conditions(
    const knishio_response_create_rule_t *response,
    size_t *count
);

/**
 * @brief Get rule actions
 * @param response CreateRule response
 * @param count Output parameter for number of actions
 * @return Array of rule actions or NULL if no actions
 */
const knishio_rule_action_t* knishio_response_create_rule_get_actions(
    const knishio_response_create_rule_t *response,
    size_t *count
);

/**
 * @brief Get rule priority
 * @param response CreateRule response
 * @return Rule priority value
 */
int knishio_response_create_rule_get_priority(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Check if rule is active
 * @param response CreateRule response
 * @return True if rule is active, false otherwise
 */
bool knishio_response_create_rule_is_active(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Check if approval is required
 * @param response CreateRule response
 * @return True if approval required, false otherwise
 */
bool knishio_response_create_rule_requires_approval(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get approval token
 * @param response CreateRule response
 * @return Approval token or NULL if no approval needed
 */
const char* knishio_response_create_rule_get_approval_token(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Check if rule creation was successful
 * @param response CreateRule response
 * @return True if successful, false otherwise
 */
bool knishio_response_create_rule_is_successful(
    const knishio_response_create_rule_t *response
);

/**
 * @brief Get rule type as string
 * @param type Rule type
 * @return Type string representation
 */
const char* knishio_rule_type_to_string(knishio_rule_type_t type);

/**
 * @brief Parse rule type from string
 * @param type_str Type string
 * @return Parsed type or KNISHIO_RULE_TYPE_CUSTOM if unknown
 */
knishio_rule_type_t knishio_rule_type_from_string(const char *type_str);

/**
 * @brief Get enforcement level as string
 * @param enforcement Enforcement level
 * @return Enforcement string representation
 */
const char* knishio_rule_enforcement_to_string(knishio_rule_enforcement_t enforcement);

/**
 * @brief Parse enforcement level from string
 * @param enforcement_str Enforcement string
 * @return Parsed enforcement or KNISHIO_RULE_ENFORCEMENT_ADVISORY if unknown
 */
knishio_rule_enforcement_t knishio_rule_enforcement_from_string(const char *enforcement_str);

/* Rule component utilities */

/**
 * @brief Free rule condition
 * @param condition Condition to free
 */
void knishio_rule_condition_free(knishio_rule_condition_t *condition);

/**
 * @brief Free rule action
 * @param action Action to free
 */
void knishio_rule_action_free(knishio_rule_action_t *action);

/**
 * @brief Copy rule condition
 * @param condition Condition to copy
 * @return Copied condition or NULL on error
 */
knishio_rule_condition_t* knishio_rule_condition_copy(
    const knishio_rule_condition_t *condition
);

/**
 * @brief Copy rule action
 * @param action Action to copy
 * @return Copied action or NULL on error
 */
knishio_rule_action_t* knishio_rule_action_copy(const knishio_rule_action_t *action);

/* Factory function for response creation */

/**
 * @brief Factory function for CreateRule response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_create_rule_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for rule operations */
extern const char* const KNISHIO_CREATE_RULE_SUCCESS_MESSAGE;
extern const char* const KNISHIO_RULE_DEFAULT_SCOPE;
extern const int KNISHIO_RULE_DEFAULT_PRIORITY;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_create_rule_t) > sizeof(knishio_response_t),
    "CreateRule response must be larger than base response");
_Static_assert(sizeof(knishio_rule_type_t) == sizeof(int),
    "Rule type must be integer-sized for ABI compatibility");
_Static_assert(sizeof(knishio_rule_enforcement_t) == sizeof(int),
    "Rule enforcement must be integer-sized for ABI compatibility");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_CREATE_RULE_H */