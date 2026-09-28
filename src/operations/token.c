/**
 * @file operations/token.c
 * @brief Token operations implementation for KnishIO C SDK
 * 
 * Provides full JS SDK alignment for token management operations.
 */

#include "knishio/knishio.h"
#include "knishio/operations/token.h"
#include "knishio/client_ops.h"
#include "knishio/wallet.h"
#include "knishio/graphql.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"
#include "operations_internal.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* GraphQL mutation templates */

static const char* CREATE_TOKEN_MUTATION =
    "mutation CreateToken($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecularHash"
    "    status"
    "    reason"
    "    payload"
    "  }"
    "}";

static const char* REQUEST_TOKENS_MUTATION = 
    "mutation RequestTokens($token: String!, $amount: String, $units: [String]) {"
    "  RequestTokens(token: $token, amount: $amount, units: $units) {"
    "    molecular_hash"
    "    status"
    "    wallet {"
    "      address"
    "      token"
    "      amount"
    "    }"
    "  }"
    "}";

/* Helper function to convert fungibility enum to string */
const char* knishio_token_fungibility_to_string(knishio_token_fungibility_t fungibility) {
    switch (fungibility) {
        case KNISHIO_TOKEN_FUNGIBLE:
            return "fungible";
        case KNISHIO_TOKEN_NONFUNGIBLE:
            return "nonfungible";
        case KNISHIO_TOKEN_STACKABLE:
            return "stackable";
        case KNISHIO_TOKEN_REPLENISHABLE:
            return "replenishable";
        default:
            return "fungible";
    }
}

/* Helper function to convert string to fungibility enum */
knishio_token_fungibility_t knishio_token_fungibility_from_string(const char* str) {
    if (!str) return KNISHIO_TOKEN_FUNGIBLE;
    
    if (strcmp(str, "nonfungible") == 0) {
        return KNISHIO_TOKEN_NONFUNGIBLE;
    } else if (strcmp(str, "stackable") == 0) {
        return KNISHIO_TOKEN_STACKABLE;
    } else if (strcmp(str, "replenishable") == 0) {
        return KNISHIO_TOKEN_REPLENISHABLE;
    }
    return KNISHIO_TOKEN_FUNGIBLE;
}

/* Create a new token */
knishio_error_t knishio_client_create_token(
    knishio_client_t* client,
    const knishio_create_token_params_t* params,
    knishio_create_token_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Validate required parameters */
    if (!params->token || !params->name) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Cycle 39 (slice 1): build the PARITY-CORRECT token-creation molecule (C-atom 'token' with
     * the 7 prefixed wallet* keys + a ContinuID I-atom) via knishio_molecule_init_token_creation,
     * signed, on a REAL client-secret-derived source wallet. Live submission + auth is the next
     * slice — the propose_molecule call below serializes the correct molecule but its GraphQL
     * transport/auth is not yet verified against the validator. */
    knishio_wallet_t* source = NULL;
    knishio_wallet_t* recipient = NULL;
    knishio_wallet_t* remainder = NULL;
    char* remainder_position = NULL;
    knishio_molecule_t* molecule = NULL;
    knishio_graphql_response_t* response = NULL;

    /* Source wallet at the bundle's LIVE on-ledger ContinuID position (slice 2c): the molecule
     * signs at the current chain head so it passes the validator's ContinuID chain validation. */
    knishio_error_t error = knishio_client_get_source_wallet_continuid(
        (knishio_client_t*)client, "USER", &source
    );
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Recipient (new-token) wallet from the source secret at a FRESH position (JS
     * Wallet.create). A fixed position put every token's genesis wallet at one position, so once
     * one of them signed, the validator refused every other as a consumed one-time key. */
    if (!knishio_generate_position(&remainder_position)) {
        error = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }
    error = knishio_wallet_create_simple(&recipient, source->secret, params->token, remainder_position);
    knishio_free(remainder_position);
    remainder_position = NULL;
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    /* Remainder (ContinuID I-atom) at a FRESH random position — designates the bundle's NEXT
     * chain head (the relay race; mirrors JS Wallet.generatePosition). */
    if (!knishio_generate_position(&remainder_position)) {
        error = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }
    error = knishio_wallet_create_simple(
        &remainder, source->secret, "USER", remainder_position
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Molecule: init_token_creation reads molecule->source_wallet (+ add_continuid_atom reads
     * molecule->remainder_wallet). */
    error = knishio_molecule_create(
        &molecule, source->secret, source->bundle_hash, source, remainder,
        knishio_client_get_cell_slug((knishio_client_t*)client), "V4"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Token meta: name, fungibility, + any params->meta (the init appends the 7 wallet* keys).
     * The amount is the C-atom VALUE, not meta. For a stackable/non-fungible token with units,
     * mirror the JS canonical contract: amount = unit count + meta splittable/decimals/tokenUnits. */
    {
        const char* keys[24];
        const char* vals[24];
        size_t n = 0;
        char amount_str[32];
        char* tu_json = NULL;   /* freed after init_token_creation copies the meta */

        keys[n] = "name";        vals[n] = params->name; n++;
        keys[n] = "fungibility"; vals[n] = knishio_token_fungibility_to_string(params->fungibility); n++;
        if (params->meta && params->meta_count > 0) {
            for (size_t i = 0; i < params->meta_count && n < 20; i++) {
                keys[n] = params->meta[i]->key;
                vals[n] = params->meta[i]->value;
                n++;
            }
        }

        bool is_stackable = (params->fungibility == KNISHIO_TOKEN_STACKABLE ||
                             params->fungibility == KNISHIO_TOKEN_NONFUNGIBLE);
        if (is_stackable && params->units && params->unit_count > 0) {
            /* tokenUnits = JSON array of the unit ids ["u1","u2",...] (mirror JS JSON.stringify(units)) */
            knishio_json_array_builder_t* ub = knishio_json_array_builder_create();
            if (ub) {
                for (size_t i = 0; i < params->unit_count; i++) {
                    knishio_json_array_add_string(ub, params->units[i] ? params->units[i] : "");
                }
                knishio_json_t* uarr = knishio_json_array_build(ub);
                if (uarr) {
                    tu_json = knishio_json_serialize(uarr, false);
                    knishio_json_free(uarr);
                }
                knishio_json_array_builder_free(ub);
            }
            if (tu_json && n + 3 <= 24) {
                keys[n] = "splittable"; vals[n] = "1";      n++;
                keys[n] = "decimals";   vals[n] = "0";      n++;
                keys[n] = "tokenUnits"; vals[n] = tu_json;  n++;
            }
        }

        if (is_stackable && params->unit_count > 0) {
            snprintf(amount_str, sizeof(amount_str), "%zu", params->unit_count); /* supply = unit count */
        } else {
            snprintf(amount_str, sizeof(amount_str), "%d", params->amount);
        }

        error = knishio_molecule_init_token_creation(molecule, recipient, amount_str, keys, vals, n);
        if (tu_json) {
            knishio_free(tu_json);
        }
    }
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Hash + sign (real timestamps — this is a live op, not a fixed-vector test). */
    error = knishio_molecule_generate_hash(molecule);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    error = knishio_molecule_sign(molecule, source->bundle_hash, false, true);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Check (contract 9.7) + submit. */
    error = knishio_client_submit_molecule(client, molecule, "CreateToken", CREATE_TOKEN_MUTATION, &response);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Build result */
    {
        knishio_create_token_result_t* res = knishio_calloc(1, sizeof(knishio_create_token_result_t));
        if (!res) {
            error = KNISHIO_ERROR_MEMORY;
            goto cleanup;
        }
        if (response->success && response->molecular_hash) {
            res->success = true;
            res->token_slug = knishio_strdup(params->token);
            res->molecular_hash = knishio_strdup(response->molecular_hash);
            res->response = response->data ? knishio_strdup(response->data) : NULL;
        } else {
            res->success = false;
            res->error_message = response->errors ?
                knishio_strdup(response->errors) :
                knishio_strdup("Token creation failed");
        }
        *result = res;
    }

cleanup:
    if (response) knishio_graphql_response_free(response);
    if (remainder_position) knishio_free(remainder_position);
    /* The molecule owns the C and I atoms init_token_creation built; the wallets stay ours. */
    if (molecule) knishio_molecule_free_deep(molecule);
    if (source) knishio_wallet_free(source);
    if (recipient) knishio_wallet_free(recipient);
    if (remainder) knishio_wallet_free(remainder);
    return error;
}

/* Request tokens (mint new tokens) */
knishio_error_t knishio_client_request_tokens(
    knishio_client_t* client,
    const knishio_request_tokens_params_t* params,
    knishio_request_tokens_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Validate required parameters */
    if (!params->token) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables JSON */
    char variables[1024];
    strcpy(variables, "{");
    
    /* Add token */
    strcat(variables, "\"token\":\"");
    strcat(variables, params->token);
    strcat(variables, "\"");
    
    /* Add amount if provided */
    if (params->requested_amount) {
        strcat(variables, ",\"amount\":\"");
        strcat(variables, params->requested_amount);
        strcat(variables, "\"");
    }
    
    /* Add units if provided */
    if (params->requested_units && params->unit_count > 0) {
        strcat(variables, ",\"units\":[");
        for (size_t i = 0; i < params->unit_count; i++) {
            if (i > 0) strcat(variables, ",");
            strcat(variables, "\"");
            strcat(variables, params->requested_units[i]);
            strcat(variables, "\"");
        }
        strcat(variables, "]");
    }
    
    strcat(variables, "}");
    
    /* Execute mutation */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "RequestTokens",
        .query = REQUEST_TOKENS_MUTATION,
        .variables_json = variables,
        .requires_auth = true,
        .is_mutation = true
    };
    
    knishio_graphql_client_t* graphql_client = (knishio_graphql_client_t*)client;
    knishio_error_t error = knishio_graphql_execute(graphql_client, &operation, &response);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result */
    knishio_request_tokens_result_t* res = knishio_calloc(1, sizeof(knishio_request_tokens_result_t));
    if (!res) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->success && response->molecular_hash) {
        res->success = true;
        res->molecular_hash = knishio_strdup(response->molecular_hash);
        res->response = response->data ? knishio_strdup(response->data) : NULL;
    } else {
        res->success = false;
        res->error_message = response->errors ? 
            knishio_strdup(response->errors) : 
            knishio_strdup("Token request failed");
    }
    
    knishio_graphql_response_free(response);
    *result = res;
    
    return KNISHIO_SUCCESS;
}

/* Result of a submitted token molecule (replenish, fuse), from the ProposeMolecule response. */
static knishio_error_t token_result_from_response(
    const knishio_graphql_response_t* response,
    const char* failure_text,
    knishio_request_tokens_result_t** result
) {
    knishio_request_tokens_result_t* res = knishio_calloc(1, sizeof(knishio_request_tokens_result_t));
    if (!res) {
        return KNISHIO_ERROR_MEMORY;
    }
    if (response->success && response->molecular_hash) {
        res->success = true;
        res->molecular_hash = knishio_strdup(response->molecular_hash);
        res->response = response->data ? knishio_strdup(response->data) : NULL;
    } else {
        res->success = false;
        res->error_message = knishio_strdup(response->errors ? response->errors : failure_text);
    }
    *result = res;
    return KNISHIO_SUCCESS;
}

/* Replenish token supply (contract 9.1): C(action add) + I, signed by the USER wallet at the
 * ContinuID pointer, crediting the identity's existing wallet for the token (or a new one). */
knishio_error_t knishio_client_replenish_token(
    knishio_client_t* client,
    const char* token,
    int amount,
    const knishio_token_unit_t* units,
    size_t unit_count,
    knishio_request_tokens_result_t** result
) {
    if (!client || !token || !result || (unit_count > 0 && !units)) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (unit_count == 0 && amount <= 0) {
        return KNISHIO_ERROR_NEGATIVE_AMOUNT;
    }

    knishio_wallet_t* source = NULL;
    knishio_wallet_t* existing = NULL;
    knishio_wallet_t* credited = NULL;
    knishio_wallet_t* remainder = NULL;
    char* position = NULL;
    knishio_molecule_t* molecule = NULL;
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_client_get_source_wallet_continuid(client, "USER", &source);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Credited wallet: the identity's wallet for the token (queryBalance), else a new one. */
    error = knishio_client_query_balance_wallet_of_type(client, token, NULL, &existing);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    if (existing && existing->position && strlen(existing->position) == 64) {
        if (unit_count == 0 && existing->token_unit_count > 0) {
            error = KNISHIO_ERROR_STACKABLE_UNIT_AMOUNT;  /* a stackable token needs its new units */
            goto cleanup;
        }
        error = knishio_wallet_create_simple(&credited, source->secret, token, existing->position);
        if (error == KNISHIO_SUCCESS && existing->batch_id) {
            credited->batch_id = knishio_strdup(existing->batch_id);
        }
    } else if (!knishio_generate_position(&position)) {
        error = KNISHIO_ERROR_CRYPTO;
    } else {
        error = knishio_wallet_create_simple(&credited, source->secret, token, position);
    }
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* ContinuID remainder at a fresh USER position (the bundle's next chain head). */
    knishio_free(position);
    position = NULL;
    if (!knishio_generate_position(&position)) {
        error = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }
    error = knishio_wallet_create_simple(&remainder, source->secret, "USER", position);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    error = knishio_molecule_create(&molecule, source->secret, source->bundle_hash, source, remainder,
                                    knishio_client_get_cell_slug(client), "V4");
    if (error == KNISHIO_SUCCESS) error = knishio_molecule_init_replenish(molecule, credited, amount, units, unit_count);
    if (error == KNISHIO_SUCCESS) error = knishio_molecule_generate_hash(molecule);
    if (error == KNISHIO_SUCCESS) error = knishio_molecule_sign(molecule, source->bundle_hash, false, true);
    if (error == KNISHIO_SUCCESS) {
        error = knishio_client_submit_molecule(client, molecule, "ReplenishToken", CREATE_TOKEN_MUTATION, &response);
    }
    if (error == KNISHIO_SUCCESS) {
        error = token_result_from_response(response, "Token replenish failed", result);
    }

cleanup:
    if (response) knishio_graphql_response_free(response);
    if (molecule) knishio_molecule_free_deep(molecule);
    knishio_free(position);
    if (remainder) knishio_wallet_free(remainder);
    if (credited) knishio_wallet_free(credited);
    if (existing) knishio_wallet_free(existing);
    if (source) knishio_wallet_free(source);
    return error;
}

/* Fuse stackable token units (contract 9.2): V(S,-B) V(burn,+(M-1)) F(recipient,+1) V(remainder,
 * +(B-M)), signed by the source token wallet S at its own position; no ContinuID atom. */
knishio_error_t knishio_client_fuse_token(
    knishio_client_t* client,
    const char* bundle_hash,
    const char* token_slug,
    const char* new_token_unit,
    const char* const* fused_token_unit_ids,
    size_t fused_count,
    knishio_request_tokens_result_t** result
) {
    if (!client || !token_slug || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    /* Refuse a request that cannot be valid before touching the network. */
    if (fused_count < 2 || !fused_token_unit_ids || !new_token_unit || !new_token_unit[0]) {
        return KNISHIO_ERROR_TRANSFER_BALANCE;
    }

    knishio_wallet_t* user = NULL;
    knishio_wallet_t* source = NULL;
    knishio_wallet_t* recipient = NULL;
    knishio_wallet_t* remainder = NULL;
    char* position = NULL;
    knishio_molecule_t* molecule = NULL;
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_resolve_funded_source(client, token_slug, NULL, &user, &source);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    if (knishio_molecule_fusion_error(source, fused_token_unit_ids, fused_count, new_token_unit)) {
        error = KNISHIO_ERROR_TRANSFER_BALANCE;
        goto cleanup;
    }

    /* F recipient: own bundle -> a new wallet of this identity (Wallet.create); another bundle ->
     * an addressless wallet keyed by that bundle (as transferToken builds its recipient). */
    if (!bundle_hash || strcmp(bundle_hash, user->bundle_hash) == 0) {
        if (!knishio_generate_position(&position)) {
            error = KNISHIO_ERROR_CRYPTO;
            goto cleanup;
        }
        error = knishio_wallet_create_simple(&recipient, user->secret, token_slug, position);
        knishio_free(position);
        position = NULL;
        if (error != KNISHIO_SUCCESS) {
            goto cleanup;
        }
    } else {
        recipient = knishio_calloc(1, sizeof(knishio_wallet_t));
        if (!recipient) {
            error = KNISHIO_ERROR_MEMORY;
            goto cleanup;
        }
        recipient->token = knishio_strdup(token_slug);
        recipient->bundle_hash = knishio_strdup(bundle_hash);
        recipient->position = knishio_strdup("");
        recipient->address = knishio_strdup("");
    }

    /* Remainder: a fresh position for the kept units. */
    if (!knishio_generate_position(&position)) {
        error = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }
    error = knishio_wallet_create_simple(&remainder, user->secret, token_slug, position);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    error = knishio_molecule_create(&molecule, user->secret, user->bundle_hash, source, remainder,
                                    knishio_client_get_cell_slug(client), "V4");
    if (error == KNISHIO_SUCCESS) {
        error = knishio_molecule_init_fuse_token(molecule, recipient, fused_token_unit_ids,
                                                 fused_count, new_token_unit);
    }
    if (error == KNISHIO_SUCCESS) error = knishio_molecule_generate_hash(molecule);
    if (error == KNISHIO_SUCCESS) error = knishio_molecule_sign(molecule, user->bundle_hash, false, true);
    if (error == KNISHIO_SUCCESS) {
        error = knishio_client_submit_molecule(client, molecule, "FuseToken", CREATE_TOKEN_MUTATION, &response);
    }
    if (error == KNISHIO_SUCCESS) {
        error = token_result_from_response(response, "Token fusion failed", result);
    }

cleanup:
    if (response) knishio_graphql_response_free(response);
    if (molecule) knishio_molecule_free_deep(molecule);
    knishio_free(position);
    if (remainder) knishio_wallet_free(remainder);
    if (recipient) knishio_wallet_free(recipient);
    if (source) knishio_wallet_free(source);
    if (user) knishio_wallet_free(user);
    return error;
}

/* Result management functions */

void knishio_create_token_result_free(knishio_create_token_result_t* result) {
    if (!result) return;
    
    knishio_free(result->token_slug);
    knishio_free(result->molecular_hash);
    knishio_free(result->response);
    knishio_free(result->error_message);
    knishio_free(result);
}

void knishio_request_tokens_result_free(knishio_request_tokens_result_t* result) {
    if (!result) return;
    
    knishio_free(result->molecular_hash);
    knishio_free(result->response);
    knishio_free(result->error_message);
    knishio_free(result);
}

/* Result accessor functions */

bool knishio_create_token_result_is_success(const knishio_create_token_result_t* result) {
    return result ? result->success : false;
}

const char* knishio_create_token_result_get_slug(const knishio_create_token_result_t* result) {
    return result ? result->token_slug : NULL;
}

const char* knishio_create_token_result_get_hash(const knishio_create_token_result_t* result) {
    return result ? result->molecular_hash : NULL;
}

bool knishio_request_tokens_result_is_success(const knishio_request_tokens_result_t* result) {
    return result ? result->success : false;
}

const char* knishio_request_tokens_result_get_hash(const knishio_request_tokens_result_t* result) {
    return result ? result->molecular_hash : NULL;
}