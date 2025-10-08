/**
 * @file operations/transfer.c
 * @brief Token transfer operations implementation for KnishIO C SDK
 */

#include "knishio/knishio.h"  /* Include main header first for all type definitions */
#include "knishio/operations/transfer.h"
#include "knishio/graphql.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* TransferTokens GraphQL mutation template */
static const char* TRANSFER_TOKENS_MUTATION = 
    "mutation TransferTokens($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecular_hash"
    "    status"
    "    reason"
    "    payload"
    "    createdAt"
    "  }"
    "}";

/* QueryBalance GraphQL query template */
static const char* QUERY_BALANCE = 
    "query QueryBalance($address: String, $bundle_hashHash: String, $token: String) {"
    "  Balance(address: $address, bundle_hashHash: $bundle_hashHash, token: $token) {"
    "    address"
    "    bundle_hashHash"
    "    tokenSlug"
    "    batchId"
    "    position"
    "    amount"
    "    characters"
    "  }"
    "}";

/* Internal helper to build transfer molecule */
static knishio_error_t build_transfer_molecule(
    knishio_client_t* client,
    const knishio_transfer_params_t* params,
    knishio_molecule_t** molecule
) {
    if (!client || !params || !molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Get wallet properties if source wallet provided */
    const char* wallet_secret = NULL;
    const char* wallet_bundle_hash = NULL;
    const char* wallet_position = NULL;
    const char* wallet_address = NULL;
    
    if (params->source_wallet) {
        wallet_secret = params->source_wallet->secret;
        wallet_bundle_hash = params->source_wallet->bundle_hash;
        wallet_position = params->source_wallet->position;
        wallet_address = params->source_wallet->address;
    }
    
    /* Create molecule with all required parameters */
    knishio_molecule_t* mol = NULL;
    knishio_error_t error = knishio_molecule_create(
        &mol,
        wallet_secret,      /* secret for signing */
        wallet_bundle_hash,      /* bundle_hash hash */
        params->source_wallet,  /* source wallet */
        NULL,              /* remainder wallet (NULL for simple transfer) */
        "transfer",        /* cell slug */
        "V4"              /* protocol version */
    );
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Convert amount to string */
    char amount_str[64];
    snprintf(amount_str, sizeof(amount_str), "%.8f", params->amount);
    
    /* Create transfer atom with all required parameters */
    knishio_atom_t* atom = NULL;
    error = knishio_atom_create(
        &atom,
        wallet_position ? wallet_position : "",  /* position */
        params->recipient,                        /* recipient address */
        KNISHIO_ISOTOPE_V,                       /* Value transfer isotope */
        params->token,                           /* token type */
        amount_str,                              /* value */
        params->batch_id                         /* batch ID (can be NULL) */
    );
    if (error != KNISHIO_SUCCESS) {
        knishio_molecule_free(mol);
        return error;
    }
    
    /* Add atom to molecule */
    error = knishio_molecule_add_atom(mol, atom);
    if (error != KNISHIO_SUCCESS) {
        knishio_atom_free(atom);
        knishio_molecule_free(mol);
        return error;
    }
    
    /* Molecule signing is handled during creation with secret parameter */
    
    *molecule = mol;
    return KNISHIO_SUCCESS;
}

/* Main transfer function */
knishio_error_t knishio_client_transfer_tokens(
    knishio_client_t* client,
    const knishio_transfer_params_t* params,
    knishio_transfer_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Validate required parameters */
    if (!params->recipient || !params->token || params->amount <= 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build transfer molecule */
    knishio_molecule_t* molecule = NULL;
    knishio_error_t error = build_transfer_molecule(client, params, &molecule);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Convert molecule to JSON for GraphQL */
    char* molecule_json = NULL;
    error = knishio_molecule_to_json(molecule, &molecule_json);
    if (error != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return error;
    }
    
    /* Build variables JSON - simple string concatenation for now */
    char* variables = NULL;
    size_t var_len = strlen(molecule_json) + 32;
    variables = knishio_malloc(var_len);
    if (!variables) {
        knishio_free(molecule_json);
        knishio_molecule_free(molecule);
        return KNISHIO_ERROR_MEMORY;
    }
    snprintf(variables, var_len, "{\"molecule\":%s}", molecule_json);
    
    /* Execute GraphQL mutation */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "TransferTokens",
        .query = TRANSFER_TOKENS_MUTATION,
        .variables_json = variables,
        .requires_auth = true,
        .is_mutation = true
    };
    
    error = knishio_graphql_execute(
        (knishio_graphql_client_t*)client,  /* Cast for now */
        &operation,
        &response
    );
    
    /* Free temporary data */
    knishio_free(variables);
    knishio_free(molecule_json);
    knishio_molecule_free(molecule);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Parse response and create result */
    knishio_transfer_result_t* res = knishio_calloc(1, sizeof(knishio_transfer_result_t));
    if (!res) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Extract ProposeMolecule result */
    if (response->data && response->success) {
        res->success = true;
        
        /* Parse molecular hash from GraphQL response following 2025 C17 best practices */
        knishio_json_t* json_root = knishio_json_parse(response->data, NULL);
        if (json_root) {
            /* Extract molecular_hash from data.ProposeMolecule.molecular_hash */
            const char* molecular_hash = knishio_json_get_string_path(json_root, "data.ProposeMolecule.molecular_hash");
            if (molecular_hash && strlen(molecular_hash) > 0) {
                res->molecular_hash = knishio_strdup(molecular_hash);
                
                /* Also extract status and reason for enhanced error reporting */
                const char* status = knishio_json_get_string_path(json_root, "data.ProposeMolecule.status");
                const char* reason = knishio_json_get_string_path(json_root, "data.ProposeMolecule.reason");
                
                if (status && strcmp(status, "success") != 0) {
                    /* Transaction was submitted but may have issues */
                    res->success = false;
                    res->error_message = reason ? knishio_strdup(reason) : knishio_strdup("Transaction failed");
                }
            } else {
                /* No molecular hash found - indicates parsing or server error */
                res->success = false;
                res->error_message = knishio_strdup("Failed to extract molecular hash from response");
            }
            
            knishio_json_free(json_root);
        } else {
            /* JSON parsing failed */
            res->success = false;
            res->error_message = knishio_strdup("Failed to parse GraphQL response JSON");
        }
        
        res->response = knishio_strdup(response->data);
    } else {
        res->success = false;
        res->error_message = response->errors ? 
            knishio_strdup(response->errors) : 
            knishio_strdup("Transfer failed");
    }
    
    knishio_graphql_response_free(response);
    *result = res;
    
    return KNISHIO_SUCCESS;
}

/* Query balance function - simplified interface */
knishio_error_t knishio_client_query_balance(
    knishio_client_t* client,
    const char* token,
    const char* bundle_hash,
    char** balance
) {
    if (!client || !balance) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables JSON - simple string concatenation for now */
    char variables[512] = "{";
    bool first = true;
    
    if (bundle_hash) {
        strcat(variables, "\"bundle_hashHash\":\"");
        strcat(variables, bundle_hash);
        strcat(variables, "\"");
        first = false;
    }
    if (token) {
        if (!first) strcat(variables, ",");
        strcat(variables, "\"token\":\"");
        strcat(variables, token);
        strcat(variables, "\"");
    }
    strcat(variables, "}");
    
    /* Execute GraphQL query */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryBalance",
        .query = QUERY_BALANCE,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
    };
    
    knishio_error_t error = knishio_graphql_execute(
        (knishio_graphql_client_t*)client,  /* Cast for now */
        &operation,
        &response
    );
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Parse balance from response following 2025 C17 best practices */
    if (response->data && response->success) {
        knishio_json_t* json_root = knishio_json_parse(response->data, NULL);
        if (json_root) {
            /* Extract amount from data.Balance.amount */
            const char* balance_amount = knishio_json_get_string_path(json_root, "data.Balance.amount");
            if (balance_amount && strlen(balance_amount) > 0) {
                *balance = knishio_strdup(balance_amount);
            } else {
                /* No balance found - could be new wallet with 0 balance */
                *balance = knishio_strdup("0.0");
            }
            
            knishio_json_free(json_root);
        } else {
            /* JSON parsing failed */
            *balance = knishio_strdup("0.0");
        }
    } else {
        *balance = NULL;
        error = KNISHIO_ERROR_INVALID_RESPONSE;
    }
    
    knishio_graphql_response_free(response);
    return error;
}

/* Query source wallet for transfers with validation */
knishio_error_t knishio_client_query_source_wallet(
    knishio_client_t* client,
    const char* token,
    double amount,
    const char* type,
    knishio_wallet_t** wallet
) {
    if (!client || !token || !wallet || amount < 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Default type to "regular" if not specified */
    if (!type) {
        type = "regular";
    }
    
    /* First query the balance to get wallet information */
    char* balance_str = NULL;
    knishio_error_t error = knishio_client_query_balance(client, token, NULL, &balance_str);
    
    if (error != KNISHIO_SUCCESS || !balance_str) {
        return error == KNISHIO_SUCCESS ? KNISHIO_ERROR_INVALID_RESPONSE : error;
    }
    
    /* Parse balance value */
    double current_balance = strtod(balance_str, NULL);
    free(balance_str);
    
    /* Check if balance is sufficient */
    if (current_balance < amount) {
        return KNISHIO_ERROR_BALANCE_INSUFFICIENT;
    }
    
    /* For now, create a minimal wallet structure */
    /* TODO: Parse full wallet details from GraphQL response */
    knishio_wallet_t* source_wallet = calloc(1, sizeof(knishio_wallet_t));
    if (!source_wallet) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    source_wallet->token = knishio_strdup(token);
    source_wallet->initialized = true;
    
    /* TODO: Validate that wallet is not a shadow wallet */
    /* (JavaScript checks that position and address are not null/empty) */
    
    *wallet = source_wallet;
    return KNISHIO_SUCCESS;
}

/* Burn tokens (transfer to null address) */
knishio_error_t knishio_client_burn_tokens(
    knishio_client_t* client,
    const char* token,
    double amount,
    const char** units,
    size_t unit_count,
    knishio_transfer_result_t** result
) {
    if (!client || !token || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Validate amount or units */
    if (amount <= 0 && (!units || unit_count == 0)) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Burn is a transfer to null address */
    const char* null_address = "0000000000000000000000000000000000000000000000000000000000000000";
    
    /* Build transfer parameters */
    knishio_transfer_params_t params = {
        .recipient = null_address,
        .token = token,
        .amount = amount,
        .units = units,
        .unit_count = unit_count,
        .batch_id = NULL,
        .source_wallet = NULL  /* Will be fetched internally */
    };
    
    return knishio_client_transfer_tokens(client, &params, result);
}

/* Deposit tokens to buffer */
knishio_error_t knishio_client_deposit_buffer_token(
    knishio_client_t* client,
    const char* token,
    double amount,
    const char* buffer_id,
    knishio_transfer_result_t** result
) {
    if (!client || !token || amount <= 0 || !buffer_id || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Get source wallet */
    knishio_wallet_t* wallet = NULL;
    knishio_error_t error = knishio_client_get_source_wallet(client, token, &wallet);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create molecule for buffer deposit */
    knishio_molecule_t* molecule = NULL;
    error = knishio_molecule_create(
        &molecule,
        wallet->secret,
        wallet->bundle_hash,
        wallet,
        NULL,
        "buffer_deposit",
        "V4"
    );
    
    if (error != KNISHIO_SUCCESS) {
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    /* Convert amount to string */
    char amount_str[64];
    snprintf(amount_str, sizeof(amount_str), "%.8f", amount);
    
    /* Create deposit atom with buffer metadata */
    knishio_meta_t* meta[2];
    meta[0] = knishio_malloc(sizeof(knishio_meta_t));
    meta[0]->key = knishio_strdup("buffer_id");
    meta[0]->value = knishio_strdup(buffer_id);
    
    meta[1] = knishio_malloc(sizeof(knishio_meta_t));
    meta[1]->key = knishio_strdup("operation");
    meta[1]->value = knishio_strdup("deposit");
    
    knishio_atom_t* atom = NULL;
    error = knishio_atom_create_with_meta(
        &atom,
        wallet->position,
        wallet->address,
        KNISHIO_ISOTOPE_V,  /* Value transfer */
        token,
        amount_str,
        NULL,
        "buffer",
        buffer_id,
        meta,
        2
    );
    
    /* Free meta */
    knishio_free((void*)meta[0]->key);
    knishio_free((void*)meta[0]->value);
    knishio_free(meta[0]);
    knishio_free((void*)meta[1]->key);
    knishio_free((void*)meta[1]->value);
    knishio_free(meta[1]);
    
    if (error != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    /* Add atom to molecule */
    error = knishio_molecule_add_atom(molecule, atom);
    if (error != KNISHIO_SUCCESS) {
        knishio_atom_free(atom);
        knishio_molecule_free(molecule);
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    /* Propose molecule */
    char* molecular_hash = NULL;
    error = knishio_client_propose_molecule(client, molecule, &molecular_hash);
    
    knishio_molecule_free(molecule);
    knishio_wallet_cleanup(wallet);
    knishio_free(wallet);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result */
    knishio_transfer_result_t* res = knishio_calloc(1, sizeof(knishio_transfer_result_t));
    if (!res) {
        knishio_free(molecular_hash);
        return KNISHIO_ERROR_MEMORY;
    }
    
    res->success = true;
    res->molecular_hash = molecular_hash;
    
    *result = res;
    return KNISHIO_SUCCESS;
}

/* Withdraw tokens from buffer */
knishio_error_t knishio_client_withdraw_buffer_token(
    knishio_client_t* client,
    const char* token,
    double amount,
    const char* buffer_id,
    knishio_transfer_result_t** result
) {
    if (!client || !token || amount <= 0 || !buffer_id || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Get source wallet */
    knishio_wallet_t* wallet = NULL;
    knishio_error_t error = knishio_client_get_source_wallet(client, token, &wallet);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create molecule for buffer withdrawal */
    knishio_molecule_t* molecule = NULL;
    error = knishio_molecule_create(
        &molecule,
        wallet->secret,
        wallet->bundle_hash,
        wallet,
        NULL,
        "buffer_withdraw",
        "V4"
    );
    
    if (error != KNISHIO_SUCCESS) {
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    /* Convert amount to string */
    char amount_str[64];
    snprintf(amount_str, sizeof(amount_str), "%.8f", amount);
    
    /* Create withdrawal atom with buffer metadata */
    knishio_meta_t* meta[2];
    meta[0] = knishio_malloc(sizeof(knishio_meta_t));
    meta[0]->key = knishio_strdup("buffer_id");
    meta[0]->value = knishio_strdup(buffer_id);
    
    meta[1] = knishio_malloc(sizeof(knishio_meta_t));
    meta[1]->key = knishio_strdup("operation");
    meta[1]->value = knishio_strdup("withdraw");
    
    knishio_atom_t* atom = NULL;
    error = knishio_atom_create_with_meta(
        &atom,
        wallet->position,
        wallet->address,
        KNISHIO_ISOTOPE_V,  /* Value transfer */
        token,
        amount_str,
        NULL,
        "buffer",
        buffer_id,
        meta,
        2
    );
    
    /* Free meta */
    knishio_free((void*)meta[0]->key);
    knishio_free((void*)meta[0]->value);
    knishio_free(meta[0]);
    knishio_free((void*)meta[1]->key);
    knishio_free((void*)meta[1]->value);
    knishio_free(meta[1]);
    
    if (error != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    /* Add atom to molecule */
    error = knishio_molecule_add_atom(molecule, atom);
    if (error != KNISHIO_SUCCESS) {
        knishio_atom_free(atom);
        knishio_molecule_free(molecule);
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    /* Propose molecule */
    char* molecular_hash = NULL;
    error = knishio_client_propose_molecule(client, molecule, &molecular_hash);
    
    knishio_molecule_free(molecule);
    knishio_wallet_cleanup(wallet);
    knishio_free(wallet);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result */
    knishio_transfer_result_t* res = knishio_calloc(1, sizeof(knishio_transfer_result_t));
    if (!res) {
        knishio_free(molecular_hash);
        return KNISHIO_ERROR_MEMORY;
    }
    
    res->success = true;
    res->molecular_hash = molecular_hash;
    
    *result = res;
    return KNISHIO_SUCCESS;
}

/* Result management functions */

void knishio_transfer_result_free(knishio_transfer_result_t* result) {
    if (!result) return;
    
    knishio_free(result->molecular_hash);
    knishio_free(result->response);
    knishio_free(result->error_message);
    knishio_free(result);
}

/* Result accessor functions */

bool knishio_transfer_result_is_success(const knishio_transfer_result_t* result) {
    return result ? result->success : false;
}

const char* knishio_transfer_result_get_hash(const knishio_transfer_result_t* result) {
    return result ? result->molecular_hash : NULL;
}
