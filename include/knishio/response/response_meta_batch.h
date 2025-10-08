#ifndef KNISHIO_RESPONSE_META_BATCH_H
#define KNISHIO_RESPONSE_META_BATCH_H

/**
 * @file response_meta_batch.h
 * @brief Response for MetaBatch query operations
 * 
 * Handles responses from MetaBatch queries that retrieve multiple metadata
 * entries in batch operations following 2025 C17 best practices.
 */

#include "response.h"
#include "../atom.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_meta_batch knishio_response_meta_batch_t;

/**
 * @brief Metadata entry in batch response
 */
typedef struct knishio_meta_batch_entry {
    char *meta_type;                        /**< Metadata type */
    char *meta_id;                          /**< Metadata identifier */
    char *meta_value;                       /**< Metadata value */
    char *molecular_hash;                   /**< Associated molecular hash */
    knishio_atom_t *atom;                   /**< Associated atom */
    char *created_at;                       /**< Creation timestamp */
} knishio_meta_batch_entry_t;

/**
 * @brief MetaBatch response structure
 * 
 * Represents the response from a MetaBatch GraphQL query operation
 * that retrieves multiple metadata entries in a single request.
 */
struct knishio_response_meta_batch {
    knishio_response_t base;                /**< Base response */
    knishio_meta_batch_entry_t *entries;    /**< Array of metadata entries */
    size_t entry_count;                     /**< Number of entries */
    size_t entry_capacity;                  /**< Current capacity */
    char *batch_id;                         /**< Batch identifier */
    bool has_more_results;                  /**< Pagination flag */
    char *next_cursor;                      /**< Next page cursor */
};

/**
 * @brief Create MetaBatch response
 * @param query Original MetaBatch query
 * @param json JSON response data from GraphQL
 * @return MetaBatch response or NULL on error
 */
knishio_response_meta_batch_t* knishio_response_meta_batch_create(
    knishio_query_t *query,
    knishio_json_t *json
);

/**
 * @brief Free MetaBatch response
 * @param response MetaBatch response to free
 */
void knishio_response_meta_batch_free(knishio_response_meta_batch_t *response);

/**
 * @brief Get metadata entries
 * @param response MetaBatch response
 * @param count Output parameter for number of entries
 * @return Array of metadata entries or NULL if no entries
 */
const knishio_meta_batch_entry_t* knishio_response_meta_batch_get_entries(
    const knishio_response_meta_batch_t *response,
    size_t *count
);

/**
 * @brief Get metadata entry by index
 * @param response MetaBatch response
 * @param index Entry index
 * @return Metadata entry or NULL if index out of bounds
 */
const knishio_meta_batch_entry_t* knishio_response_meta_batch_get_entry(
    const knishio_response_meta_batch_t *response,
    size_t index
);

/**
 * @brief Find metadata entry by type and ID
 * @param response MetaBatch response
 * @param meta_type Metadata type to search for
 * @param meta_id Metadata ID to search for (optional)
 * @return First matching metadata entry or NULL if not found
 */
const knishio_meta_batch_entry_t* knishio_response_meta_batch_find_entry(
    const knishio_response_meta_batch_t *response,
    const char *meta_type,
    const char *meta_id
);

/**
 * @brief Get batch identifier
 * @param response MetaBatch response
 * @return Batch ID or NULL if not available
 */
const char* knishio_response_meta_batch_get_batch_id(
    const knishio_response_meta_batch_t *response
);

/**
 * @brief Check if more results are available
 * @param response MetaBatch response
 * @return True if more results available, false otherwise
 */
bool knishio_response_meta_batch_has_more_results(
    const knishio_response_meta_batch_t *response
);

/**
 * @brief Get next page cursor for pagination
 * @param response MetaBatch response
 * @return Next cursor string or NULL if no more pages
 */
const char* knishio_response_meta_batch_get_next_cursor(
    const knishio_response_meta_batch_t *response
);

/**
 * @brief Get number of entries in batch
 * @param response MetaBatch response
 * @return Number of metadata entries
 */
size_t knishio_response_meta_batch_get_count(
    const knishio_response_meta_batch_t *response
);

/**
 * @brief Get batch summary as formatted string
 * @param response MetaBatch response
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written or -1 on error
 */
int knishio_response_meta_batch_get_summary(
    const knishio_response_meta_batch_t *response,
    char *buffer,
    size_t buffer_size
);

/* Entry manipulation functions */

/**
 * @brief Free metadata batch entry
 * @param entry Entry to free
 */
void knishio_meta_batch_entry_free(knishio_meta_batch_entry_t *entry);

/**
 * @brief Copy metadata batch entry
 * @param entry Entry to copy
 * @return Copied entry or NULL on error
 */
knishio_meta_batch_entry_t* knishio_meta_batch_entry_copy(
    const knishio_meta_batch_entry_t *entry
);

/* Factory function for response creation */

/**
 * @brief Factory function for MetaBatch response
 * @param query Original query
 * @param json JSON response data
 * @return Response as base type for polymorphic usage
 */
knishio_response_t* knishio_response_meta_batch_factory(
    knishio_query_t *query,
    knishio_json_t *json
);

/* Constants for MetaBatch operations */
extern const char* const KNISHIO_META_BATCH_DEFAULT_LIMIT;
extern const size_t KNISHIO_META_BATCH_DEFAULT_CAPACITY;

/* C17 Static assertions for structure safety */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
_Static_assert(sizeof(knishio_response_meta_batch_t) > sizeof(knishio_response_t),
    "MetaBatch response must be larger than base response");
_Static_assert(sizeof(knishio_meta_batch_entry_t) >= sizeof(char*) * 6,
    "MetaBatch entry must have sufficient space for all fields");
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_META_BATCH_H */