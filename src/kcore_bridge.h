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

/* WOTS+ key / signature: 16 chunks of 128 hex characters. */
#define KNISHIO_KCORE_WOTS_CHUNKS ((size_t)16)
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
 * Advances the 16 WOTS+ chains of a 2048-hex key or signature: chunk i is hashed counts[i]
 * times. Returns false unless src is exactly 2048 lowercase hex characters, every count is
 * 0..64 and kcore succeeds; out is then zeroed when it was written (it may hold a copy of a
 * private key). On true, out holds the 2048 result characters plus a NUL.
 */
static inline bool knishio_kcore_wots_chains(const char *src,
                                             const int counts[KNISHIO_KCORE_WOTS_CHUNKS],
                                             char out[KNISHIO_KCORE_WOTS_HEX_LEN + 1]) {
    if (src == NULL || strlen(src) != KNISHIO_KCORE_WOTS_HEX_LEN ||
        !knishio_kcore_is_lower_hex(src, KNISHIO_KCORE_WOTS_HEX_LEN)) {
        return false;
    }
    for (size_t i = 0; i < KNISHIO_KCORE_WOTS_CHUNKS; i++) {
        if (counts[i] < 0 || counts[i] > 64) {
            return false;
        }
    }
    memcpy(out, src, KNISHIO_KCORE_WOTS_HEX_LEN);
    out[KNISHIO_KCORE_WOTS_HEX_LEN] = '\0';
    if (kcore_chains_hex(out, counts, KNISHIO_KCORE_WOTS_CHUNKS, 4) != 0) {
        volatile char *p = out;
        for (size_t i = 0; i <= KNISHIO_KCORE_WOTS_HEX_LEN; i++) {
            p[i] = 0;
        }
        return false;
    }
    return true;
}

#endif /* KNISHIO_HAVE_KCORE */

#endif /* KNISHIO_KCORE_BRIDGE_H */
