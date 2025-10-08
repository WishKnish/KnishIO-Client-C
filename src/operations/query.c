/**
 * @file operations/query.c
 * @brief Query operations implementation for KnishIO C SDK
 */

#include "knishio/knishio.h"
#include "knishio/operations/query.h"
#include "knishio/graphql.h"
#include "knishio/json/builder.h"

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

/* QueryUserActivity GraphQL query template */
static const char* QUERY_USER_ACTIVITY = 
    "query QueryUserActivity("
    "$bundleHash: String, $metaType: String, $metaId: String, "
    "$ipAddress: String, $browser: String, $osCpu: String, "
    "$resolution: String, $timeZone: String, "
    "$countBy: String, $interval: String"
    ") {"
    "  UserActivity("
    "    bundleHash: $bundleHash, metaType: $metaType, metaId: $metaId, "
    "    ipAddress: $ipAddress, browser: $browser, osCpu: $osCpu, "
    "    resolution: $resolution, timeZone: $timeZone, "
    "    countBy: $countBy, interval: $interval"
    "  ) {"
    "    bundleHash"
    "    metaType"
    "    metaId"
    "    ipAddress"
    "    browser"
    "    osCpu"
    "    resolution"
    "    timeZone"
    "    countBy"
    "    interval"
    "    createdAt"
    "  }"
    "}";

/* Helper to build JSON array from string array */
static char* build_json_array(const char** items, size_t count) {
    if (!items || count == 0) return NULL;
    
    /* Calculate required size */
    size_t total_size = 3; /* for [] and null terminator */
    for (size_t i = 0; i < count; i++) {
        total_size += strlen(items[i]) + 3; /* for quotes and comma */
    }
    
    char* json_array = malloc(total_size);
    if (!json_array) return NULL;
    
    strcpy(json_array, "[");
    for (size_t i = 0; i < count; i++) {
        if (i > 0) strcat(json_array, ",");
        strcat(json_array, "\"");
        strcat(json_array, items[i]);
        strcat(json_array, "\"");
    }
    strcat(json_array, "]");
    
    return json_array;
}

/* Helper to add field to variables JSON */
static void add_json_field(char* json, const char* key, const char* value, bool* first) {
    if (!value) return;
    
    if (!*first) strcat(json, ",");
    strcat(json, "\"");
    strcat(json, key);
    strcat(json, "\":\"");
    strcat(json, value);
    strcat(json, "\"");
    *first = false;
}

/* Helper to add array field to variables JSON */
static void add_json_array_field(char* json, const char* key, const char** values, size_t count, bool* first) {
    if (!values || count == 0) return;
    
    char* json_array = build_json_array(values, count);
    if (!json_array) return;
    
    if (!*first) strcat(json, ",");
    strcat(json, "\"");
    strcat(json, key);
    strcat(json, "\":");
    strcat(json, json_array);
    *first = false;
    
    free(json_array);
}

/* Helper to add boolean field to variables JSON */
static void add_json_bool_field(char* json, const char* key, bool value, bool* first) {
    if (!*first) strcat(json, ",");
    strcat(json, "\"");
    strcat(json, key);
    strcat(json, "\":");
    strcat(json, value ? "true" : "false");
    *first = false;
}

/* Helper to add integer field to variables JSON */
static void add_json_int_field(char* json, const char* key, int value, bool* first) {
    if (!*first) strcat(json, ",");
    strcat(json, "\"");
    strcat(json, key);
    strcat(json, "\":");
    char int_str[32];
    snprintf(int_str, sizeof(int_str), "%d", value);
    strcat(json, int_str);
    *first = false;
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
    
    /* Build variables JSON - allocate large buffer for complex query */
    char* variables = malloc(4096);
    if (!variables) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    strcpy(variables, "{");
    bool first = true;
    
    /* Add all possible parameters */
    add_json_array_field(variables, "molecularHashes", params->molecular_hashes, params->molecular_hash_count, &first);
    add_json_field(variables, "molecularHash", params->molecular_hash, &first);
    add_json_array_field(variables, "bundleHashes", params->bundle_hashes, params->bundle_hash_count, &first);
    add_json_field(variables, "bundleHash", params->bundle_hash, &first);
    add_json_array_field(variables, "positions", params->positions, params->position_count, &first);
    add_json_field(variables, "position", params->position, &first);
    add_json_array_field(variables, "walletAddresses", params->wallet_addresses, params->wallet_address_count, &first);
    add_json_field(variables, "walletAddress", params->wallet_address, &first);
    add_json_array_field(variables, "isotopes", params->isotopes, params->isotope_count, &first);
    add_json_field(variables, "isotope", params->isotope, &first);
    add_json_array_field(variables, "tokenSlugs", params->token_slugs, params->token_slug_count, &first);
    add_json_field(variables, "tokenSlug", params->token_slug, &first);
    add_json_array_field(variables, "cellSlugs", params->cell_slugs, params->cell_slug_count, &first);
    add_json_field(variables, "cellSlug", params->cell_slug, &first);
    add_json_array_field(variables, "batchIds", params->batch_ids, params->batch_id_count, &first);
    add_json_field(variables, "batchId", params->batch_id, &first);
    add_json_array_field(variables, "values", params->values, params->value_count, &first);
    add_json_field(variables, "value", params->value, &first);
    add_json_array_field(variables, "metaTypes", params->meta_types, params->meta_type_count, &first);
    add_json_field(variables, "metaType", params->meta_type, &first);
    add_json_array_field(variables, "metaIds", params->meta_ids, params->meta_id_count, &first);
    add_json_field(variables, "metaId", params->meta_id, &first);
    add_json_array_field(variables, "indexes", params->indexes, params->index_count, &first);
    add_json_field(variables, "index", params->index, &first);
    add_json_field(variables, "filter", params->filter, &first);
    add_json_bool_field(variables, "latest", params->latest, &first);
    if (params->limit > 0) add_json_int_field(variables, "limit", params->limit, &first);
    if (params->offset > 0) add_json_int_field(variables, "offset", params->offset, &first);
    
    strcat(variables, "}");
    
    /* Execute GraphQL query */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryAtom",
        .query = QUERY_ATOM,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
    };
    
    knishio_error_t error = knishio_graphql_execute(
        (knishio_graphql_client_t*)client,
        &operation,
        &response
    );
    
    free(variables);
    
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
    char variables[512];
    snprintf(variables, sizeof(variables), 
        "{\"bundleHash\":\"%s\",\"metaType\":\"%s\",\"metaId\":\"%s\"}",
        params->bundle_hash, params->meta_type, params->meta_id);
    
    /* Execute GraphQL query */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryActiveSession",
        .query = QUERY_ACTIVE_SESSION,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
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

/* Query user activity */
knishio_error_t knishio_client_query_user_activity(
    knishio_client_t* client,
    const knishio_query_user_activity_params_t* params,
    knishio_query_user_activity_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables JSON */
    char variables[1024] = "{";
    bool first = true;
    
    add_json_field(variables, "bundleHash", params->bundle_hash, &first);
    add_json_field(variables, "metaType", params->meta_type, &first);
    add_json_field(variables, "metaId", params->meta_id, &first);
    add_json_field(variables, "ipAddress", params->ip_address, &first);
    add_json_field(variables, "browser", params->browser, &first);
    add_json_field(variables, "osCpu", params->os_cpu, &first);
    add_json_field(variables, "resolution", params->resolution, &first);
    add_json_field(variables, "timeZone", params->time_zone, &first);
    add_json_field(variables, "countBy", params->count_by, &first);
    add_json_field(variables, "interval", params->interval, &first);
    
    strcat(variables, "}");
    
    /* Execute GraphQL query */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryUserActivity",
        .query = QUERY_USER_ACTIVITY,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
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
    knishio_query_user_activity_result_t* activity_result = calloc(1, sizeof(knishio_query_user_activity_result_t));
    if (!activity_result) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        activity_result->success = true;
        activity_result->response = knishio_strdup(response->data);
    } else {
        activity_result->success = false;
        activity_result->error_message = knishio_strdup(response->errors ? response->errors : "Query failed");
    }
    
    knishio_graphql_response_free(response);
    *result = activity_result;
    return KNISHIO_SUCCESS;
}

/* Free atom query result */
void knishio_query_atom_result_free(knishio_query_atom_result_t* result) {
    if (!result) return;
    
    if (result->atoms) {
        for (size_t i = 0; i < result->atom_count; i++) {
            if (result->atoms[i]) {
                knishio_atom_free(result->atoms[i]);
            }
        }
        free(result->atoms);
    }
    
    if (result->response) free(result->response);
    if (result->error_message) free(result->error_message);
    free(result);
}

/* Free active session query result */
void knishio_query_active_session_result_free(knishio_query_active_session_result_t* result) {
    if (!result) return;
    
    if (result->response) free(result->response);
    if (result->error_message) free(result->error_message);
    free(result);
}

/* Free user activity query result */
void knishio_query_user_activity_result_free(knishio_query_user_activity_result_t* result) {
    if (!result) return;
    
    if (result->response) free(result->response);
    if (result->error_message) free(result->error_message);
    free(result);
}