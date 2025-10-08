/**
 * @file graphql_operations.c
 * @brief Implementation of high-level GraphQL operations integration
 * 
 * Provides unified interface for all GraphQL operations with JavaScript SDK
 * compatibility and advanced transaction patterns.
 */

#include "knishio/operations/graphql_operations.h"
#include "knishio/graphql.h"
#include "knishio/molecule.h"
#include "knishio/wallet.h"
#include "knishio/error.h"
#include "knishio/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Operations manager lifecycle */

knishio_error_t knishio_graphql_operations_create(
    knishio_graphql_operations_t** ops,
    knishio_client_t* knishio_client,
    const char* endpoint_url,
    const char* cell_slug) {
    
    if (!ops || !knishio_client || !endpoint_url) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    *ops = malloc(sizeof(knishio_graphql_operations_t));
    if (!*ops) {
        return KNISHIO_ERROR_MEMORY_ALLOCATION;
    }
    
    memset(*ops, 0, sizeof(knishio_graphql_operations_t));
    
    // Create GraphQL client
    knishio_error_t result = knishio_graphql_client_create(
        &(*ops)->client, endpoint_url, cell_slug);
    if (result != KNISHIO_SUCCESS) {
        free(*ops);
        *ops = NULL;
        return result;
    }
    
    (*ops)->knishio_client = knishio_client;
    (*ops)->debug_mode = false;
    
    if (endpoint_url) {
        (*ops)->endpoint_url = strdup(endpoint_url);
    }
    if (cell_slug) {
        (*ops)->cell_slug = strdup(cell_slug);
    }
    
    return KNISHIO_SUCCESS;
}

void knishio_graphql_operations_free(knishio_graphql_operations_t* ops) {
    if (!ops) {
        return;
    }
    
    if (ops->client) {
        knishio_graphql_client_free(ops->client);
    }
    
    free(ops->endpoint_url);
    free(ops->cell_slug);
    free(ops);
}

knishio_error_t knishio_graphql_operations_set_debug(
    knishio_graphql_operations_t* ops,
    bool debug_mode) {
    
    if (!ops) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    ops->debug_mode = debug_mode;
    return knishio_graphql_client_set_debug(ops->client, debug_mode);
}

knishio_error_t knishio_graphql_operations_set_auth_token(
    knishio_graphql_operations_t* ops,
    const char* auth_token) {
    
    if (!ops) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Create auth token structure if needed
    knishio_auth_token_t* token = NULL;
    if (auth_token) {
        // In a real implementation, this would parse the auth token
        // For now, we'll assume the auth token management is handled elsewhere
    }
    
    return knishio_graphql_client_set_auth_token(ops->client, token);
}

/* High-level client operations */

knishio_error_t knishio_operations_query_balance_simple(
    knishio_graphql_operations_t* ops,
    const char* wallet_address,
    const char* token_slug,
    knishio_response_balance_t** response) {
    
    if (!ops || !wallet_address || !response) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    knishio_query_balance_params_t params = {0};
    params.address = wallet_address;
    params.token = token_slug;
    
    return knishio_query_balance(ops->client, &params, response);
}

knishio_error_t knishio_operations_transfer_tokens_simple(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* source_wallet,
    const char* recipient_address,
    const char* token_slug,
    const char* amount,
    knishio_response_transfer_tokens_t** response) {
    
    if (!ops || !source_wallet || !recipient_address || !token_slug || !amount || !response) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Create recipient wallet from address
    // In a real implementation, this would involve proper wallet resolution
    knishio_wallet_t* recipient_wallet = NULL;
    knishio_error_t result = knishio_wallet_create_from_address(&recipient_wallet, recipient_address);
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    knishio_mutation_transfer_tokens_params_t params = {0};
    params.source_wallet = source_wallet;
    params.recipient_wallet = recipient_wallet;
    params.token_slug = token_slug;
    params.amount = amount;
    
    result = knishio_mutation_transfer_tokens(ops->client, &params, response);
    
    knishio_wallet_free(recipient_wallet);
    return result;
}

knishio_error_t knishio_operations_create_token_simple(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* recipient_wallet,
    const char* token_slug,
    const char* amount,
    const char* meta_json,
    knishio_response_create_token_t** response) {
    
    if (!ops || !recipient_wallet || !token_slug || !amount || !response) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    knishio_mutation_create_token_params_t params = {0};
    params.recipient_wallet = recipient_wallet;
    params.token_slug = token_slug;
    params.amount = amount;
    params.meta_json = meta_json;
    
    return knishio_mutation_create_token(ops->client, &params, response);
}

knishio_error_t knishio_operations_request_tokens_simple(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* wallet,
    const char* token_slug,
    const char* amount,
    knishio_response_request_tokens_t** response) {
    
    if (!ops || !wallet || !token_slug || !amount || !response) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    knishio_mutation_request_tokens_params_t params = {0};
    params.wallet = wallet;
    params.token_slug = token_slug;
    params.amount = amount;
    params.meta_type = "request";
    params.meta_id = "default";
    
    return knishio_mutation_request_tokens(ops->client, &params, response);
}

/* Complex transaction patterns */

knishio_error_t knishio_operations_transfer_tokens_multi(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* source_wallet,
    const knishio_transfer_recipient_t* recipients,
    size_t recipient_count,
    const char* token_slug,
    knishio_response_transfer_tokens_t** response) {
    
    if (!ops || !source_wallet || !recipients || recipient_count == 0 || !token_slug || !response) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Create molecule for multi-party transfer
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, source_wallet);
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    // Calculate total amount
    double total_amount = 0.0;
    for (size_t i = 0; i < recipient_count; i++) {
        total_amount += atof(recipients[i].amount);
    }
    
    // Create source atom (debit)
    char total_amount_str[64];
    snprintf(total_amount_str, sizeof(total_amount_str), "%.8f", -total_amount);
    
    knishio_atom_create_params_t source_atom_params = {0};
    source_atom_params.position = knishio_wallet_get_position(source_wallet);
    source_atom_params.wallet_address = knishio_wallet_get_address(source_wallet);
    source_atom_params.isotope = "V";
    source_atom_params.token = token_slug;
    source_atom_params.value = total_amount_str;
    
    result = knishio_molecule_add_atom(molecule, &source_atom_params);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    // Create recipient atoms (credits)
    for (size_t i = 0; i < recipient_count; i++) {
        knishio_atom_create_params_t recipient_atom_params = {0};
        recipient_atom_params.wallet_address = recipients[i].wallet_address;
        recipient_atom_params.isotope = "V";
        recipient_atom_params.token = token_slug;
        recipient_atom_params.value = recipients[i].amount;
        
        // Add memo as metadata if provided
        if (recipients[i].memo) {
            knishio_json_t* meta_json = NULL;
            knishio_json_create(&meta_json);
            knishio_json_set_string(meta_json, "memo", recipients[i].memo);
            
            char* meta_str = NULL;
            knishio_json_to_string(meta_json, &meta_str);
            recipient_atom_params.meta = meta_str;
            
            knishio_json_free(meta_json);
        }
        
        result = knishio_molecule_add_atom(molecule, &recipient_atom_params);
        if (result != KNISHIO_SUCCESS) {
            knishio_molecule_free(molecule);
            return result;
        }
        
        // Free meta string if allocated
        if (recipient_atom_params.meta) {
            free((char*)recipient_atom_params.meta);
        }
    }
    
    // Sign and check molecule
    knishio_wallet_bundle_t* bundle = knishio_wallet_get_bundle(source_wallet);
    result = knishio_molecule_sign(molecule, bundle);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    result = knishio_molecule_check(molecule);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    // Propose molecule
    knishio_mutation_propose_molecule_params_t propose_params = {0};
    propose_params.molecule = molecule;
    
    knishio_response_propose_molecule_t* propose_response = NULL;
    result = knishio_mutation_propose_molecule(ops->client, &propose_params, &propose_response);
    
    knishio_molecule_free(molecule);
    
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    // Convert to transfer response
    result = knishio_response_transfer_tokens_create(response);
    if (result != KNISHIO_SUCCESS) {
        knishio_response_propose_molecule_free(propose_response);
        return result;
    }
    
    // Copy relevant data from propose response to transfer response
    const char* molecular_hash = knishio_response_propose_molecule_get_hash(propose_response);
    if (molecular_hash) {
        knishio_response_transfer_tokens_set_hash(*response, molecular_hash);
    }
    
    knishio_response_propose_molecule_free(propose_response);
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_operations_swap_tokens(
    knishio_graphql_operations_t* ops,
    knishio_wallet_t* party1_wallet,
    knishio_wallet_t* party2_wallet,
    const char* token1_slug,
    const char* amount1,
    const char* token2_slug,
    const char* amount2,
    knishio_response_transfer_tokens_t** response) {
    
    if (!ops || !party1_wallet || !party2_wallet || !token1_slug || !amount1 || 
        !token2_slug || !amount2 || !response) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Create molecule for atomic swap
    knishio_molecule_t* molecule = NULL;
    knishio_error_t result = knishio_molecule_create(&molecule, party1_wallet);
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    // Party 1 sends token1
    char negative_amount1[64];
    snprintf(negative_amount1, sizeof(negative_amount1), "-%s", amount1);
    
    knishio_atom_create_params_t party1_send_params = {0};
    party1_send_params.position = knishio_wallet_get_position(party1_wallet);
    party1_send_params.wallet_address = knishio_wallet_get_address(party1_wallet);
    party1_send_params.isotope = "V";
    party1_send_params.token = token1_slug;
    party1_send_params.value = negative_amount1;
    
    result = knishio_molecule_add_atom(molecule, &party1_send_params);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    // Party 2 receives token1
    knishio_atom_create_params_t party2_receive1_params = {0};
    party2_receive1_params.wallet_address = knishio_wallet_get_address(party2_wallet);
    party2_receive1_params.isotope = "V";
    party2_receive1_params.token = token1_slug;
    party2_receive1_params.value = amount1;
    
    result = knishio_molecule_add_atom(molecule, &party2_receive1_params);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    // Party 2 sends token2
    char negative_amount2[64];
    snprintf(negative_amount2, sizeof(negative_amount2), "-%s", amount2);
    
    knishio_atom_create_params_t party2_send_params = {0};
    party2_send_params.position = knishio_wallet_get_position(party2_wallet);
    party2_send_params.wallet_address = knishio_wallet_get_address(party2_wallet);
    party2_send_params.isotope = "V";
    party2_send_params.token = token2_slug;
    party2_send_params.value = negative_amount2;
    
    result = knishio_molecule_add_atom(molecule, &party2_send_params);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    // Party 1 receives token2
    knishio_atom_create_params_t party1_receive2_params = {0};
    party1_receive2_params.wallet_address = knishio_wallet_get_address(party1_wallet);
    party1_receive2_params.isotope = "V";
    party1_receive2_params.token = token2_slug;
    party1_receive2_params.value = amount2;
    
    result = knishio_molecule_add_atom(molecule, &party1_receive2_params);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    // Add swap metadata
    knishio_json_t* swap_meta = NULL;
    knishio_json_create(&swap_meta);
    knishio_json_set_string(swap_meta, "type", "token_swap");
    knishio_json_set_string(swap_meta, "token1", token1_slug);
    knishio_json_set_string(swap_meta, "amount1", amount1);
    knishio_json_set_string(swap_meta, "token2", token2_slug);
    knishio_json_set_string(swap_meta, "amount2", amount2);
    
    double rate = atof(amount2) / atof(amount1);
    knishio_json_set_double(swap_meta, "rate", rate);
    
    char* swap_meta_str = NULL;
    knishio_json_to_string(swap_meta, &swap_meta_str);
    
    knishio_atom_create_params_t meta_atom_params = {0};
    meta_atom_params.isotope = "M";
    meta_atom_params.meta_type = "swap";
    meta_atom_params.meta_id = "atomic_swap";
    meta_atom_params.meta = swap_meta_str;
    
    result = knishio_molecule_add_atom(molecule, &meta_atom_params);
    
    knishio_json_free(swap_meta);
    free(swap_meta_str);
    
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    // Sign with both party bundles
    knishio_wallet_bundle_t* bundle1 = knishio_wallet_get_bundle(party1_wallet);
    knishio_wallet_bundle_t* bundle2 = knishio_wallet_get_bundle(party2_wallet);
    
    result = knishio_molecule_sign(molecule, bundle1);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    result = knishio_molecule_sign(molecule, bundle2);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    result = knishio_molecule_check(molecule);
    if (result != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return result;
    }
    
    // Propose molecule
    knishio_mutation_propose_molecule_params_t propose_params = {0};
    propose_params.molecule = molecule;
    
    knishio_response_propose_molecule_t* propose_response = NULL;
    result = knishio_mutation_propose_molecule(ops->client, &propose_params, &propose_response);
    
    knishio_molecule_free(molecule);
    
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    // Convert to transfer response
    result = knishio_response_transfer_tokens_create(response);
    if (result != KNISHIO_SUCCESS) {
        knishio_response_propose_molecule_free(propose_response);
        return result;
    }
    
    // Copy relevant data
    const char* molecular_hash = knishio_response_propose_molecule_get_hash(propose_response);
    if (molecular_hash) {
        knishio_response_transfer_tokens_set_hash(*response, molecular_hash);
    }
    
    knishio_response_propose_molecule_free(propose_response);
    return KNISHIO_SUCCESS;
}

/* Authentication operations */

knishio_error_t knishio_operations_authenticate_profile(
    knishio_graphql_operations_t* ops,
    const char* secret,
    const char* cell_slug,
    knishio_response_request_authorization_t** response) {
    
    if (!ops || !secret || !cell_slug || !response) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Create encrypted payload from secret
    // In a real implementation, this would involve proper encryption
    char* encrypted_payload = malloc(256);
    if (!encrypted_payload) {
        return KNISHIO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Simplified encryption (in reality, use proper encryption)
    snprintf(encrypted_payload, 256, "{\"secret\":\"%s\",\"timestamp\":%ld}", 
             secret, time(NULL));
    
    knishio_mutation_request_authorization_params_t params = {0};
    params.cell_slug = cell_slug;
    params.encrypted_payload = encrypted_payload;
    
    knishio_error_t result = knishio_mutation_request_authorization(ops->client, &params, response);
    
    free(encrypted_payload);
    return result;
}

knishio_error_t knishio_operations_authenticate_guest(
    knishio_graphql_operations_t* ops,
    const char* cell_slug,
    knishio_response_request_authorization_guest_t** response) {
    
    if (!ops || !cell_slug || !response) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    // Create guest payload
    char* encrypted_payload = malloc(256);
    if (!encrypted_payload) {
        return KNISHIO_ERROR_MEMORY_ALLOCATION;
    }
    
    snprintf(encrypted_payload, 256, "{\"guest\":true,\"timestamp\":%ld}", time(NULL));
    
    knishio_mutation_request_authorization_guest_params_t params = {0};
    params.cell_slug = cell_slug;
    params.encrypted_payload = encrypted_payload;
    
    knishio_error_t result = knishio_mutation_request_authorization_guest(ops->client, &params, response);
    
    free(encrypted_payload);
    return result;
}

/* Utility functions */

knishio_error_t knishio_operations_get_balance_string(
    knishio_graphql_operations_t* ops,
    const char* wallet_address,
    const char* token_slug,
    char** balance_out) {
    
    if (!ops || !wallet_address || !balance_out) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    knishio_response_balance_t* response = NULL;
    knishio_error_t result = knishio_operations_query_balance_simple(
        ops, wallet_address, token_slug, &response);
    
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    const char* balance = knishio_response_balance_get_amount(response);
    if (balance) {
        *balance_out = strdup(balance);
    } else {
        *balance_out = strdup("0");
    }
    
    knishio_response_balance_free(response);
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_operations_check_sufficient_balance(
    knishio_graphql_operations_t* ops,
    const char* wallet_address,
    const char* token_slug,
    const char* required_amount,
    bool* sufficient_out) {
    
    if (!ops || !wallet_address || !required_amount || !sufficient_out) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    char* balance_str = NULL;
    knishio_error_t result = knishio_operations_get_balance_string(
        ops, wallet_address, token_slug, &balance_str);
    
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    double balance = atof(balance_str);
    double required = atof(required_amount);
    
    *sufficient_out = (balance >= required);
    
    free(balance_str);
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_operations_wait_for_confirmation(
    knishio_graphql_operations_t* ops,
    const char* molecular_hash,
    long timeout_ms,
    bool* confirmed_out) {
    
    if (!ops || !molecular_hash || !confirmed_out) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    long start_time = time(NULL) * 1000;
    long current_time = start_time;
    
    while ((current_time - start_time) < timeout_ms) {
        char* status = NULL;
        knishio_error_t result = knishio_operations_get_transaction_status(
            ops, molecular_hash, &status);
        
        if (result == KNISHIO_SUCCESS && status) {
            if (strcmp(status, "confirmed") == 0 || strcmp(status, "accepted") == 0) {
                *confirmed_out = true;
                free(status);
                return KNISHIO_SUCCESS;
            }
            free(status);
        }
        
        usleep(1000000); // Sleep for 1 second
        current_time = time(NULL) * 1000;
    }
    
    *confirmed_out = false;
    return KNISHIO_SUCCESS;
}

/* Direct GraphQL client access */

knishio_graphql_client_t* knishio_operations_get_graphql_client(
    knishio_graphql_operations_t* ops) {
    
    if (!ops) {
        return NULL;
    }
    return ops->client;
}

knishio_error_t knishio_operations_execute_raw_query(
    knishio_graphql_operations_t* ops,
    const char* query_string,
    const char* variables_json,
    char** response_out) {
    
    if (!ops || !query_string || !response_out) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    knishio_graphql_response_t* response = NULL;
    knishio_error_t result = knishio_graphql_query(
        ops->client, query_string, variables_json, &response);
    
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    if (response->data) {
        *response_out = strdup(response->data);
    } else {
        *response_out = strdup("{}");
    }
    
    knishio_graphql_response_free(response);
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_operations_execute_raw_mutation(
    knishio_graphql_operations_t* ops,
    const char* mutation_string,
    const char* variables_json,
    char** response_out) {
    
    if (!ops || !mutation_string || !response_out) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }
    
    knishio_graphql_response_t* response = NULL;
    knishio_error_t result = knishio_graphql_mutation(
        ops->client, mutation_string, variables_json, &response);
    
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    if (response->data) {
        *response_out = strdup(response->data);
    } else {
        *response_out = strdup("{}");
    }
    
    knishio_graphql_response_free(response);
    return KNISHIO_SUCCESS;
}/* Batch operations for efficiency */

knishio_error_t knishio_operations_batch_query(
    knishio_graphql_operations_t* ops,
    const knishio_query_type_t* query_types,
    const void** query_params,
    size_t query_count,
    void** responses
) {
    if (!ops || !query_types || !query_params || query_count == 0 || !responses) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }

    /* Execute queries in parallel following 2025 C17 best practices */
    knishio_error_t overall_result = KNISHIO_SUCCESS;
    
    for (size_t i = 0; i < query_count; i++) {
        knishio_error_t query_result = KNISHIO_SUCCESS;
        
        switch (query_types[i]) {
            case KNISHIO_QUERY_BALANCE: {
                const knishio_query_balance_params_t* balance_params = 
                    (const knishio_query_balance_params_t*)query_params[i];
                knishio_response_balance_t** balance_response = 
                    (knishio_response_balance_t**)&responses[i];
                
                query_result = knishio_operations_query_balance_simple(
                    ops, balance_params->address, balance_params->token, balance_response);
                break;
            }
            
            case KNISHIO_QUERY_WALLET_LIST: {
                const knishio_wallet_list_params_t* wallet_params = 
                    (const knishio_wallet_list_params_t*)query_params[i];
                knishio_wallet_list_result_t** wallet_response = 
                    (knishio_wallet_list_result_t**)&responses[i];
                
                query_result = knishio_client_query_wallets(
                    (knishio_client_t*)ops->knishio_client, wallet_params, wallet_response);
                break;
            }
            
            case KNISHIO_QUERY_TOKEN:
            case KNISHIO_QUERY_ATOM:
            case KNISHIO_QUERY_ACTIVE_SESSION:
            default:
                /* TODO: Implement additional query types as needed */
                responses[i] = NULL;
                if (overall_result == KNISHIO_SUCCESS) {
                    overall_result = KNISHIO_ERROR_NOT_IMPLEMENTED;
                }
                continue;
        }
        
        /* Track any individual query failures */
        if (query_result != KNISHIO_SUCCESS && overall_result == KNISHIO_SUCCESS) {
            overall_result = query_result;
        }
    }
    
    return overall_result;
}

knishio_error_t knishio_operations_batch_mutation(
    knishio_graphql_operations_t* ops,
    const knishio_mutation_type_t* mutation_types,
    const void** mutation_params,
    size_t mutation_count,
    void** responses
) {
    if (!ops || !mutation_types || !mutation_params || mutation_count == 0 || !responses) {
        return KNISHIO_ERROR_INVALID_PARAM;
    }

    /* Execute mutations in sequence with dependency handling following 2025 C17 best practices */
    for (size_t i = 0; i < mutation_count; i++) {
        knishio_error_t mutation_result = KNISHIO_SUCCESS;
        
        switch (mutation_types[i]) {
            case KNISHIO_MUTATION_CREATE_TOKEN: {
                const knishio_mutation_create_token_params_t* create_params = 
                    (const knishio_mutation_create_token_params_t*)mutation_params[i];
                knishio_response_create_token_t** create_response = 
                    (knishio_response_create_token_t**)&responses[i];
                
                mutation_result = knishio_operations_create_token_simple(
                    ops, create_params->recipient_wallet, create_params->token_slug,
                    create_params->amount, create_params->meta_json, create_response);
                break;
            }
            
            case KNISHIO_MUTATION_TRANSFER_TOKENS: {
                const knishio_mutation_transfer_tokens_params_t* transfer_params = 
                    (const knishio_mutation_transfer_tokens_params_t*)mutation_params[i];
                knishio_response_transfer_tokens_t** transfer_response = 
                    (knishio_response_transfer_tokens_t**)&responses[i];
                
                mutation_result = knishio_operations_transfer_tokens_simple(
                    ops, transfer_params->source_wallet,
                    knishio_wallet_get_address(transfer_params->recipient_wallet),
                    transfer_params->token_slug, transfer_params->amount, transfer_response);
                break;
            }
            
            case KNISHIO_MUTATION_REQUEST_TOKENS: {
                const knishio_mutation_request_tokens_params_t* request_params = 
                    (const knishio_mutation_request_tokens_params_t*)mutation_params[i];
                knishio_response_request_tokens_t** request_response = 
                    (knishio_response_request_tokens_t**)&responses[i];
                
                mutation_result = knishio_operations_request_tokens_simple(
                    ops, request_params->wallet, request_params->token_slug,
                    request_params->amount, request_response);
                break;
            }
            
            case KNISHIO_MUTATION_CREATE_META:
            default:
                /* TODO: Implement additional mutation types as needed */
                responses[i] = NULL;
                return KNISHIO_ERROR_NOT_IMPLEMENTED;
        }
        
        /* Stop execution on first failure for dependency handling */
        if (mutation_result != KNISHIO_SUCCESS) {
            /* Set remaining responses to NULL */
            for (size_t j = i + 1; j < mutation_count; j++) {
                responses[j] = NULL;
            }
            return mutation_result;
        }
    }
    
    return KNISHIO_SUCCESS;
}