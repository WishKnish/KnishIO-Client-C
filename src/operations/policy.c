/**
 * @file operations/policy.c
 * @brief Policy operations implementation for KnishIO C SDK
 */

#include "knishio/knishio.h"
#include "knishio/operations/policy.h"
#include "knishio/policy/engine.h"
#include "knishio/policy/rule.h"
#include "knishio/graphql.h"
#include "knishio/json/builder.h"
#include "knishio/molecule.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ProposeMolecule GraphQL mutation template for policy creation */
static const char* CREATE_POLICY_MUTATION = 
    "mutation CreatePolicy($molecule: MoleculeInput!) {"
    "  ProposeMolecule(molecule: $molecule) {"
    "    molecular_hash"
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
    "    molecular_hash"
    "    status"
    "    reason"
    "    payload"
    "    createdAt"
    "  }"
    "}";

/* Helper function to create policy molecule */
static knishio_error_t create_policy_molecule(
    knishio_client_t* client,
    const char* meta_type,
    const char* meta_id,
    const char* policy_json,
    knishio_molecule_t** molecule
) {
    if (!client || !meta_type || !meta_id || !molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Create molecule */
    knishio_molecule_t* mol = NULL;
    knishio_error_t err = knishio_molecule_create(&mol, "placeholder_secret", NULL, NULL, NULL, NULL, "2.0");
    if (err != KNISHIO_SUCCESS || !mol) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Create meta atom */
    knishio_atom_t* atom = NULL;
    err = knishio_atom_create(&atom, 
        "0000000000000000000000000000000000000000000000000000000000000000",
        "0000000000000000000000000000000000000000000000000000000000000000",
        KNISHIO_ISOTOPE_M,
        "USER",
        NULL,
        NULL);
    if (err != KNISHIO_SUCCESS || !atom) {
        knishio_molecule_free(mol);
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Set atom fields */
    /* Set atom metadata */
    atom->meta_type = knishio_strdup(meta_type);
    atom->meta_id = knishio_strdup(meta_id);
    /* TODO: Convert policy_json to meta array if needed */
    
    if (!atom->meta_type || !atom->meta_id) {
        knishio_atom_free(atom);
        knishio_molecule_free(mol);
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Add atom to molecule */
    knishio_error_t error = knishio_molecule_add_atom(mol, atom);
    if (error != KNISHIO_SUCCESS) {
        knishio_atom_free(atom);
        knishio_molecule_free(mol);
        return error;
    }
    
    *molecule = mol;
    return KNISHIO_SUCCESS;
}

/* Helper function to create rule molecule */
static knishio_error_t create_rule_molecule(
    knishio_client_t* client,
    const char* meta_type,
    const char* meta_id,
    const char* rule_json,
    const char* policy_json,
    knishio_molecule_t** molecule
) {
    if (!client || !meta_type || !meta_id || !rule_json || !molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Build combined meta JSON */
    cJSON* meta_obj = cJSON_CreateObject();
    if (!meta_obj) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Parse rule JSON */
    cJSON* rule_obj = cJSON_Parse(rule_json);
    if (rule_obj) {
        cJSON_AddItemToObject(meta_obj, "rule", rule_obj);
    } else {
        cJSON_Delete(meta_obj);
        return KNISHIO_ERROR_POLICY_INVALID;
    }
    
    /* Parse policy JSON if provided */
    if (policy_json) {
        cJSON* policy_obj = cJSON_Parse(policy_json);
        if (policy_obj) {
            cJSON_AddItemToObject(meta_obj, "policy", policy_obj);
        }
    }
    
    char* combined_meta = cJSON_PrintUnformatted(meta_obj);
    cJSON_Delete(meta_obj);
    
    if (!combined_meta) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Create molecule with combined meta */
    knishio_error_t error = create_policy_molecule(client, meta_type, meta_id, combined_meta, molecule);
    free(combined_meta);
    
    return error;
}

/* Create access control policy */
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
    
    /* Create policy molecule */
    knishio_molecule_t* molecule = NULL;
    knishio_error_t error = create_policy_molecule(
        client, params->meta_type, params->meta_id, params->policy_json, &molecule);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Sign molecule */
    error = knishio_molecule_sign(molecule, NULL, false, false);
    if (error != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return error;
    }
    
    /* Convert molecule to JSON */
    char* molecule_json = NULL;
    error = knishio_molecule_to_json(molecule, &molecule_json);
    if (error != KNISHIO_SUCCESS || !molecule_json) {
        knishio_molecule_free(molecule);
        return error;
    }
    knishio_molecule_free(molecule);
    
    if (!molecule_json) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Build variables JSON */
    cJSON* variables_obj = cJSON_CreateObject();
    cJSON* molecule_obj = cJSON_Parse(molecule_json);
    
    if (!variables_obj || !molecule_obj) {
        if (variables_obj) cJSON_Delete(variables_obj);
        if (molecule_obj) cJSON_Delete(molecule_obj);
        free(molecule_json);
        return KNISHIO_ERROR_MEMORY;
    }
    
    cJSON_AddItemToObject(variables_obj, "molecule", molecule_obj);
    char* variables_json = cJSON_PrintUnformatted(variables_obj);
    
    cJSON_Delete(variables_obj);
    free(molecule_json);
    
    if (!variables_json) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Execute GraphQL mutation */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "CreatePolicy",
        .query = CREATE_POLICY_MUTATION,
        .variables_json = variables_json,
        .requires_auth = true,
        .is_mutation = true
    };
    
    error = knishio_graphql_execute(
        (knishio_graphql_client_t*)client,
        &operation,
        &response
    );
    
    free(variables_json);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result structure */
    knishio_create_policy_result_t* policy_result = calloc(1, sizeof(knishio_create_policy_result_t));
    if (!policy_result) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        policy_result->success = true;
        policy_result->response = knishio_strdup(response->data);
        
        /* Parse molecular hash from response */
        cJSON* response_obj = cJSON_Parse(response->data);
        if (response_obj) {
            cJSON* data_obj = cJSON_GetObjectItem(response_obj, "data");
            cJSON* propose_obj = cJSON_GetObjectItem(data_obj, "ProposeMolecule");
            cJSON* hash_obj = cJSON_GetObjectItem(propose_obj, "molecular_hash");
            
            if (hash_obj && cJSON_IsString(hash_obj)) {
                policy_result->molecular_hash = knishio_strdup(cJSON_GetStringValue(hash_obj));
            }
            
            cJSON_Delete(response_obj);
        }
        
        if (!policy_result->molecular_hash) {
            policy_result->molecular_hash = knishio_strdup("unknown_hash");
        }
    } else {
        policy_result->success = false;
        policy_result->error_message = knishio_strdup(response->errors ? response->errors : "Policy creation failed");
    }
    
    knishio_graphql_response_free(response);
    *result = policy_result;
    return KNISHIO_SUCCESS;
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
    
    knishio_error_t error = knishio_graphql_execute(
        (knishio_graphql_client_t*)client,
        &operation,
        &response
    );
    
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

/* Create policy rule */
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
    
    /* Validate rule JSON format */
    cJSON* rule_obj = cJSON_Parse(params->rule_json);
    if (!rule_obj) {
        return KNISHIO_ERROR_POLICY_INVALID;
    }
    
    /* Validate rule structure */
    knishio_rule_t* rule = knishio_rule_from_json(rule_obj);
    cJSON_Delete(rule_obj);
    
    if (!rule) {
        return KNISHIO_ERROR_POLICY_INVALID;
    }
    knishio_rule_free(rule);
    
    /* Create rule molecule */
    knishio_molecule_t* molecule = NULL;
    knishio_error_t error = create_rule_molecule(
        client, params->meta_type, params->meta_id, 
        params->rule_json, params->policy_json, &molecule);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Sign molecule */
    error = knishio_molecule_sign(molecule, NULL, false, false);
    if (error != KNISHIO_SUCCESS) {
        knishio_molecule_free(molecule);
        return error;
    }
    
    /* Convert molecule to JSON */
    char* molecule_json = NULL;
    error = knishio_molecule_to_json(molecule, &molecule_json);
    if (error != KNISHIO_SUCCESS || !molecule_json) {
        knishio_molecule_free(molecule);
        return error;
    }
    knishio_molecule_free(molecule);
    
    if (!molecule_json) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Build variables JSON */
    cJSON* variables_obj = cJSON_CreateObject();
    cJSON* molecule_obj = cJSON_Parse(molecule_json);
    
    if (!variables_obj || !molecule_obj) {
        if (variables_obj) cJSON_Delete(variables_obj);
        if (molecule_obj) cJSON_Delete(molecule_obj);
        free(molecule_json);
        return KNISHIO_ERROR_MEMORY;
    }
    
    cJSON_AddItemToObject(variables_obj, "molecule", molecule_obj);
    char* variables_json = cJSON_PrintUnformatted(variables_obj);
    
    cJSON_Delete(variables_obj);
    free(molecule_json);
    
    if (!variables_json) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Execute GraphQL mutation */
    knishio_graphql_response_t* response = NULL;
    knishio_graphql_operation_t operation = {
        .name = "CreateRule",
        .query = CREATE_RULE_MUTATION,
        .variables_json = variables_json,
        .requires_auth = true,
        .is_mutation = true
    };
    
    error = knishio_graphql_execute(
        (knishio_graphql_client_t*)client,
        &operation,
        &response
    );
    
    free(variables_json);
    
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Create result structure */
    knishio_create_rule_result_t* rule_result = calloc(1, sizeof(knishio_create_rule_result_t));
    if (!rule_result) {
        knishio_graphql_response_free(response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (response->data && response->success) {
        rule_result->success = true;
        rule_result->response = knishio_strdup(response->data);
        
        /* Parse molecular hash from response */
        cJSON* response_obj = cJSON_Parse(response->data);
        if (response_obj) {
            cJSON* data_obj = cJSON_GetObjectItem(response_obj, "data");
            cJSON* propose_obj = cJSON_GetObjectItem(data_obj, "ProposeMolecule");
            cJSON* hash_obj = cJSON_GetObjectItem(propose_obj, "molecular_hash");
            
            if (hash_obj && cJSON_IsString(hash_obj)) {
                rule_result->molecular_hash = knishio_strdup(cJSON_GetStringValue(hash_obj));
            }
            
            cJSON_Delete(response_obj);
        }
        
        if (!rule_result->molecular_hash) {
            rule_result->molecular_hash = knishio_strdup("unknown_hash");
        }
    } else {
        rule_result->success = false;
        rule_result->error_message = knishio_strdup(response->errors ? response->errors : "Rule creation failed");
    }
    
    knishio_graphql_response_free(response);
    *result = rule_result;
    return KNISHIO_SUCCESS;
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