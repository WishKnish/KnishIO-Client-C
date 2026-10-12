/**
 * @file kcore_bridge.h
 * @brief Internal helpers for the optional kcore backend (KNISHIO_USE_KCORE=ON).
 *
 * kcore (KnishIO Crypto Core) computes the WOTS+ chains, the WOTS+ address and ML-KEM. The SDK
 * only hands it input the reference implementation would hash identically: lowercase hex of the
 * exact length, counts 0..64. Anything else (a peer's uppercase or non-hex OTS, a short key)
 * stays on the SDK's own SHAKE256 loop, so a verifier's verdict never depends on whether kcore
 * is linked. Not installed; included only by the library sources.
 */
#ifndef KNISHIO_KCORE_BRIDGE_H
#define KNISHIO_KCORE_BRIDGE_H

#ifdef KNISHIO_HAVE_KCORE

#include <kcore.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* WOTS+ key / signature: chunks of 128 hex characters, 16 (OTS v1) or 64 (OTS v2). kcore's
 * address entry point takes a 2048-hex (v1) key only. */
#define KNISHIO_KCORE_WOTS_CHUNK_HEX ((size_t)128)
#define KNISHIO_KCORE_WOTS_MAX_CHUNKS ((size_t)64)
#define KNISHIO_KCORE_WOTS_HEX_LEN ((size_t)2048)

static inline bool knishio_kcore_is_lower_hex(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

/**
 * Advances the n WOTS+ chains (1..64) of a key or signature of n*128 hex characters: chunk i
 * is hashed counts[i] times. Returns false unless src is exactly n*128 lowercase hex
 * characters, every count is 0..64 and kcore succeeds; out is then zeroed when it was written
 * (it may hold a copy of a private key). out must hold n*128+1 characters; on true it holds
 * the n*128 result characters plus a NUL.
 */
static inline bool knishio_kcore_wots_chains(const char *src, size_t n, const int *counts,
                                             char *out) {
    if (src == NULL || counts == NULL || out == NULL || n == 0 ||
        n > KNISHIO_KCORE_WOTS_MAX_CHUNKS) {
        return false;
    }
    const size_t hex_len = n * KNISHIO_KCORE_WOTS_CHUNK_HEX;
    if (strlen(src) != hex_len || !knishio_kcore_is_lower_hex(src, hex_len)) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        if (counts[i] < 0 || counts[i] > 64) {
            return false;
        }
    }
    memcpy(out, src, hex_len);
    out[hex_len] = '\0';
    if (kcore_chains_hex(out, counts, n, 4) != 0) {
        volatile char *p = out;
        for (size_t i = 0; i <= hex_len; i++) {
            p[i] = 0;
        }
        return false;
    }
    return true;
}

#endif /* KNISHIO_HAVE_KCORE */

#endif /* KNISHIO_KCORE_BRIDGE_H */
