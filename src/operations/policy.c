/**
 * @file operations/policy.c
 * @brief Policy operations implementation for KnishIO C SDK
 */

#include "knishio/knishio.h"
#include "knishio/operations/policy.h"
#include "knishio/client_ops.h"
#include "knishio/graphql.h"
#include "knishio/molecule.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"
#include "operations_internal.h"

#include <cjson/cJSON.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ProposeMolecule GraphQL mutation template for policy creation */
static const char* CREATE_POLICY_MUTATION = 
    "mutation CreatePolicy($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecularHash"
    "    status"
    "    reason"
    "    payload"
    "    createdAt"
    "  }"
    "}";

/* QueryPolicy GraphQL query template */
static const char* QUERY_POLICY = 
    "query QueryPolicy($metaType: String!, $metaId: String!) {"
    "  Policy(metaType: $metaType, metaId: $metaId) {"
    "    molecularHash"
    "    position"
    "    metaType"
    "    metaId"
    "    conditions"
    "    callback"
    "    rule"
    "    createdAt"
    "  }"
    "}";

/* CreateRule GraphQL mutation template */
static const char* CREATE_RULE_MUTATION = 
    "mutation CreateRule($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecularHash"
    "    status"
    "    reason"
    "    payload"
    "    createdAt"
    "  }"
    "}";

/* The outcome of a submitted policy or rule molecule (both result structs have these fields). */
static void molecule_outcome(
    const knishio_graphql_response_t* response,
    const knishio_molecule_t* molecule,
    const char* failure_text,
    bool* success,
    char** molecular_hash,
    char** response_text,
    char** error_message
) {
    if (response->data && response->success) {
        *success = true;
        *response_text = knishio_strdup(response->data);
        *molecular_hash = knishio_strdup(response->molecular_hash ? response->molecular_hash : molecule->molecular_hash);
    } else {
        *success = false;
        *error_message = knishio_strdup(response->errors ? response->errors : failure_text);
    }
}

/* Create access control policy: an R-atom (meta [policy]) + ContinuID I-atom signed from the
 * USER ContinuID wallet (JS createPolicy: addPolicyAtom({meta: {}}), addContinuIdAtom), checked
 * before it is sent. */
knishio_error_t knishio_client_create_policy(
    knishio_client_t* client,
    const knishio_create_policy_params_t* params,
    knishio_create_policy_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!params->meta_type || !params->meta_id) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    knishio_wallet_t* source = NULL;
    knishio_wallet_t* remainder = NULL;
    knishio_molecule_t* molecule = NULL;
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_client_continuid_molecule(client, &source, &remainder, &molecule);
    if (error == KNISHIO_SUCCESS) {
        error = knishio_molecule_add_policy_atom(molecule, params->meta_type, params->meta_id,
                                                 NULL, NULL, 0, params->policy_json);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_molecule_add_continuid_atom(molecule);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_client_sign_and_submit(client, molecule, source, "CreatePolicy",
                                               CREATE_POLICY_MUTATION, &response);
    }
    if (error == KNISHIO_SUCCESS) {
        knishio_create_policy_result_t* policy_result = calloc(1, sizeof(knishio_create_policy_result_t));
        if (policy_result) {
            molecule_outcome(response, molecule, "Policy creation failed", &policy_result->success,
                             &policy_result->molecular_hash, &policy_result->response, &policy_result->error_message);
            *result = policy_result;
        } else {
            error = KNISHIO_ERROR_MEMORY;
        }
    }

    if (response) knishio_graphql_response_free(response);
    knishio_molecule_free_deep(molecule);
    knishio_wallet_free(source);
    knishio_wallet_free(remainder);
    return error;
}

/* Query existing policies */
knishio_error_t knishio_client_query_policy(
    knishio_client_t* client,
    const knishio_query_policy_params_t* params,
    knishio_query_policy_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!params->meta_type || !params->meta_id) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build variables JSON */
    cJSON* variables_obj = cJSON_CreateObject();
    if (!variables_obj) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    cJSON_AddStringToObject(variables_obj, "metaType", params->meta_type);
    cJSON_AddStringToObject(variables_obj, "metaId", params->meta_id);
    
    char* variables_json = cJSON_PrintUnformatted(variables_obj);
    cJSON_Delete(variables_obj);
    
    if (!variables_json) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Execute GraphQL query */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "QueryPolicy",
        .query = QUERY_POLICY,
        .variables_json = variables_json,
        .requires_auth = false,
        .is_mutation = false
    };
    
    knishio_error_t error = knishio_client_execute_graphql(client, &operation, &response);
    
    free(variables_json);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    /* Create result structure */
    knishio_query_policy_result_t* policy_result = calloc(1, sizeof(knishio_query_policy_result_t));
    if (!policy_result) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        policy_result->success = true;
        policy_result->response = knishio_strdup(response->data);
        
        /* Parse policy data from response */
        cJSON* response_obj = cJSON_Parse(response->data);
        if (response_obj) {
            cJSON* data_obj = cJSON_GetObjectItem(response_obj, "data");
            cJSON* policy_array = cJSON_GetObjectItem(data_obj, "Policy");
            
            if (policy_array && cJSON_IsArray(policy_array) && cJSON_GetArraySize(policy_array) > 0) {
                cJSON* policy_obj = cJSON_GetArrayItem(policy_array, 0);
                char* policy_data = cJSON_PrintUnformatted(policy_obj);
                policy_result->policy_data = policy_data;
            } else {
                policy_result->policy_data = knishio_strdup("{}");
            }
            
            cJSON_Delete(response_obj);
        } else {
            policy_result->policy_data = knishio_strdup("{}");
        }
    } else {
        policy_result->success = false;
        policy_result->error_message = knishio_strdup(response->errors ? response->errors : "Policy query failed");
        policy_result->policy_data = knishio_strdup("{}");
    }
    
    knishio_graphql_response_free(response);
    *result = policy_result;
    return KNISHIO_SUCCESS;
}

/* Create policy rule: an R-atom (meta [rule, policy]) + ContinuID I-atom signed from the USER
 * ContinuID wallet (JS createRule -> Molecule.createRule), checked before it is sent. */
knishio_error_t knishio_client_create_rule(
    knishio_client_t* client,
    const knishio_create_rule_params_t* params,
    knishio_create_rule_result_t** result
) {
    if (!client || !params || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!params->meta_type || !params->meta_id || !params->rule_json) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    knishio_wallet_t* source = NULL;
    knishio_wallet_t* remainder = NULL;
    knishio_molecule_t* molecule = NULL;
    knishio_graphql_response_t* response = NULL;

    knishio_error_t error = knishio_client_continuid_molecule(client, &source, &remainder, &molecule);
    if (error == KNISHIO_SUCCESS) {
        error = knishio_molecule_create_rule(molecule, params->meta_type, params->meta_id,
                                             params->rule_json, params->policy_json);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_client_sign_and_submit(client, molecule, source, "CreateRule",
                                               CREATE_RULE_MUTATION, &response);
    }
    if (error == KNISHIO_SUCCESS) {
        knishio_create_rule_result_t* rule_result = calloc(1, sizeof(knishio_create_rule_result_t));
        if (rule_result) {
            molecule_outcome(response, molecule, "Rule creation failed", &rule_result->success,
                             &rule_result->molecular_hash, &rule_result->response, &rule_result->error_message);
            *result = rule_result;
        } else {
            error = KNISHIO_ERROR_MEMORY;
        }
    }

    if (response) knishio_graphql_response_free(response);
    knishio_molecule_free_deep(molecule);
    knishio_wallet_free(source);
    knishio_wallet_free(remainder);
    return error;
}

/* Client-side policy enforcement (knishio_client_enforce_policy /
 * knishio_client_check_meta_access / knishio_client_load_policy_from_query)
 * removed for cross-SDK alignment: it relied on a C-only knishio_policy_engine_*
 * that no other KnishIO SDK has — policy enforcement is the validator's
 * responsibility. The molecule-building policy API (create_policy / create_rule /
 * query_policy) is retained and matches the JS reference. */

/* Free policy creation result */
void knishio_create_policy_result_free(knishio_create_policy_result_t* result) {
    if (!result) return;
    
    if (result->molecular_hash) free(result->molecular_hash);
    if (result->response) free(result->response);
    if (result->error_message) free(result->error_message);
    free(result);
}

/* Free policy query result */
void knishio_query_policy_result_free(knishio_query_policy_result_t* result) {
    if (!result) return;
    
    if (result->policy_data) free(result->policy_data);
    if (result->response) free(result->response);
    if (result->error_message) free(result->error_message);
    free(result);
}

/* Free rule creation result */
void knishio_create_rule_result_free(knishio_create_rule_result_t* result) {
    if (!result) return;
    
    if (result->molecular_hash) free(result->molecular_hash);
    if (result->response) free(result->response);
    if (result->error_message) free(result->error_message);
    free(result);
}