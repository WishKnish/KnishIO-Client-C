#ifndef KNISHIO_STORAGE_INTERNAL_H
#define KNISHIO_STORAGE_INTERNAL_H

/**
 * @file storage_internal.h
 * @brief Internal utility declarations for secret storage modules.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build a storage key by concatenating prefix and bundle_hash safely.
 *
 * @param prefix Prefix string (e.g. KNISHIO_SECRET_STORAGE_PREFIX or KNISHIO_RECOVERY_KEY_PREFIX)
 * @param bundle_hash Bundle hash string
 * @return Dynamically allocated null-terminated string, or NULL on memory allocation failure.
 */
char *knishio_storage_build_key(const char *prefix, const char *bundle_hash);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_STORAGE_INTERNAL_H */
