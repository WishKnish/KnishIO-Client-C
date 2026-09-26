/**
 * @file operations/auth.c
 * @brief Authentication and session operations implementation for KnishIO C SDK
 */

#include "knishio/knishio.h"
#include "knishio/operations/auth.h"
#include "knishio/graphql.h"
#include "knishio/json.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"
#include "knishio/fingerprint.h"
#include "knishio/auth_token.h"
#include "knishio/wallet.h"
#include "knishio/client_ops.h"
#include "knishio/client.h"
#include "knishio/molecule.h"
#include "knishio/operations/wallet.h"
#include "knishio/utils/logging.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Authentication GraphQL mutation templates */
static const char* ACCESS_TOKEN_MUTATION = 
    "mutation AccessToken($cellSlug: String, $pubkey: String, $encrypt: Boolean) {"
    "  AccessToken(cellSlug: $cellSlug, pubkey: $pubkey, encrypt: $encrypt) {"
    "    token"
    "    pubkey"
    "    expiresAt"
    "    encrypt"
    "    reason"
    "  }"
    "}";

static const char* REQUEST_AUTHORIZATION_MUTATION = 
    "mutation ProposeMolecule($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecularHash"
    "    status"
    "    reason"
    "    payload"
    "    createdAt"
    "  }"
    "}";

static const char* ACTIVE_SESSION_MUTATION = 
    "mutation ActiveSession("
    "$bundleHash: String!, $metaType: String!, $metaId: String!, "
    "$ipAddress: String, $browser: String, $osCpu: String, "
    "$resolution: String, $timeZone: String, $json: String"
    ") {"
    "  ActiveSession("
    "    bundleHash: $bundleHash, metaType: $metaType, metaId: $metaId, "
    "    ipAddress: $ipAddress, browser: $browser, osCpu: $osCpu, "
    "    resolution: $resolution, timeZone: $timeZone, json: $json"
    "  ) {"
    "    bundleHash"
    "    metaType"
    "    metaId"
    "    ipAddress"
    "    browser"
    "    osCpu"
    "    resolution"
    "    timeZone"
    "    status"
    "    createdAt"
    "  }"
    "}";

/* Internal helper functions */
static knishio_error_t knishio_parse_guest_auth_response(
    const char* response_data,
    knishio_request_guest_auth_token_result_t* result
);

static knishio_error_t knishio_parse_profile_auth_response(
    const char* response_data,
    knishio_request_profile_auth_token_result_t* result
);

/* PQ-transport Phase E: extract the validator's advertised ML-KEM pubkey
 * (data.ProposeMolecule.payload.key) from the profile-auth response. Malloc'd (caller frees), or NULL. */
static char* knishio_extract_server_pubkey(const char* response_data);

/* Request guest authentication token */
knishio_error_t knishio_client_request_guest_auth_token(
    knishio_client_t* client,
    const knishio_request_guest_auth_token_params_t* params,
    knishio_request_guest_auth_token_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!params->cell_slug) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Generate device fingerprint */
    char* fingerprint = NULL;
    knishio_error_t error = knishio_get_fingerprint(&fingerprint);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create guest wallet from fingerprint */
    knishio_wallet_t* guest_wallet = NULL;
    error = knishio_create_guest_wallet_from_fingerprint(fingerprint, &guest_wallet);
    if (error != KNISHIO_SUCCESS) {
        free(fingerprint);
        return error;
    }
    
    /* Get wallet public key */
    const char* pubkey = knishio_wallet_get_address(guest_wallet);
    if (!pubkey) {
        knishio_wallet_cleanup(guest_wallet);
        free(fingerprint);
        return KNISHIO_ERROR_WALLET_MISMATCH;
    }
    
    /* Build variables JSON for AccessToken mutation */
    knishio_json_builder_t* builder = knishio_json_builder_create();
    if (!builder) {
        knishio_wallet_cleanup(guest_wallet);
        free(fingerprint);
        return KNISHIO_ERROR_MEMORY;
    }
    
    error = knishio_json_builder_start_object(builder);
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(builder, "cellSlug", params->cell_slug);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(builder, "pubkey", pubkey);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_boolean(builder, "encrypt", params->encrypt);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_end_object(builder);
    }
    
    char* variables_json = NULL;
    if (error == KNISHIO_SUCCESS) {
        knishio_json_t* json_obj = knishio_json_builder_build(builder);
        if (json_obj) {
            error = knishio_json_to_string(json_obj, &variables_json);
            knishio_json_free(json_obj);
        } else {
            error = KNISHIO_ERROR_MEMORY;
        }
    }
    
    knishio_json_builder_free(builder);
    
    if (error != KNISHIO_SUCCESS) {
        knishio_wallet_cleanup(guest_wallet);
        free(fingerprint);
        return error;
    }
    
    /* Execute GraphQL mutation */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "AccessToken",
        .query = ACCESS_TOKEN_MUTATION,
        .variables_json = variables_json,
        .requires_auth = false,
        .is_mutation = true
    };
    
    error = knishio_graphql_execute(
        (knishio_graphql_client_t*)client,
        &operation,
        &response
    );
    
    free(variables_json);
    
    if (error != KNISHIO_SUCCESS) {
        knishio_wallet_cleanup(guest_wallet);
        free(fingerprint);
        return error;
    }
    
    /* Create result structure */
    knishio_request_guest_auth_token_result_t* auth_result = calloc(1, sizeof(knishio_request_guest_auth_token_result_t));
    if (!auth_result) {
        knishio_graphql_response_free(response);
        knishio_wallet_cleanup(guest_wallet);
        free(fingerprint);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        auth_result->success = true;
        auth_result->response = knishio_strdup(response->data);
        
        /* Parse authentication token from response */
        error = knishio_parse_guest_auth_response(response->data, auth_result);
    } else {
        auth_result->success = false;
        auth_result->error_message = knishio_strdup(response->errors ? response->errors : "Guest auth token request failed");
    }
    
    knishio_graphql_response_free(response);
    knishio_wallet_cleanup(guest_wallet);
    free(fingerprint);
    
    *result = auth_result;
    return KNISHIO_SUCCESS;
}

/* Validator reason for a ProposeMolecule outcome (data.ProposeMolecule.reason), else the GraphQL
 * errors. Malloc'd (caller frees), or NULL. */
static char* knishio_extract_propose_reason(const knishio_graphql_response_t* response) {
    char* reason = NULL;
    knishio_json_t* json = response->data ? knishio_json_parse(response->data, NULL) : NULL;
    if (json) {
        knishio_json_t* node = knishio_json_get_path(json, "data.ProposeMolecule.reason");
        if (node) {
            const char* s = knishio_json_get_string(node);
            if (s && s[0]) {
                reason = knishio_strdup(s);
            }
            knishio_json_free(node);
        }
        knishio_json_free(json);
    }
    if (!reason && response->errors) {
        reason = knishio_strdup(response->errors);
    }
    return reason;
}

/* Build, sign and propose one authorization molecule (mirrors JS Molecule.initAuthorization): a
 * U-atom signed by `source` (meta encrypt/pubkey/walletPubkey/characters) + a ContinuID I-atom to a
 * USER remainder at a FRESH random position, which designates the bundle's next chain head (JS
 * Wallet.generatePosition). The I-atom's previousPosition is source->position. */
static knishio_error_t knishio_propose_profile_auth(
    knishio_client_t* client,
    knishio_wallet_t* source,
    bool encrypt,
    knishio_graphql_response_t** response
) {
    knishio_wallet_t* remainder = NULL;
    char* remainder_position = NULL;
    knishio_molecule_t* molecule = NULL;
    char* molecule_json = NULL;
    char* variables = NULL;
    knishio_error_t error = KNISHIO_SUCCESS;

    if (!knishio_generate_position(&remainder_position)) {
        return KNISHIO_ERROR_CRYPTO;
    }
    error = knishio_wallet_create_simple(&remainder, source->secret, "USER", remainder_position);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    error = knishio_molecule_create(
        &molecule, source->secret, source->bundle_hash, source, remainder,
        knishio_client_get_cell_slug(client), "V4"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    error = knishio_molecule_init_authorization(molecule, encrypt);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    error = knishio_molecule_generate_hash(molecule);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    /* U-isotope is OTS-exempt at the validator, but JS signs (sets otsFragment) — harmless + faithful. */
    error = knishio_molecule_sign(molecule, source->bundle_hash, false, true);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    error = knishio_molecule_to_json(molecule, &molecule_json);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    {
        size_t var_len = strlen(molecule_json) + 32;
        variables = knishio_malloc(var_len);
        if (!variables) {
            error = KNISHIO_ERROR_MEMORY;
            goto cleanup;
        }
        snprintf(variables, var_len, "{\"molecule\":%s}", molecule_json);
    }

    {
        knishio_graphql_operation_t operation = {
            .name = "ProposeMolecule",
            .query = REQUEST_AUTHORIZATION_MUTATION,
            .variables_json = variables,
            .requires_auth = false,  /* U-isotope ProposeMolecule is PUBLIC */
            .is_mutation = true
        };
        /* Submit through a proper graphql client (slice 2a/2b-i: TLS + X-Auth-Token aware). */
        error = knishio_client_execute_graphql(client, &operation, response);
    }

cleanup:
    if (variables) knishio_free(variables);
    if (molecule_json) knishio_free(molecule_json);
    /* The molecule owns the U and I atoms init_authorization built; the wallets stay ours. */
    if (molecule) knishio_molecule_free_deep(molecule);
    if (remainder) knishio_wallet_free(remainder);
    if (remainder_position) knishio_free(remainder_position);
    return error;
}

/* The identity's ContinuID pointer as a signing wallet: the USER wallet derived from `secret` at
 * the position ContinuId(bundle, USER) returns. *pointer is NULL (no error) when there is no
 * pointer — genesis, an empty position, a non-USER wallet — or the validator's address differs
 * from the derived one. Query and transport errors propagate. */
static knishio_error_t knishio_resolve_continuid_signer(
    knishio_client_t* client,
    const char* secret,
    knishio_wallet_t** pointer
) {
    *pointer = NULL;
    const char* bundle = knishio_client_get_bundle(client);
    if (!bundle) {
        return KNISHIO_ERROR_INVALID_STATE;
    }

    /* Public query (requires_auth=false, CipherHash-bypassed on a client that has never
     * authenticated), sent with token USER: without the filter the validator falls back to the
     * bundle's newest wallet of ANY token when the bundle has no ContinuID meta. */
    knishio_continuId_result_t* cid = NULL;
    knishio_error_t error = knishio_client_query_continuId(client, bundle, &cid);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    if (!cid->success) {
        knishio_log(KNISHIO_LOG_WARN, "Profile auth: ContinuID query failed: %s",
                    cid->error_message ? cid->error_message : "unknown error");
        knishio_continuId_result_free(cid);
        return KNISHIO_ERROR_INVALID_RESPONSE;
    }

    const knishio_wallet_t* head = cid->wallet;
    if (head && head->token && strcmp(head->token, "USER") == 0
        && head->position && head->position[0]) {
        knishio_wallet_t* signer = NULL;
        error = knishio_wallet_create_simple(&signer, secret, "USER", head->position);
        if (error == KNISHIO_SUCCESS) {
            const knishio_mlkem_param_t param =
                (knishio_client_get_mlkem_parameter_set(client) == 768) ? KNISHIO_MLKEM_768 : KNISHIO_MLKEM_1024;
            if (knishio_wallet_get_mlkem_param(signer) != param
                && !knishio_wallet_set_mlkem_param(signer, param)) {
                error = KNISHIO_ERROR_CRYPTO;
            }
        }
        if (error != KNISHIO_SUCCESS) {
            if (signer) knishio_wallet_free(signer);
            knishio_continuId_result_free(cid);
            return error;
        }
        if (head->address && head->address[0] && strcmp(head->address, signer->address) != 0) {
            knishio_log(KNISHIO_LOG_INFO,
                        "Profile auth: ContinuID address differs from the secret's USER wallet; "
                        "signing from a fresh AUTH wallet");
            knishio_wallet_free(signer);
        } else {
            *pointer = signer;
        }
    }

    knishio_continuId_result_free(cid);
    return KNISHIO_SUCCESS;
}

/* Turn a ProposeMolecule response into the caller's result (no client side effects). */
static knishio_error_t knishio_build_profile_auth_result(
    const knishio_graphql_response_t* response,
    knishio_request_profile_auth_token_result_t** result
) {
    knishio_request_profile_auth_token_result_t* auth_result =
        calloc(1, sizeof(knishio_request_profile_auth_token_result_t));
    if (!auth_result) {
        return KNISHIO_ERROR_MEMORY;
    }
    if (response->data && response->success) {
        auth_result->success = true;
        auth_result->response = knishio_strdup(response->data);
        /* Extract the JWT (data.ProposeMolecule.payload.token). */
        knishio_parse_profile_auth_response(response->data, auth_result);
    } else {
        auth_result->success = false;
        auth_result->error_message = knishio_strdup(
            response->errors ? response->errors : "Profile auth token request failed"
        );
    }
    *result = auth_result;
    return KNISHIO_SUCCESS;
}

/* Bind an accepted authorization to the client: the auth token (pubkey = the signing wallet's
 * address), the validator's ML-KEM pubkey + the signing wallet (which decrypts CipherHash
 * responses; its own token and position are kept), and the requested encryption mode. */
static void knishio_bind_profile_auth(
    knishio_client_t* client,
    const knishio_request_profile_auth_token_params_t* params,
    const knishio_wallet_t* source,
    const knishio_graphql_response_t* response,
    const knishio_request_profile_auth_token_result_t* auth_result
) {
    /* Build + set the client auth token so subsequent ops carry X-Auth-Token. */
    if (auth_result->token) {
        knishio_auth_token_config_t token_config = {
            .token = auth_result->token,
            .expires_at = 0,
            .encrypt = params->encrypt,
            .pubkey = source->address
        };
        knishio_auth_token_t* auth_token = NULL;
        if (knishio_auth_token_create(&auth_token, &token_config) == KNISHIO_SUCCESS && auth_token) {
            knishio_client_set_auth_token(client, auth_token);
        }
    }

    char* server_pubkey = knishio_extract_server_pubkey(response->data);
    if (server_pubkey) {
        knishio_client_set_cipher_context(client, server_pubkey, source);
        knishio_free(server_pubkey);
    }
    knishio_client_set_encryption(client, params->encrypt);
}

/* Request profile authentication token.
 * Builds + signs a real U-isotope authorization molecule (mirrors JS requestProfileAuthToken /
 * Molecule.initAuthorization) and submits it via ProposeMolecule (PUBLIC). On acceptance, extracts
 * the bundle-scoped JWT (data.ProposeMolecule.payload.token) and sets it as the client auth token
 * so subsequent ops carry X-Auth-Token.
 *
 * Signer (WOTS+ mitigation §12.5): a returning identity signs from its ContinuID pointer — the USER
 * wallet at the position ContinuId(bundle, USER) returns — so validator 0.5.0+ marks the token
 * proven (atoms[0] at the pointer, registered there) and moves the pointer to the I-atom's fresh
 * position. With no pointer, a non-USER pointer or an address mismatch, it signs from a fresh AUTH
 * wallet at a random position: U-isotope skips the ContinuID chain check, so any unused position is
 * valid (JS `new Wallet({secret, token:'AUTH'})`), and a fixed one would be rejected for OTS reuse
 * on the second login. A rejected pointer-signed proposal falls back to that AUTH login ONCE, so a
 * login sends at most two authorization molecules (testnet: 3 auths/min/IP). */
knishio_error_t knishio_client_request_profile_auth_token(
    knishio_client_t* client,
    const knishio_request_profile_auth_token_params_t* params,
    knishio_request_profile_auth_token_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!params->secret) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* get_source_wallet derives the wallet from the client's stored secret. */
    knishio_client_set_secret(client, params->secret);

    knishio_wallet_t* source = NULL;     /* signing wallet: ContinuID pointer (USER) or fresh AUTH */
    char* source_position = NULL;
    knishio_graphql_response_t* response = NULL;
    knishio_request_profile_auth_token_result_t* auth_result = NULL;

    knishio_error_t error = knishio_resolve_continuid_signer(client, params->secret, &source);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    if (source) {
        error = knishio_propose_profile_auth(client, source, params->encrypt, &response);
        if (error != KNISHIO_SUCCESS) {
            goto cleanup;
        }
        error = knishio_build_profile_auth_result(response, &auth_result);
        if (error != KNISHIO_SUCCESS) {
            goto cleanup;
        }
        if (auth_result->success && auth_result->token) {
            knishio_log(KNISHIO_LOG_INFO, "Profile auth: token issued for the ContinuID pointer-signed login");
            knishio_bind_profile_auth(client, params, source, response, auth_result);
            *result = auth_result;
            auth_result = NULL;
            goto cleanup;
        }

        char* reason = knishio_extract_propose_reason(response);
        knishio_log(KNISHIO_LOG_WARN,
                    "Profile auth: ContinuID pointer-signed login rejected (%s); retrying once from a fresh AUTH wallet",
                    reason ? reason : "no reason given");
        if (reason) knishio_free(reason);
        knishio_request_profile_auth_token_result_free(auth_result);
        auth_result = NULL;
        knishio_graphql_response_free(response);
        response = NULL;
        knishio_wallet_free(source);
        source = NULL;
    }

    /* AUTH signer at a FRESH random position (first login, or the one fallback). */
    if (!knishio_generate_position(&source_position)) {
        error = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }
    error = knishio_wallet_create_simple(&source, params->secret, "AUTH", source_position);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    error = knishio_propose_profile_auth(client, source, params->encrypt, &response);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    error = knishio_build_profile_auth_result(response, &auth_result);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    if (auth_result->success) {
        knishio_log(KNISHIO_LOG_INFO, "Profile auth: token issued for the AUTH-wallet login");
        knishio_bind_profile_auth(client, params, source, response, auth_result);
    }
    *result = auth_result;
    auth_result = NULL;

cleanup:
    if (response) knishio_graphql_response_free(response);
    if (source) knishio_wallet_free(source);
    if (source_position) knishio_free(source_position);
    if (auth_result) knishio_request_profile_auth_token_result_free(auth_result);
    return error;
}

/* Declare active session */
knishio_error_t knishio_client_active_session(
    knishio_client_t* client,
    const knishio_active_session_params_t* params,
    knishio_active_session_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!params->bundle || !params->meta_type || !params->meta_id) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables JSON */
    knishio_json_builder_t* builder = knishio_json_builder_create();
    if (!builder) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    knishio_error_t error = knishio_json_builder_start_object(builder);
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(builder, "bundleHash", params->bundle);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(builder, "metaType", params->meta_type);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(builder, "metaId", params->meta_id);
    }
    
    /* Add optional parameters */
    if (error == KNISHIO_SUCCESS && params->ip_address) {
        error = knishio_json_builder_add_string(builder, "ipAddress", params->ip_address);
    }
    if (error == KNISHIO_SUCCESS && params->browser) {
        error = knishio_json_builder_add_string(builder, "browser", params->browser);
    }
    if (error == KNISHIO_SUCCESS && params->os_cpu) {
        error = knishio_json_builder_add_string(builder, "osCpu", params->os_cpu);
    }
    if (error == KNISHIO_SUCCESS && params->resolution) {
        error = knishio_json_builder_add_string(builder, "resolution", params->resolution);
    }
    if (error == KNISHIO_SUCCESS && params->time_zone) {
        error = knishio_json_builder_add_string(builder, "timeZone", params->time_zone);
    }
    if (error == KNISHIO_SUCCESS && params->json_data) {
        error = knishio_json_builder_add_string(builder, "json", params->json_data);
    }
    
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_end_object(builder);
    }
    
    char* variables_json = NULL;
    if (error == KNISHIO_SUCCESS) {
        knishio_json_t* json_obj = knishio_json_builder_build(builder);
        if (json_obj) {
            error = knishio_json_to_string(json_obj, &variables_json);
            knishio_json_free(json_obj);
        } else {
            error = KNISHIO_ERROR_MEMORY;
        }
    }
    
    knishio_json_builder_free(builder);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Execute GraphQL mutation */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "ActiveSession",
        .query = ACTIVE_SESSION_MUTATION,
        .variables_json = variables_json,
        .requires_auth = true,
        .is_mutation = true
    };
    
    error = knishio_graphql_execute(
        (knishio_graphql_client_t*)client,
        &operation,
        &response
    );
    
    free(variables_json);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result structure */
    knishio_active_session_result_t* session_result = calloc(1, sizeof(knishio_active_session_result_t));
    if (!session_result) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        session_result->success = true;
        session_result->response = knishio_strdup(response->data);
    } else {
        session_result->success = false;
        session_result->error_message = knishio_strdup(response->errors ? response->errors : "Active session declaration failed");
    }
    
    knishio_graphql_response_free(response);
    *result = session_result;
    return KNISHIO_SUCCESS;
}

/* Unsubscribe from WebSocket subscription */
knishio_error_t knishio_client_unsubscribe(
    knishio_client_t* client,
    const char* operation_name
) {
    if (!client || !operation_name) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* TODO: Implement WebSocket unsubscription */
    /* For now, return success as placeholder */
    return KNISHIO_SUCCESS;
}

/* Unsubscribe from all WebSocket subscriptions */
knishio_error_t knishio_client_unsubscribe_all(
    knishio_client_t* client
) {
    if (!client) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* TODO: Implement WebSocket unsubscription for all */
    /* For now, return success as placeholder */
    return KNISHIO_SUCCESS;
}

/* Internal helper functions */

static knishio_error_t knishio_parse_guest_auth_response(
    const char* response_data,
    knishio_request_guest_auth_token_result_t* result
) {
    if (!response_data || !result) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Parse JSON response */
    char* error_msg = NULL;
    knishio_json_t* json = knishio_json_parse(response_data, &error_msg);
    if (!json) {
        if (error_msg) {
            knishio_free(error_msg);
        }
        return KNISHIO_ERROR_JSON_PARSE;
    }
    
    /* Extract token from response */
    const char* token = knishio_json_get_string_path(json, "data.AccessToken.token");
    if (token) {
        result->token = knishio_strdup(token);
    }
    
    knishio_json_free(json);
    return KNISHIO_SUCCESS;
}

static knishio_error_t knishio_parse_profile_auth_response(
    const char* response_data,
    knishio_request_profile_auth_token_result_t* result
) {
    if (!response_data || !result) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Parse JSON response */
    char* error_msg = NULL;
    knishio_json_t* json = knishio_json_parse(response_data, &error_msg);
    if (!json) {
        if (error_msg) {
            knishio_free(error_msg);
        }
        return KNISHIO_ERROR_JSON_PARSE;
    }

    /* Extract data.ProposeMolecule.payload (a stringified JSON), then payload.token.
     * NOTE: knishio_json_get_string_path is use-after-free (it frees the node before returning
     * the borrowed string -> empty/garbage), so navigate with get_path + get_string and COPY
     * the string out BEFORE freeing the node. */
    knishio_json_t* payload_node = knishio_json_get_path(json, "data.ProposeMolecule.payload");
    if (payload_node) {
        const char* payload_str = knishio_json_get_string(payload_node);
        char* payload_copy = payload_str ? knishio_strdup(payload_str) : NULL;
        knishio_json_free(payload_node);

        if (payload_copy) {
            knishio_json_t* payload_json = knishio_json_parse(payload_copy, NULL);
            if (payload_json) {
                knishio_json_t* token_node = knishio_json_get_path(payload_json, "token");
                if (token_node) {
                    const char* token = knishio_json_get_string(token_node);
                    if (token) {
                        result->token = knishio_strdup(token);
                    }
                    knishio_json_free(token_node);
                }
                knishio_json_free(payload_json);
            }
            knishio_free(payload_copy);
        }
    }

    knishio_json_free(json);
    return KNISHIO_SUCCESS;
}

/* PQ-transport Phase E: extract the validator's advertised ML-KEM pubkey from the profile-auth
 * response (data.ProposeMolecule.payload.key). Mirrors knishio_parse_profile_auth_response's
 * navigation (copy borrowed strings out BEFORE freeing the node — get_string_path is use-after-free). */
static char* knishio_extract_server_pubkey(const char* response_data) {
    if (!response_data) {
        return NULL;
    }
    char* result = NULL;
    knishio_json_t* json = knishio_json_parse(response_data, NULL);
    if (!json) {
        return NULL;
    }
    knishio_json_t* payload_node = knishio_json_get_path(json, "data.ProposeMolecule.payload");
    if (payload_node) {
        const char* payload_str = knishio_json_get_string(payload_node);
        char* payload_copy = payload_str ? knishio_strdup(payload_str) : NULL;
        knishio_json_free(payload_node);
        if (payload_copy) {
            knishio_json_t* payload_json = knishio_json_parse(payload_copy, NULL);
            if (payload_json) {
                knishio_json_t* key_node = knishio_json_get_path(payload_json, "key");
                if (key_node) {
                    const char* key = knishio_json_get_string(key_node);
                    if (key) {
                        result = knishio_strdup(key);
                    }
                    knishio_json_free(key_node);
                }
                knishio_json_free(payload_json);
            }
            knishio_free(payload_copy);
        }
    }
    knishio_json_free(json);
    return result;
}

/* Free guest auth token result */
void knishio_request_guest_auth_token_result_free(knishio_request_guest_auth_token_result_t* result) {
    if (!result) return;
    
    if (result->token) knishio_free(result->token);
    if (result->response) knishio_free(result->response);
    if (result->error_message) knishio_free(result->error_message);
    knishio_free(result);
}

/* Free profile auth token result */
void knishio_request_profile_auth_token_result_free(knishio_request_profile_auth_token_result_t* result) {
    if (!result) return;
    
    if (result->token) knishio_free(result->token);
    if (result->response) knishio_free(result->response);
    if (result->error_message) knishio_free(result->error_message);
    knishio_free(result);
}

/* Free active session result */
void knishio_active_session_result_free(knishio_active_session_result_t* result) {
    if (!result) return;
    
    if (result->response) knishio_free(result->response);
    if (result->error_message) knishio_free(result->error_message);
    knishio_free(result);
}