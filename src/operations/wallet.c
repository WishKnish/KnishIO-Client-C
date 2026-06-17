/**
 * @file operations/wallet.c
 * @brief Wallet operations implementation for KnishIO C SDK
 */

#include "knishio/knishio.h"
#include "knishio/operations/wallet.h"
#include "knishio/client_ops.h"
#include "knishio/graphql.h"
#include "knishio/json/parser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* CreateWallet GraphQL mutation template */
static const char* CREATE_WALLET_MUTATION =
    "mutation CreateWallet($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecularHash"
    "    status"
    "    reason"
    "    payload"
    "  }"
    "}";

/* QueryWalletList GraphQL query template */
static const char* QUERY_WALLET_LIST = 
    "query QueryWalletList($bundle: String, $token: String, $includeShadow: Boolean) {"
    "  Wallets(bundleHash: $bundle, token: $token, includeShadow: $includeShadow) {"
    "    address"
    "    bundleHash"
    "    token"
    "    amount"
    "    position"
    "    batchId"
    "    characters"
    "    isShadow"
    "  }"
    "}";

/* QueryWalletBundle GraphQL query template */
static const char* QUERY_WALLET_BUNDLE = 
    "query QueryWalletBundle($bundle: String) {"
    "  WalletBundle(bundleHash: $bundle) {"
    "    bundleHash"
    "    wallets {"
    "      address"
    "      token"
    "      amount"
    "      position"
    "    }"
    "  }"
    "}";

/* QueryContinuId GraphQL query template */
static const char* QUERY_CONTINUID =
    "query QueryContinuId($bundle: String, $token: String) {"
    "  ContinuId(bundle: $bundle, token: $token) {"
    "    position"
    "    address"
    "    tokenSlug"
    "    bundleHash"
    "    pubkey"
    "    characters"
    "  }"
    "}";

/* Create a new wallet */
knishio_error_t knishio_client_create_wallet(
    knishio_client_t* client,
    const knishio_create_wallet_params_t* params,
    knishio_create_wallet_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Validate required parameters */
    if (!params->token) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Cycle 39 (slice 1): build the PARITY-CORRECT wallet-creation molecule (C-atom 'wallet' with
     * the 7 prefixed wallet* keys + a ContinuID I-atom) via knishio_molecule_init_wallet_creation,
     * signed, on a REAL client-secret-derived source wallet. Live submission + auth = next slice. */
    knishio_wallet_t* source = NULL;
    knishio_wallet_t* new_wallet = NULL;
    knishio_wallet_t* remainder = NULL;
    char* remainder_position = NULL;
    knishio_molecule_t* molecule = NULL;
    char* variables = NULL;
    knishio_graphql_response_t* response = NULL;

    /* Source wallet at the bundle's LIVE on-ledger ContinuID position (slice 2c). */
    knishio_error_t error = knishio_client_get_source_wallet_continuid(
        (knishio_client_t*)client, "USER", &source
    );
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* The new wallet being defined (caller's token + position; fall back to a canonical 64-hex
     * position if the caller didn't supply a valid one), from the source secret. */
    {
        const char* new_pos = (params->position && strlen(params->position) == 64)
            ? params->position
            : "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
        error = knishio_wallet_create_simple(&new_wallet, source->secret, params->token, new_pos);
    }
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Remainder (ContinuID I-atom) at a FRESH random position — the bundle's NEXT chain head. */
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

    error = knishio_molecule_create(
        &molecule, source->secret, source->bundle_hash, source, remainder,
        knishio_client_get_cell_slug((knishio_client_t*)client), "V4"
    );
    if (error != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    error = knishio_molecule_init_wallet_creation(molecule, new_wallet, NULL, NULL, 0);
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
            .name = "CreateWallet",
            .query = CREATE_WALLET_MUTATION,
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

    /* Build result */
    {
        knishio_create_wallet_result_t* res = knishio_calloc(1, sizeof(knishio_create_wallet_result_t));
        if (!res) {
            error = KNISHIO_ERROR_MEMORY;
            goto cleanup;
        }
        if (response->success && response->molecular_hash) {
            res->success = true;
            res->wallet = new_wallet;  /* transfer ownership to the caller */
            new_wallet = NULL;         /* so cleanup below does not free it */
            res->molecular_hash = knishio_strdup(response->molecular_hash);
        } else {
            res->success = false;
            res->error_message = response->errors ?
                knishio_strdup(response->errors) :
                knishio_strdup("Wallet creation failed");
        }
        *result = res;
    }

cleanup:
    if (response) knishio_graphql_response_free(response);
    if (variables) knishio_free(variables);
    if (remainder_position) knishio_free(remainder_position);
    if (molecule) knishio_molecule_free(molecule);
    if (source) knishio_wallet_free(source);
    if (new_wallet) knishio_wallet_free(new_wallet);
    if (remainder) knishio_wallet_free(remainder);
    return error;
}

/* Query list of wallets */
knishio_error_t knishio_client_query_wallets(
    knishio_client_t* client,
    const knishio_wallet_list_params_t* params,
    knishio_wallet_list_result_t** result
) {
    if (!client || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables JSON */
    char variables[512] = "{";
    bool first = true;
    
    if (params) {
        if (params->bundle) {
            strcat(variables, "\"bundle\":\"");
            strcat(variables, params->bundle);
            strcat(variables, "\"");
            first = false;
        }
        if (params->token) {
            if (!first) strcat(variables, ",");
            strcat(variables, "\"token\":\"");
            strcat(variables, params->token);
            strcat(variables, "\"");
            first = false;
        }
        if (!first) strcat(variables, ",");
        strcat(variables, "\"includeShadow\":");
        strcat(variables, params->include_shadow ? "true" : "false");
    }
    strcat(variables, "}");
    
    /* Execute query */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryWalletList",
        .query = QUERY_WALLET_LIST,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
    };
    
    knishio_graphql_client_t* graphql_client = (knishio_graphql_client_t*)client;
    knishio_error_t error = knishio_graphql_execute(graphql_client, &operation, &response);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result */
    knishio_wallet_list_result_t* res = knishio_calloc(1, sizeof(knishio_wallet_list_result_t));
    if (!res) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->success && response->data) {
        res->success = true;
        
        /* Parse wallet list from GraphQL response following 2025 C17 best practices */
        knishio_json_t* json_root = knishio_json_parse(response->data, NULL);
        if (json_root) {
            /* Extract Wallets array from data.Wallets */
            knishio_json_t* wallets_array = knishio_json_get_path(json_root, "data.Wallets");
            if (wallets_array && knishio_json_get_type(wallets_array) == KNISHIO_JSON_ARRAY) {
                size_t array_size = knishio_json_array_size(wallets_array);
                
                if (array_size > 0) {
                    /* Allocate wallet array */
                    res->wallets = knishio_calloc(array_size, sizeof(knishio_wallet_t*));
                    if (res->wallets) {
                        res->wallet_count = 0;
                        
                        /* Parse each wallet in the array */
                        for (size_t i = 0; i < array_size; i++) {
                            knishio_json_t* wallet_obj = knishio_json_array_get(wallets_array, i);
                            if (wallet_obj && knishio_json_get_type(wallet_obj) == KNISHIO_JSON_OBJECT) {
                                knishio_wallet_t* wallet = knishio_calloc(1, sizeof(knishio_wallet_t));
                                if (wallet) {
                                    /* Extract wallet fields */
                                    const char* address = knishio_json_get_string_path(wallet_obj, "address");
                                    const char* bundle_hash = knishio_json_get_string_path(wallet_obj, "bundleHash");
                                    const char* token = knishio_json_get_string_path(wallet_obj, "token");
                                    const char* amount = knishio_json_get_string_path(wallet_obj, "amount");
                                    const char* position = knishio_json_get_string_path(wallet_obj, "position");
                                    const char* batch_id = knishio_json_get_string_path(wallet_obj, "batchId");
                                    
                                    /* Populate wallet structure with extracted data */
                                    if (address) wallet->address = knishio_strdup(address);
                                    if (bundle_hash) wallet->bundle_hash = knishio_strdup(bundle_hash);
                                    if (token) wallet->token = knishio_strdup(token);
                                    if (amount) wallet->balance = strtod(amount, NULL);
                                    if (position) wallet->position = knishio_strdup(position);
                                    if (batch_id) wallet->batch_id = knishio_strdup(batch_id);
                                    
                                    /* Extract shadow wallet flag */
                                    bool is_shadow = false;
                                    knishio_json_get_bool_path(wallet_obj, "isShadow", &is_shadow);
                                    wallet->is_shadow = is_shadow;
                                    
                                    res->wallets[res->wallet_count] = wallet;
                                    res->wallet_count++;
                                } else {
                                    /* Memory allocation failed for individual wallet */
                                    break;
                                }
                            }
                        }
                    } else {
                        /* Failed to allocate wallet array */
                        res->success = false;
                        res->error_message = knishio_strdup("Failed to allocate memory for wallet list");
                    }
                } else {
                    /* Empty wallet list */
                    res->wallets = NULL;
                    res->wallet_count = 0;
                }
            } else {
                /* No Wallets array found in response */
                res->wallets = NULL;
                res->wallet_count = 0;
            }
            
            knishio_json_free(json_root);
        } else {
            /* JSON parsing failed */
            res->success = false;
            res->error_message = knishio_strdup("Failed to parse JSON response");
        }
    } else {
        res->success = false;
        res->error_message = response->errors ? 
            knishio_strdup(response->errors) : 
            knishio_strdup("Query failed");
    }
    
    knishio_graphql_response_free(response);
    *result = res;
    
    return KNISHIO_SUCCESS;
}

/* Query wallet bundle */
knishio_error_t knishio_client_query_bundle(
    knishio_client_t* client,
    const char* bundle,
    knishio_wallet_list_result_t** result
) {
    if (!client || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables */
    char variables[256] = "{}";
    if (bundle) {
        snprintf(variables, sizeof(variables), "{\"bundle\":\"%s\"}", bundle);
    }
    
    /* Execute query */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryWalletBundle",
        .query = QUERY_WALLET_BUNDLE,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
    };
    
    knishio_graphql_client_t* graphql_client = (knishio_graphql_client_t*)client;
    knishio_error_t error = knishio_graphql_execute(graphql_client, &operation, &response);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result */
    knishio_wallet_list_result_t* res = knishio_calloc(1, sizeof(knishio_wallet_list_result_t));
    if (!res) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->success && response->data) {
        res->success = true;
        
        /* Parse wallet bundle from GraphQL response following 2025 C17 best practices */
        knishio_json_t* json_root = knishio_json_parse(response->data, NULL);
        if (json_root) {
            /* Extract bundle data from data.WalletBundle */
            knishio_json_t* bundle_obj = knishio_json_get_path(json_root, "data.WalletBundle");
            if (bundle_obj && knishio_json_get_type(bundle_obj) == KNISHIO_JSON_OBJECT) {
                /* Get wallets array from bundle */
                knishio_json_t* wallets_array = knishio_json_get_path(bundle_obj, "wallets");
                if (wallets_array && knishio_json_get_type(wallets_array) == KNISHIO_JSON_ARRAY) {
                    size_t array_size = knishio_json_array_size(wallets_array);
                    
                    if (array_size > 0) {
                        /* Allocate wallet array */
                        res->wallets = knishio_calloc(array_size, sizeof(knishio_wallet_t*));
                        if (res->wallets) {
                            res->wallet_count = 0;
                            
                            /* Parse each wallet in the bundle */
                            for (size_t i = 0; i < array_size; i++) {
                                knishio_json_t* wallet_obj = knishio_json_array_get(wallets_array, i);
                                if (wallet_obj && knishio_json_get_type(wallet_obj) == KNISHIO_JSON_OBJECT) {
                                    knishio_wallet_t* wallet = knishio_calloc(1, sizeof(knishio_wallet_t));
                                    if (wallet) {
                                        /* Extract wallet fields from bundle */
                                        const char* address = knishio_json_get_string_path(wallet_obj, "address");
                                        const char* token = knishio_json_get_string_path(wallet_obj, "token");
                                        const char* amount = knishio_json_get_string_path(wallet_obj, "amount");
                                        const char* position = knishio_json_get_string_path(wallet_obj, "position");
                                        
                                        /* Extract bundle hash from parent object */
                                        const char* bundle_hash = knishio_json_get_string_path(bundle_obj, "bundleHash");
                                        
                                        /* Populate wallet structure */
                                        if (address) wallet->address = knishio_strdup(address);
                                        if (bundle_hash) wallet->bundle_hash = knishio_strdup(bundle_hash);
                                        if (token) wallet->token = knishio_strdup(token);
                                        if (amount) wallet->balance = strtod(amount, NULL);
                                        if (position) wallet->position = knishio_strdup(position);
                                        
                                        res->wallets[res->wallet_count] = wallet;
                                        res->wallet_count++;
                                    } else {
                                        /* Memory allocation failed */
                                        break;
                                    }
                                }
                            }
                        } else {
                            /* Failed to allocate wallet array */
                            res->success = false;
                            res->error_message = knishio_strdup("Failed to allocate memory for bundle wallets");
                        }
                    } else {
                        /* Empty bundle */
                        res->wallets = NULL;
                        res->wallet_count = 0;
                    }
                } else {
                    /* No wallets array in bundle */
                    res->wallets = NULL;
                    res->wallet_count = 0;
                }
            } else {
                /* No WalletBundle found in response */
                res->success = false;
                res->error_message = knishio_strdup("No wallet bundle found in response");
            }
            
            knishio_json_free(json_root);
        } else {
            /* JSON parsing failed */
            res->success = false;
            res->error_message = knishio_strdup("Failed to parse bundle JSON response");
        }
    } else {
        res->success = false;
        res->error_message = response->errors ? 
            knishio_strdup(response->errors) : 
            knishio_strdup("Query failed");
    }
    
    knishio_graphql_response_free(response);
    *result = res;
    
    return KNISHIO_SUCCESS;
}

/* Query ContinuId */
knishio_error_t knishio_client_query_continuId(
    knishio_client_t* client,
    const char* bundle,
    knishio_continuId_result_t** result
) {
    if (!client || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables — the ContinuID chain is the USER token relay. */
    char variables[512] = "{}";
    if (bundle) {
        snprintf(variables, sizeof(variables),
                 "{\"bundle\":\"%s\",\"token\":\"USER\"}", bundle);
    }

    /* Execute query through a proper graphql client (slice 2a/2b transport: TLS + auth header +
     * the valid-body serialization). Was the old (knishio_graphql_client_t*)client cast. */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryContinuId",
        .query = QUERY_CONTINUID,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
    };

    knishio_error_t error = knishio_client_execute_graphql(client, &operation, &response);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Create result */
    knishio_continuId_result_t* res = knishio_calloc(1, sizeof(knishio_continuId_result_t));
    if (!res) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }

    if (response->success && response->data) {
        res->success = true;

        /* The validator's ContinuId returns a Wallet DIRECTLY at data.ContinuId (fields:
         * position/address/tokenSlug/bundleHash/pubkey/characters). A null data.ContinuId means
         * the bundle has no ContinuID yet (genesis) -> res->wallet = NULL (NOT an error).
         * NOTE: knishio_json_get_string_path is use-after-free — navigate with get_path +
         * get_string and COPY each value out BEFORE freeing the node. */
        knishio_json_t* json_root = knishio_json_parse(response->data, NULL);
        if (json_root) {
            knishio_json_t* cid = knishio_json_get_path(json_root, "data.ContinuId");
            if (cid && knishio_json_get_type(cid) == KNISHIO_JSON_OBJECT) {
                knishio_wallet_t* wallet = knishio_calloc(1, sizeof(knishio_wallet_t));
                if (wallet) {
                    static const char* fields[] = {
                        "position", "address", "tokenSlug", "bundleHash", "pubkey", "characters"
                    };
                    char* vals[6] = {0};
                    for (size_t i = 0; i < 6; i++) {
                        knishio_json_t* node = knishio_json_get_path(cid, fields[i]);
                        if (node) {
                            const char* s = knishio_json_get_string(node);
                            if (s) vals[i] = knishio_strdup(s);
                            knishio_json_free(node);
                        }
                    }
                    wallet->position    = vals[0];
                    wallet->address     = vals[1];
                    wallet->token       = vals[2];
                    wallet->bundle_hash = vals[3] ? vals[3] : (bundle ? knishio_strdup(bundle) : NULL);
                    wallet->pubkey      = vals[4];
                    wallet->characters  = vals[5];
                    res->wallet = wallet;
                    if (wallet->bundle_hash) res->bundle_hash = knishio_strdup(wallet->bundle_hash);
                } else {
                    res->success = false;
                    res->error_message = knishio_strdup("Failed to allocate memory for ContinuId wallet");
                }
                knishio_json_free(cid);
            } else {
                /* Genesis: no ContinuID registered for this bundle yet. */
                if (cid) knishio_json_free(cid);
                res->wallet = NULL;
                if (bundle) res->bundle_hash = knishio_strdup(bundle);
            }

            knishio_json_free(json_root);
        } else {
            res->success = false;
            res->error_message = knishio_strdup("Failed to parse ContinuId JSON response");
        }
    } else {
        res->success = false;
        res->error_message = response->errors ?
            knishio_strdup(response->errors) :
            knishio_strdup("Query failed");
    }

    knishio_graphql_response_free(response);
    *result = res;
    
    return KNISHIO_SUCCESS;
}

/* Result management functions */

void knishio_create_wallet_result_free(knishio_create_wallet_result_t* result) {
    if (!result) return;
    
    if (result->wallet) {
        knishio_wallet_cleanup(result->wallet);
        knishio_free(result->wallet);
    }
    knishio_free(result->molecular_hash);
    knishio_free(result->error_message);
    knishio_free(result);
}

void knishio_wallet_list_result_free(knishio_wallet_list_result_t* result) {
    if (!result) return;
    
    if (result->wallets) {
        for (size_t i = 0; i < result->wallet_count; i++) {
            if (result->wallets[i]) {
                knishio_wallet_cleanup(result->wallets[i]);
                knishio_free(result->wallets[i]);
            }
        }
        knishio_free(result->wallets);
    }
    knishio_free(result->error_message);
    knishio_free(result);
}

void knishio_continuId_result_free(knishio_continuId_result_t* result) {
    if (!result) return;
    
    knishio_free(result->bundle_hash);
    if (result->wallet) {
        knishio_wallet_cleanup(result->wallet);
        knishio_free(result->wallet);
    }
    knishio_free(result->error_message);
    knishio_free(result);
}

/* Result accessor functions */

bool knishio_create_wallet_result_is_success(const knishio_create_wallet_result_t* result) {
    return result ? result->success : false;
}

knishio_wallet_t* knishio_create_wallet_result_get_wallet(const knishio_create_wallet_result_t* result) {
    return result ? result->wallet : NULL;
}

size_t knishio_wallet_list_result_get_count(const knishio_wallet_list_result_t* result) {
    return result ? result->wallet_count : 0;
}

knishio_wallet_t* knishio_wallet_list_result_get_wallet(
    const knishio_wallet_list_result_t* result,
    size_t index
) {
    if (!result || index >= result->wallet_count) {
        return NULL;
    }
    return result->wallets[index];
}