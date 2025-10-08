#ifndef KNISHIO_RESPONSE_CREATE_META_H
#define KNISHIO_RESPONSE_CREATE_META_H

/**
 * @file response_create_meta.h
 * @brief Response for CreateMeta mutation operations
 * 
 * Handles responses from CreateMeta mutations that create metadata atoms
 * in the KnishIO distributed ledger following 2025 C17 best practices.
 */

#include "response.h"
#include "../molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_create_meta knishio_response_create_meta_t;

/**
 * @brief CreateMeta response structure
 * 
 * Represents the response from a CreateMeta GraphQL mutation operation
 * that creates metadata atoms with key-value pairs.
 */
struct knishio_response_create_meta {
    knishio_response_t base;                /**< Base response */
    knishio_molecule_t *molecule;           /**< Created molecule with metadata */
    char *molecular_hash;                   /**< Hash of created molecule */
    char *meta_type;                        /**< Type of metadata created */
    char *meta_id;                          /**< Metadata identifier */
    char *meta_value;                       /**< Metadata value */
    bool success;                           /**< Operation success flag */
};

/**
 * @brief Create CreateMeta response
 * @param query Original CreateMeta mutation query
 * @param json JSON response data from GraphQL
 * @return CreateMeta response or NULL on error
 */
knishio_response_create_meta_t* knishio_response_create_meta_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free CreateMeta response
 * @param response CreateMeta response to free
 */
void knishio_response_create_meta_free(knishio_response_create_meta_t *response);

/**
 * @brief Get created molecule
 * @param response CreateMeta response
 * @return Created molecule or NULL if operation failed
 */
knishio_molecule_t* knishio_response_create_meta_get_molecule(
    const knishio_response_create_meta_t *response
);

/**
 * @brief Get molecular hash of created metadata
 * @param response CreateMeta response
 * @return Molecular hash or NULL if operation failed
 */
const char* knishio_response_create_meta_get_molecular_hash(
    const knishio_response_create_meta_t *response
);

/**
 * @brief Get metadata type
 * @param response CreateMeta response
 * @return Metadata type string or NULL if not available
 */
const char* knishio_response_create_meta_get_meta_type(
    const knishio_response_create_meta_t *response
);

/**
 * @brief Get metadata identifier
 * @param response CreateMeta response
 * @return Metadata ID string or NULL if not available
 */
const char* knishio_response_create_meta_get_meta_id(
    const knishio_response_create_meta_t *response
);

/**
 * @brief Get metadata value
 * @param response CreateMeta response
 * @return Metadata value string or NULL if not available
 */
const char* knishio_response_create_meta_get_meta_value(
    const knishio_response_create_meta_t *response
);

/**
 * @brief Check if CreateMeta operation was successful
 * @param response CreateMeta response
 * @return True if successful, false otherwise
 */
bool knishio_response_create_meta_is_successful(
    const knishio_response_create_meta_t *response
);

/**
 * @brief Get created metadata information as formatted string
 * @param response CreateMeta response
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written or -1 on error
 */
int knishio_response_create_meta_get_info(
    const knishio_response_create_meta_t *response,
    char *buffer,
    size_t buffer_size
);

/* Factory function for response creation */

/**
 * @brief Factory function for CreateMeta response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_create_meta_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for CreateMeta operations */
extern const char* const KNISHIO_CREATE_META_SUCCESS_MESSAGE;
extern const char* const KNISHIO_CREATE_META_DEFAULT_TYPE;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_create_meta_t) > sizeof(knishio_response_t),
    "CreateMeta response must be larger than base response");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_CREATE_META_H */