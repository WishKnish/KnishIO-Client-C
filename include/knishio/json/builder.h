#ifndef KNISHIO_JSON_BUILDER_H
#define KNISHIO_JSON_BUILDER_H

/**
 * @file builder.h
 * @brief JSON building utilities for KnishIO SDK
 * 
 * Provides JSON construction functionality for building GraphQL queries,
 * mutations, and complex data structures compatible with the JavaScript SDK.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "knishio/error/context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_json knishio_json_t;
typedef struct knishio_json_builder knishio_json_builder_t;
typedef struct knishio_json_array_builder knishio_json_array_builder_t;
typedef struct knishio_json_object_builder knishio_json_object_builder_t;

/* Builder creation */

/**
 * @brief Create a new JSON builder
 * @return New JSON builder or NULL on error
 */
knishio_json_builder_t* knishio_json_builder_create(void);

/**
 * @brief Free JSON builder
 * @param builder Builder to free
 */
void knishio_json_builder_free(knishio_json_builder_t *builder);

/**
 * @brief Build JSON object from builder
 * @param builder Builder with constructed JSON
 * @return Built JSON object (caller must free) or NULL on error
 */
knishio_json_t* knishio_json_builder_build(knishio_json_builder_t *builder);

/**
 * @brief Build JSON string from builder
 * @param builder Builder with constructed JSON
 * @param pretty Pretty print if true
 * @return JSON string (caller must free) or NULL on error
 */
char* knishio_json_builder_to_string(knishio_json_builder_t *builder, bool pretty);

/* Value builders */

/**
 * @brief Create null value
 * @return JSON null value
 */
knishio_json_t* knishio_json_create_null(void);

/**
 * @brief Create boolean value
 * @param value Boolean value
 * @return JSON boolean value
 */
knishio_json_t* knishio_json_create_bool(bool value);

/**
 * @brief Create number value
 * @param value Number value
 * @return JSON number value
 */
knishio_json_t* knishio_json_create_number(double value);

/**
 * @brief Create integer value
 * @param value Integer value
 * @return JSON integer value
 */
knishio_json_t* knishio_json_create_int(int64_t value);

/**
 * @brief Create string value
 * @param value String value
 * @return JSON string value
 */
knishio_json_t* knishio_json_create_string(const char *value);

/**
 * @brief Create string value with length
 * @param value String value
 * @param length String length
 * @return JSON string value
 */
knishio_json_t* knishio_json_create_string_n(const char *value, size_t length);

/* Array builder */

/**
 * @brief Create array builder
 * @return New array builder or NULL on error
 */
knishio_json_array_builder_t* knishio_json_array_builder_create(void);

/**
 * @brief Free array builder
 * @param builder Array builder to free
 */
void knishio_json_array_builder_free(knishio_json_array_builder_t *builder);

/**
 * @brief Add null to array
 * @param builder Array builder
 * @return True on success, false on error
 */
bool knishio_json_array_add_null(knishio_json_array_builder_t *builder);

/**
 * @brief Add boolean to array
 * @param builder Array builder
 * @param value Boolean value
 * @return True on success, false on error
 */
bool knishio_json_array_add_bool(knishio_json_array_builder_t *builder, bool value);

/**
 * @brief Add number to array
 * @param builder Array builder
 * @param value Number value
 * @return True on success, false on error
 */
bool knishio_json_array_add_number(knishio_json_array_builder_t *builder, double value);

/**
 * @brief Add integer to array
 * @param builder Array builder
 * @param value Integer value
 * @return True on success, false on error
 */
bool knishio_json_array_add_int(knishio_json_array_builder_t *builder, int64_t value);

/**
 * @brief Add string to array
 * @param builder Array builder
 * @param value String value
 * @return True on success, false on error
 */
bool knishio_json_array_add_string(knishio_json_array_builder_t *builder, const char *value);

/**
 * @brief Add JSON value to array
 * @param builder Array builder
 * @param value JSON value to add (ownership transferred)
 * @return True on success, false on error
 */
bool knishio_json_array_add(knishio_json_array_builder_t *builder, knishio_json_t *value);

/**
 * @brief Add array to array
 * @param builder Array builder
 * @param array Array builder to add as nested array
 * @return True on success, false on error
 */
bool knishio_json_array_add_array(knishio_json_array_builder_t *builder, 
                                  knishio_json_array_builder_t *array);

/**
 * @brief Add object to array
 * @param builder Array builder
 * @param object Object builder to add as nested object
 * @return True on success, false on error
 */
bool knishio_json_array_add_object(knishio_json_array_builder_t *builder,
                                   knishio_json_object_builder_t *object);

/**
 * @brief Build array
 * @param builder Array builder
 * @return Built JSON array (caller must free) or NULL on error
 */
knishio_json_t* knishio_json_array_build(knishio_json_array_builder_t *builder);

/* Object builder */

/**
 * @brief Create object builder
 * @return New object builder or NULL on error
 */
knishio_json_object_builder_t* knishio_json_object_builder_create(void);

/**
 * @brief Free object builder
 * @param builder Object builder to free
 */
void knishio_json_object_builder_free(knishio_json_object_builder_t *builder);

/**
 * @brief Add null property
 * @param builder Object builder
 * @param key Property key
 * @return True on success, false on error
 */
bool knishio_json_object_set_null(knishio_json_object_builder_t *builder, const char *key);

/**
 * @brief Add boolean property
 * @param builder Object builder
 * @param key Property key
 * @param value Boolean value
 * @return True on success, false on error
 */
bool knishio_json_object_set_bool(knishio_json_object_builder_t *builder, 
                                  const char *key, bool value);

/**
 * @brief Add number property
 * @param builder Object builder
 * @param key Property key
 * @param value Number value
 * @return True on success, false on error
 */
bool knishio_json_object_set_number(knishio_json_object_builder_t *builder,
                                    const char *key, double value);

/**
 * @brief Add integer property
 * @param builder Object builder
 * @param key Property key
 * @param value Integer value
 * @return True on success, false on error
 */
bool knishio_json_object_set_int(knishio_json_object_builder_t *builder,
                                 const char *key, int64_t value);

/**
 * @brief Add string property
 * @param builder Object builder
 * @param key Property key
 * @param value String value
 * @return True on success, false on error
 */
bool knishio_json_object_set_string(knishio_json_object_builder_t *builder,
                                    const char *key, const char *value);

/**
 * @brief Add JSON value property
 * @param builder Object builder
 * @param key Property key
 * @param value JSON value (ownership transferred)
 * @return True on success, false on error
 */
bool knishio_json_object_set(knishio_json_object_builder_t *builder,
                             const char *key, knishio_json_t *value);

/**
 * @brief Add array property
 * @param builder Object builder
 * @param key Property key
 * @param array Array builder to add as property
 * @return True on success, false on error
 */
bool knishio_json_object_set_array(knishio_json_object_builder_t *builder,
                                   const char *key, knishio_json_array_builder_t *array);

/**
 * @brief Add object property
 * @param builder Object builder
 * @param key Property key
 * @param object Object builder to add as nested object
 * @return True on success, false on error
 */
bool knishio_json_object_set_object(knishio_json_object_builder_t *builder,
                                    const char *key, knishio_json_object_builder_t *object);

/**
 * @brief Build object
 * @param builder Object builder
 * @return Built JSON object (caller must free) or NULL on error
 */
knishio_json_t* knishio_json_object_build(knishio_json_object_builder_t *builder);

/* Convenience functions */

/**
 * @brief Create empty array
 * @return Empty JSON array
 */
knishio_json_t* knishio_json_create_array(void);

/**
 * @brief Create empty object
 * @return Empty JSON object
 */
knishio_json_t* knishio_json_create_object(void);

/**
 * @brief Create array from values
 * @param values Array of JSON values (ownership transferred)
 * @param count Number of values
 * @return JSON array or NULL on error
 */
knishio_json_t* knishio_json_create_array_from(knishio_json_t **values, size_t count);

/**
 * @brief Create object from key-value pairs
 * @param keys Array of keys
 * @param values Array of JSON values (ownership transferred)
 * @param count Number of key-value pairs
 * @return JSON object or NULL on error
 */
knishio_json_t* knishio_json_create_object_from(const char **keys, knishio_json_t **values, size_t count);

/* Streaming JSON builder functions */

/**
 * @brief Start building an object
 * @param builder JSON builder
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_builder_start_object(knishio_json_builder_t *builder);

/**
 * @brief End building an object
 * @param builder JSON builder
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_builder_end_object(knishio_json_builder_t *builder);

/**
 * @brief Add a key for the next value
 * @param builder JSON builder
 * @param key Key string
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_builder_add_key(knishio_json_builder_t *builder, const char *key);

/**
 * @brief Add a string value
 * @param builder JSON builder
 * @param key Key string
 * @param value String value
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_builder_add_string(knishio_json_builder_t *builder, const char *key, const char *value);

/**
 * @brief Add a number value
 * @param builder JSON builder
 * @param key Key string
 * @param value Number value
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_builder_add_number(knishio_json_builder_t *builder, const char *key, double value);

/**
 * @brief Add a boolean value
 * @param builder JSON builder
 * @param key Key string
 * @param value Boolean value
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_builder_add_boolean(knishio_json_builder_t *builder, const char *key, bool value);

/**
 * @brief Add a null value
 * @param builder JSON builder
 * @param key Key string
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_builder_add_null(knishio_json_builder_t *builder, const char *key);

/* GraphQL-specific builders */

/**
 * @brief Create GraphQL query builder
 * @param operation Query operation name
 * @return Object builder configured for GraphQL query
 */
knishio_json_object_builder_t* knishio_json_graphql_query(const char *operation);

/**
 * @brief Create GraphQL mutation builder
 * @param operation Mutation operation name
 * @return Object builder configured for GraphQL mutation
 */
knishio_json_object_builder_t* knishio_json_graphql_mutation(const char *operation);

/**
 * @brief Add GraphQL variables to query/mutation
 * @param builder Query/mutation builder
 * @param variables Variables object builder
 * @return True on success, false on error
 */
bool knishio_json_graphql_add_variables(knishio_json_object_builder_t *builder,
                                        knishio_json_object_builder_t *variables);

/* Enhanced GraphQL builders */

/**
 * @brief Create GraphQL query with variables
 * @param query_string Complete GraphQL query string
 * @param variables Variables object builder (can be NULL)
 * @return Object builder configured for GraphQL query
 */
knishio_json_object_builder_t* knishio_json_graphql_query_with_variables(const char *query_string,
                                                                         knishio_json_object_builder_t *variables);

/**
 * @brief Create GraphQL mutation with variables
 * @param mutation_string Complete GraphQL mutation string
 * @param variables Variables object builder (can be NULL)
 * @return Object builder configured for GraphQL mutation
 */
knishio_json_object_builder_t* knishio_json_graphql_mutation_with_variables(const char *mutation_string,
                                                                             knishio_json_object_builder_t *variables);

/**
 * @brief Add GraphQL fragment to query/mutation
 * @param builder Query/mutation builder
 * @param fragment_name Fragment name
 * @param fragment_definition Fragment definition
 * @return True on success, false on error
 */
bool knishio_json_graphql_add_fragment(knishio_json_object_builder_t *builder,
                                       const char *fragment_name,
                                       const char *fragment_definition);

/* KnishIO-specific GraphQL builders */

/**
 * @brief Build wallet query
 * @param bundle_hash Wallet bundle hash
 * @param token Token identifier (can be NULL)
 * @return GraphQL query builder
 */
knishio_json_object_builder_t* knishio_json_build_wallet_query(const char *bundle_hash, const char *token);

/**
 * @brief Build molecule query
 * @param molecule_hash Molecule hash
 * @return GraphQL query builder
 */
knishio_json_object_builder_t* knishio_json_build_molecule_query(const char *molecule_hash);

/**
 * @brief Build balance query
 * @param address Wallet address
 * @param token Token identifier
 * @return GraphQL query builder
 */
knishio_json_object_builder_t* knishio_json_build_balance_query(const char *address, const char *token);

/**
 * @brief Build propose molecule mutation
 * @param molecule_json Serialized molecule JSON
 * @return GraphQL mutation builder
 */
knishio_json_object_builder_t* knishio_json_build_propose_molecule_mutation(const char *molecule_json);

/* Forward declaration for atom structure */
typedef struct knishio_atom knishio_atom_t;

/**
 * @brief Build atom input for GraphQL
 * @param atom Source atom
 * @return Object builder for atom input
 */
knishio_json_object_builder_t* knishio_json_build_atom_input(const knishio_atom_t *atom);

/**
 * @brief Build wallet input for GraphQL
 * @param address Wallet address
 * @param bundle_hash Bundle hash
 * @param position Wallet position
 * @param token Token identifier
 * @return Object builder for wallet input
 */
knishio_json_object_builder_t* knishio_json_build_wallet_input(const char *address, const char *bundle_hash,
                                                                const char *position, const char *token);

/* Response validation helpers */

/**
 * @brief Check if JSON is a valid GraphQL response
 * @param json JSON to validate
 * @return True if valid GraphQL response, false otherwise
 */
bool knishio_json_is_graphql_response(const knishio_json_t *json);

/**
 * @brief Check if GraphQL response indicates success
 * @param json GraphQL response JSON
 * @return True if successful (no errors), false otherwise
 */
bool knishio_json_is_successful_response(const knishio_json_t *json);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_JSON_BUILDER_H */
