/**
 * @file operations/operations_internal.h
 * @brief Helpers shared by the operation sources (not installed, not public API)
 */
#ifndef KNISHIO_OPERATIONS_INTERNAL_H
#define KNISHIO_OPERATIONS_INTERNAL_H

#include "knishio/error.h"

typedef struct knishio_client knishio_client_t;
typedef struct knishio_wallet knishio_wallet_t;

/**
 * @brief Resolve the wallet an operation spends from
 *
 * USER ContinuID (client secret + bundle) -> Balance(bundle, token, type) -> a source wallet
 * re-derived at that position (so it can sign) carrying the balance, batchId and token units.
 * type NULL selects the regular wallet, "buffer" the buffer wallet (validator 0.6.1). On
 * success the caller owns *out_user and *out_source; on failure both are NULL. No wallet of
 * that type -> KNISHIO_ERROR_BALANCE_INSUFFICIENT.
 */
knishio_error_t knishio_resolve_funded_source(
    knishio_client_t* client,
    const char* token,
    const char* type,
    knishio_wallet_t** out_user,
    knishio_wallet_t** out_source
);

#endif /* KNISHIO_OPERATIONS_INTERNAL_H */
