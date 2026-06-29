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
 * Gated on CIPHERHASH_TEST_URL (skips cleanly when unset → CI-safe). Run live:
 *   CIPHERHASH_TEST_URL=http://localhost:8081/graphql ./build/tests/cipherhash_live_test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "knishio/knishio.h"
#include "knishio/client_ops.h"
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
        return 0;
    }

    int rc = 1;
    char* secret = NULL;
    knishio_client_t* client = NULL;
    knishio_request_profile_auth_token_result_t* auth_result = NULL;
    knishio_wallet_t* enc_wallet = NULL;
    knishio_wallet_t* plain_wallet = NULL;

    if (!knishio_generate_secret("phase-e-live-c-cipherhash-secret-0123456789", 2048, &secret)) {
        fprintf(stderr, "FAIL: could not generate secret\n");
        goto cleanup;
    }

    knishio_client_config_t config = {
        .uri = url,
        .cell_slug = "public",   /* the active dev cell (TESTCELL is inactive there) */
        .client = NULL,
        .socket = NULL,
        .server_sdk_version = 3,
        .logging = false,
        .insecure_tls = false
    };
    if (knishio_client_create(&client, &config) != KNISHIO_SUCCESS || !client) {
        fprintf(stderr, "FAIL: could not create client\n");
        goto cleanup;
    }

    /* ONE authenticated session (encrypt=true → conveys the AUTH wallet's ML-KEM pubkey as a signed
     * walletPubkey U-atom meta, so the validator can encrypt responses back to it). We then vary
     * ONLY the transport on this SAME session — the queried balance wallet stays fixed. */
    knishio_request_profile_auth_token_params_t params = { .secret = secret, .encrypt = true };
    knishio_error_t aerr = knishio_client_request_profile_auth_token(client, &params, &auth_result);
    if (aerr != KNISHIO_SUCCESS || !auth_result || !auth_result->success) {
        fprintf(stderr, "FAIL: authentication failed%s%s\n",
                (auth_result && auth_result->error_message) ? ": " : "",
                (auth_result && auth_result->error_message) ? auth_result->error_message : "");
        goto cleanup;
    }

    /* Encrypted round-trip: the validator ML-KEM-decrypts the request, executes it, and encrypts
     * the response back to the client's ML-KEM pubkey; the client decrypts it. */
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
    rc = 0;

cleanup:
    if (enc_wallet) knishio_wallet_free(enc_wallet);
    if (plain_wallet) knishio_wallet_free(plain_wallet);
    if (auth_result) knishio_request_profile_auth_token_result_free(auth_result);
    if (client) knishio_client_destroy(client);
    if (secret) knishio_free(secret);
    return rc;
}
