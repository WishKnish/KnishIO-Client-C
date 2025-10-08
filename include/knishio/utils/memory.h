#ifndef KNISHIO_UTILS_MEMORY_H
#define KNISHIO_UTILS_MEMORY_H

/**
 * @file memory.h
 * @brief Memory management utilities for KnishIO SDK
 * 
 * Provides reference counting, memory pools, and safe memory operations
 * to manage the complex object lifecycle similar to JavaScript garbage collection.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_ref knishio_ref_t;
typedef struct knishio_pool knishio_pool_t;

/**
 * @brief Destructor function type for reference counted objects
 * @param data Pointer to data being destroyed
 */
typedef void (*knishio_destructor_t)(void *data);

/**
 * @brief Reference counted object
 */
struct knishio_ref {
    void *data;                     /**< Pointer to managed data */
    size_t ref_count;               /**< Reference count */
    knishio_destructor_t destructor; /**< Destructor function */
    bool is_valid;                  /**< Validity flag */
};

/**
 * @brief Memory pool for efficient allocation
 */
struct knishio_pool {
    uint8_t *memory;                /**< Pool memory block */
    size_t size;                    /**< Total pool size */
    size_t used;                    /**< Used memory */
    size_t alignment;               /**< Memory alignment */
    struct knishio_pool *next;      /**< Next pool in chain */
};

/* Reference counting functions */

/**
 * @brief Create a new reference counted object
 * @param data Pointer to data to manage
 * @param destructor Destructor function (can be NULL)
 * @return Reference object or NULL on failure
 */
knishio_ref_t* knishio_ref_create(void *data, knishio_destructor_t destructor);

/**
 * @brief Increase reference count
 * @param ref Reference object
 * @return New reference count or 0 on error
 */
size_t knishio_ref_retain(knishio_ref_t *ref);

/**
 * @brief Decrease reference count and possibly free object
 * @param ref Reference object
 * @return New reference count or 0 if freed
 */
size_t knishio_ref_release(knishio_ref_t *ref);

/**
 * @brief Get current reference count
 * @param ref Reference object
 * @return Reference count
 */
size_t knishio_ref_count(const knishio_ref_t *ref);

/**
 * @brief Get data from reference object
 * @param ref Reference object
 * @return Data pointer or NULL if invalid
 */
void* knishio_ref_data(const knishio_ref_t *ref);

/**
 * @brief Check if reference is valid
 * @param ref Reference object
 * @return True if valid, false otherwise
 */
bool knishio_ref_is_valid(const knishio_ref_t *ref);

/* Memory pool functions */

/**
 * @brief Create a new memory pool
 * @param initial_size Initial pool size in bytes
 * @param alignment Memory alignment (0 for default)
 * @return Memory pool or NULL on failure
 */
knishio_pool_t* knishio_pool_create(size_t initial_size, size_t alignment);

/**
 * @brief Allocate memory from pool
 * @param pool Memory pool
 * @param size Size to allocate
 * @return Allocated memory or NULL on failure
 */
void* knishio_pool_alloc(knishio_pool_t *pool, size_t size);

/**
 * @brief Allocate zeroed memory from pool
 * @param pool Memory pool
 * @param size Size to allocate
 * @return Allocated zeroed memory or NULL on failure
 */
void* knishio_pool_calloc(knishio_pool_t *pool, size_t size);

/**
 * @brief Reset pool (mark all memory as available)
 * @param pool Memory pool
 */
void knishio_pool_reset(knishio_pool_t *pool);

/**
 * @brief Get pool statistics
 * @param pool Memory pool
 * @param total_size Total pool size (output)
 * @param used_size Used memory (output)
 * @param free_size Free memory (output)
 */
void knishio_pool_stats(const knishio_pool_t *pool, 
                       size_t *total_size, 
                       size_t *used_size, 
                       size_t *free_size);

/**
 * @brief Destroy memory pool
 * @param pool Memory pool
 */
void knishio_pool_destroy(knishio_pool_t *pool);

/* Safe memory operations */

/**
 * @brief Safe memory allocation with zero initialization
 * @param size Size to allocate
 * @return Allocated memory or NULL on failure
 */
void* knishio_malloc(size_t size);

/**
 * @brief Safe memory allocation for arrays with zero initialization
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Allocated memory or NULL on failure
 */
void* knishio_calloc(size_t nmemb, size_t size);

/**
 * @brief Safe memory reallocation
 * @param ptr Existing pointer (can be NULL)
 * @param size New size
 * @return Reallocated memory or NULL on failure
 */
void* knishio_realloc(void *ptr, size_t size);

/**
 * @brief Safe memory deallocation
 * @param ptr Pointer to free (can be NULL)
 */
void knishio_free(void *ptr);

/**
 * @brief Safe string duplication
 * @param str String to duplicate
 * @return Duplicated string or NULL on failure
 */
char* knishio_strdup(const char *str);

/**
 * @brief Safe string duplication with length limit
 * @param str String to duplicate
 * @param max_len Maximum length to copy
 * @return Duplicated string or NULL on failure
 */
char* knishio_strndup(const char *str, size_t max_len);

/**
 * @brief Secure memory zero (prevents optimization)
 * @param ptr Memory to zero
 * @param size Size to zero
 */
void knishio_secure_zero(void *ptr, size_t size);

/**
 * @brief Secure memory deallocation with zeroing
 * @param ptr Memory to securely free
 * @param size Size to zero before freeing
 */
void knishio_secure_free(void *ptr, size_t size);

/**
 * @brief Generate cryptographically secure random bytes
 * @param buffer Buffer to fill with random data
 * @param size Number of bytes to generate
 * @return True on success, false on failure
 */
bool knishio_random_bytes(uint8_t *buffer, size_t size);

/* Memory debugging (debug builds only) */
#ifdef KNISHIO_DEBUG
/**
 * @brief Get current memory usage statistics
 * @param allocations Number of active allocations (output)
 * @param total_bytes Total allocated bytes (output)
 */
void knishio_memory_stats(size_t *allocations, size_t *total_bytes);

/**
 * @brief Check for memory leaks
 * @return Number of leaked allocations
 */
size_t knishio_memory_check_leaks(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_UTILS_MEMORY_H */
