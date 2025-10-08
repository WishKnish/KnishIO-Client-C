#ifndef KNISHIO_RESPONSE_ATOM_H
#define KNISHIO_RESPONSE_ATOM_H

/**
 * @file response_atom.h
 * @brief Response for atom queries
 * 
 * Handles responses from atom queries, providing access to meta instances,
 * pagination information, and instance counts compatible with the JavaScript
 * SDK's ResponseAtom.
 */

#include "response.h"
#include "../atom.h"
#include "../meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_atom knishio_response_atom_t;

/**
 * @brief Pagination information
 */
typedef struct {
    int current_page;                   /**< Current page number */
    int last_page;                      /**< Last page number */
    int per_page;                       /**< Items per page */
    int total;                          /**< Total items */
    bool has_more_pages;                /**< Whether more pages exist */
} knishio_paginator_info_t;

/**
 * @brief Meta instance structure
 */
typedef struct {
    char *id;                           /**< Instance ID */
    char *meta_type;                    /**< Meta type */
    char *meta_id;                      /**< Meta ID */
    char *metas_json;                   /**< Serialized metas JSON */
    knishio_json_t *metas;              /**< Parsed metas object */
    char *created_at;                   /**< Creation timestamp */
    char *updated_at;                   /**< Update timestamp */
} knishio_meta_instance_t;

/**
 * @brief Instance count structure
 */
typedef struct {
    char *meta_type;                    /**< Meta type */
    int count;                          /**< Instance count */
} knishio_instance_count_t;

/**
 * @brief Atom response structure
 */
struct knishio_response_atom {
    knishio_response_t base;                /**< Base response */
    knishio_meta_instance_t *instances;     /**< Array of meta instances */
    size_t instance_count;                  /**< Number of instances */
    knishio_instance_count_t *counts;       /**< Instance counts by type */
    size_t count_types;                     /**< Number of count types */
    knishio_paginator_info_t paginator;     /**< Pagination info */
    bool data_cached;                       /**< Whether data is cached */
};

/**
 * @brief Create atom response
 * @param query Original query
 * @param json JSON response data
 * @return Atom response or NULL on error
 */
knishio_response_atom_t* knishio_response_atom_create(knishio_query_t *query,
                                                      knishio_json_t *json);

/**
 * @brief Free atom response
 * @param response Atom response to free
 */
void knishio_response_atom_free(knishio_response_atom_t *response);

/**
 * @brief Get meta instances
 * @param response Atom response
 * @param count Output instance count
 * @return Array of meta instances or NULL
 */
knishio_meta_instance_t* knishio_response_atom_get_instances(knishio_response_atom_t *response,
                                                             size_t *count);

/**
 * @brief Get meta instance by index
 * @param response Atom response
 * @param index Instance index
 * @return Meta instance or NULL if index out of bounds
 */
knishio_meta_instance_t* knishio_response_atom_get_instance(knishio_response_atom_t *response,
                                                            size_t index);

/**
 * @brief Get instance counts
 * @param response Atom response
 * @param count Output count types
 * @return Array of instance counts or NULL
 */
knishio_instance_count_t* knishio_response_atom_get_instance_counts(knishio_response_atom_t *response,
                                                                    size_t *count);

/**
 * @brief Get instance count for specific meta type
 * @param response Atom response
 * @param meta_type Meta type to query
 * @return Instance count or -1 if not found
 */
int knishio_response_atom_get_count_for_type(knishio_response_atom_t *response,
                                             const char *meta_type);

/**
 * @brief Get pagination information
 * @param response Atom response
 * @return Pagination info pointer
 */
knishio_paginator_info_t* knishio_response_atom_get_paginator_info(knishio_response_atom_t *response);

/**
 * @brief Get total number of instances
 * @param response Atom response
 * @return Total instance count
 */
size_t knishio_response_atom_get_total_instances(knishio_response_atom_t *response);

/**
 * @brief Check if response has more pages
 * @param response Atom response
 * @return True if more pages available, false otherwise
 */
bool knishio_response_atom_has_more_pages(knishio_response_atom_t *response);

/**
 * @brief Get current page number
 * @param response Atom response
 * @return Current page number
 */
int knishio_response_atom_get_current_page(knishio_response_atom_t *response);

/**
 * @brief Get last page number
 * @param response Atom response
 * @return Last page number
 */
int knishio_response_atom_get_last_page(knishio_response_atom_t *response);

/* Meta extraction functions */

/**
 * @brief Extract all metas from instances
 * @param response Atom response
 * @param metas Output array of meta objects (caller must free)
 * @param count Output meta count
 * @return True on success, false on error
 */
bool knishio_response_atom_extract_metas(knishio_response_atom_t *response,
                                         knishio_json_t ***metas,
                                         size_t *count);

/**
 * @brief Get metas from specific instance
 * @param instance Meta instance
 * @return Parsed metas JSON or NULL
 */
knishio_json_t* knishio_response_atom_get_instance_metas(knishio_meta_instance_t *instance);

/**
 * @brief Extract meta value from instance
 * @param instance Meta instance
 * @param key Meta key
 * @return Meta value as string or NULL if not found
 */
const char* knishio_response_atom_get_instance_meta_value(knishio_meta_instance_t *instance,
                                                          const char *key);

/* Iterator functions */

/**
 * @brief Meta instance iteration callback
 * @param index Instance index
 * @param instance Meta instance
 * @param user_data User data
 * @return True to continue iteration, false to stop
 */
typedef bool (*knishio_meta_instance_iterator_fn)(size_t index,
                                                  knishio_meta_instance_t *instance,
                                                  void *user_data);

/**
 * @brief Iterate over meta instances
 * @param response Atom response
 * @param callback Iterator callback
 * @param user_data User data for callback
 * @return True if all iterations completed, false if stopped early
 */
bool knishio_response_atom_foreach_instance(knishio_response_atom_t *response,
                                            knishio_meta_instance_iterator_fn callback,
                                            void *user_data);

/* Filter functions */

/**
 * @brief Filter instances by meta type
 * @param response Atom response
 * @param meta_type Meta type to filter by
 * @param filtered_instances Output filtered instances array (caller must free)
 * @param count Output filtered count
 * @return True on success, false on error
 */
bool knishio_response_atom_filter_by_meta_type(knishio_response_atom_t *response,
                                               const char *meta_type,
                                               knishio_meta_instance_t **filtered_instances,
                                               size_t *count);

/**
 * @brief Find instance by ID
 * @param response Atom response
 * @param instance_id Instance ID to search for
 * @return Meta instance or NULL if not found
 */
knishio_meta_instance_t* knishio_response_atom_find_by_id(knishio_response_atom_t *response,
                                                          const char *instance_id);

/* Utility functions */

/**
 * @brief Check if response has instance data
 * @param response Atom response
 * @return True if has instance data, false otherwise
 */
bool knishio_response_atom_has_instances(knishio_response_atom_t *response);

/**
 * @brief Check if response has count data
 * @param response Atom response
 * @return True if has count data, false otherwise
 */
bool knishio_response_atom_has_counts(knishio_response_atom_t *response);

/**
 * @brief Check if response has pagination info
 * @param response Atom response
 * @return True if has pagination info, false otherwise
 */
bool knishio_response_atom_has_pagination(knishio_response_atom_t *response);

/* Memory management helpers */

/**
 * @brief Free meta instance
 * @param instance Meta instance to free
 */
void knishio_meta_instance_free(knishio_meta_instance_t *instance);

/**
 * @brief Free array of meta instances
 * @param instances Array of instances
 * @param count Number of instances
 */
void knishio_meta_instances_free(knishio_meta_instance_t *instances, size_t count);

/**
 * @brief Free instance count
 * @param count Instance count to free
 */
void knishio_instance_count_free(knishio_instance_count_t *count);

/**
 * @brief Free array of instance counts
 * @param counts Array of counts
 * @param count_types Number of count types
 */
void knishio_instance_counts_free(knishio_instance_count_t *counts, size_t count_types);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param atom_response Atom response
 * @return Base response
 */
knishio_response_t* knishio_response_atom_to_base(knishio_response_atom_t *atom_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Atom response or NULL if not an atom response
 */
knishio_response_atom_t* knishio_response_atom_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_ATOM_H */