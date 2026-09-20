/*
 * Encrypted transport must fail CLOSED (PQ-transport Phase E).
 *
 * knishio_client_execute_graphql used to gate the CipherHash envelope on
 * `cipher_enabled && cipher_server_pubkey && cipher_wallet && cipher_should_encrypt(op)` and, when
 * that was false, executed the operation as PLAINTEXT. It had a second downgrade too: when the
 * body could not be built, or knishio_cipher_hash_encrypt() itself failed, the `if (... ==
 * KNISHIO_SUCCESS)` fell through and the unencrypted operation still went out. The caller asked
 * for an encrypted transport and silently got none.
 *
 * PHP (Libraries/Cipher.php) and Kotlin (httpClient/HttpClient.kt) already raised
 * `Authorized wallet missing.` / `Server public key missing.` here. C has no exceptions, so the
 * equivalent is a returned error code: KNISHIO_ERROR_INVALID_STATE for missing transport keys,
 * KNISHIO_ERROR_CRYPTO when the envelope cannot be produced.
 *
 * The bypass set must keep working: the auth bootstrap (__schema, ContinuId, AccessToken,
 * U-isotope ProposeMolecule) cannot be encrypted, because the validator's public key is what it
 * is fetching. No validator is needed here — the URI is a closed loopback port, so a request that
 * IS attempted comes back as a transport failure rather than a fail-closed error code.
 */
#include "unity.h"
#include "knishio/knishio.h"
#include "knishio/client.h"
#include "knishio/client_ops.h"
#include "knishio/graphql.h"
#include "knishio/molecule.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"

/* Closed loopback port: an attempted request fails fast instead of reaching a real validator. */
static const char* const TEST_URI = "http://127.0.0.1:1/graphql";

static knishio_client_t* make_encrypted_client(void) {
    knishio_client_config_t config = {0};
    config.uri = TEST_URI;
    config.cell_slug = "public";
    knishio_client_t* client = NULL;
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, knishio_client_create(&client, &config));
    TEST_ASSERT_NOT_NULL(client);
    knishio_client_set_encryption(client, true);
    return client;
}

static knishio_graphql_operation_t balance_operation(void) {
    knishio_graphql_operation_t op = {0};
    op.name = "Balance";
    op.query = "query Balance { Balance(token: \"USER\") { address } }";
    op.variables_json = "{}";
    op.requires_auth = true;
    op.is_mutation = false;
    return op;
}

/* A non-bypassed operation with no transport keys must NOT be sent in the clear. */
void test_encrypted_transport_missing_keys_fails_closed(void) {
    knishio_client_t* client = make_encrypted_client();
    knishio_graphql_operation_t op = balance_operation();
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_client_execute_graphql(client, &op, &response);

    TEST_ASSERT_EQUAL(KNISHIO_ERROR_INVALID_STATE, error);
    TEST_ASSERT_NULL(response);

    knishio_client_destroy(client);
}

/* An envelope that cannot be produced (malformed validator key) must NOT degrade to plaintext.
 * This is the second C-only defect: the old code tested the encrypt result for success and
 * simply carried on with the unencrypted body when it failed. */
void test_encrypted_transport_encryption_failure_fails_closed(void) {
    char* secret = NULL;
    TEST_ASSERT_TRUE(knishio_generate_secret("fail-closed-test-seed", 2048, &secret));

    knishio_wallet_t* wallet = NULL;
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS,
                      knishio_wallet_create_simple(
                          &wallet, secret, "AUTH",
                          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    knishio_client_t* client = make_encrypted_client();
    /* Not an ML-KEM public key: encapsulation against it cannot succeed. */
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS,
                      knishio_client_set_cipher_context(client, "bm90LWEta2V5", wallet));

    knishio_graphql_operation_t op = balance_operation();
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_client_execute_graphql(client, &op, &response);

    TEST_ASSERT_EQUAL(KNISHIO_ERROR_CRYPTO, error);
    TEST_ASSERT_NULL(response);

    knishio_client_destroy(client);
    knishio_wallet_free(wallet);
    knishio_free(secret);
}

/* An empty advertised key is not a key: it must be reported as missing state (the caller has no
 * transport key), not as a crypto failure, and must match what JS/TS report for `""`. */
void test_encrypted_transport_empty_pubkey_is_missing(void) {
    char* secret = NULL;
    TEST_ASSERT_TRUE(knishio_generate_secret("empty-key-test-seed", 2048, &secret));

    knishio_wallet_t* wallet = NULL;
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS,
                      knishio_wallet_create_simple(
                          &wallet, secret, "AUTH",
                          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    knishio_client_t* client = make_encrypted_client();
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS,
                      knishio_client_set_cipher_context(client, "", wallet));

    knishio_graphql_operation_t op = balance_operation();
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_client_execute_graphql(client, &op, &response);

    TEST_ASSERT_EQUAL(KNISHIO_ERROR_INVALID_STATE, error);
    TEST_ASSERT_NULL(response);

    knishio_client_destroy(client);
    knishio_wallet_free(wallet);
    knishio_free(secret);
}

/* The auth bootstrap must still go out in plaintext, or an encrypted client could never
 * authenticate: it would be encrypting to a validator key it has not learned yet. */
void test_encrypted_transport_bypassed_operation_is_attempted(void) {
    knishio_client_t* client = make_encrypted_client();
    knishio_graphql_operation_t op = {0};
    op.name = "__schema";
    op.query = "query { __schema { types { name } } }";
    op.variables_json = "{}";
    op.requires_auth = false;
    op.is_mutation = false;
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_client_execute_graphql(client, &op, &response);

    /* Whatever the closed port returns, it must not be one of the fail-closed codes — those
     * would mean the bypass had been swallowed by the encryption precondition. */
    TEST_ASSERT_NOT_EQUAL(KNISHIO_ERROR_INVALID_STATE, error);
    TEST_ASSERT_NOT_EQUAL(KNISHIO_ERROR_CRYPTO, error);

    if (response) {
        knishio_graphql_response_free(response);
    }
    knishio_client_destroy(client);
}

/* A plaintext client is unaffected: no keys, no encryption, request still attempted. */
void test_plaintext_client_is_unaffected(void) {
    knishio_client_config_t config = {0};
    config.uri = TEST_URI;
    config.cell_slug = "public";
    knishio_client_t* client = NULL;
    TEST_ASSERT_EQUAL(KNISHIO_SUCCESS, knishio_client_create(&client, &config));

    knishio_graphql_operation_t op = balance_operation();
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_client_execute_graphql(client, &op, &response);

    TEST_ASSERT_NOT_EQUAL(KNISHIO_ERROR_INVALID_STATE, error);
    TEST_ASSERT_NOT_EQUAL(KNISHIO_ERROR_CRYPTO, error);

    if (response) {
        knishio_graphql_response_free(response);
    }
    knishio_client_destroy(client);
}

void test_encrypted_transport_suite(void) {
    RUN_TEST(test_encrypted_transport_missing_keys_fails_closed);
    RUN_TEST(test_encrypted_transport_encryption_failure_fails_closed);
    RUN_TEST(test_encrypted_transport_empty_pubkey_is_missing);
    RUN_TEST(test_encrypted_transport_bypassed_operation_is_attempted);
    RUN_TEST(test_plaintext_client_is_unaffected);
}
