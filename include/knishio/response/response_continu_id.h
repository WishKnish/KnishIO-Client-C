#ifndef KNISHIO_RESPONSE_CONTINU_ID_H
#define KNISHIO_RESPONSE_CONTINU_ID_H

/**
 * @file response_continu_id.h
 * @brief Response for ContinuID queries
 * 
 * Handles responses from ContinuID queries, providing access to identity
 * information and metadata compatible with the JavaScript SDK's ResponseContinuId.
 */

#include "response.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_continu_id knishio_response_continu_id_t;

/**
 * @brief ContinuID identity information
 */
typedef struct {
    char *bundle_hash;                  /**< Identity bundle hash */
    char *position;                     /**< Wallet position */
    char *address;                      /**< Wallet address */
    char *pubkey;                       /**< Public key */
    char *created_at;                   /**< Creation timestamp */
    knishio_json_t *metadata;           /**< Identity metadata */
    double balance;                     /**< Identity token balance */
    bool has_balance;                   /**< Whether balance is set */
} knishio_continu_id_info_t;

/**
 * @brief ContinuID response structure
 */
struct knishio_response_continu_id {
    knishio_response_t base;            /**< Base response */
    knishio_continu_id_info_t *identities; /**< Array of identities */
    size_t identity_count;              /**< Number of identities */
    bool identities_cached;             /**< Whether identities are cached */
    char *primary_bundle;               /**< Primary identity bundle */
    char *requested_slug;               /**< Requested token slug */
};

/**
 * @brief Create ContinuID response
 * @param query Original query
 * @param json JSON response data
 * @return ContinuID response or NULL on error
 */
knishio_response_continu_id_t* knishio_response_continu_id_create(knishio_query_t *query,
                                                                  knishio_json_t *json);

/**
 * @brief Free ContinuID response
 * @param response ContinuID response to free
 */
void knishio_response_continu_id_free(knishio_response_continu_id_t *response);

/**
 * @brief Get identities array
 * @param response ContinuID response
 * @param count Output identity count
 * @return Array of identities or NULL
 */
knishio_continu_id_info_t* knishio_response_continu_id_get_identities(knishio_response_continu_id_t *response,
                                                                      size_t *count);

/**
 * @brief Get identity at specific index
 * @param response ContinuID response
 * @param index Identity index
 * @return Identity info or NULL if index out of bounds
 */
knishio_continu_id_info_t* knishio_response_continu_id_get_identity(knishio_response_continu_id_t *response,
                                                                    size_t index);

/**
 * @brief Get number of identities
 * @param response ContinuID response
 * @return Number of identities
 */
size_t knishio_response_continu_id_count(knishio_response_continu_id_t *response);

/**
 * @brief Get primary identity
 * @param response ContinuID response
 * @return Primary identity or NULL if not found
 */
knishio_continu_id_info_t* knishio_response_continu_id_get_primary(knishio_response_continu_id_t *response);

/**
 * @brief Get primary bundle hash
 * @param response ContinuID response
 * @return Primary bundle hash or NULL if not available
 */
const char* knishio_response_continu_id_get_primary_bundle(knishio_response_continu_id_t *response);

/**
 * @brief Get requested token slug
 * @param response ContinuID response
 * @return Token slug or NULL if not available
 */
const char* knishio_response_continu_id_get_token_slug(knishio_response_continu_id_t *response);

/**
 * @brief Check if response has identity data
 * @param response ContinuID response
 * @return True if has identity data, false otherwise
 */
bool knishio_response_continu_id_has_identities(knishio_response_continu_id_t *response);

/**
 * @brief Find identity by bundle hash
 * @param response ContinuID response
 * @param bundle_hash Bundle hash to search for
 * @return Identity info or NULL if not found
 */
knishio_continu_id_info_t* knishio_response_continu_id_find_by_bundle(knishio_response_continu_id_t *response,
                                                                      const char *bundle_hash);

/**
 * @brief Find identity by address
 * @param response ContinuID response
 * @param address Address to search for
 * @return Identity info or NULL if not found
 */
knishio_continu_id_info_t* knishio_response_continu_id_find_by_address(knishio_response_continu_id_t *response,
                                                                       const char *address);

/* Identity information accessors */

/**
 * @brief Get identity bundle hash
 * @param identity Identity info
 * @return Bundle hash or NULL
 */
const char* knishio_continu_id_info_get_bundle_hash(knishio_continu_id_info_t *identity);

/**
 * @brief Get identity position
 * @param identity Identity info
 * @return Position or NULL
 */
const char* knishio_continu_id_info_get_position(knishio_continu_id_info_t *identity);

/**
 * @brief Get identity address
 * @param identity Identity info
 * @return Address or NULL
 */
const char* knishio_continu_id_info_get_address(knishio_continu_id_info_t *identity);

/**
 * @brief Get identity public key
 * @param identity Identity info
 * @return Public key or NULL
 */
const char* knishio_continu_id_info_get_pubkey(knishio_continu_id_info_t *identity);

/**
 * @brief Get identity creation timestamp
 * @param identity Identity info
 * @return Creation timestamp or NULL
 */
const char* knishio_continu_id_info_get_created_at(knishio_continu_id_info_t *identity);

/**
 * @brief Get identity metadata
 * @param identity Identity info
 * @return Metadata JSON or NULL
 */
knishio_json_t* knishio_continu_id_info_get_metadata(knishio_continu_id_info_t *identity);

/**
 * @brief Get identity balance
 * @param identity Identity info
 * @param balance Output balance
 * @return True if balance is available, false otherwise
 */
bool knishio_continu_id_info_get_balance(knishio_continu_id_info_t *identity, double *balance);

/**
 * @brief Get metadata value by key
 * @param identity Identity info
 * @param key Metadata key
 * @return Metadata value as string or NULL if not found
 */
const char* knishio_continu_id_info_get_metadata_string(knishio_continu_id_info_t *identity,
                                                        const char *key);

/**
 * @brief Get metadata number value by key
 * @param identity Identity info
 * @param key Metadata key
 * @param value Output number value
 * @return True if key exists and is number, false otherwise
 */
bool knishio_continu_id_info_get_metadata_number(knishio_continu_id_info_t *identity,
                                                 const char *key,
                                                 double *value);

/* Iterator functions */

/**
 * @brief Identity iteration callback
 * @param index Identity index
 * @param identity Identity info
 * @param user_data User data
 * @return True to continue iteration, false to stop
 */
typedef bool (*knishio_continu_id_iterator_fn)(size_t index,
                                               knishio_continu_id_info_t *identity,
                                               void *user_data);

/**
 * @brief Iterate over identities
 * @param response ContinuID response
 * @param callback Iterator callback
 * @param user_data User data for callback
 * @return True if all iterations completed, false if stopped early
 */
bool knishio_response_continu_id_foreach(knishio_response_continu_id_t *response,
                                         knishio_continu_id_iterator_fn callback,
                                         void *user_data);

/* Memory management helpers */

/**
 * @brief Free ContinuID info
 * @param identity Identity info to free
 */
void knishio_continu_id_info_free(knishio_continu_id_info_t *identity);

/**
 * @brief Free array of ContinuID info
 * @param identities Array of identities
 * @param count Number of identities
 */
void knishio_continu_id_info_free_array(knishio_continu_id_info_t *identities, size_t count);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param continu_id_response ContinuID response
 * @return Base response
 */
knishio_response_t* knishio_response_continu_id_to_base(knishio_response_continu_id_t *continu_id_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return ContinuID response or NULL if not a ContinuID response
 */
knishio_response_continu_id_t* knishio_response_continu_id_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_CONTINU_ID_H */