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

/* Request profile authentication token */
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
    
    /* Create wallet for authorization */
    knishio_wallet_t* auth_wallet = NULL;
    bool success = knishio_wallet_create(&auth_wallet, params->secret, "AUTH", NULL);
    knishio_error_t error = success ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Build molecule for authorization request */
    knishio_json_builder_t* molecule_builder = knishio_json_builder_create();
    if (!molecule_builder) {
        knishio_wallet_cleanup(auth_wallet);
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Create molecule structure */
    error = knishio_json_builder_start_object(molecule_builder);
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(molecule_builder, "cellSlug", "");
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(molecule_builder, "bundleHash", auth_wallet->bundle_hash);
    }
    /* TODO: Build atoms array properly - JSON builder doesn't support arrays yet */
    char atoms_json[2048];
    snprintf(atoms_json, sizeof(atoms_json), 
        "[{\"position\":\"%s\",\"walletAddress\":\"%s\",\"isotope\":\"M\","
        "\"token\":\"AUTH\",\"value\":null,\"metaType\":\"walletBundle\","
        "\"metaId\":\"%s\"}]",
        auth_wallet->position ? auth_wallet->position : "",
        auth_wallet->address ? auth_wallet->address : "",
        auth_wallet->bundle_hash ? auth_wallet->bundle_hash : "");
    
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_raw(molecule_builder, "atoms", atoms_json);
    }
    
    /* Add metadata as raw JSON */
    if (error == KNISHIO_SUCCESS) {
        char meta_json[512];
        snprintf(meta_json, sizeof(meta_json),
            "{\"encrypt\":%s}",
            params->encrypt ? "true" : "false");
        error = knishio_json_builder_add_raw(molecule_builder, "meta", meta_json);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_end_object(molecule_builder); /* end molecule */
    }
    
    char* molecule_json = NULL;
    if (error == KNISHIO_SUCCESS) {
        knishio_json_t* json_obj = knishio_json_builder_build(molecule_builder);
        if (json_obj) {
            error = knishio_json_to_string(json_obj, &molecule_json);
            knishio_json_free(json_obj);
        } else {
            error = KNISHIO_ERROR_MEMORY;
        }
    }
    
    knishio_json_builder_free(molecule_builder);
    
    if (error != KNISHIO_SUCCESS) {
        knishio_wallet_cleanup(auth_wallet);
        return error;
    }
    
    /* Build variables JSON */
    knishio_json_builder_t* variables_builder = knishio_json_builder_create();
    if (!variables_builder) {
        free(molecule_json);
        knishio_wallet_cleanup(auth_wallet);
        return KNISHIO_ERROR_MEMORY;
    }
    
    error = knishio_json_builder_start_object(variables_builder);
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_raw(variables_builder, "molecule", molecule_json);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_end_object(variables_builder);
    }
    
    char* variables_json = NULL;
    if (error == KNISHIO_SUCCESS) {
        knishio_json_t* json_obj = knishio_json_builder_build(variables_builder);
        if (json_obj) {
            error = knishio_json_to_string(json_obj, &variables_json);
            knishio_json_free(json_obj);
        } else {
            error = KNISHIO_ERROR_MEMORY;
        }
    }
    
    knishio_json_builder_free(variables_builder);
    free(molecule_json);
    
    if (error != KNISHIO_SUCCESS) {
        knishio_wallet_cleanup(auth_wallet);
        return error;
    }
    
    /* Execute GraphQL mutation */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "ProposeMolecule",
        .query = REQUEST_AUTHORIZATION_MUTATION,
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
        knishio_wallet_cleanup(auth_wallet);
        return error;
    }
    
    /* Create result structure */
    knishio_request_profile_auth_token_result_t* auth_result = calloc(1, sizeof(knishio_request_profile_auth_token_result_t));
    if (!auth_result) {
        knishio_graphql_response_free(response);
        knishio_wallet_cleanup(auth_wallet);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        auth_result->success = true;
        auth_result->response = knishio_strdup(response->data);
        
        /* Parse authentication token from response */
        error = knishio_parse_profile_auth_response(response->data, auth_result);
    } else {
        auth_result->success = false;
        auth_result->error_message = knishio_strdup(response->errors ? response->errors : "Profile auth token request failed");
    }
    
    knishio_graphql_response_free(response);
    knishio_wallet_cleanup(auth_wallet);
    
    *result = auth_result;
    return KNISHIO_SUCCESS;
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
    
    /* Extract token from payload */
    const char* payload = knishio_json_get_string_path(json, "data.ProposeMolecule.payload");
    if (payload) {
        /* Parse payload as JSON to extract token */
        knishio_json_t* payload_json = knishio_json_parse(payload, NULL);
        if (payload_json) {
            const char* token = knishio_json_get_string_path(payload_json, "token");
            if (token) {
                result->token = knishio_strdup(token);
            }
            knishio_json_free(payload_json);
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