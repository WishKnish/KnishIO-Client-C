#ifndef KNISHIO_RESPONSE_POLICY_H
#define KNISHIO_RESPONSE_POLICY_H

/**
 * @file response_policy.h
 * @brief Response for Policy query operations
 * 
 * Handles responses from Policy queries that retrieve policy information
 * and rule sets for governance operations following 2025 C17 best practices.
 */

#include "response.h"
#include "response_create_rule.h"  // For rule structures

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_policy knishio_response_policy_t;

/**
 * @brief Policy status enumeration
 */
typedef enum {
    KNISHIO_POLICY_STATUS_DRAFT = 0,            /**< Policy in draft state */
    KNISHIO_POLICY_STATUS_ACTIVE = 1,           /**< Policy active and enforced */
    KNISHIO_POLICY_STATUS_SUSPENDED = 2,        /**< Policy temporarily suspended */
    KNISHIO_POLICY_STATUS_ARCHIVED = 3,         /**< Policy archived/inactive */
    KNISHIO_POLICY_STATUS_DEPRECATED = 4        /**< Policy deprecated */
} knishio_policy_status_t;

/**
 * @brief Policy governance model enumeration
 */
typedef enum {
    KNISHIO_GOVERNANCE_CENTRALIZED = 0,         /**< Centralized governance */
    KNISHIO_GOVERNANCE_DECENTRALIZED = 1,       /**< Decentralized governance */
    KNISHIO_GOVERNANCE_FEDERATED = 2,           /**< Federated governance */
    KNISHIO_GOVERNANCE_HYBRID = 3               /**< Hybrid governance model */
} knishio_governance_model_t;

/**
 * @brief Policy rule reference structure
 */
typedef struct knishio_policy_rule_ref {
    char *rule_id;                              /**< Rule identifier */
    char *rule_name;                            /**< Rule name */
    knishio_rule_type_t rule_type;              /**< Rule type */
    knishio_rule_enforcement_t enforcement;     /**< Enforcement level */
    int priority;                               /**< Rule priority within policy */
    bool is_active;                             /**< Rule active status */
    char *added_at;                             /**< When rule was added to policy */
    char *modified_at;                          /**< When rule was last modified */
} knishio_policy_rule_ref_t;

/**
 * @brief Policy governance settings
 */
typedef struct knishio_policy_governance {
    knishio_governance_model_t model;           /**< Governance model */
    char **approvers;                           /**< List of approver bundle hashes */
    size_t approver_count;                      /**< Number of approvers */
    int required_approvals;                     /**< Required approvals for changes */
    bool requires_consensus;                    /**< Consensus requirement flag */
    char *voting_period;                        /**< Voting period for changes */
    char *quorum_threshold;                     /**< Quorum threshold percentage */
} knishio_policy_governance_t;

/**
 * @brief Policy metrics and statistics
 */
typedef struct knishio_policy_metrics {
    int total_rules;                            /**< Total rules in policy */
    int active_rules;                           /**< Currently active rules */
    int suspended_rules;                        /**< Temporarily suspended rules */
    int violations_detected;                    /**< Policy violations detected */
    int enforcement_actions;                    /**< Enforcement actions taken */
    char *last_violation_at;                    /**< Last violation timestamp */
    char *last_enforcement_at;                  /**< Last enforcement timestamp */
    double compliance_score;                    /**< Compliance score (0.0-1.0) */
} knishio_policy_metrics_t;

/**
 * @brief Policy response structure
 * 
 * Represents the response from a Policy GraphQL query operation
 * that retrieves comprehensive policy information and associated rules.
 */
struct knishio_response_policy {
    knishio_response_t base;                    /**< Base response */
    char *policy_id;                            /**< Unique policy identifier */
    char *policy_name;                          /**< Human-readable policy name */
    char *policy_description;                   /**< Policy description */
    char *version;                              /**< Policy version */
    knishio_policy_status_t status;             /**< Current policy status */
    char *scope;                                /**< Policy scope (JSON) */
    knishio_policy_rule_ref_t *rules;           /**< Associated rule references */
    size_t rule_count;                          /**< Number of rules */
    size_t rule_capacity;                       /**< Rule array capacity */
    knishio_policy_governance_t governance;     /**< Governance settings */
    knishio_policy_metrics_t metrics;           /**< Policy metrics */
    char *created_by;                           /**< Policy creator bundle hash */
    char *created_at;                           /**< Policy creation timestamp */
    char *updated_at;                           /**< Last update timestamp */
    char *effective_from;                       /**< Policy effective start time */
    char *effective_until;                      /**< Policy effective end time */
    char *approval_hash;                        /**< Approval transaction hash */
    char **tags;                                /**< Policy tags array */
    size_t tag_count;                           /**< Number of tags */
    char *metadata;                             /**< Additional metadata (JSON) */
    bool is_system_policy;                      /**< System policy flag */
    bool is_inheritable;                        /**< Can be inherited by sub-policies */
    bool requires_approval_for_changes;         /**< Change approval requirement */
};

/**
 * @brief Create Policy response
 * @param query Original Policy query
 * @param json JSON response data from GraphQL
 * @return Policy response or NULL on error
 */
knishio_response_policy_t* knishio_response_policy_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free Policy response
 * @param response Policy response to free
 */
void knishio_response_policy_free(knishio_response_policy_t *response);

/**
 * @brief Get policy identifier
 * @param response Policy response
 * @return Policy ID string or NULL if not available
 */
const char* knishio_response_policy_get_policy_id(
    const knishio_response_policy_t *response
);

/**
 * @brief Get policy name
 * @param response Policy response
 * @return Policy name string or NULL if not available
 */
const char* knishio_response_policy_get_policy_name(
    const knishio_response_policy_t *response
);

/**
 * @brief Get policy status
 * @param response Policy response
 * @return Current policy status
 */
knishio_policy_status_t knishio_response_policy_get_status(
    const knishio_response_policy_t *response
);

/**
 * @brief Get policy version
 * @param response Policy response
 * @return Version string or NULL if not versioned
 */
const char* knishio_response_policy_get_version(
    const knishio_response_policy_t *response
);

/**
 * @brief Get associated rule references
 * @param response Policy response
 * @param count Output parameter for number of rules
 * @return Array of rule references or NULL if no rules
 */
const knishio_policy_rule_ref_t* knishio_response_policy_get_rules(
    const knishio_response_policy_t *response,
    size_t *count
);

/**
 * @brief Get rule reference by index
 * @param response Policy response
 * @param index Rule index
 * @return Rule reference or NULL if index out of bounds
 */
const knishio_policy_rule_ref_t* knishio_response_policy_get_rule(
    const knishio_response_policy_t *response,
    size_t index
);

/**
 * @brief Find rule reference by rule ID
 * @param response Policy response
 * @param rule_id Rule ID to search for
 * @return First matching rule reference or NULL if not found
 */
const knishio_policy_rule_ref_t* knishio_response_policy_find_rule(
    const knishio_response_policy_t *response,
    const char *rule_id
);

/**
 * @brief Get governance settings
 * @param response Policy response
 * @return Governance settings structure
 */
const knishio_policy_governance_t* knishio_response_policy_get_governance(
    const knishio_response_policy_t *response
);

/**
 * @brief Get policy metrics
 * @param response Policy response
 * @return Policy metrics structure
 */
const knishio_policy_metrics_t* knishio_response_policy_get_metrics(
    const knishio_response_policy_t *response
);

/**
 * @brief Check if policy is active
 * @param response Policy response
 * @return True if policy is active, false otherwise
 */
bool knishio_response_policy_is_active(const knishio_response_policy_t *response);

/**
 * @brief Check if policy is system policy
 * @param response Policy response
 * @return True if system policy, false otherwise
 */
bool knishio_response_policy_is_system_policy(const knishio_response_policy_t *response);

/**
 * @brief Get policy tags
 * @param response Policy response
 * @param count Output parameter for number of tags
 * @return Array of tag strings or NULL if no tags
 */
const char* const* knishio_response_policy_get_tags(
    const knishio_response_policy_t *response,
    size_t *count
);

/**
 * @brief Get policy status as string
 * @param status Policy status
 * @return Status string representation
 */
const char* knishio_policy_status_to_string(knishio_policy_status_t status);

/**
 * @brief Parse policy status from string
 * @param status_str Status string
 * @return Parsed status or KNISHIO_POLICY_STATUS_DRAFT if unknown
 */
knishio_policy_status_t knishio_policy_status_from_string(const char *status_str);

/**
 * @brief Get governance model as string
 * @param model Governance model
 * @return Model string representation
 */
const char* knishio_governance_model_to_string(knishio_governance_model_t model);

/**
 * @brief Parse governance model from string
 * @param model_str Model string
 * @return Parsed model or KNISHIO_GOVERNANCE_CENTRALIZED if unknown
 */
knishio_governance_model_t knishio_governance_model_from_string(const char *model_str);

/* Policy component utilities */

/**
 * @brief Free policy rule reference
 * @param rule Rule reference to free
 */
void knishio_policy_rule_ref_free(knishio_policy_rule_ref_t *rule);

/**
 * @brief Free policy governance settings
 * @param governance Governance settings to free
 */
void knishio_policy_governance_free(knishio_policy_governance_t *governance);

/**
 * @brief Free policy metrics
 * @param metrics Metrics to free
 */
void knishio_policy_metrics_free(knishio_policy_metrics_t *metrics);

/* Factory function for response creation */

/**
 * @brief Factory function for Policy response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_policy_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for policy operations */
extern const char* const KNISHIO_POLICY_DEFAULT_VERSION;
extern const double KNISHIO_POLICY_MIN_COMPLIANCE_SCORE;
extern const int KNISHIO_POLICY_DEFAULT_REQUIRED_APPROVALS;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_policy_t) > sizeof(knishio_response_t),
    "Policy response must be larger than base response");
_Static_assert(sizeof(knishio_policy_status_t) == sizeof(int),
    "Policy status must be integer-sized for ABI compatibility");
_Static_assert(sizeof(knishio_governance_model_t) == sizeof(int),
    "Governance model must be integer-sized for ABI compatibility");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_POLICY_H */