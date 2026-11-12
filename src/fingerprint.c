/**
 * @file fingerprint.c
 * @brief Guest fingerprint for KnishIO C SDK
 *
 * The JS SDK seeds a guest wallet with a browser fingerprint (thumbmarkjs). A C process has no
 * browser to fingerprint, so its fingerprint is SHAKE256-256 of 32 bytes from the CSPRNG,
 * drawn once per process: one guest identity per process, never shared across processes.
 */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "knishio/fingerprint.h"
#include "knishio/crypto/shake256.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/security.h"

static pthread_once_t g_fingerprint_once = PTHREAD_ONCE_INIT;
static char g_fingerprint[65];
static bool g_fingerprint_ready = false;

static void make_fingerprint(void) {
    uint8_t seed[32];
    uint8_t digest[32];
    if (knishio_secure_random(seed, sizeof seed)
        && knishio_shake256_hash_raw(seed, sizeof seed, digest, sizeof digest)) {
        for (size_t i = 0; i < sizeof digest; i++) {
            snprintf(g_fingerprint + i * 2, 3, "%02x", digest[i]);
        }
        g_fingerprint_ready = true;
    }
    knishio_secure_zero(seed, sizeof seed);
    knishio_secure_zero(digest, sizeof digest);
}

knishio_error_t knishio_get_fingerprint(char** fingerprint) {
    if (!fingerprint) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *fingerprint = NULL;
    if (pthread_once(&g_fingerprint_once, make_fingerprint) != 0 || !g_fingerprint_ready) {
        return KNISHIO_ERROR_CRYPTO;
    }
    *fingerprint = malloc(sizeof g_fingerprint);
    if (!*fingerprint) {
        return KNISHIO_ERROR_MEMORY;
    }
    memcpy(*fingerprint, g_fingerprint, sizeof g_fingerprint);
    return KNISHIO_SUCCESS;
}
