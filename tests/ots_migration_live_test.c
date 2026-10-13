/*
 * Live OTS version migration of one identity against a running validator.
 *
 * One fresh secret: a client at otsVersion 1 signs in (its ContinuID head is v1); a client at
 * otsVersion 2 signs in with the same secret and moves the head to v2; that client's createMeta
 * keeps it at v2; a later client at otsVersion 1 signs in and the head stays at v2.
 *
 * Gated on OTS_MIGRATION_TEST_URL: unset, it exits 77, which ctest reports as Skipped
 * (SKIP_RETURN_CODE in CMakeLists.txt) rather than Passed. Run live:
 *   OTS_MIGRATION_TEST_URL=http://localhost:8081/graphql ./build/ots_migration_live_test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "knishio/knishio.h"
#include "knishio/client_ops.h"
#include "knishio/meta.h"
#include "knishio/operations/auth.h"
#include "knishio/operations/wallet.h"
#include "knishio/wallet.h"
#include "knishio/crypto/shake256.h"
#include "knishio/utils/memory.h"

typedef struct {
    char position[65];
    int version; /* the OTS version the head's address was derived at; 0 when neither */
} head_t;

/* The bundle's USER ContinuID head and the OTS version its address was derived at. */
static int read_head(knishio_client_t* client, const char* secret, const char* bundle, head_t* head) {
    knishio_continuId_result_t* cid = NULL;
    int ok = knishio_client_query_continuId(client, bundle, &cid) == KNISHIO_SUCCESS && cid && cid->success
        && cid->wallet && cid->wallet->position && strlen(cid->wallet->position) == 64
        && cid->wallet->address && cid->wallet->address[0];
    if (ok) {
        snprintf(head->position, sizeof(head->position), "%s", cid->wallet->position);
        head->version = knishio_detect_ots_version(secret, "USER", cid->wallet->position, cid->wallet->address, 1);
    }
    if (cid) knishio_continuId_result_free(cid);
    return ok;
}

/* A client at `ots_version` that signs in with `secret`; NULL (and a FAIL line) on failure. */
static knishio_client_t* sign_in(const char* url, const char* secret, int ots_version, const char* step) {
    knishio_client_config_t config = {
        .uri = url,
        .cell_slug = "public",
        .client = NULL,
        .socket = NULL,
        .server_sdk_version = 3,
        .logging = false,
        .mlkem_parameter_set = 1024,
        .ots_version = ots_version
    };
    knishio_client_t* client = NULL;
    if (knishio_client_create(&client, &config) != KNISHIO_SUCCESS || !client) {
        printf("FAIL: %s could not create a client at otsVersion %d\n", step, ots_version);
        return NULL;
    }
    knishio_request_profile_auth_token_params_t params = { .secret = secret, .encrypt = false };
    knishio_request_profile_auth_token_result_t* result = NULL;
    knishio_error_t err = knishio_client_request_profile_auth_token(client, &params, &result);
    if (err != KNISHIO_SUCCESS || !result || !result->success) {
        printf("FAIL: %s sign-in at otsVersion %d failed (err=%d)%s%s\n", step, ots_version, (int)err,
               (result && result->error_message) ? ": " : "",
               (result && result->error_message) ? result->error_message : "");
        if (result) knishio_request_profile_auth_token_result_free(result);
        knishio_client_destroy(client);
        return NULL;
    }
    knishio_request_profile_auth_token_result_free(result);
    return client;
}

int main(void) {
    const char* url = getenv("OTS_MIGRATION_TEST_URL");
    if (url == NULL || url[0] == '\0') {
        printf("SKIP: OTS_MIGRATION_TEST_URL not set — skipping live OTS migration test\n");
        return 77; /* ctest SKIP_RETURN_CODE: reported as Skipped, never as Passed */
    }

    int rc = 1;
    char* secret = NULL;
    char* bundle = NULL;
    char* hash = NULL;
    knishio_client_t* client = NULL;
    head_t h1 = {{0}, 0}, h2 = {{0}, 0}, h3 = {{0}, 0}, h4 = {{0}, 0};

    /* 1. A fresh secret (2048 hex characters) and its bundle. */
    char seed[128];
    snprintf(seed, sizeof(seed), "ots-migration|%ld|%ld", (long)time(NULL), (long)getpid());
    if (!knishio_shake256_hash(seed, 8192, &secret) || !secret || strlen(secret) != 2048
        || !knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)) {
        printf("FAIL: 1 could not generate a secret and its bundle\n");
        goto cleanup;
    }

    /* 2. A v1 client signs in: the head is v1. */
    client = sign_in(url, secret, 1, "2");
    if (!client) goto cleanup;
    if (!read_head(client, secret, bundle, &h1) || h1.version != 1) {
        printf("FAIL: 2 head after a v1 sign-in is not v1 (version %d)\n", h1.version);
        goto cleanup;
    }
    printf("PASS: 2 v1 sign-in, head %s at v1\n", h1.position);
    knishio_client_destroy(client);

    /* 3. A v2 client signs in with the same secret: the head moves and is v2. */
    client = sign_in(url, secret, 2, "3");
    if (!client) goto cleanup;
    if (!read_head(client, secret, bundle, &h2) || strcmp(h2.position, h1.position) == 0 || h2.version != 2) {
        printf("FAIL: 3 head after a v2 sign-in: position %s (was %s), version %d\n", h2.position, h1.position,
               h2.version);
        goto cleanup;
    }
    printf("PASS: 3 v2 sign-in, head %s at v2\n", h2.position);

    /* 4. The v2 client's createMeta is accepted and keeps the head at v2. */
    {
        char meta_id[96], key[] = "step", value[] = "4";
        snprintf(meta_id, sizeof(meta_id), "c-%.32s-%ld", bundle, (long)time(NULL));
        knishio_meta_t pair = { .key = key, .value = value };
        knishio_meta_t* pairs[] = { &pair };
        knishio_error_t err = knishio_client_create_meta(client, "ots-migration", meta_id, pairs, 1, &hash);
        if (err != KNISHIO_SUCCESS || !hash) {
            printf("FAIL: 4 createMeta was not accepted (err=%d)\n", (int)err);
            goto cleanup;
        }
    }
    if (!read_head(client, secret, bundle, &h3) || strcmp(h3.position, h2.position) == 0 || h3.version != 2) {
        printf("FAIL: 4 head after createMeta: position %s (was %s), version %d\n", h3.position, h2.position,
               h3.version);
        goto cleanup;
    }
    printf("PASS: 4 v2 createMeta accepted, head %s at v2\n", h3.position);
    knishio_client_destroy(client);

    /* 5. A later v1 client signs in: the head moves and stays v2. */
    client = sign_in(url, secret, 1, "5");
    if (!client) goto cleanup;
    if (!read_head(client, secret, bundle, &h4) || strcmp(h4.position, h3.position) == 0 || h4.version != 2) {
        printf("FAIL: 5 head after a v1 sign-in: position %s (was %s), version %d\n", h4.position, h3.position,
               h4.version);
        goto cleanup;
    }
    printf("PASS: 5 v1 sign-in, head %s stays at v2\n", h4.position);
    rc = 0;

cleanup:
    if (client) knishio_client_destroy(client);
    if (hash) knishio_free(hash);
    if (bundle) knishio_free(bundle);
    if (secret) {
        knishio_secure_zero(secret, strlen(secret));
        knishio_free(secret);
    }
    return rc;
}
