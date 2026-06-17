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

/* Request profile authentication token.
 * Builds + signs a real U-isotope authorization molecule (mirrors JS requestProfileAuthToken /
 * Molecule.initAuthorization): U-atom (AUTH wallet, meta encrypt/pubkey/characters) + ContinuID
 * I-atom, submitted via ProposeMolecule (PUBLIC). On acceptance, extracts the bundle-scoped JWT
 * (data.ProposeMolecule.payload.token) and sets it as the client auth token so subsequent ops
 * carry X-Auth-Token. (Replaces the prior broken M-isotope/unsigned/no-hash hand-roll.) */
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

    knishio_wallet_t* source = NULL;     /* AUTH signing wallet (address = pubkey) */
    knishio_wallet_t* remainder = NULL;  /* USER remainder (ContinuID I-atom) */
    char* source_position = NULL;
    char* remainder_position = NULL;
    knishio_molecule_t* molecule = NULL;
    char* molecule_json = NULL;
    char* variables = NULL;
    knishio_graphql_response_t* response = NULL;
    knishio_request_profile_auth_token_result_t* auth_result = NULL;

    /* U-source (AUTH) at a FRESH random position. The U-atom is the index-0 OTS signer, so a fixed
     * position is consumed (used_positions) and a 2nd auth from the same bundle would be rejected
     * for OTS reuse. U-isotope SKIPS the ContinuID chain check, so any unused position is valid —
     * mirrors JS `new Wallet({secret, token:'AUTH'})`, which uses a random position. */
    knishio_error_t error = KNISHIO_SUCCESS;
    if (!knishio_generate_position(&source_position)) {
        return KNISHIO_ERROR_CRYPTO;
    }
    error = knishio_wallet_create_simple(&source, params->secret, "AUTH", source_position);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* USER remainder (ContinuID I-atom) at a FRESH random position — designates the bundle's next
     * chain head (mirrors JS Wallet.generatePosition). */
    if (!knishio_generate_position(&remainder_position)) {
        error = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }
    error = knishio_wallet_create_simple(&remainder, params->secret, "USER", remainder_position);
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

    error = knishio_molecule_init_authorization(molecule, params->encrypt);
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
        error = knishio_client_execute_graphql(client, &operation, &response);
    }
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    auth_result = calloc(1, sizeof(knishio_request_profile_auth_token_result_t));
    if (!auth_result) {
        error = KNISHIO_ERROR_MEMORY;
        goto cleanup;
    }

    if (response->data && response->success) {
        auth_result->success = true;
        auth_result->response = knishio_strdup(response->data);

        /* Extract the JWT (data.ProposeMolecule.payload.token). */
        knishio_parse_profile_auth_response(response->data, auth_result);

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
    } else {
        auth_result->success = false;
        auth_result->error_message = knishio_strdup(
            response->errors ? response->errors : "Profile auth token request failed"
        );
    }

    *result = auth_result;
    auth_result = NULL;

cleanup:
    if (response) knishio_graphql_response_free(response);
    if (variables) knishio_free(variables);
    if (molecule_json) knishio_free(molecule_json);
    if (molecule) knishio_molecule_free(molecule);
    if (source) knishio_wallet_free(source);
    if (remainder) knishio_wallet_free(remainder);
    if (source_position) knishio_free(source_position);
    if (remainder_position) knishio_free(remainder_position);
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