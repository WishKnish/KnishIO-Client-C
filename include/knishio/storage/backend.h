#ifndef KNISHIO_STORAGE_BACKEND_H
#define KNISHIO_STORAGE_BACKEND_H

#include "knishio/storage/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an in-memory thread-safe key-value storage backend
 * @param backend_out Output pointer to created backend
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_memory_storage_backend_create(knishio_storage_backend_t **backend_out);

/**
 * @brief Create an atomic file-backed key-value storage backend
 *
 * Persists key-value pairs atomically to disk via tempfile + rename with 0600 permissions.
 *
 * @param file_path Path to the JSON storage file
 * @param backend_out Output pointer to created backend
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_file_storage_backend_create(const char *file_path, knishio_storage_backend_t **backend_out);

/**
 * @brief Free a storage backend instance and its resources
 * @param backend Storage backend to free
 */
void knishio_storage_backend_free(knishio_storage_backend_t *backend);

/**
 * @brief Delete a secret and its associated recovery record from backend
 *
 * Removes both "knishio:secret:<bundle_hash>" and "knishio:recovery:<bundle_hash>".
 *
 * @param backend Storage backend instance
 * @param bundle_hash Bundle hash
 * @param existed_out Optional pointer set to true if either record existed
 * @return KNISHIO_SUCCESS or error code
 */
knishio_error_t knishio_storage_backend_delete_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    bool *existed_out
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_STORAGE_BACKEND_H */
