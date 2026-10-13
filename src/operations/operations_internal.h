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

/**
 * @brief OTS version of one of this client's own wallets returned by the ledger
 *
 * Without a position or an address: the client's version. Otherwise the version at which
 * `secret` derives `address` at (token, position), trying the client's version first. When
 * neither version derives it, "KnishIO: wallet <address> is not derived from this secret at
 * position <position> under OTS version 1 or 2." goes to the global error handler and the
 * result is KNISHIO_ERROR_WALLET_MISMATCH.
 */
knishio_error_t knishio_client_bind_own_wallet_ots_version(
    const knishio_client_t* client,
    const char* secret,
    const char* token,
    const char* position,
    const char* address,
    int* ots_version
);

/**
 * @brief Re-derive one of this client's own ledger wallets (a signing key at `position`) at the
 *        OTS version knishio_client_bind_own_wallet_ots_version() binds; errors as there
 */
knishio_error_t knishio_client_own_wallet_at(
    const knishio_client_t* client,
    const char* secret,
    const char* token,
    const char* position,
    const char* address,
    knishio_wallet_t** wallet
);

/**
 * @brief OTS version of the wallets a molecule creates for its signer (the USER ContinuID
 *        remainder, token remainders, the deposit buffer): the higher of the source wallet's
 *        version (0 counts as 1) and the client's configured version
 */
int knishio_client_successor_ots_version(const knishio_client_t* client, int source_ots_version);

/**
 * @brief The client's canonical (64-hex) bundle hash, from its secret alone (no ledger query);
 *        the caller frees *bundle
 */
knishio_error_t knishio_client_own_bundle(knishio_client_t* client, char** bundle);

#endif /* KNISHIO_OPERATIONS_INTERNAL_H */
