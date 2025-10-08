#ifndef KNISHIO_RESPONSE_META_TYPE_H
#define KNISHIO_RESPONSE_META_TYPE_H

/**
 * @file response_meta_type.h
 * @brief Response for meta type queries
 * 
 * Handles responses from meta type queries, extracting metadata information
 * compatible with the JavaScript SDK's ResponseMetaType.
 */

#include "response.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_meta_type knishio_response_meta_type_t;

/**
 * @brief Meta type information structure
 */
typedef struct {
    char *meta_type;                    /**< Meta type identifier */
    char *meta_id;                      /**< Meta instance ID */
    char *meta_key;                     /**< Meta key */
    char *meta_value;                   /**< Meta value */
    char *meta_mime;                    /**< Meta MIME type */
    char *bundle_hash;                  /**< Associated bundle hash */
    char *created_at;                   /**< Creation timestamp */
    bool is_policy;                     /**< Whether this is a policy meta */
    bool is_public;                     /**< Whether meta is public */
} knishio_meta_info_t;

/**
 * @brief Meta type response structure
 */
struct knishio_response_meta_type {
    knishio_response_t base;            /**< Base response */
    knishio_meta_info_t **meta_items;   /**< Array of meta items */
    size_t meta_count;                  /**< Number of meta items */
    size_t meta_capacity;               /**< Allocated meta capacity */
    char *queried_meta_type;            /**< The queried meta type */
    char *queried_meta_id;              /**< The queried meta ID */
};

/**
 * @brief Create meta type response
 * @param query Original query
 * @param json JSON response data
 * @return Meta type response or NULL on error
 */
knishio_response_meta_type_t* knishio_response_meta_type_create(knishio_query_t *query,
                                                                knishio_json_t *json);

/**
 * @brief Free meta type response
 * @param response Meta type response to free
 */
void knishio_response_meta_type_free(knishio_response_meta_type_t *response);

/**
 * @brief Get meta items array
 * @param response Meta type response
 * @param meta_count Output meta count (can be NULL)
 * @return Array of meta items or NULL if no meta data
 */
knishio_meta_info_t** knishio_response_meta_type_get_meta_items(knishio_response_meta_type_t *response,
                                                                size_t *meta_count);

/**
 * @brief Get meta item by index
 * @param response Meta type response
 * @param index Meta item index
 * @return Meta item or NULL if index invalid
 */
knishio_meta_info_t* knishio_response_meta_type_get_meta_at(knishio_response_meta_type_t *response,
                                                            size_t index);

/**
 * @brief Get first meta item with matching key
 * @param response Meta type response
 * @param meta_key Meta key to search for
 * @return First meta item with matching key or NULL if not found
 */
knishio_meta_info_t* knishio_response_meta_type_get_meta_by_key(knishio_response_meta_type_t *response,
                                                                const char *meta_key);

/**
 * @brief Get meta count
 * @param response Meta type response
 * @return Number of meta items
 */
size_t knishio_response_meta_type_get_count(knishio_response_meta_type_t *response);

/**
 * @brief Get queried meta type
 * @param response Meta type response
 * @return Queried meta type or NULL if not available
 */
const char* knishio_response_meta_type_get_queried_type(knishio_response_meta_type_t *response);

/**
 * @brief Get queried meta ID
 * @param response Meta type response
 * @return Queried meta ID or NULL if not available
 */
const char* knishio_response_meta_type_get_queried_id(knishio_response_meta_type_t *response);

/**
 * @brief Check if response has valid meta data
 * @param response Meta type response
 * @return True if has valid meta data, false otherwise
 */
bool knishio_response_meta_type_has_data(knishio_response_meta_type_t *response);

/**
 * @brief Filter meta items by policy status
 * @param response Meta type response
 * @param is_policy Whether to get policy or non-policy items
 * @param filtered_items Output array of filtered items (caller must free)
 * @param filtered_count Output count of filtered items
 * @return True if filtering successful, false otherwise
 */
bool knishio_response_meta_type_filter_by_policy(knishio_response_meta_type_t *response,
                                                  bool is_policy,
                                                  knishio_meta_info_t ***filtered_items,
                                                  size_t *filtered_count);

/**
 * @brief Get meta value by key
 * @param response Meta type response
 * @param meta_key Meta key to search for
 * @return Meta value or NULL if key not found
 */
const char* knishio_response_meta_type_get_value_by_key(knishio_response_meta_type_t *response,
                                                        const char *meta_key);

/**
 * @brief Check if meta key exists
 * @param response Meta type response
 * @param meta_key Meta key to check
 * @return True if key exists, false otherwise
 */
bool knishio_response_meta_type_has_key(knishio_response_meta_type_t *response,
                                        const char *meta_key);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param meta_response Meta type response
 * @return Base response
 */
knishio_response_t* knishio_response_meta_type_to_base(knishio_response_meta_type_t *meta_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Meta type response or NULL if not a meta type response
 */
knishio_response_meta_type_t* knishio_response_meta_type_from_base(knishio_response_t *base_response);

/* Utility functions */

/**
 * @brief Free meta info structure
 * @param meta_info Meta info to free
 */
void knishio_meta_info_free(knishio_meta_info_t *meta_info);

/**
 * @brief Create meta info structure
 * @param meta_type Meta type
 * @param meta_id Meta ID
 * @param meta_key Meta key
 * @param meta_value Meta value
 * @return Created meta info or NULL on error
 */
knishio_meta_info_t* knishio_meta_info_create(const char *meta_type,
                                               const char *meta_id,
                                               const char *meta_key,
                                               const char *meta_value);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_META_TYPE_H */