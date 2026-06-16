/**
 * @file operations/token.c
 * @brief Token operations implementation for KnishIO C SDK
 * 
 * Provides full JS SDK alignment for token management operations.
 */

#include "knishio/knishio.h"
#include "knishio/operations/token.h"
#include "knishio/graphql.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* GraphQL mutation templates */

static const char* CREATE_TOKEN_MUTATION = 
    "mutation CreateToken($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecular_hash"
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
    knishio_molecule_t* molecule = NULL;
    char* variables = NULL;
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_client_get_source_wallet(
        (knishio_client_t*)client, "USER", &source
    );
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Recipient (new-token) wallet + canonical USER remainder, from the source secret.
     * Deterministic 64-hex positions for this slice (live ContinuID position = next slice). */
    error = knishio_wallet_create_simple(
        &recipient, source->secret, params->token,
        "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    error = knishio_wallet_create_simple(
        &remainder, source->secret, "USER",
        "bbbb000000000000cccc111111111111dddd222222222222eeee333333333333"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Molecule: init_token_creation reads molecule->source_wallet (+ add_continuid_atom reads
     * molecule->remainder_wallet). */
    error = knishio_molecule_create(
        &molecule, source->secret, source->bundle_hash, source, remainder, NULL, "V4"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Token meta: name, fungibility, + any params->meta (the init appends the 7 wallet* keys).
     * The amount is the C-atom VALUE, not meta. */
    {
        const char* keys[16];
        const char* vals[16];
        size_t n = 0;
        char amount_str[32];

        keys[n] = "name";        vals[n] = params->name; n++;
        keys[n] = "fungibility"; vals[n] = knishio_token_fungibility_to_string(params->fungibility); n++;
        if (params->meta && params->meta_count > 0) {
            for (size_t i = 0; i < params->meta_count && n < 16; i++) {
                keys[n] = params->meta[i]->key;
                vals[n] = params->meta[i]->value;
                n++;
            }
        }
        snprintf(amount_str, sizeof(amount_str), "%d", params->amount);

        error = knishio_molecule_init_token_creation(molecule, recipient, amount_str, keys, vals, n);
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
            .name = "CreateToken",
            .query = CREATE_TOKEN_MUTATION,
            .variables_json = variables,
            .requires_auth = true,
            .is_mutation = true
        };
        knishio_graphql_client_t* graphql_client = (knishio_graphql_client_t*)client;
        error = knishio_graphql_execute(graphql_client, &operation, &response);
    }
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
    if (variables) knishio_free(variables);
    if (molecule) knishio_molecule_free(molecule);
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

/* Replenish token supply */
knishio_error_t knishio_client_replenish_token(
    knishio_client_t* client,
    const char* token,
    int amount,
    knishio_request_tokens_result_t** result
) {
    if (!client || !token || !result || amount <= 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Use RequestTokens with specific amount for replenishable tokens */
    char amount_str[32];
    snprintf(amount_str, sizeof(amount_str), "%d", amount);
    
    knishio_request_tokens_params_t params = {
        .token = token,
        .requested_amount = amount_str,
        .requested_units = NULL,
        .unit_count = 0,
        .to = NULL,
        .meta = NULL,
        .meta_count = 0
    };
    
    return knishio_client_request_tokens(client, &params, result);
}

/* Fuse tokens - combine multiple tokens into one */
knishio_error_t knishio_client_fuse_token(
    knishio_client_t* client,
    const char* token,
    const char** source_tokens,
    size_t token_count,
    knishio_request_tokens_result_t** result
) {
    if (!client || !token || !source_tokens || token_count == 0 || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Get source wallet */
    knishio_wallet_t* wallet = NULL;
    knishio_error_t error = knishio_client_get_source_wallet(
        (knishio_client_t*)client, 
        token, 
        &wallet
    );
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create molecule for token fusion */
    knishio_molecule_t* molecule = NULL;
    error = knishio_molecule_create(
        &molecule,
        wallet->secret,
        wallet->bundle_hash,
        wallet,
        NULL,
        "fusion",
        "V4"
    );
    
    if (error != KNISHIO_SUCCESS) {
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    /* Add fusion atoms for each source token */
    for (size_t i = 0; i < token_count; i++) {
        knishio_atom_t* atom = NULL;
        error = knishio_atom_create(
            &atom,
            wallet->position,
            wallet->address,
            KNISHIO_ISOTOPE_F,  /* Fusion isotope */
            source_tokens[i],
            "-1",  /* Consume source token */
            NULL
        );
        
        if (error != KNISHIO_SUCCESS) {
            knishio_molecule_free(molecule);
            knishio_wallet_cleanup(wallet);
            knishio_free(wallet);
            return error;
        }
        
        error = knishio_molecule_add_atom(molecule, atom);
        if (error != KNISHIO_SUCCESS) {
            knishio_atom_free(atom);
            knishio_molecule_free(molecule);
            knishio_wallet_cleanup(wallet);
            knishio_free(wallet);
            return error;
        }
    }
    
    /* Add result fusion atom */
    knishio_atom_t* result_atom = NULL;
    error = knishio_atom_create(
        &result_atom,
        wallet->position,
        wallet->address,
        KNISHIO_ISOTOPE_F,  /* Fusion isotope */
        token,
        "1",  /* Create fused token */
        NULL
    );
    
    if (error != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    error = knishio_molecule_add_atom(molecule, result_atom);
    if (error != KNISHIO_SUCCESS) {
        knishio_atom_free(result_atom);
        knishio_molecule_free(molecule);
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    /* Propose molecule */
    char* molecular_hash = NULL;
    error = knishio_client_propose_molecule(
        (knishio_client_t*)client,
        molecule,
        &molecular_hash
    );
    
    knishio_molecule_free(molecule);
    knishio_wallet_cleanup(wallet);
    knishio_free(wallet);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result */
    knishio_request_tokens_result_t* res = knishio_calloc(1, sizeof(knishio_request_tokens_result_t));
    if (!res) {
        knishio_free(molecular_hash);
        return KNISHIO_ERROR_MEMORY;
    }
    
    res->success = (molecular_hash != NULL);
    res->molecular_hash = molecular_hash;
    
    *result = res;
    return KNISHIO_SUCCESS;
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