/*
 * Live ML-KEM768 CipherHash encrypted-transport round-trip against a running validator
 * (PQ-transport Phase E, cycle 169 — C, the final SDK of the arc).
 *
 * End-to-end: the client authenticates (conveying its AUTH source wallet's ML-KEM public key via a
 * signed walletPubkey U-atom meta), then issues an encrypted balance query — the validator
 * ML-KEM-decrypts the request, executes it, and encrypts the response back to the client's ML-KEM
 * pubkey, which the client decrypts. The transport must be TRANSPARENT, so we assert the encrypted
 * result's wallet DATA equals a plaintext baseline (not merely a non-error response). A decrypt
 * failure yields a NULL wallet here (query_balance_wallet returns NULL when the response isn't a
 * Balance), so a false-pass is not possible.
 *
 * Gated on CIPHERHASH_TEST_URL: unset, it exits 77, which ctest reports as Skipped (SKIP_RETURN_CODE
 * in CMakeLists.txt) rather than Passed. Run live:
 *   CIPHERHASH_TEST_URL=http://localhost:8081/graphql ./build/tests/cipherhash_live_test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "knishio/knishio.h"
#include "knishio/client_ops.h"
#include "knishio/graphql.h"
#include "knishio/operations/auth.h"
#include "knishio/operations/transfer.h"
#include "knishio/wallet.h"

static int streq(const char* a, const char* b) {
    return a && b && strcmp(a, b) == 0;
}

int main(void) {
    const char* url = getenv("CIPHERHASH_TEST_URL");
    if (url == NULL || url[0] == '\0') {
        printf("SKIP: CIPHERHASH_TEST_URL not set — skipping live CipherHash test\n");
        return 77;  /* ctest SKIP_RETURN_CODE: reported as Skipped, never as Passed */
    }

    int rc = 1;
    char* secret = NULL;
    knishio_client_t* client = NULL;
    knishio_request_profile_auth_token_result_t* auth_result = NULL;
    knishio_wallet_t* enc_wallet = NULL;
    knishio_wallet_t* plain_wallet = NULL;
    char* secret2 = NULL;
    knishio_client_t* client2 = NULL;
    knishio_request_profile_auth_token_result_t* auth_result2 = NULL;
    knishio_wallet_t* enc_wallet2 = NULL;
    knishio_graphql_response_t* plain_response = NULL;

    if (!knishio_generate_secret("phase-e-live-c-cipherhash-secret-0123456789", 2048, &secret)) {
        fprintf(stderr, "FAIL: could not generate secret\n");
        goto cleanup;
    }

    const char* param_env = getenv("CIPHERHASH_MLKEM_PARAMETER_SET");
    int param = (param_env && strcmp(param_env, "768") == 0) ? 768 : 1024;

    knishio_client_config_t config = {
        .uri = url,
        .cell_slug = "public",   /* the active dev cell (TESTCELL is inactive there) */
        .client = NULL,
        .socket = NULL,
        .server_sdk_version = 3,
        .logging = false,
        .insecure_tls = false,
        .mlkem_parameter_set = param
    };
    if (knishio_client_create(&client, &config) != KNISHIO_SUCCESS || !client) {
        fprintf(stderr, "FAIL: could not create client\n");
        goto cleanup;
    }

    /* ONE session, transport toggled on it — the queried balance wallet stays fixed. (A fresh
     * second auth would rotate the USER remainder via ContinuID → a different address/position.)
     *
     * The session authenticates PLAINTEXT on purpose. The AUTH wallet's ML-KEM pubkey is conveyed
     * as a signed walletPubkey U-atom meta regardless of `encrypt`, and the cipher context is
     * plumbed either way (src/operations/auth.c), so a plaintext-authenticated session still
     * speaks the encrypted transport. Authenticating with encrypt=true instead would make the
     * plaintext baseline leg below a silent downgrade, which the validator rejects when
     * ENFORCE_ENCRYPTED_TRANSPORT is at its secure default. */
    knishio_request_profile_auth_token_params_t params = { .secret = secret, .encrypt = false };
    knishio_error_t aerr = knishio_client_request_profile_auth_token(client, &params, &auth_result);
    if (aerr != KNISHIO_SUCCESS || !auth_result || !auth_result->success) {
        fprintf(stderr, "FAIL: authentication failed%s%s\n",
                (auth_result && auth_result->error_message) ? ": " : "",
                (auth_result && auth_result->error_message) ? auth_result->error_message : "");
        goto cleanup;
    }

    /* Encrypted round-trip: the validator ML-KEM-decrypts the request, executes it, and encrypts
     * the response back to the client's ML-KEM pubkey; the client decrypts it. */
    knishio_client_switch_encryption(client, true);
    knishio_client_query_balance_wallet(client, "USER", &enc_wallet);

    /* Plaintext baseline of the SAME wallet on the SAME authed session — only the transport differs. */
    knishio_client_switch_encryption(client, false);
    knishio_client_query_balance_wallet(client, "USER", &plain_wallet);

    /* The PQ transport must be transparent: not just a non-error response, but the SAME wallet.
     * A decrypt failure → enc_wallet is NULL → FAIL (no false-pass). */
    if (!enc_wallet || !enc_wallet->address) {
        fprintf(stderr, "FAIL: encrypted query returned no wallet (transport must deliver data)\n");
        goto cleanup;
    }
    if (!plain_wallet || !plain_wallet->address) {
        fprintf(stderr, "FAIL: plaintext query returned no wallet\n");
        goto cleanup;
    }
    if (!streq(enc_wallet->address, plain_wallet->address)) {
        fprintf(stderr, "FAIL: encrypted address != plaintext address\n  enc=%s\n  plain=%s\n",
                enc_wallet->address, plain_wallet->address);
        goto cleanup;
    }
    if (enc_wallet->position && plain_wallet->position && !streq(enc_wallet->position, plain_wallet->position)) {
        fprintf(stderr, "FAIL: encrypted position != plaintext position\n");
        goto cleanup;
    }

    printf("PASS: encrypted balance query round-trips (matches plaintext); address=%s\n",
           enc_wallet->address);

    /* Scenario 2 — live coverage of the enforcement path: extract_encrypt_flag →
     * auth_tokens.encrypted → requires_encrypted_transport. A session that authenticated with
     * encrypt=true must NOT be able to fall back to plaintext. This also proves this SDK's signed
     * `encrypt` meta literal is the one the validator honours.
     *
     * The refusal is read from the RAW GraphQL response: knishio_client_query_balance_wallet
     * reports any GraphQL error as *wallet == NULL (transfer.c), which cannot tell a refusal from
     * an absent balance. */
    if (!knishio_generate_secret("phase-e-live-c-enforcement-secret-0123456789", 2048, &secret2)) {
        fprintf(stderr, "FAIL: could not generate the enforcement-scenario secret\n");
        rc = 1;
        goto cleanup;
    }
    if (knishio_client_create(&client2, &config) != KNISHIO_SUCCESS || !client2) {
        fprintf(stderr, "FAIL: could not create the enforcement-scenario client\n");
        rc = 1;
        goto cleanup;
    }
    {
        knishio_request_profile_auth_token_params_t enc_params = { .secret = secret2, .encrypt = true };
        knishio_error_t err2 = knishio_client_request_profile_auth_token(client2, &enc_params, &auth_result2);
        if (err2 != KNISHIO_SUCCESS || !auth_result2 || !auth_result2->success) {
            fprintf(stderr, "FAIL: encrypt=true authentication failed%s%s\n",
                    (auth_result2 && auth_result2->error_message) ? ": " : "",
                    (auth_result2 && auth_result2->error_message) ? auth_result2->error_message : "");
            rc = 1;
            goto cleanup;
        }
    }

    /* The encrypted transport still works for this session. */
    knishio_client_query_balance_wallet(client2, "USER", &enc_wallet2);
    if (!enc_wallet2 || !enc_wallet2->address) {
        fprintf(stderr, "FAIL: encrypt=true session could not complete an encrypted query\n");
        rc = 1;
        goto cleanup;
    }

    /* Dropping to plaintext on the same session must be refused by the validator. */
    knishio_client_switch_encryption(client2, false);
    {
        knishio_graphql_operation_t plain_op = {
            .name = "Balance",
            .query = "query { Balance(token: \"USER\") { address } }",
            .variables_json = "{}",
            .requires_auth = true,
            .is_mutation = false
        };
        knishio_error_t err3 = knishio_client_execute_graphql(client2, &plain_op, &plain_response);
        if (err3 != KNISHIO_SUCCESS || !plain_response) {
            fprintf(stderr, "FAIL: plaintext request could not be executed (err=%d)\n", (int)err3);
            rc = 1;
            goto cleanup;
        }
        if (plain_response->success || !plain_response->errors
            || !strstr(plain_response->errors, "CipherHash encrypted transport")) {
            fprintf(stderr,
                    "FAIL: plaintext request from an encrypt=true session was not refused\n  errors=%s\n",
                    plain_response->errors ? plain_response->errors : "(none)");
            rc = 1;
            goto cleanup;
        }
    }

    printf("PASS: an encrypt=true session is refused when it drops to plaintext\n");
    rc = 0;

cleanup:
    if (enc_wallet) knishio_wallet_free(enc_wallet);
    if (plain_wallet) knishio_wallet_free(plain_wallet);
    if (enc_wallet2) knishio_wallet_free(enc_wallet2);
    if (plain_response) knishio_graphql_response_free(plain_response);
    if (auth_result) knishio_request_profile_auth_token_result_free(auth_result);
    if (auth_result2) knishio_request_profile_auth_token_result_free(auth_result2);
    if (client) knishio_client_destroy(client);
    if (client2) knishio_client_destroy(client2);
    if (secret) knishio_free(secret);
    if (secret2) knishio_free(secret2);
    return rc;
}
