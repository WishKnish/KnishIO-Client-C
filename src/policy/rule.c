/**
 * @file policy/rule.c
 * @brief Rule system implementation for KnishIO policy engine
 */

#include "knishio/policy/rule.h"
#include "knishio/knishio.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

/* Operator string mappings */
static const struct {
    knishio_condition_operator_t operator;
    const char* string;
} operator_map[] = {
    { KNISHIO_CONDITION_EQUAL, "==" },
    { KNISHIO_CONDITION_NOT_EQUAL, "!=" },
    { KNISHIO_CONDITION_LESS_THAN, "<" },
    { KNISHIO_CONDITION_LESS_EQUAL, "<=" },
    { KNISHIO_CONDITION_GREATER_THAN, ">" },
    { KNISHIO_CONDITION_GREATER_EQUAL, ">=" },
    { KNISHIO_CONDITION_CONTAINS, "contains" },
    { KNISHIO_CONDITION_REGEX, "regex" },
    { KNISHIO_CONDITION_IN, "in" },
    { KNISHIO_CONDITION_NOT_IN, "not_in" }
};

/* Action string mappings */
static const struct {
    knishio_callback_action_t action;
    const char* string;
} action_map[] = {
    { KNISHIO_CALLBACK_REJECT, "reject" },
    { KNISHIO_CALLBACK_META, "meta" },
    { KNISHIO_CALLBACK_COLLECT, "collect" },
    { KNISHIO_CALLBACK_BUFFER, "buffer" },
    { KNISHIO_CALLBACK_REMIT, "remit" },
    { KNISHIO_CALLBACK_BURN, "burn" }
};

/* Helper function to compare values */
static bool compare_values(const char* left, const char* right, knishio_condition_operator_t operator) {
    if (!left || !right) {
        return false;
    }
    
    double left_num, right_num;
    bool left_is_num = (sscanf(left, "%lf", &left_num) == 1);
    bool right_is_num = (sscanf(right, "%lf", &right_num) == 1);
    
    switch (operator) {
        case KNISHIO_CONDITION_EQUAL:
            if (left_is_num && right_is_num) {
                return left_num == right_num;
            }
            return strcmp(left, right) == 0;
            
        case KNISHIO_CONDITION_NOT_EQUAL:
            if (left_is_num && right_is_num) {
                return left_num != right_num;
            }
            return strcmp(left, right) != 0;
            
        case KNISHIO_CONDITION_LESS_THAN:
            if (left_is_num && right_is_num) {
                return left_num < right_num;
            }
            return strcmp(left, right) < 0;
            
        case KNISHIO_CONDITION_LESS_EQUAL:
            if (left_is_num && right_is_num) {
                return left_num <= right_num;
            }
            return strcmp(left, right) <= 0;
            
        case KNISHIO_CONDITION_GREATER_THAN:
            if (left_is_num && right_is_num) {
                return left_num > right_num;
            }
            return strcmp(left, right) > 0;
            
        case KNISHIO_CONDITION_GREATER_EQUAL:
            if (left_is_num && right_is_num) {
                return left_num >= right_num;
            }
            return strcmp(left, right) >= 0;
            
        case KNISHIO_CONDITION_CONTAINS:
            return strstr(left, right) != NULL;
            
        case KNISHIO_CONDITION_REGEX: {
            regex_t regex;
            int ret = regcomp(&regex, right, REG_EXTENDED);
            if (ret != 0) {
                return false;
            }
            ret = regexec(&regex, left, 0, NULL, 0);
            regfree(&regex);
            return ret == 0;
        }
        
        case KNISHIO_CONDITION_IN:
        case KNISHIO_CONDITION_NOT_IN:
            /* For array operations, we'd need to parse JSON arrays */
            /* For now, implement as simple string search */
            return (strstr(right, left) != NULL) == (operator == KNISHIO_CONDITION_IN);
    }
    
    return false;
}

/* Helper function to get value from context */
static const char* get_context_value(const knishio_rule_context_t* context, const char* key) {
    if (!context || !key) {
        return NULL;
    }
    
    if (strcmp(key, "wallet_address") == 0) {
        return context->wallet_address;
    } else if (strcmp(key, "token") == 0) {
        return context->token;
    } else if (strcmp(key, "amount") == 0) {
        return context->amount;
    } else if (strcmp(key, "meta_type") == 0) {
        return context->meta_type;
    } else if (strcmp(key, "meta_id") == 0) {
        return context->meta_id;
    }
    
    /* Check meta data */
    if (context->meta_data) {
        cJSON* item = cJSON_GetObjectItem(context->meta_data, key);
        if (item && cJSON_IsString(item)) {
            return cJSON_GetStringValue(item);
        }
    }
    
    /* Check transaction data */
    if (context->transaction_data) {
        cJSON* item = cJSON_GetObjectItem(context->transaction_data, key);
        if (item && cJSON_IsString(item)) {
            return cJSON_GetStringValue(item);
        }
    }
    
    return NULL;
}

/* Condition operations */

knishio_condition_t* knishio_condition_create(
    const char* key,
    const char* value,
    knishio_condition_operator_t comparison
) {
    if (!key || !value) {
        return NULL;
    }
    
    knishio_condition_t* condition = calloc(1, sizeof(knishio_condition_t));
    if (!condition) {
        return NULL;
    }
    
    condition->key = knishio_strdup(key);
    condition->value = knishio_strdup(value);
    condition->comparison = comparison;
    
    if (!condition->key || !condition->value) {
        knishio_condition_free(condition);
        return NULL;
    }
    
    return condition;
}

knishio_condition_t* knishio_condition_from_json(cJSON* json) {
    if (!json) {
        return NULL;
    }
    
    cJSON* key_json = cJSON_GetObjectItem(json, "key");
    cJSON* value_json = cJSON_GetObjectItem(json, "value");
    cJSON* comparison_json = cJSON_GetObjectItem(json, "comparison");
    
    if (!key_json || !value_json || !comparison_json ||
        !cJSON_IsString(key_json) || !cJSON_IsString(value_json) ||
        !cJSON_IsString(comparison_json)) {
        return NULL;
    }
    
    const char* key = cJSON_GetStringValue(key_json);
    const char* value = cJSON_GetStringValue(value_json);
    const char* comparison_str = cJSON_GetStringValue(comparison_json);
    
    knishio_condition_operator_t comparison = knishio_condition_parse_operator(comparison_str);
    if (comparison == (knishio_condition_operator_t)-1) {
        return NULL;
    }
    
    return knishio_condition_create(key, value, comparison);
}

bool knishio_condition_evaluate(
    const knishio_condition_t* condition,
    const knishio_rule_context_t* context
) {
    if (!condition || !context) {
        return false;
    }
    
    const char* context_value = get_context_value(context, condition->key);
    if (!context_value) {
        return false;
    }
    
    return compare_values(context_value, condition->value, condition->comparison);
}

cJSON* knishio_condition_to_json(const knishio_condition_t* condition) {
    if (!condition) {
        return NULL;
    }
    
    cJSON* json = cJSON_CreateObject();
    if (!json) {
        return NULL;
    }
    
    cJSON_AddStringToObject(json, "key", condition->key);
    cJSON_AddStringToObject(json, "value", condition->value);
    cJSON_AddStringToObject(json, "comparison", 
                           knishio_condition_operator_to_string(condition->comparison));
    
    return json;
}

void knishio_condition_free(knishio_condition_t* condition) {
    if (!condition) return;
    
    if (condition->key) free(condition->key);
    if (condition->value) free(condition->value);
    free(condition);
}

/* Rule meta operations */

knishio_rule_meta_t* knishio_rule_meta_create(cJSON* data) {
    knishio_rule_meta_t* meta = calloc(1, sizeof(knishio_rule_meta_t));
    if (!meta) {
        return NULL;
    }
    
    if (data) {
        meta->data = cJSON_Duplicate(data, true);
        if (!meta->data) {
            free(meta);
            return NULL;
        }
    }
    
    return meta;
}

knishio_rule_meta_t* knishio_rule_meta_from_json(cJSON* json) {
    return knishio_rule_meta_create(json);
}

cJSON* knishio_rule_meta_to_json(const knishio_rule_meta_t* meta) {
    if (!meta || !meta->data) {
        return NULL;
    }
    
    return cJSON_Duplicate(meta->data, true);
}

void knishio_rule_meta_free(knishio_rule_meta_t* meta) {
    if (!meta) return;
    
    if (meta->data) {
        cJSON_Delete(meta->data);
    }
    free(meta);
}

/* Callback operations */

knishio_callback_t* knishio_callback_create(knishio_callback_action_t action) {
    knishio_callback_t* callback = calloc(1, sizeof(knishio_callback_t));
    if (!callback) {
        return NULL;
    }
    
    callback->action = action;
    return callback;
}

knishio_callback_t* knishio_callback_from_json(cJSON* json) {
    if (!json) {
        return NULL;
    }
    
    cJSON* action_json = cJSON_GetObjectItem(json, "action");
    if (!action_json || !cJSON_IsString(action_json)) {
        return NULL;
    }
    
    const char* action_str = cJSON_GetStringValue(action_json);
    knishio_callback_action_t action = knishio_callback_parse_action(action_str);
    if (action == (knishio_callback_action_t)-1) {
        return NULL;
    }
    
    knishio_callback_t* callback = knishio_callback_create(action);
    if (!callback) {
        return NULL;
    }
    
    /* Set optional fields */
    cJSON* meta_type_json = cJSON_GetObjectItem(json, "metaType");
    if (meta_type_json && cJSON_IsString(meta_type_json)) {
        callback->meta_type = knishio_strdup(cJSON_GetStringValue(meta_type_json));
    }
    
    cJSON* meta_id_json = cJSON_GetObjectItem(json, "metaId");
    if (meta_id_json && cJSON_IsString(meta_id_json)) {
        callback->meta_id = knishio_strdup(cJSON_GetStringValue(meta_id_json));
    }
    
    cJSON* meta_json = cJSON_GetObjectItem(json, "meta");
    if (meta_json) {
        callback->meta = knishio_rule_meta_from_json(meta_json);
    }
    
    cJSON* address_json = cJSON_GetObjectItem(json, "address");
    if (address_json && cJSON_IsString(address_json)) {
        callback->address = knishio_strdup(cJSON_GetStringValue(address_json));
    }
    
    cJSON* token_json = cJSON_GetObjectItem(json, "token");
    if (token_json && cJSON_IsString(token_json)) {
        callback->token = knishio_strdup(cJSON_GetStringValue(token_json));
    }
    
    cJSON* amount_json = cJSON_GetObjectItem(json, "amount");
    if (amount_json && cJSON_IsString(amount_json)) {
        callback->amount = knishio_strdup(cJSON_GetStringValue(amount_json));
    }
    
    cJSON* comparison_json = cJSON_GetObjectItem(json, "comparison");
    if (comparison_json && cJSON_IsString(comparison_json)) {
        callback->comparison = knishio_strdup(cJSON_GetStringValue(comparison_json));
    }
    
    return callback;
}

bool knishio_callback_is_reject(const knishio_callback_t* callback) {
    return callback && callback->action == KNISHIO_CALLBACK_REJECT;
}

bool knishio_callback_is_meta(const knishio_callback_t* callback) {
    if (!callback || callback->action != KNISHIO_CALLBACK_META) {
        return false;
    }
    
    return callback->meta_type && callback->meta_id && callback->meta;
}

bool knishio_callback_is_collect(const knishio_callback_t* callback) {
    if (!callback || callback->action != KNISHIO_CALLBACK_COLLECT) {
        return false;
    }
    
    return callback->address && callback->token && callback->amount && callback->comparison;
}

bool knishio_callback_is_buffer(const knishio_callback_t* callback) {
    if (!callback || callback->action != KNISHIO_CALLBACK_BUFFER) {
        return false;
    }
    
    return callback->address && callback->token && callback->amount && callback->comparison;
}

bool knishio_callback_is_remit(const knishio_callback_t* callback) {
    if (!callback || callback->action != KNISHIO_CALLBACK_REMIT) {
        return false;
    }
    
    return callback->token && callback->amount;
}

bool knishio_callback_is_burn(const knishio_callback_t* callback) {
    if (!callback || callback->action != KNISHIO_CALLBACK_BURN) {
        return false;
    }
    
    return callback->token && callback->amount && callback->comparison;
}

cJSON* knishio_callback_to_json(const knishio_callback_t* callback) {
    if (!callback) {
        return NULL;
    }
    
    cJSON* json = cJSON_CreateObject();
    if (!json) {
        return NULL;
    }
    
    cJSON_AddStringToObject(json, "action", 
                           knishio_callback_action_to_string(callback->action));
    
    if (callback->meta_type) {
        cJSON_AddStringToObject(json, "metaType", callback->meta_type);
    }
    if (callback->meta_id) {
        cJSON_AddStringToObject(json, "metaId", callback->meta_id);
    }
    if (callback->meta) {
        cJSON* meta_json = knishio_rule_meta_to_json(callback->meta);
        if (meta_json) {
            cJSON_AddItemToObject(json, "meta", meta_json);
        }
    }
    if (callback->address) {
        cJSON_AddStringToObject(json, "address", callback->address);
    }
    if (callback->token) {
        cJSON_AddStringToObject(json, "token", callback->token);
    }
    if (callback->amount) {
        cJSON_AddStringToObject(json, "amount", callback->amount);
    }
    if (callback->comparison) {
        cJSON_AddStringToObject(json, "comparison", callback->comparison);
    }
    
    return json;
}

void knishio_callback_free(knishio_callback_t* callback) {
    if (!callback) return;
    
    if (callback->meta_type) free(callback->meta_type);
    if (callback->meta_id) free(callback->meta_id);
    if (callback->meta) knishio_rule_meta_free(callback->meta);
    if (callback->address) free(callback->address);
    if (callback->token) free(callback->token);
    if (callback->amount) free(callback->amount);
    if (callback->comparison) free(callback->comparison);
    free(callback);
}

/* Rule operations */

knishio_rule_t* knishio_rule_create(void) {
    knishio_rule_t* rule = calloc(1, sizeof(knishio_rule_t));
    return rule;
}

knishio_rule_t* knishio_rule_from_json(cJSON* json) {
    if (!json) {
        return NULL;
    }
    
    knishio_rule_t* rule = knishio_rule_create();
    if (!rule) {
        return NULL;
    }
    
    /* Parse conditions */
    cJSON* conditions_json = cJSON_GetObjectItem(json, "condition");
    if (conditions_json && cJSON_IsArray(conditions_json)) {
        cJSON* condition_json;
        cJSON_ArrayForEach(condition_json, conditions_json) {
            knishio_condition_t* condition = knishio_condition_from_json(condition_json);
            if (condition) {
                knishio_rule_add_condition(rule, condition);
            }
        }
    }
    
    /* Parse callbacks */
    cJSON* callbacks_json = cJSON_GetObjectItem(json, "callback");
    if (callbacks_json && cJSON_IsArray(callbacks_json)) {
        cJSON* callback_json;
        cJSON_ArrayForEach(callback_json, callbacks_json) {
            knishio_callback_t* callback = knishio_callback_from_json(callback_json);
            if (callback) {
                knishio_rule_add_callback(rule, callback);
            }
        }
    }
    
    return rule;
}

knishio_error_t knishio_rule_add_condition(
    knishio_rule_t* rule,
    knishio_condition_t* condition
) {
    if (!rule || !condition) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Reallocate conditions array */
    size_t new_size = (rule->condition_count + 1) * sizeof(knishio_condition_t*);
    knishio_condition_t** new_conditions = realloc(rule->conditions, new_size);
    if (!new_conditions) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    rule->conditions = new_conditions;
    rule->conditions[rule->condition_count++] = condition;
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_rule_add_callback(
    knishio_rule_t* rule,
    knishio_callback_t* callback
) {
    if (!rule || !callback) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Reallocate callbacks array */
    size_t new_size = (rule->callback_count + 1) * sizeof(knishio_callback_t*);
    knishio_callback_t** new_callbacks = realloc(rule->callbacks, new_size);
    if (!new_callbacks) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    rule->callbacks = new_callbacks;
    rule->callbacks[rule->callback_count++] = callback;
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_rule_evaluate(
    const knishio_rule_t* rule,
    const knishio_rule_context_t* context,
    knishio_rule_result_t** result
) {
    if (!rule || !context || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    knishio_rule_result_t* eval_result = calloc(1, sizeof(knishio_rule_result_t));
    if (!eval_result) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Evaluate all conditions (AND logic) */
    bool all_conditions_pass = true;
    for (size_t i = 0; i < rule->condition_count; i++) {
        if (!knishio_condition_evaluate(rule->conditions[i], context)) {
            all_conditions_pass = false;
            break;
        }
    }
    
    eval_result->passed = all_conditions_pass;
    
    if (all_conditions_pass && rule->callback_count > 0) {
        /* Copy triggered callbacks */
        size_t callbacks_size = rule->callback_count * sizeof(knishio_callback_t*);
        eval_result->triggered_callbacks = malloc(callbacks_size);
        if (!eval_result->triggered_callbacks) {
            knishio_rule_result_free(eval_result);
            return KNISHIO_ERROR_MEMORY;
        }
        
        memcpy(eval_result->triggered_callbacks, rule->callbacks, callbacks_size);
        eval_result->callback_count = rule->callback_count;
    }
    
    *result = eval_result;
    return KNISHIO_SUCCESS;
}

cJSON* knishio_rule_to_json(const knishio_rule_t* rule) {
    if (!rule) {
        return NULL;
    }
    
    cJSON* json = cJSON_CreateObject();
    if (!json) {
        return NULL;
    }
    
    /* Add conditions array */
    cJSON* conditions_json = cJSON_CreateArray();
    if (conditions_json) {
        for (size_t i = 0; i < rule->condition_count; i++) {
            cJSON* condition_json = knishio_condition_to_json(rule->conditions[i]);
            if (condition_json) {
                cJSON_AddItemToArray(conditions_json, condition_json);
            }
        }
        cJSON_AddItemToObject(json, "condition", conditions_json);
    }
    
    /* Add callbacks array */
    cJSON* callbacks_json = cJSON_CreateArray();
    if (callbacks_json) {
        for (size_t i = 0; i < rule->callback_count; i++) {
            cJSON* callback_json = knishio_callback_to_json(rule->callbacks[i]);
            if (callback_json) {
                cJSON_AddItemToArray(callbacks_json, callback_json);
            }
        }
        cJSON_AddItemToObject(json, "callback", callbacks_json);
    }
    
    return json;
}

void knishio_rule_free(knishio_rule_t* rule) {
    if (!rule) return;
    
    /* Free conditions */
    for (size_t i = 0; i < rule->condition_count; i++) {
        knishio_condition_free(rule->conditions[i]);
    }
    if (rule->conditions) free(rule->conditions);
    
    /* Free callbacks */
    for (size_t i = 0; i < rule->callback_count; i++) {
        knishio_callback_free(rule->callbacks[i]);
    }
    if (rule->callbacks) free(rule->callbacks);
    
    free(rule);
}

void knishio_rule_result_free(knishio_rule_result_t* result) {
    if (!result) return;
    
    if (result->error_message) free(result->error_message);
    if (result->triggered_callbacks) free(result->triggered_callbacks);
    free(result);
}

/* Utility functions */

knishio_condition_operator_t knishio_condition_parse_operator(const char* operator_str) {
    if (!operator_str) {
        return (knishio_condition_operator_t)-1;
    }
    
    for (size_t i = 0; i < sizeof(operator_map) / sizeof(operator_map[0]); i++) {
        if (strcmp(operator_str, operator_map[i].string) == 0) {
            return operator_map[i].operator;
        }
    }
    
    return (knishio_condition_operator_t)-1;
}

const char* knishio_condition_operator_to_string(knishio_condition_operator_t operator) {
    for (size_t i = 0; i < sizeof(operator_map) / sizeof(operator_map[0]); i++) {
        if (operator_map[i].operator == operator) {
            return operator_map[i].string;
        }
    }
    
    return NULL;
}

knishio_callback_action_t knishio_callback_parse_action(const char* action_str) {
    if (!action_str) {
        return (knishio_callback_action_t)-1;
    }
    
    for (size_t i = 0; i < sizeof(action_map) / sizeof(action_map[0]); i++) {
        if (strcasecmp(action_str, action_map[i].string) == 0) {
            return action_map[i].action;
        }
    }
    
    return (knishio_callback_action_t)-1;
}

const char* knishio_callback_action_to_string(knishio_callback_action_t action) {
    for (size_t i = 0; i < sizeof(action_map) / sizeof(action_map[0]); i++) {
        if (action_map[i].action == action) {
            return action_map[i].string;
        }
    }
    
    return NULL;
}