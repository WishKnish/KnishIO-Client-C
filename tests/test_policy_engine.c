/**
 * @file tests/test_policy_engine.c
 * @brief Test program for KnishIO policy engine
 */

#include "knishio/policy/engine.h"
#include "knishio/policy/rule.h"
#include "knishio/operations/policy.h"
#include "knishio/knishio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test rule creation and evaluation */
static void test_rule_creation_and_evaluation() {
    printf("Testing rule creation and evaluation...\n");
    
    /* Create a simple condition */
    knishio_condition_t* condition = knishio_condition_create(
        "wallet_address", 
        "Kk4xBqV5G6SPGAdEQE6oLCULQhGnDUJBjzFEJoQhG3qB", 
        KNISHIO_CONDITION_EQUAL
    );
    assert(condition != NULL);
    
    /* Create a reject callback */
    knishio_callback_t* callback = knishio_callback_create(KNISHIO_CALLBACK_REJECT);
    assert(callback != NULL);
    
    /* Create rule and add components */
    knishio_rule_t* rule = knishio_rule_create();
    assert(rule != NULL);
    
    assert(knishio_rule_add_condition(rule, condition) == KNISHIO_SUCCESS);
    assert(knishio_rule_add_callback(rule, callback) == KNISHIO_SUCCESS);
    
    /* Create evaluation context */
    knishio_rule_context_t context = {
        .wallet_address = "Kk4xBqV5G6SPGAdEQE6oLCULQhGnDUJBjzFEJoQhG3qB",
        .token = "TEST",
        .amount = "100",
        .meta_type = "user",
        .meta_id = "test_user",
        .meta_data = NULL,
        .transaction_data = NULL
    };
    
    /* Evaluate rule */
    knishio_rule_result_t* result = NULL;
    assert(knishio_rule_evaluate(rule, &context, &result) == KNISHIO_SUCCESS);
    assert(result != NULL);
    assert(result->passed == true);
    assert(result->callback_count == 1);
    
    knishio_rule_result_free(result);
    knishio_rule_free(rule);
    
    printf("✓ Rule creation and evaluation test passed\n");
}

/* Test rule JSON serialization */
static void test_rule_json_serialization() {
    printf("Testing rule JSON serialization...\n");
    
    /* Create rule from JSON */
    const char* rule_json = "{"
        "\"condition\": ["
        "  {\"key\": \"token\", \"value\": \"TEST\", \"comparison\": \"==\"}"
        "],"
        "\"callback\": ["
        "  {\"action\": \"reject\"}"
        "]"
        "}";
    
    cJSON* rule_obj = cJSON_Parse(rule_json);
    assert(rule_obj != NULL);
    
    knishio_rule_t* rule = knishio_rule_from_json(rule_obj);
    assert(rule != NULL);
    assert(rule->condition_count == 1);
    assert(rule->callback_count == 1);
    
    /* Convert back to JSON */
    cJSON* serialized = knishio_rule_to_json(rule);
    assert(serialized != NULL);
    
    char* json_str = cJSON_Print(serialized);
    printf("Serialized rule: %s\n", json_str);
    
    cJSON_Delete(rule_obj);
    cJSON_Delete(serialized);
    free(json_str);
    knishio_rule_free(rule);
    
    printf("✓ Rule JSON serialization test passed\n");
}

/* Test policy engine */
static void test_policy_engine() {
    printf("Testing policy engine...\n");
    
    /* Create policy engine */
    knishio_policy_engine_t* engine = knishio_policy_engine_create();
    assert(engine != NULL);
    
    /* Create policy with rules */
    const char* rules_json = "["
        "{"
        "  \"condition\": ["
        "    {\"key\": \"wallet_address\", \"value\": \"blacklisted_address\", \"comparison\": \"==\"}"
        "  ],"
        "  \"callback\": ["
        "    {\"action\": \"reject\"}"
        "  ]"
        "}"
        "]";
    
    cJSON* rules_array = cJSON_Parse(rules_json);
    assert(rules_array != NULL);
    
    assert(knishio_policy_engine_add_policy(
        engine, "user", "test_policy", rules_array, NULL) == KNISHIO_SUCCESS);
    
    /* Test policy enforcement */
    knishio_rule_context_t context = {
        .wallet_address = "blacklisted_address",
        .token = "TEST",
        .amount = "100",
        .meta_type = "user",
        .meta_id = "test_policy"
    };
    
    knishio_policy_result_t* result = NULL;
    assert(knishio_policy_engine_enforce(
        engine, KNISHIO_POLICY_WALLET_OPERATION, &context, &result) == KNISHIO_SUCCESS);
    
    assert(result != NULL);
    assert(result->allowed == false);
    
    knishio_policy_result_free(result);
    cJSON_Delete(rules_array);
    knishio_policy_engine_free(engine);
    
    printf("✓ Policy engine test passed\n");
}

/* Test meta access control */
static void test_meta_access_control() {
    printf("Testing meta access control...\n");
    
    knishio_policy_engine_t* engine = knishio_policy_engine_create();
    assert(engine != NULL);
    
    /* Create policy with read/write access control */
    const char* policy_json = "{"
        "\"read\": {"
        "  \"profile\": [\"all\"],"
        "  \"private\": [\"self\"]"
        "},"
        "\"write\": {"
        "  \"profile\": [\"self\"],"
        "  \"private\": [\"self\"]"
        "}"
        "}";
    
    cJSON* policy_obj = cJSON_Parse(policy_json);
    assert(policy_obj != NULL);
    
    assert(knishio_policy_engine_add_policy(
        engine, "user", "access_test", NULL, policy_obj) == KNISHIO_SUCCESS);
    
    /* Test read access - should allow */
    knishio_policy_result_t* result = NULL;
    assert(knishio_policy_engine_check_meta_access(
        engine, "user", "access_test", "any_wallet", false, &result) == KNISHIO_SUCCESS);
    
    assert(result != NULL);
    assert(result->allowed == true);
    
    knishio_policy_result_free(result);
    cJSON_Delete(policy_obj);
    knishio_policy_engine_free(engine);
    
    printf("✓ Meta access control test passed\n");
}

/* Test complex rule with multiple conditions */
static void test_complex_rule() {
    printf("Testing complex rule with multiple conditions...\n");
    
    const char* rule_json = "{"
        "\"condition\": ["
        "  {\"key\": \"token\", \"value\": \"RESTRICTED\", \"comparison\": \"==\"},"
        "  {\"key\": \"amount\", \"value\": \"1000\", \"comparison\": \">\"}"
        "],"
        "\"callback\": ["
        "  {\"action\": \"collect\", \"address\": \"fee_address\", \"token\": \"FEE\", \"amount\": \"10\", \"comparison\": \"percent\"}"
        "]"
        "}";
    
    cJSON* rule_obj = cJSON_Parse(rule_json);
    assert(rule_obj != NULL);
    
    knishio_rule_t* rule = knishio_rule_from_json(rule_obj);
    assert(rule != NULL);
    assert(rule->condition_count == 2);
    assert(rule->callback_count == 1);
    
    /* Test evaluation with matching conditions */
    knishio_rule_context_t context = {
        .wallet_address = "test_wallet",
        .token = "RESTRICTED",
        .amount = "1500",
        .meta_type = "transaction",
        .meta_id = "test_tx"
    };
    
    knishio_rule_result_t* result = NULL;
    assert(knishio_rule_evaluate(rule, &context, &result) == KNISHIO_SUCCESS);
    
    assert(result != NULL);
    assert(result->passed == true);
    assert(result->callback_count == 1);
    assert(knishio_callback_is_collect(result->triggered_callbacks[0]) == true);
    
    knishio_rule_result_free(result);
    cJSON_Delete(rule_obj);
    knishio_rule_free(rule);
    
    printf("✓ Complex rule test passed\n");
}

/* Test policy meta functionality */
static void test_policy_meta() {
    printf("Testing PolicyMeta functionality...\n");
    
    /* Test policy normalization */
    const char* policy_json = "{"
        "\"read\": {"
        "  \"profile\": [\"all\"]"
        "},"
        "\"write\": {"
        "  \"profile\": [\"self\"]"
        "}"
        "}";
    
    cJSON* policy_obj = cJSON_Parse(policy_json);
    assert(policy_obj != NULL);
    
    cJSON* normalized = knishio_policy_normalize_json(policy_obj);
    assert(normalized != NULL);
    
    cJSON* read_section = cJSON_GetObjectItem(normalized, "read");
    assert(read_section != NULL);
    
    cJSON* write_section = cJSON_GetObjectItem(normalized, "write");
    assert(write_section != NULL);
    
    cJSON_Delete(policy_obj);
    cJSON_Delete(normalized);
    
    printf("✓ PolicyMeta test passed\n");
}

/* Main test function */
int main() {
    printf("Running KnishIO Policy Engine Tests\n");
    printf("===================================\n\n");
    
    test_rule_creation_and_evaluation();
    test_rule_json_serialization();
    test_policy_engine();
    test_meta_access_control();
    test_complex_rule();
    test_policy_meta();
    
    printf("\n✓ All policy engine tests passed!\n");
    
    return 0;
}