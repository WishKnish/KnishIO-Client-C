/**
 * @file operations/query.c
 * @brief Query operations implementation for KnishIO C SDK
 */

#include "knishio/knishio.h"
#include "knishio/operations/query.h"
#include "knishio/graphql.h"
#include "knishio/client_ops.h"
#include "knishio/utils/memory.h"

#include <cjson/cJSON.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* QueryAtom GraphQL query template */
static const char* QUERY_ATOM = 
    "query QueryAtom("
    "$molecularHashes: [String], $molecularHash: String, "
    "$bundleHashes: [String], $bundleHash: String, "
    "$positions: [String], $position: String, "
    "$walletAddresses: [String], $walletAddress: String, "
    "$isotopes: [String], $isotope: String, "
    "$tokenSlugs: [String], $tokenSlug: String, "
    "$cellSlugs: [String], $cellSlug: String, "
    "$batchIds: [String], $batchId: String, "
    "$values: [String], $value: String, "
    "$metaTypes: [String], $metaType: String, "
    "$metaIds: [String], $metaId: String, "
    "$indexes: [String], $index: String, "
    "$filter: String, $latest: Boolean, "
    "$limit: Int, $offset: Int"
    ") {"
    "  Atom("
    "    molecularHashes: $molecularHashes, molecularHash: $molecularHash, "
    "    bundleHashes: $bundleHashes, bundleHash: $bundleHash, "
    "    positions: $positions, position: $position, "
    "    walletAddresses: $walletAddresses, walletAddress: $walletAddress, "
    "    isotopes: $isotopes, isotope: $isotope, "
    "    tokenSlugs: $tokenSlugs, tokenSlug: $tokenSlug, "
    "    cellSlugs: $cellSlugs, cellSlug: $cellSlug, "
    "    batchIds: $batchIds, batchId: $batchId, "
    "    values: $values, value: $value, "
    "    metaTypes: $metaTypes, metaType: $metaType, "
    "    metaIds: $metaIds, metaId: $metaId, "
    "    indexes: $indexes, index: $index, "
    "    filter: $filter, latest: $latest, "
    "    limit: $limit, offset: $offset"
    "  ) {"
    "    molecularHash"
    "    position"
    "    walletAddress"
    "    isotope"
    "    token"
    "    value"
    "    batchId"
    "    metaType"
    "    metaId"
    "    index"
    "    createdAt"
    "  }"
    "}";

/* QueryActiveSession GraphQL query template */
static const char* QUERY_ACTIVE_SESSION = 
    "query QueryActiveSession($bundleHash: String!, $metaType: String!, $metaId: String!) {"
    "  ActiveSession(bundleHash: $bundleHash, metaType: $metaType, metaId: $metaId) {"
    "    bundleHash"
    "    metaType"
    "    metaId"
    "    ipAddress"
    "    browser"
    "    osCpu"
    "    resolution"
    "    timeZone"
    "    createdAt"
    "  }"
    "}";


/* Add a string variable when present; false only on allocation failure. */
static bool add_string(cJSON* vars, const char* key, const char* value) {
    return !value || cJSON_AddStringToObject(vars, key, value) != NULL;
}

/* Add a string-array variable when non-empty; false only on allocation failure. */
static bool add_string_array(cJSON* vars, const char* key, const char** values, size_t count) {
    if (!values || count == 0) return true;
    cJSON* array = cJSON_AddArrayToObject(vars, key);
    for (size_t i = 0; array && i < count; i++) {
        if (!cJSON_AddItemToArray(array, cJSON_CreateString(values[i] ? values[i] : ""))) {
            return false;
        }
    }
    return array != NULL;
}

/* Query atoms by various parameters */
knishio_error_t knishio_client_query_atom(
    knishio_client_t* client,
    const knishio_query_atom_params_t* params,
    knishio_query_atom_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables JSON (cJSON escapes every value and sizes the text) */
    cJSON* vars = cJSON_CreateObject();
    bool built = vars
        && add_string_array(vars, "molecularHashes", params->molecular_hashes, params->molecular_hash_count)
        && add_string(vars, "molecularHash", params->molecular_hash)
        && add_string_array(vars, "bundleHashes", params->bundle_hashes, params->bundle_hash_count)
        && add_string(vars, "bundleHash", params->bundle_hash)
        && add_string_array(vars, "positions", params->positions, params->position_count)
        && add_string(vars, "position", params->position)
        && add_string_array(vars, "walletAddresses", params->wallet_addresses, params->wallet_address_count)
        && add_string(vars, "walletAddress", params->wallet_address)
        && add_string_array(vars, "isotopes", params->isotopes, params->isotope_count)
        && add_string(vars, "isotope", params->isotope)
        && add_string_array(vars, "tokenSlugs", params->token_slugs, params->token_slug_count)
        && add_string(vars, "tokenSlug", params->token_slug)
        && add_string_array(vars, "cellSlugs", params->cell_slugs, params->cell_slug_count)
        && add_string(vars, "cellSlug", params->cell_slug)
        && add_string_array(vars, "batchIds", params->batch_ids, params->batch_id_count)
        && add_string(vars, "batchId", params->batch_id)
        && add_string_array(vars, "values", params->values, params->value_count)
        && add_string(vars, "value", params->value)
        && add_string_array(vars, "metaTypes", params->meta_types, params->meta_type_count)
        && add_string(vars, "metaType", params->meta_type)
        && add_string_array(vars, "metaIds", params->meta_ids, params->meta_id_count)
        && add_string(vars, "metaId", params->meta_id)
        && add_string_array(vars, "indexes", params->indexes, params->index_count)
        && add_string(vars, "index", params->index)
        && add_string(vars, "filter", params->filter)
        && cJSON_AddBoolToObject(vars, "latest", params->latest)
        && (params->limit <= 0 || cJSON_AddNumberToObject(vars, "limit", params->limit))
        && (params->offset <= 0 || cJSON_AddNumberToObject(vars, "offset", params->offset));
    char* variables = built ? cJSON_PrintUnformatted(vars) : NULL;
    cJSON_Delete(vars);
    if (!variables) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Execute GraphQL query */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryAtom",
        .query = QUERY_ATOM,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
    };

    knishio_error_t error = knishio_client_execute_graphql(client, &operation, &response);

    cJSON_free(variables);

    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result structure */
    knishio_query_atom_result_t* atom_result = calloc(1, sizeof(knishio_query_atom_result_t));
    if (!atom_result) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        atom_result->success = true;
        atom_result->response = knishio_strdup(response->data);
        /* TODO: Parse atoms from JSON response */
        atom_result->atoms = NULL;
        atom_result->atom_count = 0;
    } else {
        atom_result->success = false;
        atom_result->error_message = knishio_strdup(response->errors ? response->errors : "Query failed");
    }
    
    knishio_graphql_response_free(response);
    *result = atom_result;
    return KNISHIO_SUCCESS;
}

/* Query active sessions */
knishio_error_t knishio_client_query_active_session(
    knishio_client_t* client,
    const knishio_query_active_session_params_t* params,
    knishio_query_active_session_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!params->bundle_hash || !params->meta_type || !params->meta_id) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables JSON */
    cJSON* vars = cJSON_CreateObject();
    bool built = vars
        && add_string(vars, "bundleHash", params->bundle_hash)
        && add_string(vars, "metaType", params->meta_type)
        && add_string(vars, "metaId", params->meta_id);
    char* variables = built ? cJSON_PrintUnformatted(vars) : NULL;
    cJSON_Delete(vars);
    if (!variables) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Execute GraphQL query */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryActiveSession",
        .query = QUERY_ACTIVE_SESSION,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
    };

    knishio_error_t error = knishio_client_execute_graphql(client, &operation, &response);
    cJSON_free(variables);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result structure */
    knishio_query_active_session_result_t* session_result = calloc(1, sizeof(knishio_query_active_session_result_t));
    if (!session_result) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        session_result->success = true;
        session_result->response = knishio_strdup(response->data);
    } else {
        session_result->success = false;
        session_result->error_message = knishio_strdup(response->errors ? response->errors : "Query failed");
    }
    
    knishio_graphql_response_free(response);
    *result = session_result;
    return KNISHIO_SUCCESS;
}

void knishio_query_atom_result_free(knishio_query_atom_result_t* result) {
    if (!result) return;
    for (size_t i = 0; result->atoms && i < result->atom_count; i++) {
        knishio_atom_free_deep(result->atoms[i]);
    }
    free(result->atoms);
    knishio_free(result->response);
    knishio_free(result->error_message);
    free(result);
}

void knishio_query_active_session_result_free(knishio_query_active_session_result_t* result) {
    if (!result) return;
    knishio_free(result->response);
    knishio_free(result->error_message);
    free(result);
}


