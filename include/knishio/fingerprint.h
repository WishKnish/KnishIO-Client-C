#ifndef KNISHIO_FINGERPRINT_H
#define KNISHIO_FINGERPRINT_H

/**
 * @file fingerprint.h
 * @brief Guest fingerprint for KnishIO C SDK
 *
 * The JS SDK derives the guest wallet secret from a browser fingerprint (thumbmarkjs
 * getFingerprint). A C process has no browser, so its fingerprint is random per process.
 */

#include "knishio/error/context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The process's guest fingerprint (JS getFingerprint)
 *
 * SHAKE256-256 (64 lowercase hex characters) of 32 bytes from the CSPRNG, drawn on the first
 * call and returned unchanged for the life of the process; another process gets another one.
 * Guest authorization seeds its AUTH wallet with generateSecret(fingerprint).
 *
 * @param fingerprint Output string (malloc'd; the caller frees it with free)
 * @return KNISHIO_SUCCESS, KNISHIO_ERROR_CRYPTO when the CSPRNG fails, or KNISHIO_ERROR_MEMORY
 */
knishio_error_t knishio_get_fingerprint(char** fingerprint);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_FINGERPRINT_H */
