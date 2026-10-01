/*
 * A ProposeMolecule the validator answers with status != "accepted" is a failure.
 *
 * knishio_graphql_response_apply_body used to set `success` from the HTTP status alone, and no
 * C caller read ProposeMolecule.status, so a rejected transfer, burn, token, wallet or meta came
 * back as success. These cases drive the body parser directly on canned validator answers.
 */
#include "unity.h"
#include "knishio/graphql.h"
#include "graphql_internal.h"

#include <string.h>

static knishio_graphql_response_t resp;

static void apply(const char* body, bool is_mutation) {
    memset(&resp, 0, sizeof resp);
    resp.status_code = 200;
    knishio_graphql_response_apply_body(&resp, body, is_mutation);
}

static void release(void) {
    /* A second apply with no body frees every owned string. */
    knishio_graphql_response_apply_body(&resp, NULL, false);
}

static void test_rejected_status_is_failure_with_reason(void) {
    apply("{\"data\":{\"ProposeMolecule\":{\"molecularHash\":\"abc123\",\"status\":\"rejected\","
          "\"reason\":\"Wallet not found\"}}}",
          true);
    TEST_ASSERT_FALSE(resp.success);
    TEST_ASSERT_NOT_NULL(resp.errors);
    TEST_ASSERT_NOT_NULL(strstr(resp.errors, "ProposeMolecule rejected: Wallet not found"));
    TEST_ASSERT_NOT_NULL(resp.molecular_hash);
    TEST_ASSERT_EQUAL_STRING("abc123", resp.molecular_hash);
    release();
}

static void test_accepted_status_is_success(void) {
    apply("{\"data\":{\"ProposeMolecule\":{\"molecularHash\":\"abc123\",\"status\":\"accepted\","
          "\"reason\":null}}}",
          true);
    TEST_ASSERT_TRUE(resp.success);
    TEST_ASSERT_NULL(resp.errors);
    release();
}

static void test_mutation_without_status_keeps_http_success(void) {
    apply("{\"data\":{\"ProposeMolecule\":{\"molecularHash\":\"abc123\"}}}", true);
    TEST_ASSERT_TRUE(resp.success);
    TEST_ASSERT_NULL(resp.errors);
    release();
}

static void test_query_ignores_rejected_looking_body(void) {
    apply("{\"data\":{\"ProposeMolecule\":{\"status\":\"rejected\",\"reason\":\"x\"}}}", false);
    TEST_ASSERT_TRUE(resp.success);
    TEST_ASSERT_NULL(resp.errors);
    release();
}

static void test_graphql_errors_are_kept_over_rejection(void) {
    apply("{\"errors\":[{\"message\":\"original graphql error\"}],"
          "\"data\":{\"ProposeMolecule\":{\"status\":\"rejected\",\"reason\":\"other\"}}}",
          true);
    TEST_ASSERT_FALSE(resp.success);
    TEST_ASSERT_NOT_NULL(resp.errors);
    TEST_ASSERT_NOT_NULL(strstr(resp.errors, "original graphql error"));
    TEST_ASSERT_NULL(strstr(resp.errors, "ProposeMolecule rejected"));
    release();
}

void test_propose_status_suite(void) {
    RUN_TEST(test_rejected_status_is_failure_with_reason);
    RUN_TEST(test_accepted_status_is_success);
    RUN_TEST(test_mutation_without_status_keeps_http_success);
    RUN_TEST(test_query_ignores_rejected_looking_body);
    RUN_TEST(test_graphql_errors_are_kept_over_rejection);
}
