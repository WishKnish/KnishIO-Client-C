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

/* QueryBalance GraphQL query template */
static const char* QUERY_BALANCE =
    "query QueryBalance($address: String, $bundleHash: String, $token: String) {"
    "  Balance(address: $address, bundleHash: $bundleHash, token: $token) {"
    "    address"
    "    bundleHash"
    "    tokenSlug"
    "    batchId"
    "    position"
    "    amount"
    "    characters"
    "  }"
    "}";

/* Generic ProposeMolecule mutation (camelCase molecularHash — the cycle-40 fix). Used by BOTH
 * burn and transfer: all molecule submission goes through the validator's ProposeMolecule.
 * (The old snake-case TransferTokens mutation + single-atom build_transfer_molecule were removed.) */
static const char* PROPOSE_MOLECULE_MUTATION =
    "mutation ProposeMolecule($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecularHash"
    "    status"
    "    reason"
    "    payload"
    "    createdAt"
    "  }"
    "}";

/* Main transfer function (canonical 3-V molecule; mirrors burn's resolve+submit). */
knishio_error_t knishio_client_transfer_tokens(
    knishio_client_t* client,
    const knishio_transfer_params_t* params,
    knishio_transfer_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!params->recipient || !params->token || params->amount <= 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Canonical transfer = 3 V-atoms (source -balance, recipient +amount as a claimable shadow,
     * remainder +(balance-amount)); sum 0. Mirrors burn's resolve+submit pattern; the SOURCE is the
     * funded TOKEN wallet (V-isotope signs at its registered position), NOT the USER ContinuID. */
    knishio_wallet_t* user = NULL;        /* USER ContinuID wallet → client secret + bundle */
    knishio_wallet_t* source = NULL;      /* the funded token wallet (transfer source) */
    knishio_wallet_t* recipient = NULL;   /* shadow wallet for the recipient bundle */
    knishio_wallet_t* remainder = NULL;
    char* remainder_position = NULL;
    char* tok_position = NULL;
    char* tok_amount = NULL;
    knishio_molecule_t* molecule = NULL;
    char* variables = NULL;
    knishio_graphql_response_t* response = NULL;

    /* 1. Resolve the client secret + bundle via the USER ContinuID wallet. */
    knishio_error_t error = knishio_client_get_source_wallet_continuid(client, "USER", &user);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* 2. Query Balance(bundleHash, token) → the funded token wallet's position + amount. */
    {
        char vars[256];
        snprintf(vars, sizeof(vars), "{\"bundleHash\":\"%s\",\"token\":\"%s\"}",
                 user->bundle_hash, params->token);
        knishio_graphql_operation_t op = {
            .name = "QueryBalance", .query = QUERY_BALANCE,
            .variables_json = vars, .requires_auth = false, .is_mutation = false
        };
        error = knishio_client_execute_graphql(client, &op, &response);
        if (error != KNISHIO_SUCCESS) {
            goto cleanup;
        }
        if (response->success && response->data) {
            knishio_json_t* root = knishio_json_parse(response->data, NULL);
            if (root) {
                const char* p = knishio_json_get_string_path(root, "data.Balance.position");
                const char* a = knishio_json_get_string_path(root, "data.Balance.amount");
                if (p) tok_position = knishio_strdup(p);
                if (a) tok_amount = knishio_strdup(a);
                knishio_json_free(root);
            }
        }
        knishio_graphql_response_free(response);
        response = NULL;
        if (!tok_position || strlen(tok_position) != 64 || !tok_amount) {
            error = KNISHIO_ERROR_INVALID_RESPONSE;  /* no funded token wallet to transfer from */
            goto cleanup;
        }
    }

    /* 3. Source = the funded token wallet (re-derives address from secret+token+position). */
    error = knishio_wallet_create_simple(&source, user->secret, params->token, tok_position);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    source->balance = (double) atoi(tok_amount);
    if (source->balance < params->amount) {
        error = KNISHIO_ERROR_BALANCE_INSUFFICIENT;
        goto cleanup;
    }

    /* 4. Recipient = a shadow wallet for the recipient bundle (no secret → empty position/address;
     * the validator keys the claimable shadow by bundle + token + batchId). A fresh recipient in a
     * pure-V molecule REQUIRES a batchId; callers pass one for a claimable transfer. */
    recipient = knishio_calloc(1, sizeof(knishio_wallet_t));
    if (!recipient) {
        error = KNISHIO_ERROR_MEMORY;
        goto cleanup;
    }
    recipient->token = knishio_strdup(params->token);
    recipient->bundle_hash = knishio_strdup(params->recipient);
    recipient->position = knishio_strdup("");
    recipient->address = knishio_strdup("");
    if (params->batch_id) {
        recipient->batch_id = knishio_strdup(params->batch_id);
    }

    /* 5. Remainder: a fresh same-token wallet (the change returns to the source identity). */
    if (!knishio_generate_position(&remainder_position)) {
        error = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }
    error = knishio_wallet_create_simple(&remainder, user->secret, params->token, remainder_position);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* 5b. Stackable (NFT) transfer: partition the source's tokenUnits → source + recipient get the
     * SENT units, remainder keeps the rest; init_value emits each atom's tokenUnits. No-op for a
     * fungible transfer (unit_count == 0). */
    if (params->unit_count > 0) {
        knishio_wallet_split_units(source, (char**)params->units, params->unit_count, remainder, recipient);
    }

    /* 6. Build the molecule + the canonical 3-V-atom value transfer. */
    error = knishio_molecule_create(
        &molecule, user->secret, user->bundle_hash, source, remainder,
        knishio_client_get_cell_slug(client), "V4"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    error = knishio_molecule_init_value(molecule, recipient, (int) params->amount);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* 7. Hash + sign (the source token wallet's bundle = the source identity's bundle). */
    error = knishio_molecule_generate_hash(molecule);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    error = knishio_molecule_sign(molecule, user->bundle_hash, false, true);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* 8. Serialize + submit via ProposeMolecule. */
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
        knishio_graphql_operation_t op = {
            .name = "ProposeMolecule", .query = PROPOSE_MOLECULE_MUTATION,
            .variables_json = variables, .requires_auth = true, .is_mutation = true
        };
        error = knishio_client_execute_graphql(client, &op, &response);
    }
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* 9. Build result. */
    {
        knishio_transfer_result_t* res = knishio_calloc(1, sizeof(knishio_transfer_result_t));
        if (!res) {
            error = KNISHIO_ERROR_MEMORY;
            goto cleanup;
        }
        if (response->success && response->molecular_hash) {
            res->success = true;
            res->molecular_hash = knishio_strdup(response->molecular_hash);
            res->response = response->data ? knishio_strdup(response->data) : NULL;
        } else {
            res->success = false;
            res->error_message = response->errors ?
                knishio_strdup(response->errors) :
                knishio_strdup("Transfer failed");
        }
        *result = res;
    }

cleanup:
    if (response) knishio_graphql_response_free(response);
    if (variables) knishio_free(variables);
    if (molecule) knishio_molecule_free(molecule);
    if (remainder_position) knishio_free(remainder_position);
    if (tok_position) knishio_free(tok_position);
    if (tok_amount) knishio_free(tok_amount);
    if (source) knishio_wallet_free(source);
    if (recipient) knishio_wallet_free(recipient);
    if (remainder) knishio_wallet_free(remainder);
    if (user) knishio_wallet_free(user);
    return error;
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
    if (amount <= 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Canonical burn = 3 V-atoms (source -balance, +amount to the all-zeros burn bundle,
     * remainder +(balance-amount)); sum 0. Mirrors create_token's resolve+submit pattern,
     * but the SOURCE is the funded TOKEN wallet (V-isotope signs at its registered position),
     * NOT the USER ContinuID. (The old delegation to transfer_tokens built a single float
     * atom to the all-zeros ADDRESS — non-canonical.) */
    knishio_wallet_t* user = NULL;       /* USER ContinuID wallet → client secret + bundle */
    knishio_wallet_t* source = NULL;     /* the funded token wallet (burn source) */
    knishio_wallet_t* remainder = NULL;
    char* remainder_position = NULL;
    char* tok_position = NULL;
    char* tok_amount = NULL;
    knishio_molecule_t* molecule = NULL;
    char* variables = NULL;
    knishio_graphql_response_t* response = NULL;

    /* 1. Resolve the client secret + bundle via the USER ContinuID wallet. */
    knishio_error_t error = knishio_client_get_source_wallet_continuid(client, "USER", &user);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* 2. Query Balance(bundleHash, token) → the funded token wallet's position + amount. */
    {
        char vars[256];
        snprintf(vars, sizeof(vars), "{\"bundleHash\":\"%s\",\"token\":\"%s\"}",
                 user->bundle_hash, token);
        knishio_graphql_operation_t op = {
            .name = "QueryBalance", .query = QUERY_BALANCE,
            .variables_json = vars, .requires_auth = false, .is_mutation = false
        };
        error = knishio_client_execute_graphql(client, &op, &response);
        if (error != KNISHIO_SUCCESS) {
            goto cleanup;
        }
        if (response->success && response->data) {
            knishio_json_t* root = knishio_json_parse(response->data, NULL);
            if (root) {
                const char* p = knishio_json_get_string_path(root, "data.Balance.position");
                const char* a = knishio_json_get_string_path(root, "data.Balance.amount");
                if (p) tok_position = knishio_strdup(p);
                if (a) tok_amount = knishio_strdup(a);
                knishio_json_free(root);
            }
        }
        knishio_graphql_response_free(response);
        response = NULL;
        if (!tok_position || strlen(tok_position) != 64 || !tok_amount) {
            error = KNISHIO_ERROR_INVALID_RESPONSE;  /* no funded token wallet to burn from */
            goto cleanup;
        }
    }

    /* 3. Burn source = the funded token wallet (re-derives address from secret+token+position). */
    error = knishio_wallet_create_simple(&source, user->secret, token, tok_position);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    source->balance = (double) atoi(tok_amount);

    /* 4. Remainder: a fresh same-token wallet (the change returns to the source identity). */
    if (!knishio_generate_position(&remainder_position)) {
        error = KNISHIO_ERROR_CRYPTO;
        goto cleanup;
    }
    error = knishio_wallet_create_simple(&remainder, user->secret, token, remainder_position);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* 4b. Stackable (NFT) burn: partition the source's tokenUnits → source keeps the BURNED units,
     * remainder keeps the rest (no recipient); init_burn then emits each atom's tokenUnits. No-op
     * for a fungible burn (unit_count == 0). NOTE: a live-queried source carries units only once
     * GraphQL tokenUnits response-parsing lands (follow-up); offline drivers set units directly. */
    if (unit_count > 0) {
        knishio_wallet_split_units(source, (char**)units, unit_count, remainder, NULL);
    }

    /* 5. Build the molecule + the canonical 3-V-atom burn. */
    error = knishio_molecule_create(
        &molecule, user->secret, user->bundle_hash, source, remainder,
        knishio_client_get_cell_slug(client), "V4"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    error = knishio_molecule_init_burn(molecule, (int) amount);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* 6. Hash + sign (the source token wallet's bundle = the source identity's bundle). */
    error = knishio_molecule_generate_hash(molecule);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    error = knishio_molecule_sign(molecule, user->bundle_hash, false, true);
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* 7. Serialize + submit via ProposeMolecule. */
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
        knishio_graphql_operation_t op = {
            .name = "ProposeMolecule", .query = PROPOSE_MOLECULE_MUTATION,
            .variables_json = variables, .requires_auth = true, .is_mutation = true
        };
        error = knishio_client_execute_graphql(client, &op, &response);
    }
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* 8. Build result. */
    {
        knishio_transfer_result_t* res = knishio_calloc(1, sizeof(knishio_transfer_result_t));
        if (!res) {
            error = KNISHIO_ERROR_MEMORY;
            goto cleanup;
        }
        if (response->success && response->molecular_hash) {
            res->success = true;
            res->molecular_hash = knishio_strdup(response->molecular_hash);
            res->response = response->data ? knishio_strdup(response->data) : NULL;
        } else {
            res->success = false;
            res->error_message = response->errors ?
                knishio_strdup(response->errors) :
                knishio_strdup("Burn failed");
        }
        *result = res;
    }

cleanup:
    if (response) knishio_graphql_response_free(response);
    if (variables) knishio_free(variables);
    if (molecule) knishio_molecule_free(molecule);
    if (remainder_position) knishio_free(remainder_position);
    if (tok_position) knishio_free(tok_position);
    if (tok_amount) knishio_free(tok_amount);
    if (source) knishio_wallet_free(source);
    if (remainder) knishio_wallet_free(remainder);
    if (user) knishio_wallet_free(user);
    return error;
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
