/**
 * @file graphql.c
 * @brief Enhanced GraphQL operations implementation for KnishIO C SDK
 * 
 * Implements JavaScript-compatible GraphQL operations with full JSON builder integration
 * and enhanced HTTP client support. Production-ready for all GraphQL operations.
 */

#include "knishio/graphql.h"
#include "knishio/http/client.h"
#include "knishio/auth_token.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/utils/logging.h"
#include "knishio/json/parser.h"
#include "knishio/json/builder.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* GraphQL client structure is defined in knishio/graphql.h */

/* Internal helper function declarations */
static knishio_error_t copy_graphql_string(char** dest, const char* src);
static void free_graphql_string(char** field);
static knishio_error_t parse_balance_response(const char* response_json, knishio_balance_result_t* result);
static knishio_error_t parse_proposal_response(const char* response_json, knishio_proposal_result_t* result);
static knishio_error_t extract_graphql_errors(knishio_json_t* response_json, char** errors_json);
static void set_graphql_error(knishio_graphql_client_t* client, const char* error);
static void clear_graphql_error(knishio_graphql_client_t* client);
static knishio_error_t build_graphql_request_json(const char* query, const char* variables_json, knishio_json_t** request_json);

/* GraphQL client lifecycle functions */

knishio_error_t knishio_graphql_client_create(
    knishio_graphql_client_t** client,
    const char* endpoint_url,
    const char* cell_slug
) {
    if (!client || !endpoint_url) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate client structure */
    knishio_graphql_client_t* c = knishio_calloc(1, sizeof(knishio_graphql_client_t));
    if (!c) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Create enhanced HTTP client */
    /* Use function from http.h that returns error code */
    knishio_error_t err = knishio_http_client_create(&c->http_client, endpoint_url);
    if (err != KNISHIO_SUCCESS || !c->http_client) {
        knishio_free(c);
        return err != KNISHIO_SUCCESS ? err : KNISHIO_ERROR_MEMORY;
    }

    /* Copy endpoint URL */
    knishio_error_t error = copy_graphql_string(&c->endpoint_url, endpoint_url);
    if (error != KNISHIO_SUCCESS) {
        knishio_http_client_free(c->http_client);
        knishio_free(c);
        return error;
    }

    /* Copy cell slug if provided */
    if (cell_slug) {
        error = copy_graphql_string(&c->cell_slug, cell_slug);
        if (error != KNISHIO_SUCCESS) {
            knishio_graphql_client_free(c);
            return error;
        }
    }

    /* Set default values */
    c->debug_mode = false;
    c->auth_token = NULL;
    /* last_error field can be added to structure if enhanced error tracking needed */

    /* Set up HTTP client defaults for GraphQL */
    const char* default_headers[] = {
        "Content-Type: application/json",
        "Accept: application/json"
    };
    knishio_http_client_set_headers(c->http_client, default_headers, 2);
    knishio_http_client_set_timeout(c->http_client, 30000); /* 30 second timeout */

    *client = c;
    return KNISHIO_SUCCESS;
}

void knishio_graphql_client_free(knishio_graphql_client_t* client) {
    if (!client) {
        return;
    }

    /* Free HTTP client */
    if (client->http_client) {
        knishio_http_client_free(client->http_client);
    }

    /* Free string fields */
    free_graphql_string(&client->endpoint_url);
    free_graphql_string(&client->cell_slug);
    /* last_error field can be freed here if added to structure */

    /* Note: auth_token is not owned by this client, don't free it */

    /* Free the client structure itself */
    knishio_free(client);
}

knishio_error_t knishio_graphql_client_set_debug(
    knishio_graphql_client_t* client,
    bool debug_mode
) {
    if (!client) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    client->debug_mode = debug_mode;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_graphql_client_set_auth_token(
    knishio_graphql_client_t* client,
    knishio_auth_token_t* auth_token
) {
    if (!client) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Store auth token reference */
    client->auth_token = auth_token;
    
    /* Update HTTP client with auth headers if token is provided */
    if (auth_token && client->http_client) {
        const char* token = knishio_auth_token_get_token(auth_token);
        if (token) {
            knishio_http_client_set_auth_token(client->http_client, token);
        }
    } else if (client->http_client) {
        /* Clear auth if no token provided */
        knishio_http_client_clear_auth(client->http_client);
    }
    
    return KNISHIO_SUCCESS;
}

/* Generic GraphQL operation execution with enhanced error handling */

knishio_error_t knishio_graphql_execute(
    knishio_graphql_client_t* client,
    const knishio_graphql_operation_t* operation,
    knishio_graphql_response_t** response
) {
    if (!client || !operation || !response) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    clear_graphql_error(client);
    
    /* Check authentication requirement */
    if (operation->requires_auth && !client->auth_token) {
        set_graphql_error(client, "Operation requires authentication but no auth token provided");
        return KNISHIO_ERROR_AUTH;
    }
    
    /* Build GraphQL request JSON using JSON builder */
    knishio_json_t* request_json = NULL;
    knishio_error_t error = build_graphql_request_json(operation->query, operation->variables_json, &request_json);
    if (error != KNISHIO_SUCCESS) {
        set_graphql_error(client, "Failed to build GraphQL request JSON");
        return error;
    }

    /* Serialize the request object to a JSON string for the POST body. (The prior code passed the
     * knishio_json_t* OBJECT straight into knishio_http_post_graphql's `const char* query` param,
     * which re-wrapped it as {"query":"<raw-pointer-bytes>"} — a malformed body the validator could
     * not parse, so it fell back to the protected operation default and rejected even public
     * U-isotope ProposeMolecule with "X-Auth-Token header is required". Serialize the already-built,
     * properly-escaped request object and POST it raw instead.) */
    char* request_str = knishio_json_serialize(request_json, false);
    knishio_json_free(request_json);
    if (!request_str) {
        set_graphql_error(client, "Failed to serialize GraphQL request JSON");
        return KNISHIO_ERROR_MEMORY;
    }

    /* Log request if debug mode enabled */
    if (client->debug_mode) {
        knishio_log(KNISHIO_LOG_DEBUG, "GraphQL Request: %s", request_str);
    }

    /* Execute HTTP request — POST the full body to the endpoint as-is (base_url is already the full
     * GraphQL endpoint; X-Auth-Token + insecure-TLS ride on the http client). */
    knishio_http_response_t* http_response = NULL;
    knishio_error_t post_error = knishio_http_post(
        client->http_client, "", request_str, "application/json", &http_response);
    knishio_free(request_str);
    
    if (post_error != KNISHIO_SUCCESS || !http_response) {
        const char* http_error = knishio_http_client_error(client->http_client);
        set_graphql_error(client, http_error ? http_error : "HTTP request failed");
        return KNISHIO_ERROR_NETWORK;
    }
    
    /* Create GraphQL response structure */
    knishio_graphql_response_t* resp = knishio_calloc(1, sizeof(knishio_graphql_response_t));
    if (!resp) {
        knishio_http_response_free(http_response);
        return KNISHIO_ERROR_MEMORY;
    }
    
    resp->status_code = http_response->status_code;
    resp->success = (http_response->status_code >= 200 && http_response->status_code < 300);
    
    /* Parse response body if present */
    if (http_response->data) {
        copy_graphql_string(&resp->data, http_response->data);
        
        /* Log response if debug mode enabled */
        if (client->debug_mode) {
            knishio_log(KNISHIO_LOG_DEBUG, "GraphQL Response: %s", http_response->data);
        }
        
        /* Parse JSON response for error extraction */
        char* parse_error = NULL;
        knishio_json_t* response_json = knishio_json_parse(http_response->data, &parse_error);
        if (response_json) {
            /* Extract GraphQL errors if present */
            char* errors_json = NULL;
            if (extract_graphql_errors(response_json, &errors_json) == KNISHIO_SUCCESS && errors_json) {
                resp->errors = errors_json;
                resp->success = false;
            }
            
            /* Extract molecular hash for mutations */
            if (operation->is_mutation) {
                knishio_json_t* data = knishio_json_object_get(response_json, "data");
                if (data) {
                    const char* hash = knishio_json_get_string_path(data, "ProposeMolecule.molecularHash");
                    if (hash) {
                        copy_graphql_string(&resp->molecular_hash, hash);
                    }
                    knishio_json_free(data);
                }
            }
            
            knishio_json_free(response_json);
        }
        
        if (parse_error) {
            knishio_log(KNISHIO_LOG_WARN, "Failed to parse GraphQL response JSON: %s", parse_error);
            knishio_free(parse_error);
        }
    } else if (http_response->data) {
        copy_graphql_string(&resp->errors, http_response->data);
        resp->success = false;
    }
    
    knishio_http_response_free(http_response);
    *response = resp;
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_graphql_query(
    knishio_graphql_client_t* client,
    const char* query,
    const char* variables,
    knishio_graphql_response_t** response
) {
    knishio_graphql_operation_t operation = {
        .name = "Query",
        .query = query,
        .variables_json = variables,
        .requires_auth = false,
        .is_mutation = false
    };
    
    return knishio_graphql_execute(client, &operation, response);
}

knishio_error_t knishio_graphql_mutation(
    knishio_graphql_client_t* client,
    const char* mutation,
    const char* variables,
    knishio_graphql_response_t** response
) {
    knishio_graphql_operation_t operation = {
        .name = "Mutation",
        .query = mutation,
        .variables_json = variables,
        .requires_auth = true,  /* Most mutations require auth */
        .is_mutation = true
    };
    
    return knishio_graphql_execute(client, &operation, response);
}

void knishio_graphql_response_free(knishio_graphql_response_t* response) {
    if (!response) {
        return;
    }
    
    free_graphql_string(&response->data);
    free_graphql_string(&response->errors);
    free_graphql_string(&response->molecular_hash);
    
    knishio_free(response);
}

/* Essential GraphQL operations with enhanced JSON building */

knishio_error_t knishio_graphql_propose_molecule(
    knishio_graphql_client_t* client,
    const knishio_molecule_t* molecule,
    knishio_proposal_result_t** result
) {
    if (!client || !molecule || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Create result structure */
    knishio_error_t error = knishio_proposal_result_create(result);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Build GraphQL mutation using JSON builder */
    char* molecule_json = NULL;
    error = knishio_molecule_to_json(molecule, &molecule_json);
    if (error != KNISHIO_SUCCESS) {
        knishio_proposal_result_free(*result);
        *result = NULL;
        return error;
    }

    /* Use JSON builder for better GraphQL construction */
    knishio_json_object_builder_t* mutation_builder = knishio_json_build_propose_molecule_mutation(molecule_json);
    knishio_free(molecule_json);
    
    if (!mutation_builder) {
        knishio_proposal_result_free(*result);
        *result = NULL;
        return KNISHIO_ERROR_MEMORY;
    }

    /* Build and serialize the mutation */
    knishio_json_t* mutation_json = knishio_json_object_build(mutation_builder);
    knishio_json_object_builder_free(mutation_builder);
    
    if (!mutation_json) {
        knishio_proposal_result_free(*result);
        *result = NULL;
        return KNISHIO_ERROR_MEMORY;
    }

    /* Execute the mutation using enhanced HTTP client */
    knishio_http_response_t* response = NULL;
    knishio_error_t post_error = knishio_http_post_graphql(client->http_client, mutation_json, NULL, &response);
    knishio_json_free(mutation_json);
    
    if (post_error != KNISHIO_SUCCESS || !response) {
        error = KNISHIO_ERROR_NETWORK;
    } else {
        /* Parse response */
        (*result)->status_code = response->status_code;
        (*result)->success = (response->status_code >= 200 && response->status_code < 300);
        
        if (response->data) {
            /* Copy full response */
            copy_graphql_string(&(*result)->response_json, response->data);
            
            /* Parse for molecular hash and other data */
            error = parse_proposal_response(response->data, *result);
        }
        
        knishio_http_response_free(response);
    }

    /* Handle error cases */
    if (error != KNISHIO_SUCCESS) {
        knishio_proposal_result_free(*result);
        *result = NULL;
    }

    return error;
}

knishio_error_t knishio_graphql_query_balance(
    knishio_graphql_client_t* client,
    const char* wallet_address,
    const char* token,
    knishio_balance_result_t** result
) {
    if (!client || !wallet_address || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Create result structure */
    knishio_error_t error = knishio_balance_result_create(result);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Build GraphQL query using JSON builder */
    knishio_json_object_builder_t* query_builder = knishio_json_build_balance_query(wallet_address, token);
    if (!query_builder) {
        knishio_balance_result_free(*result);
        *result = NULL;
        return KNISHIO_ERROR_MEMORY;
    }

    /* Build and serialize the query */
    knishio_json_t* query_json = knishio_json_object_build(query_builder);
    knishio_json_object_builder_free(query_builder);
    
    if (!query_json) {
        knishio_balance_result_free(*result);
        *result = NULL;
        return KNISHIO_ERROR_MEMORY;
    }

    /* Execute the query using enhanced HTTP client */
    knishio_http_response_t* response = NULL;
    knishio_error_t post_error = knishio_http_post_graphql(client->http_client, query_json, NULL, &response);
    knishio_json_free(query_json);
    
    if (post_error != KNISHIO_SUCCESS || !response) {
        error = KNISHIO_ERROR_NETWORK;
    } else {
        /* Parse response */
        (*result)->success = (response->status_code >= 200 && response->status_code < 300);
        
        if (response->data && (*result)->success) {
            error = parse_balance_response(response->data, *result);
        }
        
        knishio_http_response_free(response);
    }

    /* Handle error cases */
    if (error != KNISHIO_SUCCESS) {
        knishio_balance_result_free(*result);
        *result = NULL;
    }

    return error;
}

/* Result structure lifecycle */

knishio_error_t knishio_balance_result_create(knishio_balance_result_t** result) {
    if (!result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate result structure */
    knishio_balance_result_t* r = knishio_calloc(1, sizeof(knishio_balance_result_t));
    if (!r) {
        return KNISHIO_ERROR_MEMORY;
    }

    *result = r;
    return KNISHIO_SUCCESS;
}

void knishio_balance_result_free(knishio_balance_result_t* result) {
    if (!result) {
        return;
    }

    /* Free string fields */
    free_graphql_string(&result->token);
    free_graphql_string(&result->value);
    free_graphql_string(&result->wallet_address);
    free_graphql_string(&result->position);

    /* Free the result structure itself */
    knishio_free(result);
}

knishio_error_t knishio_proposal_result_create(knishio_proposal_result_t** result) {
    if (!result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate result structure */
    knishio_proposal_result_t* r = knishio_calloc(1, sizeof(knishio_proposal_result_t));
    if (!r) {
        return KNISHIO_ERROR_MEMORY;
    }

    *result = r;
    return KNISHIO_SUCCESS;
}

void knishio_proposal_result_free(knishio_proposal_result_t* result) {
    if (!result) {
        return;
    }

    /* Free string fields */
    free_graphql_string(&result->molecular_hash);
    free_graphql_string(&result->response_json);

    /* Free the result structure itself */
    knishio_free(result);
}

/* Result accessor functions */

const char* knishio_balance_result_get_value(const knishio_balance_result_t* result) {
    return result ? result->value : NULL;
}

const char* knishio_balance_result_get_token(const knishio_balance_result_t* result) {
    return result ? result->token : NULL;
}

const char* knishio_balance_result_get_wallet_address(const knishio_balance_result_t* result) {
    return result ? result->wallet_address : NULL;
}

bool knishio_balance_result_is_success(const knishio_balance_result_t* result) {
    return result ? result->success : false;
}

const char* knishio_proposal_result_get_hash(const knishio_proposal_result_t* result) {
    return result ? result->molecular_hash : NULL;
}

const char* knishio_proposal_result_get_response(const knishio_proposal_result_t* result) {
    return result ? result->response_json : NULL;
}

bool knishio_proposal_result_is_success(const knishio_proposal_result_t* result) {
    return result ? result->success : false;
}

long knishio_proposal_result_get_status_code(const knishio_proposal_result_t* result) {
    return result ? result->status_code : 0;
}

/* Utility functions with JSON builder integration */

knishio_error_t knishio_graphql_build_balance_query(
    const char* wallet_address,
    const char* token,
    char** query
) {
    if (!wallet_address || !query) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Use JSON builder for proper GraphQL query construction */
    knishio_json_object_builder_t* builder = knishio_json_build_balance_query(wallet_address, token);
    if (!builder) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Extract the query string from the built object */
    knishio_json_t* query_obj = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);
    
    if (!query_obj) {
        return KNISHIO_ERROR_MEMORY;
    }

    const char* query_str = knishio_json_get_string_path(query_obj, "query");
    if (query_str) {
        copy_graphql_string(query, query_str);
    }
    
    knishio_json_free(query_obj);
    
    return query_str ? KNISHIO_SUCCESS : KNISHIO_ERROR_INVALID_STATE;
}

knishio_error_t knishio_graphql_build_propose_mutation(
    const knishio_molecule_t* molecule,
    char** mutation
) {
    if (!molecule || !mutation) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Convert molecule to JSON */
    char* molecule_json = NULL;
    knishio_error_t error = knishio_molecule_to_json(molecule, &molecule_json);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Use JSON builder for proper GraphQL mutation construction */
    knishio_json_object_builder_t* builder = knishio_json_build_propose_molecule_mutation(molecule_json);
    knishio_free(molecule_json);
    
    if (!builder) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Extract the mutation string from the built object */
    knishio_json_t* mutation_obj = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);
    
    if (!mutation_obj) {
        return KNISHIO_ERROR_MEMORY;
    }

    const char* mutation_str = knishio_json_get_string_path(mutation_obj, "query");
    if (mutation_str) {
        copy_graphql_string(mutation, mutation_str);
    }
    
    knishio_json_free(mutation_obj);
    
    return mutation_str ? KNISHIO_SUCCESS : KNISHIO_ERROR_INVALID_STATE;
}

/* Internal helper functions */

static knishio_error_t copy_graphql_string(char** dest, const char* src) {
    if (!dest || !src) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    size_t len = strlen(src);
    char* copy = knishio_malloc(len + 1);
    if (!copy) {
        return KNISHIO_ERROR_MEMORY;
    }

    strcpy(copy, src);
    *dest = copy;
    return KNISHIO_SUCCESS;
}

static void free_graphql_string(char** field) {
    if (field && *field) {
        knishio_free(*field);
        *field = NULL;
    }
}

static knishio_error_t parse_balance_response(const char* response_json, knishio_balance_result_t* result) {
    if (!response_json || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Enhanced JSON parsing using the JSON parser */
    char* error_msg = NULL;
    knishio_json_t* json = knishio_json_parse(response_json, &error_msg);
    if (!json) {
        if (error_msg) {
            knishio_free(error_msg);
        }
        return KNISHIO_ERROR_INVALID_STATE;
    }

    /* Extract balance data from GraphQL response structure */
    knishio_json_t* data = knishio_json_object_get(json, "data");
    if (data) {
        knishio_json_t* balance = knishio_json_object_get(data, "Balance");
        if (balance) {
            const char* value = knishio_json_get_string_path(balance, "balance");
            const char* token = knishio_json_get_string_path(balance, "token");
            const char* address = knishio_json_get_string_path(balance, "address");
            
            if (value) copy_graphql_string(&result->value, value);
            if (token) copy_graphql_string(&result->token, token);
            if (address) copy_graphql_string(&result->wallet_address, address);
            
            knishio_json_free(balance);
        }
        knishio_json_free(data);
    }

    knishio_json_free(json);
    return KNISHIO_SUCCESS;
}

static knishio_error_t parse_proposal_response(const char* response_json, knishio_proposal_result_t* result) {
    if (!response_json || !result) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Enhanced JSON parsing */
    char* error_msg = NULL;
    knishio_json_t* json = knishio_json_parse(response_json, &error_msg);
    if (!json) {
        if (error_msg) {
            knishio_free(error_msg);
        }
        return KNISHIO_ERROR_INVALID_STATE;
    }

    /* Extract proposal data from GraphQL response structure */
    knishio_json_t* data = knishio_json_object_get(json, "data");
    if (data) {
        knishio_json_t* propose_result = knishio_json_object_get(data, "ProposeMolecule");
        if (propose_result) {
            const char* hash = knishio_json_get_string_path(propose_result, "molecularHash");
            if (hash) {
                copy_graphql_string(&result->molecular_hash, hash);
            }
            knishio_json_free(propose_result);
        }
        knishio_json_free(data);
    }

    knishio_json_free(json);
    return KNISHIO_SUCCESS;
}

static knishio_error_t extract_graphql_errors(knishio_json_t* response_json, char** errors_json) {
    if (!response_json || !errors_json) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    knishio_json_t* errors = knishio_json_object_get(response_json, "errors");
    if (errors && knishio_json_get_type(errors) == KNISHIO_JSON_ARRAY && knishio_json_array_size(errors) > 0) {
        *errors_json = knishio_json_serialize(errors, false);
        knishio_json_free(errors);
        return KNISHIO_SUCCESS;
    }

    if (errors) {
        knishio_json_free(errors);
    }
    
    /* No errors found - this is actually success */
    return KNISHIO_SUCCESS;
}

static void set_graphql_error(knishio_graphql_client_t* client, const char* error) {
    if (!client || !error) return;
    
    /* TODO: Add last_error to structure if needed
    knishio_free(client->last_error);
    client->last_error = knishio_strdup(error); */
}

static void clear_graphql_error(knishio_graphql_client_t* client) {
    if (!client) return;
    
    /* TODO: Add last_error to structure if needed
    knishio_free(client->last_error);
    client->last_error = NULL; */
}

static knishio_error_t build_graphql_request_json(const char* query, const char* variables_json, knishio_json_t** request_json) {
    if (!query || !request_json) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Use JSON builder for proper request construction */
    knishio_json_object_builder_t* builder = knishio_json_object_builder_create();
    if (!builder) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Add query */
    if (!knishio_json_object_set_string(builder, "query", query)) {
        knishio_json_object_builder_free(builder);
        return KNISHIO_ERROR_MEMORY;
    }

    /* Add variables if provided */
    if (variables_json && strlen(variables_json) > 0) {
        char* error_msg = NULL;
        knishio_json_t* variables = knishio_json_parse(variables_json, &error_msg);
        if (variables) {
            knishio_json_object_set(builder, "variables", variables);
        }
        if (error_msg) {
            knishio_free(error_msg);
        }
    }

    *request_json = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);

    return *request_json ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
}

/* Public error access function */
const char* knishio_graphql_client_get_error(const knishio_graphql_client_t* client) {
    /* TODO: Add last_error to structure if needed
    return client ? client->last_error : NULL; */
    return NULL;
}