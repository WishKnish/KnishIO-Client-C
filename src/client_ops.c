/**
 * @file client_ops.c
 * @brief Unified client operations implementation for KnishIO C SDK
 * 
 * Implements high-level operations matching JavaScript SDK functionality.
 */

#include "knishio/knishio.h"
#include "knishio/client_ops.h"
#include "knishio/operations/transfer.h"
#include "knishio/operations/wallet.h"
#include "knishio/operations/token.h"
#include "knishio/operations/auth.h"
#include "knishio/graphql.h"
#include "knishio/auth_token.h"
#include "knishio/crypto/shake256.h"
#include "client_internal.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Simple client state for secret storage (since client_auth.c is disabled) */
typedef struct {
    char* secret;
} knishio_simple_client_state_t;

/* Global storage for client secrets (temporary solution) */
static knishio_simple_client_state_t g_client_state = {0};

/* Generate simple bundle hash from secret using SHAKE256 (equivalent to JS generateBundleHash) */
static char* knishio_generate_simple_bundle_hash(const char* secret) {
    if (!secret) {
        return NULL;
    }
    
    char* bundle_hash = NULL;
    bool result = knishio_shake256_hash(secret, 512, &bundle_hash);  /* 512 bits = 64 bytes like JS SDK */
    
    if (!result || !bundle_hash) {
        return NULL;
    }
    
    return bundle_hash;
}

/* Authenticate with profile credentials: store the secret, build + submit a U-isotope auth
 * molecule, and set the resulting bundle-scoped JWT as the client auth token (mirrors JS
 * requestAuthToken -> requestProfileAuthToken). */
knishio_error_t knishio_client_authenticate(
    knishio_client_t* client,
    const char* secret,
    bool encrypt
) {
    if (!client || !secret) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Store secret in global state (get_source_wallet reads it). */
    if (g_client_state.secret) {
        free(g_client_state.secret);
    }
    g_client_state.secret = knishio_strdup(secret);

    knishio_request_profile_auth_token_params_t params = {
        .secret = secret,
        .encrypt = encrypt
    };
    knishio_request_profile_auth_token_result_t* result = NULL;
    knishio_error_t error = knishio_client_request_profile_auth_token(client, &params, &result);

    bool authorized = (error == KNISHIO_SUCCESS && result && result->success && result->token);
    if (result) {
        knishio_request_profile_auth_token_result_free(result);
    }
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    return authorized ? KNISHIO_SUCCESS : KNISHIO_ERROR_AUTHORIZATION_REJECTED;
}

knishio_error_t knishio_client_configure_auth(
    knishio_client_t* client,
    const knishio_client_auth_config_t* config
) {
    /* Store secret in global state for now */
    if (config && config->secret) {
        if (g_client_state.secret) {
            free(g_client_state.secret);
        }
        g_client_state.secret = knishio_strdup(config->secret);
    }
    return KNISHIO_SUCCESS;
}

/* Initialize client with authentication */
knishio_error_t knishio_client_init_authenticated(
    knishio_client_t** client,
    const char* uri,
    const char* secret,
    const char* cell_slug
) {
    if (!client || !uri || !secret) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Create client configuration */
    knishio_client_config_t config = {
        .uri = uri,
        .cell_slug = cell_slug,
        .client = NULL,
        .socket = NULL,
        .server_sdk_version = 4,
        .logging = false
    };
    
    /* Create client */
    knishio_error_t error = knishio_client_create(client, &config);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Set secret */
    error = knishio_client_set_secret(*client, secret);
    if (error != KNISHIO_SUCCESS) {
        knishio_client_destroy(*client);
        *client = NULL;
        return error;
    }
    
    /* Authenticate with profile credentials using the new auth system */
    error = knishio_client_authenticate(*client, secret, false);
    if (error != KNISHIO_SUCCESS) {
        knishio_client_destroy(*client);
        *client = NULL;
        return error;
    }
    
    return KNISHIO_SUCCESS;
}

/* Get client bundle hash */
const char* knishio_client_get_bundle(knishio_client_t* client) {
    if (!client) {
        return NULL;
    }
    
    /* Check if we have a stored secret in global state */
    if (!g_client_state.secret) {
        return NULL;
    }
    
    /* Generate and return bundle hash from secret */
    return knishio_generate_simple_bundle_hash(g_client_state.secret);
}

/* Set client secret */
knishio_error_t knishio_client_set_secret(
    knishio_client_t* client,
    const char* secret
) {
    if (!client || !secret) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Configure authentication with the secret */
    knishio_client_auth_config_t config = {
        .secret = (char*)secret,
        .encrypt = false,
        .auto_refresh = true,
        .refresh_threshold_ms = 300000  /* 5 minutes */
    };
    
    return knishio_client_configure_auth(client, &config);
}

/* Get source wallet for operations — derives a REAL wallet from the client's stored
 * secret (g_client_state.secret, set via knishio_client_set_secret). The returned wallet
 * carries secret/position/address/bundle, so callers can build recipient/remainder wallets
 * from wallet->secret and sign molecules.
 *
 * NOTE (cycle 39, slice 1): the source position is the canonical KNISHIO_FIXED_POSITION
 * placeholder. The live ContinuID-position query (so the source wallet binds to the bundle's
 * current on-ledger position) is the next slice. */
knishio_error_t knishio_client_get_source_wallet(
    knishio_client_t* client,
    const char* token,
    knishio_wallet_t** wallet
) {
    if (!client || !wallet) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    if (!g_client_state.secret) {
        return KNISHIO_ERROR_INVALID_STATE;  /* no secret set — call knishio_client_set_secret first */
    }

    return knishio_wallet_create_simple(
        wallet,
        g_client_state.secret,
        token ? token : "USER",
        KNISHIO_FIXED_POSITION
    );
}

/* Resolve the SIGNING source wallet at the bundle's live on-ledger ContinuID position (slice 2c,
 * mirrors JS getSourceWallet). Queries ContinuId(bundle, "USER"); if a 64-char position is
 * returned, the wallet is built there so it binds to the bundle's current chain head and passes
 * the validator's ContinuID chain validation; otherwise (genesis / no ContinuID yet) it falls
 * back to KNISHIO_FIXED_POSITION. */
knishio_error_t knishio_client_get_source_wallet_continuid(
    knishio_client_t* client,
    const char* token,
    knishio_wallet_t** wallet
) {
    if (!client || !wallet) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!g_client_state.secret) {
        return KNISHIO_ERROR_INVALID_STATE;
    }

    const char* tok = token ? token : "USER";

    /* Derive the bundle hash from the secret (temp wallet at the fixed position). */
    knishio_wallet_t* tmp = NULL;
    knishio_error_t error = knishio_wallet_create_simple(
        &tmp, g_client_state.secret, tok, KNISHIO_FIXED_POSITION);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Query the live ContinuID position for the bundle; fall back to the fixed position (genesis). */
    const char* position = KNISHIO_FIXED_POSITION;
    knishio_continuId_result_t* cid = NULL;
    if (knishio_client_query_continuId(client, tmp->bundle_hash, &cid) == KNISHIO_SUCCESS
        && cid && cid->wallet && cid->wallet->position
        && strlen(cid->wallet->position) == 64) {
        position = cid->wallet->position;
    }

    error = knishio_wallet_create_simple(wallet, g_client_state.secret, tok, position);

    if (cid) knishio_continuId_result_free(cid);
    knishio_wallet_free(tmp);
    return error;
}

/* Create and sign a molecule */
knishio_error_t knishio_client_create_molecule(
    knishio_client_t* client,
    knishio_wallet_t* source_wallet,
    const char* cell_slug,
    knishio_molecule_t** molecule
) {
    if (!client || !molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Get wallet properties */
    const char* secret = source_wallet ? source_wallet->secret : NULL;
    const char* bundle = source_wallet ? source_wallet->bundle_hash : NULL;
    
    /* Create molecule */
    return knishio_molecule_create(
        molecule,
        secret,
        bundle,
        source_wallet,
        NULL,  /* remainder wallet */
        cell_slug,
        "V4"   /* protocol version */
    );
}

/* Propose a molecule to the network */
knishio_error_t knishio_client_propose_molecule(
    knishio_client_t* client,
    knishio_molecule_t* molecule,
    char** molecular_hash
) {
    if (!client || !molecule || !molecular_hash) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Convert molecule to JSON */
    char* molecule_json = NULL;
    knishio_error_t error = knishio_molecule_to_json(molecule, &molecule_json);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Build ProposeMolecule mutation (camelCase response fields — the validator's MoleculeResponse
     * exposes molecularHash, not molecular_hash). */
    const char* mutation =
        "mutation ProposeMolecule($molecule: MoleculeInput!) {"
        "  ProposeMolecule(molecule: $molecule) {"
        "    molecularHash"
        "    status"
        "    reason"
        "    payload"
        "  }"
        "}";
    
    /* Build variables */
    size_t var_len = strlen(molecule_json) + 32;
    char* variables = knishio_malloc(var_len);
    if (!variables) {
        knishio_free(molecule_json);
        return KNISHIO_ERROR_MEMORY;
    }
    snprintf(variables, var_len, "{\"molecule\":%s}", molecule_json);
    knishio_free(molecule_json);
    
    /* Execute mutation */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "ProposeMolecule",
        .query = mutation,
        .variables_json = variables,
        .requires_auth = true,
        .is_mutation = true
    };
    
    /* Submit through a proper graphql client (built from the client; auth token propagates),
     * not the old (knishio_graphql_client_t*)client cast. */
    error = knishio_client_execute_graphql(client, &operation, &response);
    knishio_free(variables);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Extract molecular hash from response */
    if (response->success && response->molecular_hash) {
        *molecular_hash = knishio_strdup(response->molecular_hash);
    } else {
        *molecular_hash = NULL;
        error = KNISHIO_ERROR_INVALID_RESPONSE;
    }
    
    knishio_graphql_response_free(response);
    return error;
}

/* Query batch information */
knishio_error_t knishio_client_query_batch(
    knishio_client_t* client,
    const char* batch_id,
    char** result
) {
    if (!client || !batch_id || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build query */
    const char* query = 
        "query QueryBatch($batchId: String!) {"
        "  Batch(batch_id: $batchId) {"
        "    batch_id"
        "    status"
        "    count"
        "    molecules {"
        "      molecular_hash"
        "      status"
        "    }"
        "  }"
        "}";
    
    /* Build variables */
    char variables[256];
    snprintf(variables, sizeof(variables), "{\"batchId\":\"%s\"}", batch_id);
    
    /* Execute query */
    return knishio_client_execute_query(client, query, variables, result);
}

/* Query batch history */
knishio_error_t knishio_client_query_batch_history(
    knishio_client_t* client,
    const char* batch_id,
    char** result
) {
    if (!client || !batch_id || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build query */
    const char* query = 
        "query QueryBatchHistory($batchId: String!) {"
        "  BatchHistory(batch_id: $batchId) {"
        "    batch_id"
        "    history {"
        "      timestamp"
        "      status"
        "      molecular_hash"
        "    }"
        "  }"
        "}";
    
    /* Build variables */
    char variables[256];
    snprintf(variables, sizeof(variables), "{\"batchId\":\"%s\"}", batch_id);
    
    /* Execute query */
    return knishio_client_execute_query(client, query, variables, result);
}

/* Create metadata */
knishio_error_t knishio_client_create_meta(
    knishio_client_t* client,
    const char* meta_type,
    const char* meta_id,
    knishio_meta_t** meta,
    size_t meta_count,
    char** molecular_hash
) {
    if (!client || !meta_type || !molecular_hash) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Get source wallet */
    knishio_wallet_t* wallet = NULL;
    knishio_error_t error = knishio_client_get_source_wallet(client, "USER", &wallet);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create molecule */
    knishio_molecule_t* molecule = NULL;
    error = knishio_client_create_molecule(client, wallet, "meta", &molecule);
    if (error != KNISHIO_SUCCESS) {
        knishio_wallet_cleanup(wallet);
        knishio_free(wallet);
        return error;
    }
    
    /* Create meta atom */
    knishio_atom_t* atom = NULL;
    error = knishio_atom_create_with_meta(
        &atom,
        wallet->position,
        wallet->address,
        KNISHIO_ISOTOPE_M,  /* Meta isotope */
        "USER",
        NULL,  /* no value for meta */
        NULL,  /* no batch ID */
        meta_type,
        meta_id,
        meta,
        meta_count
    );
    
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
    error = knishio_client_propose_molecule(client, molecule, molecular_hash);
    
    /* Cleanup */
    knishio_molecule_free(molecule);
    knishio_wallet_cleanup(wallet);
    knishio_free(wallet);
    
    return error;
}

/* Query metadata */
knishio_error_t knishio_client_query_meta(
    knishio_client_t* client,
    const char* meta_type,
    const char* meta_id,
    char** result
) {
    if (!client || !meta_type || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build query */
    const char* query = 
        "query QueryMeta($metaType: String!, $metaId: String) {"
        "  Meta(metaType: $metaType, metaId: $metaId) {"
        "    meta_type"
        "    meta_id"
        "    meta {"
        "      key"
        "      value"
        "    }"
        "  }"
        "}";
    
    /* Build variables */
    char variables[512];
    if (meta_id) {
        snprintf(variables, sizeof(variables), 
                "{\"metaType\":\"%s\",\"metaId\":\"%s\"}", 
                meta_type, meta_id);
    } else {
        snprintf(variables, sizeof(variables), 
                "{\"metaType\":\"%s\"}", meta_type);
    }
    
    /* Execute query */
    return knishio_client_execute_query(client, query, variables, result);
}

/* Execute raw GraphQL query */
knishio_error_t knishio_client_execute_query(
    knishio_client_t* client,
    const char* query,
    const char* variables,
    char** result
) {
    if (!client || !query || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Execute query via GraphQL client */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "CustomQuery",
        .query = query,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
    };
    
    /* Cast client to GraphQL client for now */
    knishio_graphql_client_t* graphql_client = (knishio_graphql_client_t*)client;
    knishio_error_t error = knishio_graphql_execute(graphql_client, &operation, &response);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Return response data */
    if (response->success && response->data) {
        *result = knishio_strdup(response->data);
    } else {
        *result = NULL;
        error = KNISHIO_ERROR_INVALID_RESPONSE;
    }
    
    knishio_graphql_response_free(response);
    return error;
}

/* Execute raw GraphQL mutation */
knishio_error_t knishio_client_execute_mutation(
    knishio_client_t* client,
    const char* mutation,
    const char* variables,
    char** result
) {
    if (!client || !mutation || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Execute mutation via GraphQL client */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "CustomMutation",
        .query = mutation,
        .variables_json = variables,
        .requires_auth = true,
        .is_mutation = true
    };
    
    /* Cast client to GraphQL client for now */
    knishio_graphql_client_t* graphql_client = (knishio_graphql_client_t*)client;
    knishio_error_t error = knishio_graphql_execute(graphql_client, &operation, &response);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Return response data */
    if (response->success && response->data) {
        *result = knishio_strdup(response->data);
    } else {
        *result = NULL;
        error = KNISHIO_ERROR_INVALID_RESPONSE;
    }
    
    knishio_graphql_response_free(response);
    return error;
}
