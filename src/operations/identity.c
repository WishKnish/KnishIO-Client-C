/**
 * @file operations/identity.c
 * @brief Identity operations implementation for KnishIO C SDK
 */

#include "knishio/knishio.h"
#include "knishio/operations/identity.h"
#include "knishio/operations/wallet.h"
#include "knishio/graphql.h"
#include "knishio/json.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* CreateIdentifier GraphQL mutation template */
static const char* CREATE_IDENTIFIER_MUTATION = 
    "mutation CreateIdentifier($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecular_hash"
    "    status"
    "    reason"
    "    payload"
    "    createdAt"
    "  }"
    "}";

/* ClaimShadowWallet GraphQL mutation template */
static const char* CLAIM_SHADOW_WALLET_MUTATION =
    "mutation ClaimShadowWallet($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecularHash"
    "    status"
    "    reason"
    "    payload"
    "    createdAt"
    "  }"
    "}";

/* Create identity identifier */
knishio_error_t knishio_client_create_identifier(
    knishio_client_t* client,
    const knishio_create_identifier_params_t* params,
    knishio_create_identifier_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!params->type || !params->contact || !params->code) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build molecule JSON for identifier creation */
    char molecule_json[2048];
    snprintf(molecule_json, sizeof(molecule_json),
        "{"
        "\"secret\":\"%s\","
        "\"cellSlug\":null,"
        "\"atoms\":["
        "{"
        "\"position\":\"0000000000000000000000000000000000000000000000000000000000000000\","
        "\"walletAddress\":\"0000000000000000000000000000000000000000000000000000000000000000\","
        "\"isotope\":\"I\","
        "\"token\":\"USER\","
        "\"value\":null,"
        "\"metaType\":\"identifier\","
        "\"metaId\":\"%s\","
        "\"meta\":{\"type\":\"%s\",\"contact\":\"%s\",\"code\":\"%s\"}"
        "}"
        "]"
        "}",
        "placeholder_secret", /* TODO: Get from client */
        params->contact,
        params->type,
        params->contact,
        params->code
    );
    
    /* Build variables JSON */
    char variables[4096];
    snprintf(variables, sizeof(variables), "{\"molecule\":%s}", molecule_json);
    
    /* Execute GraphQL mutation */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "CreateIdentifier",
        .query = CREATE_IDENTIFIER_MUTATION,
        .variables_json = variables,
        .requires_auth = true,
        .is_mutation = true
    };
    
    knishio_error_t error = knishio_graphql_execute(
        (knishio_graphql_client_t*)client,
        &operation,
        &response
    );
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result structure */
    knishio_create_identifier_result_t* identifier_result = calloc(1, sizeof(knishio_create_identifier_result_t));
    if (!identifier_result) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        identifier_result->success = true;
        identifier_result->response = knishio_strdup(response->data);
        
        /* Parse molecular hash from GraphQL response following 2025 C17 best practices */
        knishio_json_t* json_root = knishio_json_parse(response->data, NULL);
        if (json_root) {
            const char* molecular_hash = knishio_json_get_string_path(json_root, "data.ProposeMolecule.molecular_hash");
            if (molecular_hash && strlen(molecular_hash) > 0) {
                identifier_result->molecular_hash = knishio_strdup(molecular_hash);
            } else {
                identifier_result->molecular_hash = knishio_strdup("placeholder_hash");
            }
            knishio_json_free(json_root);
        } else {
            identifier_result->molecular_hash = knishio_strdup("placeholder_hash");
        }
    } else {
        identifier_result->success = false;
        identifier_result->error_message = knishio_strdup(response->errors ? response->errors : "Identifier creation failed");
    }
    
    knishio_graphql_response_free(response);
    *result = identifier_result;
    return KNISHIO_SUCCESS;
}

/* Claim individual shadow wallet */
knishio_error_t knishio_client_claim_shadow_wallet(
    knishio_client_t* client,
    const knishio_claim_shadow_wallet_params_t* params,
    knishio_claim_shadow_wallet_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!params->token) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Cycle 39 (slice 1): build the PARITY-CORRECT shadow-wallet-claim molecule (C-atom 'wallet'
     * with meta [shadowWalletClaim, then the 7 prefixed wallet* keys] + a ContinuID I-atom) via
     * knishio_molecule_init_shadow_wallet_claim, signed, on a REAL client-secret-derived source
     * wallet. Live submission + auth = next slice. Replaces the prior hand-crafted molecule-JSON
     * string (placeholder secret + zeroed positions + divergent shape). */
    knishio_wallet_t* source = NULL;
    knishio_wallet_t* claim_wallet = NULL;
    knishio_wallet_t* remainder = NULL;
    knishio_molecule_t* molecule = NULL;
    char* variables = NULL;
    knishio_graphql_response_t* response = NULL;
    knishio_claim_shadow_wallet_result_t* claim_result = NULL;

    knishio_error_t error = knishio_client_get_source_wallet(
        (knishio_client_t*)client, "USER", &source
    );
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* The shadow wallet being claimed (caller's token; canonical claim position for this slice),
     * from the source secret. Carry the caller's batch_id so it rides on walletBatchId. */
    error = knishio_wallet_create_simple(
        &claim_wallet, source->secret, params->token,
        "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    if (params->batch_id && !claim_wallet->batch_id) {
        claim_wallet->batch_id = knishio_strdup(params->batch_id);
    }

    error = knishio_wallet_create_simple(
        &remainder, source->secret, "USER",
        "bbbb000000000000cccc111111111111dddd222222222222eeee333333333333"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    error = knishio_molecule_create(
        &molecule, source->secret, source->bundle_hash, source, remainder, NULL, "V4"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    error = knishio_molecule_init_shadow_wallet_claim(molecule, claim_wallet);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Hash + sign (real timestamps — live op). */
    error = knishio_molecule_generate_hash(molecule);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    error = knishio_molecule_sign(molecule, source->bundle_hash, false, true);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Serialize + submit (transport/auth unverified — next slice). */
    {
        char* molecule_json = NULL;
        error = knishio_molecule_to_json(molecule, &molecule_json);
        if (error != KNISHIO_SUCCESS) {
            goto cleanup;
        }
        size_t var_len = strlen(molecule_json) + 32;
        variables = knishio_malloc(var_len);
        if (!variables) {
            knishio_free(molecule_json);
            error = KNISHIO_ERROR_MEMORY;
            goto cleanup;
        }
        snprintf(variables, var_len, "{\"molecule\":%s}", molecule_json);
        knishio_free(molecule_json);
    }

    {
        knishio_graphql_operation_t operation = {
            .name = "ClaimShadowWallet",
            .query = CLAIM_SHADOW_WALLET_MUTATION,
            .variables_json = variables,
            .requires_auth = true,
            .is_mutation = true
        };
        /* Submit through a proper graphql client (auth token propagates), not the old cast. */
        error = knishio_client_execute_graphql(client, &operation, &response);
    }
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Create result structure */
    claim_result = knishio_calloc(1, sizeof(knishio_claim_shadow_wallet_result_t));
    if (!claim_result) {
        error = KNISHIO_ERROR_MEMORY;
        goto cleanup;
    }

    if (response->data && response->success) {
        claim_result->success = true;
        claim_result->response = knishio_strdup(response->data);

        /* Parse molecular hash from GraphQL response */
        knishio_json_t* json_root = knishio_json_parse(response->data, NULL);
        if (json_root) {
            const char* molecular_hash = knishio_json_get_string_path(json_root, "data.ProposeMolecule.molecularHash");
            if (molecular_hash && strlen(molecular_hash) > 0) {
                claim_result->molecular_hash = knishio_strdup(molecular_hash);
            } else if (molecule->molecular_hash) {
                claim_result->molecular_hash = knishio_strdup(molecule->molecular_hash);
            }
            knishio_json_free(json_root);
        } else if (molecule->molecular_hash) {
            claim_result->molecular_hash = knishio_strdup(molecule->molecular_hash);
        }
    } else {
        claim_result->success = false;
        claim_result->error_message = knishio_strdup(response->errors ? response->errors : "Shadow wallet claim failed");
    }

    *result = claim_result;
    error = KNISHIO_SUCCESS;

cleanup:
    if (response) knishio_graphql_response_free(response);
    if (variables) knishio_free(variables);
    if (molecule) knishio_molecule_free(molecule);
    if (source) knishio_wallet_free(source);
    if (claim_wallet) knishio_wallet_free(claim_wallet);
    if (remainder) knishio_wallet_free(remainder);
    return error;
}

/* Claim all shadow wallets for token */
knishio_error_t knishio_client_claim_shadow_wallets(
    knishio_client_t* client,
    const knishio_claim_shadow_wallets_params_t* params,
    knishio_claim_shadow_wallets_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!params->token) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* First, query wallets to get shadow wallets */
    knishio_wallet_list_params_t wallet_params = {
        .bundle = NULL, /* Use client bundle */
        .token = params->token,
        .include_shadow = true
    };
    
    knishio_wallet_list_result_t* wallet_list = NULL;
    knishio_error_t error = knishio_client_query_wallets(client, &wallet_params, &wallet_list);
    
    if (error != KNISHIO_SUCCESS || !wallet_list || !wallet_list->success) {
        return error == KNISHIO_SUCCESS ? KNISHIO_ERROR_INVALID_RESPONSE : error;
    }
    
    /* Parse wallet list and filter for shadow wallets following 2025 C17 best practices */
    size_t shadow_wallet_count = 0;
    knishio_wallet_t** shadow_wallets = NULL;
    
    /* Filter shadow wallets from wallet list */
    if (wallet_list->wallets && wallet_list->wallet_count > 0) {
        for (size_t i = 0; i < wallet_list->wallet_count; i++) {
            if (wallet_list->wallets[i] && wallet_list->wallets[i]->is_shadow) {
                shadow_wallet_count++;
            }
        }
        
        if (shadow_wallet_count > 0) {
            shadow_wallets = malloc(sizeof(knishio_wallet_t*) * shadow_wallet_count);
            if (shadow_wallets) {
                size_t shadow_index = 0;
                for (size_t i = 0; i < wallet_list->wallet_count; i++) {
                    if (wallet_list->wallets[i] && wallet_list->wallets[i]->is_shadow) {
                        shadow_wallets[shadow_index] = wallet_list->wallets[i];
                        shadow_index++;
                    }
                }
            } else {
                knishio_wallet_list_result_free(wallet_list);
                return KNISHIO_ERROR_MEMORY;
            }
        }
    }
    
    /* Create result structure */
    knishio_claim_shadow_wallets_result_t* claim_result = calloc(1, sizeof(knishio_claim_shadow_wallets_result_t));
    if (!claim_result) {
        if (shadow_wallets) free(shadow_wallets);
        knishio_wallet_list_result_free(wallet_list);
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Process shadow wallets if found */
    if (shadow_wallet_count > 0) {
        claim_result->result_count = shadow_wallet_count;
        claim_result->results = malloc(sizeof(knishio_claim_shadow_wallet_result_t*) * shadow_wallet_count);
        if (!claim_result->results) {
            free(shadow_wallets);
            free(claim_result);
            knishio_wallet_list_result_free(wallet_list);
            return KNISHIO_ERROR_MEMORY;
        }
        
        bool all_successful = true;
        for (size_t i = 0; i < shadow_wallet_count; i++) {
            knishio_claim_shadow_wallet_params_t claim_params = {
                .token = params->token,
                .batch_id = shadow_wallets[i]->batch_id
            };
    
            error = knishio_client_claim_shadow_wallet(client, &claim_params, &claim_result->results[i]);
            if (error != KNISHIO_SUCCESS || !claim_result->results[i]->success) {
                all_successful = false;
            }
        }
        
        claim_result->success = all_successful;
        if (!all_successful) {
            claim_result->error_message = knishio_strdup("One or more shadow wallet claims failed");
        }
    } else {
        claim_result->result_count = 0;
        claim_result->results = NULL;
        claim_result->success = true;
    }
    
    if (shadow_wallets) free(shadow_wallets);
    
    knishio_wallet_list_result_free(wallet_list);
    *result = claim_result;
    return KNISHIO_SUCCESS;
}

/* Free identifier creation result */
void knishio_create_identifier_result_free(knishio_create_identifier_result_t* result) {
    if (!result) return;
    
    if (result->molecular_hash) knishio_free(result->molecular_hash);
    if (result->response) knishio_free(result->response);
    if (result->error_message) knishio_free(result->error_message);
    knishio_free(result);
}

/* Free shadow wallet claim result */
void knishio_claim_shadow_wallet_result_free(knishio_claim_shadow_wallet_result_t* result) {
    if (!result) return;
    
    if (result->molecular_hash) knishio_free(result->molecular_hash);
    if (result->response) knishio_free(result->response);
    if (result->error_message) knishio_free(result->error_message);
    knishio_free(result);
}

/* Free shadow wallets claim result */
void knishio_claim_shadow_wallets_result_free(knishio_claim_shadow_wallets_result_t* result) {
    if (!result) return;
    
    if (result->results) {
        for (size_t i = 0; i < result->result_count; i++) {
            knishio_claim_shadow_wallet_result_free(result->results[i]);
        }
        knishio_free(result->results);
    }
    
    if (result->error_message) knishio_free(result->error_message);
    knishio_free(result);
}