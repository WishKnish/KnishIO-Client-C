/**
 * @file builder.c
 * @brief JSON building implementation for KnishIO SDK
 */

#include "knishio/json/builder.h"
#include "knishio/json/parser.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Include cJSON properly */
#ifdef __has_include
  #if __has_include(<cjson/cJSON.h>)
    #include <cjson/cJSON.h>
  #elif __has_include(<cJSON.h>)
    #include <cJSON.h>
  #endif
#else
  #include <cjson/cJSON.h>
#endif

/* Builder structures */
struct knishio_json_builder {
    cJSON *root;
    bool owns_root;
};

struct knishio_json_array_builder {
    cJSON *array;
    bool owns_array;
};

struct knishio_json_object_builder {
    cJSON *object;
    bool owns_object;
};

/* Main builder */
knishio_json_builder_t* knishio_json_builder_create(void) {
    knishio_json_builder_t *builder = knishio_calloc(1, sizeof(knishio_json_builder_t));
    if (!builder) return NULL;
    
    builder->root = NULL;
    builder->owns_root = true;
    
    return builder;
}

void knishio_json_builder_free(knishio_json_builder_t *builder) {
    if (!builder) return;
    
    if (builder->owns_root && builder->root) {
        cJSON_Delete(builder->root);
    }
    
    knishio_free(builder);
}

knishio_json_t* knishio_json_builder_build(knishio_json_builder_t *builder) {
    if (!builder || !builder->root) return NULL;
    
    char *json_str = cJSON_PrintUnformatted(builder->root);
    if (!json_str) return NULL;
    
    knishio_json_t *result = knishio_json_parse(json_str, NULL);
    cJSON_free(json_str);
    
    return result;
}

char* knishio_json_builder_to_string(knishio_json_builder_t *builder, bool pretty) {
    if (!builder || !builder->root) return NULL;
    
    return pretty ? cJSON_Print(builder->root) : cJSON_PrintUnformatted(builder->root);
}

/* Value creators */
knishio_json_t* knishio_json_create_null(void) {
    cJSON *cjson = cJSON_CreateNull();
    if (!cjson) return NULL;
    
    char *json_str = cJSON_PrintUnformatted(cjson);
    cJSON_Delete(cjson);
    if (!json_str) return NULL;
    
    knishio_json_t *result = knishio_json_parse(json_str, NULL);
    cJSON_free(json_str);
    
    return result;
}

knishio_json_t* knishio_json_create_bool(bool value) {
    cJSON *cjson = cJSON_CreateBool(value);
    if (!cjson) return NULL;
    
    char *json_str = cJSON_PrintUnformatted(cjson);
    cJSON_Delete(cjson);
    if (!json_str) return NULL;
    
    knishio_json_t *result = knishio_json_parse(json_str, NULL);
    cJSON_free(json_str);
    
    return result;
}

knishio_json_t* knishio_json_create_number(double value) {
    cJSON *cjson = cJSON_CreateNumber(value);
    if (!cjson) return NULL;
    
    char *json_str = cJSON_PrintUnformatted(cjson);
    cJSON_Delete(cjson);
    if (!json_str) return NULL;
    
    knishio_json_t *result = knishio_json_parse(json_str, NULL);
    cJSON_free(json_str);
    
    return result;
}

knishio_json_t* knishio_json_create_int(int64_t value) {
    return knishio_json_create_number((double)value);
}

knishio_json_t* knishio_json_create_string(const char *value) {
    if (!value) return NULL;
    
    cJSON *cjson = cJSON_CreateString(value);
    if (!cjson) return NULL;
    
    char *json_str = cJSON_PrintUnformatted(cjson);
    cJSON_Delete(cjson);
    if (!json_str) return NULL;
    
    knishio_json_t *result = knishio_json_parse(json_str, NULL);
    cJSON_free(json_str);
    
    return result;
}

knishio_json_t* knishio_json_create_string_n(const char *value, size_t length) {
    if (!value) return NULL;
    
    char *temp = knishio_strndup(value, length);
    if (!temp) return NULL;
    
    knishio_json_t *result = knishio_json_create_string(temp);
    knishio_free(temp);
    
    return result;
}

/* Array builder */
knishio_json_array_builder_t* knishio_json_array_builder_create(void) {
    knishio_json_array_builder_t *builder = knishio_calloc(1, sizeof(knishio_json_array_builder_t));
    if (!builder) return NULL;
    
    builder->array = cJSON_CreateArray();
    if (!builder->array) {
        knishio_free(builder);
        return NULL;
    }
    
    builder->owns_array = true;
    return builder;
}

void knishio_json_array_builder_free(knishio_json_array_builder_t *builder) {
    if (!builder) return;
    
    if (builder->owns_array && builder->array) {
        cJSON_Delete(builder->array);
    }
    
    knishio_free(builder);
}

bool knishio_json_array_add_null(knishio_json_array_builder_t *builder) {
    if (!builder || !builder->array) return false;
    
    cJSON *null_item = cJSON_CreateNull();
    if (!null_item) return false;
    
    cJSON_AddItemToArray(builder->array, null_item);
    return true;
}

bool knishio_json_array_add_bool(knishio_json_array_builder_t *builder, bool value) {
    if (!builder || !builder->array) return false;
    
    cJSON *bool_item = cJSON_CreateBool(value);
    if (!bool_item) return false;
    
    cJSON_AddItemToArray(builder->array, bool_item);
    return true;
}

bool knishio_json_array_add_number(knishio_json_array_builder_t *builder, double value) {
    if (!builder || !builder->array) return false;
    
    cJSON *number_item = cJSON_CreateNumber(value);
    if (!number_item) return false;
    
    cJSON_AddItemToArray(builder->array, number_item);
    return true;
}

bool knishio_json_array_add_int(knishio_json_array_builder_t *builder, int64_t value) {
    return knishio_json_array_add_number(builder, (double)value);
}

bool knishio_json_array_add_string(knishio_json_array_builder_t *builder, const char *value) {
    if (!builder || !builder->array || !value) return false;
    
    cJSON *string_item = cJSON_CreateString(value);
    if (!string_item) return false;
    
    cJSON_AddItemToArray(builder->array, string_item);
    return true;
}

bool knishio_json_array_add(knishio_json_array_builder_t *builder, knishio_json_t *value) {
    if (!builder || !builder->array || !value) return false;
    
    /* Serialize and re-parse to get cJSON object */
    char *json_str = knishio_json_serialize(value, false);
    if (!json_str) return false;
    
    cJSON *item = cJSON_Parse(json_str);
    knishio_free(json_str);
    
    if (!item) return false;
    
    cJSON_AddItemToArray(builder->array, item);
    knishio_json_free(value); /* Ownership transferred */
    
    return true;
}

bool knishio_json_array_add_array(knishio_json_array_builder_t *builder, 
                                  knishio_json_array_builder_t *array) {
    if (!builder || !builder->array || !array || !array->array) return false;
    
    /* Duplicate the array */
    cJSON *dup = cJSON_Duplicate(array->array, true);
    if (!dup) return false;
    
    cJSON_AddItemToArray(builder->array, dup);
    return true;
}

bool knishio_json_array_add_object(knishio_json_array_builder_t *builder,
                                   knishio_json_object_builder_t *object) {
    if (!builder || !builder->array || !object || !object->object) return false;
    
    /* Duplicate the object */
    cJSON *dup = cJSON_Duplicate(object->object, true);
    if (!dup) return false;
    
    cJSON_AddItemToArray(builder->array, dup);
    return true;
}

knishio_json_t* knishio_json_array_build(knishio_json_array_builder_t *builder) {
    if (!builder || !builder->array) return NULL;
    
    char *json_str = cJSON_PrintUnformatted(builder->array);
    if (!json_str) return NULL;
    
    knishio_json_t *result = knishio_json_parse(json_str, NULL);
    cJSON_free(json_str);
    
    return result;
}

/* Object builder */
knishio_json_object_builder_t* knishio_json_object_builder_create(void) {
    knishio_json_object_builder_t *builder = knishio_calloc(1, sizeof(knishio_json_object_builder_t));
    if (!builder) return NULL;
    
    builder->object = cJSON_CreateObject();
    if (!builder->object) {
        knishio_free(builder);
        return NULL;
    }
    
    builder->owns_object = true;
    return builder;
}

void knishio_json_object_builder_free(knishio_json_object_builder_t *builder) {
    if (!builder) return;
    
    if (builder->owns_object && builder->object) {
        cJSON_Delete(builder->object);
    }
    
    knishio_free(builder);
}

bool knishio_json_object_set_null(knishio_json_object_builder_t *builder, const char *key) {
    if (!builder || !builder->object || !key) return false;
    
    cJSON *null_item = cJSON_CreateNull();
    if (!null_item) return false;
    
    cJSON_AddItemToObject(builder->object, key, null_item);
    return true;
}

bool knishio_json_object_set_bool(knishio_json_object_builder_t *builder, 
                                  const char *key, bool value) {
    if (!builder || !builder->object || !key) return false;
    
    cJSON *bool_item = cJSON_CreateBool(value);
    if (!bool_item) return false;
    
    cJSON_AddItemToObject(builder->object, key, bool_item);
    return true;
}

bool knishio_json_object_set_number(knishio_json_object_builder_t *builder,
                                    const char *key, double value) {
    if (!builder || !builder->object || !key) return false;
    
    cJSON *number_item = cJSON_CreateNumber(value);
    if (!number_item) return false;
    
    cJSON_AddItemToObject(builder->object, key, number_item);
    return true;
}

bool knishio_json_object_set_int(knishio_json_object_builder_t *builder,
                                 const char *key, int64_t value) {
    return knishio_json_object_set_number(builder, key, (double)value);
}

bool knishio_json_object_set_string(knishio_json_object_builder_t *builder,
                                    const char *key, const char *value) {
    if (!builder || !builder->object || !key || !value) return false;
    
    cJSON *string_item = cJSON_CreateString(value);
    if (!string_item) return false;
    
    cJSON_AddItemToObject(builder->object, key, string_item);
    return true;
}

bool knishio_json_object_set(knishio_json_object_builder_t *builder,
                             const char *key, knishio_json_t *value) {
    if (!builder || !builder->object || !key || !value) return false;
    
    /* Serialize and re-parse to get cJSON object */
    char *json_str = knishio_json_serialize(value, false);
    if (!json_str) return false;
    
    cJSON *item = cJSON_Parse(json_str);
    knishio_free(json_str);
    
    if (!item) return false;
    
    cJSON_AddItemToObject(builder->object, key, item);
    knishio_json_free(value); /* Ownership transferred */
    
    return true;
}

bool knishio_json_object_set_array(knishio_json_object_builder_t *builder,
                                   const char *key, knishio_json_array_builder_t *array) {
    if (!builder || !builder->object || !key || !array || !array->array) return false;
    
    /* Duplicate the array */
    cJSON *dup = cJSON_Duplicate(array->array, true);
    if (!dup) return false;
    
    cJSON_AddItemToObject(builder->object, key, dup);
    return true;
}

bool knishio_json_object_set_object(knishio_json_object_builder_t *builder,
                                    const char *key, knishio_json_object_builder_t *object) {
    if (!builder || !builder->object || !key || !object || !object->object) return false;
    
    /* Duplicate the object */
    cJSON *dup = cJSON_Duplicate(object->object, true);
    if (!dup) return false;
    
    cJSON_AddItemToObject(builder->object, key, dup);
    return true;
}

knishio_json_t* knishio_json_object_build(knishio_json_object_builder_t *builder) {
    if (!builder || !builder->object) return NULL;
    
    char *json_str = cJSON_PrintUnformatted(builder->object);
    if (!json_str) return NULL;
    
    knishio_json_t *result = knishio_json_parse(json_str, NULL);
    cJSON_free(json_str);
    
    return result;
}

/* Convenience functions */
knishio_json_t* knishio_json_create_array(void) {
    knishio_json_array_builder_t *builder = knishio_json_array_builder_create();
    if (!builder) return NULL;
    
    knishio_json_t *result = knishio_json_array_build(builder);
    knishio_json_array_builder_free(builder);
    
    return result;
}

knishio_json_t* knishio_json_create_object(void) {
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    knishio_json_t *result = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);
    
    return result;
}

knishio_json_t* knishio_json_create_array_from(knishio_json_t **values, size_t count) {
    if (!values) return NULL;
    
    knishio_json_array_builder_t *builder = knishio_json_array_builder_create();
    if (!builder) return NULL;
    
    for (size_t i = 0; i < count; i++) {
        if (values[i]) {
            knishio_json_array_add(builder, values[i]);
        }
    }
    
    knishio_json_t *result = knishio_json_array_build(builder);
    knishio_json_array_builder_free(builder);
    
    return result;
}

knishio_json_t* knishio_json_create_object_from(const char **keys, knishio_json_t **values, size_t count) {
    if (!keys || !values) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    for (size_t i = 0; i < count; i++) {
        if (keys[i] && values[i]) {
            knishio_json_object_set(builder, keys[i], values[i]);
        }
    }
    
    knishio_json_t *result = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);
    
    return result;
}

/* GraphQL-specific builders */
knishio_json_object_builder_t* knishio_json_graphql_query(const char *operation) {
    if (!operation) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    /* Create the query string */
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), "query { %s }", operation);
    
    if (!knishio_json_object_set_string(builder, "query", query_str)) {
        knishio_json_object_builder_free(builder);
        return NULL;
    }
    
    return builder;
}

knishio_json_object_builder_t* knishio_json_graphql_mutation(const char *operation) {
    if (!operation) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    /* Create the mutation string */
    char mutation_str[1024];
    snprintf(mutation_str, sizeof(mutation_str), "mutation { %s }", operation);
    
    if (!knishio_json_object_set_string(builder, "query", mutation_str)) {
        knishio_json_object_builder_free(builder);
        return NULL;
    }
    
    return builder;
}

bool knishio_json_graphql_add_variables(knishio_json_object_builder_t *builder,
                                        knishio_json_object_builder_t *variables) {
    if (!builder || !variables) return false;
    
    return knishio_json_object_set_object(builder, "variables", variables);
}

/* Enhanced GraphQL builders */
knishio_json_object_builder_t* knishio_json_graphql_query_with_variables(const char *query_string,
                                                                         knishio_json_object_builder_t *variables) {
    if (!query_string) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    if (!knishio_json_object_set_string(builder, "query", query_string)) {
        knishio_json_object_builder_free(builder);
        return NULL;
    }
    
    if (variables) {
        if (!knishio_json_object_set_object(builder, "variables", variables)) {
            knishio_json_object_builder_free(builder);
            return NULL;
        }
    }
    
    return builder;
}

knishio_json_object_builder_t* knishio_json_graphql_mutation_with_variables(const char *mutation_string,
                                                                             knishio_json_object_builder_t *variables) {
    if (!mutation_string) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    if (!knishio_json_object_set_string(builder, "query", mutation_string)) {
        knishio_json_object_builder_free(builder);
        return NULL;
    }
    
    if (variables) {
        if (!knishio_json_object_set_object(builder, "variables", variables)) {
            knishio_json_object_builder_free(builder);
            return NULL;
        }
    }
    
    return builder;
}

/* Fragment support */
bool knishio_json_graphql_add_fragment(knishio_json_object_builder_t *builder,
                                       const char *fragment_name,
                                       const char *fragment_definition) {
    if (!builder || !fragment_name || !fragment_definition) return false;
    
    /* GraphQL fragments are typically included in the query string itself,
     * but we can store them as metadata for processing */
    char fragment_key[256];
    snprintf(fragment_key, sizeof(fragment_key), "fragment_%s", fragment_name);
    
    return knishio_json_object_set_string(builder, fragment_key, fragment_definition);
}

/* KnishIO-specific GraphQL builders */
knishio_json_object_builder_t* knishio_json_build_wallet_query(const char *bundle_hash, const char *token) {
    knishio_json_object_builder_t *variables = knishio_json_object_builder_create();
    if (!variables) return NULL;
    
    if (bundle_hash) {
        knishio_json_object_set_string(variables, "bundleHash", bundle_hash);
    }
    if (token) {
        knishio_json_object_set_string(variables, "token", token);
    }
    
    const char *query = "query WalletBundle($bundleHash: String!, $token: String) { "
                       "Wallet(bundleHash: $bundleHash, token: $token) { "
                       "address bundleHash position token balance createdAt "
                       "} }";
    
    knishio_json_object_builder_t *builder = knishio_json_graphql_query_with_variables(query, variables);
    knishio_json_object_builder_free(variables);
    
    return builder;
}

knishio_json_object_builder_t* knishio_json_build_molecule_query(const char *molecule_hash) {
    if (!molecule_hash) return NULL;
    
    knishio_json_object_builder_t *variables = knishio_json_object_builder_create();
    if (!variables) return NULL;
    
    knishio_json_object_set_string(variables, "moleculeHash", molecule_hash);
    
    const char *query = "query Molecule($moleculeHash: String!) { "
                       "Molecule(molecularHash: $moleculeHash) { "
                       "molecularHash cellSlug height depth status createdAt "
                       "atoms { position walletAddress isotope token value batchId index "
                       "meta { key value } otsFragment } "
                       "} }";
    
    knishio_json_object_builder_t *builder = knishio_json_graphql_query_with_variables(query, variables);
    knishio_json_object_builder_free(variables);
    
    return builder;
}

knishio_json_object_builder_t* knishio_json_build_balance_query(const char *address, const char *token) {
    knishio_json_object_builder_t *variables = knishio_json_object_builder_create();
    if (!variables) return NULL;
    
    if (address) {
        knishio_json_object_set_string(variables, "address", address);
    }
    if (token) {
        knishio_json_object_set_string(variables, "token", token);
    }
    
    const char *query = "query Balance($address: String!, $token: String) { "
                       "Balance(address: $address, token: $token) { "
                       "address token balance "
                       "} }";
    
    knishio_json_object_builder_t *builder = knishio_json_graphql_query_with_variables(query, variables);
    knishio_json_object_builder_free(variables);
    
    return builder;
}

knishio_json_object_builder_t* knishio_json_build_propose_molecule_mutation(const char *molecule_json) {
    if (!molecule_json) return NULL;
    
    knishio_json_object_builder_t *variables = knishio_json_object_builder_create();
    if (!variables) return NULL;
    
    knishio_json_object_set_string(variables, "molecule", molecule_json);
    
    const char *mutation = "mutation ProposeMolecule($molecule: String!) { "
                          "ProposeMolecule(molecule: $molecule) { "
                          "molecularHash status reason payload "
                          "} }";
    
    knishio_json_object_builder_t *builder = knishio_json_graphql_mutation_with_variables(mutation, variables);
    knishio_json_object_builder_free(variables);
    
    return builder;
}

/* Utility functions for building complex nested structures */
knishio_json_object_builder_t* knishio_json_build_atom_input(const knishio_atom_t *atom) {
    if (!atom) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    /* This would serialize an atom for GraphQL input - placeholder for now */
    /* Implementation would depend on actual knishio_atom_t structure */
    
    return builder;
}

knishio_json_object_builder_t* knishio_json_build_wallet_input(const char *address, const char *bundle_hash,
                                                                const char *position, const char *token) {
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    if (address) knishio_json_object_set_string(builder, "address", address);
    if (bundle_hash) knishio_json_object_set_string(builder, "bundleHash", bundle_hash);
    if (position) knishio_json_object_set_string(builder, "position", position);
    if (token) knishio_json_object_set_string(builder, "token", token);
    
    return builder;
}

/* JSON response validation helpers */
bool knishio_json_is_graphql_response(const knishio_json_t *json) {
    if (!json || json->type != KNISHIO_JSON_OBJECT) {
        return false;
    }
    
    /* Valid GraphQL response should have either 'data' or 'errors' field */
    return knishio_json_object_has(json, "data") || knishio_json_object_has(json, "errors");
}

bool knishio_json_is_successful_response(const knishio_json_t *json) {
    if (!knishio_json_is_graphql_response(json)) {
        return false;
    }
    
    knishio_json_t *errors = knishio_json_object_get(json, "errors");
    if (errors) {
        bool has_errors = (errors->type == KNISHIO_JSON_ARRAY && knishio_json_array_size(errors) > 0);
        knishio_json_free(errors);
        return !has_errors;
    }
    
    return knishio_json_object_has(json, "data");
}

/* Streaming JSON builder functions implementation */

knishio_error_t knishio_json_builder_start_object(knishio_json_builder_t *builder) {
    if (!builder) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (builder->root) {
        cJSON_Delete(builder->root);
    }
    
    builder->root = cJSON_CreateObject();
    if (!builder->root) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_json_builder_end_object(knishio_json_builder_t *builder) {
    if (!builder || !builder->root) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Object is already complete, nothing special needed */
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_json_builder_add_key(knishio_json_builder_t *builder, const char *key) {
    if (!builder || !key) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* This is a no-op in this implementation - keys are added with values */
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_json_builder_add_string(knishio_json_builder_t *builder, const char *key, const char *value) {
    if (!builder || !key) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!builder->root) {
        knishio_error_t error = knishio_json_builder_start_object(builder);
        if (error != KNISHIO_SUCCESS) {
            return error;
        }
    }
    
    cJSON *string_item = cJSON_CreateString(value ? value : "");
    if (!string_item) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    cJSON_AddItemToObject(builder->root, key, string_item);
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_json_builder_add_number(knishio_json_builder_t *builder, const char *key, double value) {
    if (!builder || !key) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!builder->root) {
        knishio_error_t error = knishio_json_builder_start_object(builder);
        if (error != KNISHIO_SUCCESS) {
            return error;
        }
    }
    
    cJSON *number_item = cJSON_CreateNumber(value);
    if (!number_item) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    cJSON_AddItemToObject(builder->root, key, number_item);
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_json_builder_add_boolean(knishio_json_builder_t *builder, const char *key, bool value) {
    if (!builder || !key) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!builder->root) {
        knishio_error_t error = knishio_json_builder_start_object(builder);
        if (error != KNISHIO_SUCCESS) {
            return error;
        }
    }
    
    cJSON *bool_item = value ? cJSON_CreateTrue() : cJSON_CreateFalse();
    if (!bool_item) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    cJSON_AddItemToObject(builder->root, key, bool_item);
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_json_builder_add_null(knishio_json_builder_t *builder, const char *key) {
    if (!builder || !key) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!builder->root) {
        knishio_error_t error = knishio_json_builder_start_object(builder);
        if (error != KNISHIO_SUCCESS) {
            return error;
        }
    }
    
    cJSON *null_item = cJSON_CreateNull();
    if (!null_item) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    cJSON_AddItemToObject(builder->root, key, null_item);
    return KNISHIO_SUCCESS;
}

/* Additional helper functions */

#include "knishio/json.h"

knishio_error_t knishio_json_builder_add_raw(knishio_json_builder_t *builder, const char *key, const char *raw_json) {
    if (!builder || !key) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!raw_json) {
        /* Treat NULL as null value */
        return knishio_json_builder_add_null(builder, key);
    }
    
    #ifdef HAVE_CJSON
    /* Parse the raw JSON string */
    cJSON *parsed = cJSON_Parse(raw_json);
    if (!parsed) {
        /* If parsing fails, add as string */
        return knishio_json_builder_add_string(builder, key, raw_json);
    }
    
    /* Add parsed JSON to builder */
    cJSON_AddItemToObject(builder->root, key, parsed);
    return KNISHIO_SUCCESS;
    #else
    /* Without cJSON, just add as string */
    return knishio_json_builder_add_string(builder, key, raw_json);
    #endif
}

knishio_error_t knishio_json_builder_add_json(knishio_json_builder_t *builder, const char *key, const knishio_json_t *json) {
    if (!builder || !key || !json) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Convert JSON to string and add as raw */
    char *json_str = NULL;
    knishio_error_t error = knishio_json_to_string(json, &json_str);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    error = knishio_json_builder_add_raw(builder, key, json_str);
    knishio_free(json_str);
    
    return error;
}
