#ifndef KNISHIO_GRAPHQL_H
#define KNISHIO_GRAPHQL_H

/**
 * @file graphql.h
 * @brief GraphQL operations for KnishIO C SDK
 * 
 * Essential GraphQL operations for post-blockchain DLT communication.
 * JavaScript SDK compatible query/mutation handling using KISS principles.
 * Focuses on 80% use case: ProposeMolecule and QueryBalance operations.
 */

#include "knishio/knishio.h"
#include "knishio/http.h"
#include "knishio/molecule.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_http_client knishio_http_client_t;

/* Forward declarations for auth integration */
typedef struct knishio_auth_token knishio_auth_token_t;

/* GraphQL client structure */
typedef struct knishio_graphql_client {
    knishio_http_client_t* http_client;  /**< HTTP client for requests */
    char* endpoint_url;                  /**< GraphQL endpoint URL */
    char* cell_slug;                     /**< Cell identifier */
    knishio_auth_token_t* auth_token;    /**< Authentication token */
    bool debug_mode;                     /**< Enable debug logging */
} knishio_graphql_client_t;

/* Generic GraphQL operation structure for full JS SDK alignment */
typedef struct knishio_graphql_operation {
    const char* name;                    /**< Operation name */
    const char* query;                    /**< GraphQL query/mutation string */
    const char* variables_json;          /**< Variables as JSON string */
    bool requires_auth;                  /**< Requires authentication */
    bool is_mutation;                    /**< True for mutation, false for query */
} knishio_graphql_operation_t;

/* Generic GraphQL response structure */
typedef struct knishio_graphql_response {
    bool success;                        /**< Operation success flag */
    char* data;                          /**< Response data as JSON */
    char* errors;                        /**< Errors as JSON if any */
    char* molecular_hash;                /**< Molecular hash for mutations */
    long status_code;                    /**< HTTP status code */
} knishio_graphql_response_t;

/* Balance query result structure */
typedef struct knishio_balance_result {
    char* token;                         /**< Token type */
    char* value;                         /**< Balance value */
    char* wallet_address;                /**< Wallet address */
    char* position;                      /**< Wallet position */
    bool success;                        /**< Query success flag */
} knishio_balance_result_t;

/* Molecule proposal result structure */
typedef struct knishio_proposal_result {
    char* molecular_hash;                /**< Resulting molecular hash */
    char* response_json;                 /**< Full response JSON */
    bool success;                        /**< Proposal success flag */
    long status_code;                    /**< HTTP status code */
} knishio_proposal_result_t;

/* GraphQL client lifecycle */

/**
 * @brief Create GraphQL client
 * @param client Output client pointer
 * @param endpoint_url GraphQL endpoint URL
 * @param cell_slug Cell identifier (optional)
 * @return Success or error code
 */
knishio_error_t knishio_graphql_client_create(
    knishio_graphql_client_t** client,
    const char* endpoint_url,
    const char* cell_slug
);

/**
 * @brief Free GraphQL client
 * @param client Client to free
 */
void knishio_graphql_client_free(knishio_graphql_client_t* client);

/**
 * @brief Set debug mode
 * @param client GraphQL client
 * @param debug_mode Enable/disable debug mode
 * @return Success or error code
 */
knishio_error_t knishio_graphql_client_set_debug(
    knishio_graphql_client_t* client,
    bool debug_mode
);

/**
 * @brief Set authentication token for GraphQL client
 * @param client GraphQL client
 * @param auth_token Authentication token
 * @return Success or error code
 */
knishio_error_t knishio_graphql_client_set_auth_token(
    knishio_graphql_client_t* client,
    knishio_auth_token_t* auth_token
);

/* Generic GraphQL operation execution for full JS SDK alignment */

/**
 * @brief Execute generic GraphQL operation
 * @param client GraphQL client
 * @param operation Operation to execute
 * @param response Output response
 * @return Success or error code
 */
knishio_error_t knishio_graphql_execute(
    knishio_graphql_client_t* client,
    const knishio_graphql_operation_t* operation,
    knishio_graphql_response_t** response
);

/**
 * @brief Execute GraphQL query
 * @param client GraphQL client
 * @param query Query string
 * @param variables Variables JSON (optional)
 * @param response Output response
 * @return Success or error code
 */
knishio_error_t knishio_graphql_query(
    knishio_graphql_client_t* client,
    const char* query,
    const char* variables,
    knishio_graphql_response_t** response
);

/**
 * @brief Execute GraphQL mutation
 * @param client GraphQL client
 * @param mutation Mutation string
 * @param variables Variables JSON (optional)
 * @param response Output response
 * @return Success or error code
 */
knishio_error_t knishio_graphql_mutation(
    knishio_graphql_client_t* client,
    const char* mutation,
    const char* variables,
    knishio_graphql_response_t** response
);

/**
 * @brief Free GraphQL response
 * @param response Response to free
 */
void knishio_graphql_response_free(knishio_graphql_response_t* response);

/* Essential GraphQL operations (80/20 rule) */

/**
 * @brief Propose molecule to the network
 * @param client GraphQL client
 * @param molecule Molecule to propose
 * @param result Output proposal result
 * @return Success or error code
 */
knishio_error_t knishio_graphql_propose_molecule(
    knishio_graphql_client_t* client,
    const knishio_molecule_t* molecule,
    knishio_proposal_result_t** result
);

/**
 * @brief Query balance for wallet
 * @param client GraphQL client
 * @param wallet_address Wallet address to query
 * @param token Token type (optional, queries all if NULL)
 * @param result Output balance result
 * @return Success or error code
 */
knishio_error_t knishio_graphql_query_balance(
    knishio_graphql_client_t* client,
    const char* wallet_address,
    const char* token,
    knishio_balance_result_t** result
);

/* Result structure lifecycle */

/**
 * @brief Create balance result
 * @param result Output result pointer
 * @return Success or error code
 */
knishio_error_t knishio_balance_result_create(knishio_balance_result_t** result);

/**
 * @brief Free balance result
 * @param result Result to free
 */
void knishio_balance_result_free(knishio_balance_result_t* result);

/**
 * @brief Create proposal result
 * @param result Output result pointer
 * @return Success or error code
 */
knishio_error_t knishio_proposal_result_create(knishio_proposal_result_t** result);

/**
 * @brief Free proposal result
 * @param result Result to free
 */
void knishio_proposal_result_free(knishio_proposal_result_t* result);

/* Result accessor functions */

/**
 * @brief Get balance value
 * @param result Balance result
 * @return Balance value string (do not free)
 */
const char* knishio_balance_result_get_value(const knishio_balance_result_t* result);

/**
 * @brief Get balance token
 * @param result Balance result
 * @return Token type string (do not free)
 */
const char* knishio_balance_result_get_token(const knishio_balance_result_t* result);

/**
 * @brief Get balance wallet address
 * @param result Balance result
 * @return Wallet address string (do not free)
 */
const char* knishio_balance_result_get_wallet_address(const knishio_balance_result_t* result);

/**
 * @brief Check if balance query was successful
 * @param result Balance result
 * @return True if successful
 */
bool knishio_balance_result_is_success(const knishio_balance_result_t* result);

/**
 * @brief Get proposal molecular hash
 * @param result Proposal result
 * @return Molecular hash string (do not free)
 */
const char* knishio_proposal_result_get_hash(const knishio_proposal_result_t* result);

/**
 * @brief Get proposal response JSON
 * @param result Proposal result
 * @return Response JSON string (do not free)
 */
const char* knishio_proposal_result_get_response(const knishio_proposal_result_t* result);

/**
 * @brief Check if proposal was successful
 * @param result Proposal result
 * @return True if successful
 */
bool knishio_proposal_result_is_success(const knishio_proposal_result_t* result);

/**
 * @brief Get proposal status code
 * @param result Proposal result
 * @return HTTP status code
 */
long knishio_proposal_result_get_status_code(const knishio_proposal_result_t* result);

/* Utility functions */

/**
 * @brief Build GraphQL query for balance
 * @param wallet_address Wallet address
 * @param token Token type (optional)
 * @param query Output query string (caller must free)
 * @return Success or error code
 */
knishio_error_t knishio_graphql_build_balance_query(
    const char* wallet_address,
    const char* token,
    char** query
);

/**
 * @brief Build GraphQL mutation for molecule proposal
 * @param molecule Molecule to propose
 * @param mutation Output mutation string (caller must free)
 * @return Success or error code
 */
knishio_error_t knishio_graphql_build_propose_mutation(
    const knishio_molecule_t* molecule,
    char** mutation
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_GRAPHQL_H */
