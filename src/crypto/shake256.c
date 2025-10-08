/*
 * tiny_sha3 - A small SHA-3 (Keccak) and SHAKE implementation in C
 * Based on the public domain implementation by Markku-Juhani O. Saarinen
 * <mjos@iki.fi>. Revised 07-Aug-15 to match with official release of FIPS PUB 202.
 * 
 * This implementation replaces the broken SHAKE256 in KnishIO Client C SDK.
 */

#include "knishio/crypto/shake256.h"
#include "knishio/utils/memory.h"
#include <string.h>
#include <stdio.h>

#define KECCAKF_ROUNDS 24

#ifndef ROTLEFT
#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (64 - (b))))
#endif

// Constants for Keccak-f[1600]
static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

// Rotation offsets for Keccak-f[1600]
static const int keccakf_rotc[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
};

// Permutation on the string
static const int keccakf_piln[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
};

// Keccak-f[1600] permutation
static void sha3_keccakf(uint64_t st[25])
{
    int i, j, r;
    uint64_t t, bc[5];

    for (r = 0; r < KECCAKF_ROUNDS; r++) {

        // Theta
        for (i = 0; i < 5; i++)
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];

        for (i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ ROTLEFT(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5)
                st[j + i] ^= t;
        }

        // Rho Pi
        t = st[1];
        for (i = 0; i < 24; i++) {
            j = keccakf_piln[i];
            bc[0] = st[j];
            st[j] = ROTLEFT(t, keccakf_rotc[i]);
            t = bc[0];
        }

        //  Chi
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++)
                bc[i] = st[j + i];
            for (i = 0; i < 5; i++)
                st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }

        //  Iota
        st[0] ^= keccakf_rndc[r];
    }
}

// Initialize SHA-3 context
static void sha3_init(knishio_shake256_ctx_t *c, int mdlen)
{
    memset(c, 0, sizeof(*c));
    c->rate = 200 - 2 * mdlen;
    c->initialized = true;
    c->finalized = false;
}

// Update state with input
static int sha3_update(knishio_shake256_ctx_t *c, const void *data, size_t len)
{
    size_t i;
    int j;

    j = c->buffer_len;
    for (i = 0; i < len; i++) {
        c->buffer[j++] = ((const uint8_t *)data)[i];
        if (j >= c->rate) {
            for (int k = 0; k < c->rate; k++) {
                c->state[k / 8] ^= ((uint64_t) c->buffer[k]) << (8 * (k % 8));
            }
            sha3_keccakf(c->state);
            j = 0;
        }
    }
    c->buffer_len = j;

    return 1;
}

// Finalize the hash (apply padding)
static void sha3_final(knishio_shake256_ctx_t *c)
{
    int i;

    // Apply SHAKE256 padding byte (0x1F)
    c->buffer[c->buffer_len++] = 0x1f;
    
    // Check if we need to process this block and start a new one
    if (c->buffer_len == c->rate) {
        // Buffer is now full after adding padding byte
        // Set the final bit and process this block
        c->buffer[c->rate - 1] |= 0x80;
        
        // XOR buffer into state and permute
        for (i = 0; i < c->rate; i++) {
            c->state[i / 8] ^= ((uint64_t) c->buffer[i]) << (8 * (i % 8));
        }
        sha3_keccakf(c->state);
    } else {
        // Buffer not full - pad with zeros and set final bit
        // Fill the rest with zeros (from buffer_len to rate-1)
        while (c->buffer_len < c->rate) {
            c->buffer[c->buffer_len++] = 0x00;
        }
        
        // Set the final bit
        c->buffer[c->rate - 1] |= 0x80;
        
        // XOR buffer into state and permute
        for (i = 0; i < c->rate; i++) {
            c->state[i / 8] ^= ((uint64_t) c->buffer[i]) << (8 * (i % 8));
        }
        sha3_keccakf(c->state);
    }
    
    c->finalized = true;
    c->squeeze_offset = 0;
}

// Squeeze output from state
static void shake_out(knishio_shake256_ctx_t *c, void *out, size_t len)
{
    size_t i;
    int j;

    j = c->squeeze_offset;
    for (i = 0; i < len; i++) {
        if (j >= c->rate) {
            sha3_keccakf(c->state);
            j = 0;
        }
        ((uint8_t *) out)[i] = (c->state[j / 8] >> (8 * (j % 8))) & 0xFF;
        j++;
    }
    c->squeeze_offset = j;
}

// Implementation of the public API

bool knishio_shake256_init(knishio_shake256_ctx_t *ctx)
{
    if (!ctx) return false;
    
    sha3_init(ctx, 32); // SHAKE256 has 32-byte (256-bit) security
    ctx->rate = 136;    // SHAKE256 rate: 1088 bits = 136 bytes
    return true;
}

bool knishio_shake256_update(knishio_shake256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !ctx->initialized || ctx->finalized) return false;
    if (!data && len > 0) return false;
    
    return sha3_update(ctx, data, len) == 1;
}

bool knishio_shake256_final(knishio_shake256_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized || ctx->finalized) return false;
    
    sha3_final(ctx);
    return true;
}

bool knishio_shake256_squeeze(knishio_shake256_ctx_t *ctx, uint8_t *output, size_t output_len)
{
    if (!ctx || !ctx->initialized || !ctx->finalized || !output) return false;
    
    shake_out(ctx, output, output_len);
    return true;
}

bool knishio_shake256_hash(const char *input, size_t output_bits, char **hex_output)
{
    if (!input || !hex_output) return false;
    return knishio_shake256_hash_binary((const uint8_t*)input, strlen(input), 
                                       output_bits, hex_output);
}

bool knishio_shake256_hash_binary(const uint8_t *input, size_t input_len, 
                                 size_t output_bits, char **hex_output)
{
    if (!input || !hex_output || output_bits == 0) return false;
    
    knishio_shake256_ctx_t ctx;
    if (!knishio_shake256_init(&ctx)) return false;
    if (!knishio_shake256_update(&ctx, input, input_len)) return false;
    if (!knishio_shake256_final(&ctx)) return false;
    
    size_t output_bytes = (output_bits + 7) / 8;
    uint8_t *output = knishio_malloc(output_bytes);
    if (!output) return false;
    
    bool result = knishio_shake256_squeeze(&ctx, output, output_bytes);
    if (result) {
        result = knishio_binary_to_hex(output, output_bytes, hex_output);
    }
    
    knishio_free(output);
    knishio_shake256_cleanup(&ctx);
    return result;
}

bool knishio_shake256_hash_raw(const uint8_t *input, size_t input_len,
                              uint8_t *output, size_t output_len)
{
    if (!input || !output || output_len == 0) return false;
    
    knishio_shake256_ctx_t ctx;
    if (!knishio_shake256_init(&ctx)) return false;
    if (!knishio_shake256_update(&ctx, input, input_len)) return false;
    if (!knishio_shake256_final(&ctx)) return false;
    
    bool result = knishio_shake256_squeeze(&ctx, output, output_len);
    knishio_shake256_cleanup(&ctx);
    return result;
}

bool knishio_shake256_update_string(knishio_shake256_ctx_t *ctx, const char *str)
{
    if (!str) return true;
    return knishio_shake256_update(ctx, (const uint8_t*)str, strlen(str));
}

bool knishio_shake256_squeeze_hex(knishio_shake256_ctx_t *ctx, size_t output_bits, char **hex_output)
{
    if (!ctx || !hex_output || output_bits == 0) return false;
    
    size_t output_bytes = (output_bits + 7) / 8;
    uint8_t *output = knishio_malloc(output_bytes);
    if (!output) return false;
    
    bool result = knishio_shake256_squeeze(ctx, output, output_bytes);
    if (result) {
        result = knishio_binary_to_hex(output, output_bytes, hex_output);
    }
    
    knishio_free(output);
    return result;
}

void knishio_shake256_reset(knishio_shake256_ctx_t *ctx)
{
    if (ctx) knishio_shake256_init(ctx);
}

void knishio_shake256_cleanup(knishio_shake256_ctx_t *ctx)
{
    if (ctx) knishio_secure_zero(ctx, sizeof(*ctx));
}

// Utility functions (keep existing implementations)
bool knishio_binary_to_hex(const uint8_t *data, size_t data_len, char **hex_str)
{
    if (!data || !hex_str) return false;
    
    size_t hex_len = data_len * 2 + 1;
    *hex_str = knishio_malloc(hex_len);
    if (!*hex_str) return false;
    
    for (size_t i = 0; i < data_len; i++) {
        sprintf(*hex_str + i * 2, "%02x", data[i]);
    }
    
    (*hex_str)[hex_len - 1] = '\0';
    return true;
}

bool knishio_hex_to_binary(const char *hex_str, uint8_t **data, size_t *data_len)
{
    if (!hex_str || !data || !data_len) return false;
    
    size_t hex_len = strlen(hex_str);
    if (hex_len % 2 != 0) return false;
    
    *data_len = hex_len / 2;
    *data = knishio_malloc(*data_len);
    if (!*data) return false;
    
    for (size_t i = 0; i < *data_len; i++) {
        int val;
        if (sscanf(hex_str + i * 2, "%2x", &val) != 1) {
            knishio_free(*data);
            *data = NULL;
            *data_len = 0;
            return false;
        }
        (*data)[i] = (uint8_t)val;
    }
    
    return true;
}

bool knishio_is_valid_hex(const char *hex_str)
{
    if (!hex_str) return false;
    
    size_t len = strlen(hex_str);
    if (len == 0 || len % 2 != 0) return false;
    
    for (size_t i = 0; i < len; i++) {
        char c = hex_str[i];
        if (!((c >= '0' && c <= '9') || 
              (c >= 'a' && c <= 'f') || 
              (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    
    return true;
}

bool knishio_shake256_js_compatible(const char *input, size_t output_length, char **hex_output)
{
    return knishio_shake256_hash(input, output_length, hex_output);
}

bool knishio_shake256_self_test(void)
{
    const char *test_input = "abc";
    const char *expected = "483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739";
    
    char *output = NULL;
    if (!knishio_shake256_hash(test_input, 256, &output)) return false;
    
    bool result = (strcmp(output, expected) == 0);
    knishio_free(output);
    
    return result;
}

bool knishio_shake256_test_vector(const char *input, size_t output_bits, const char *expected_hex)
{
    if (!input || !expected_hex) return false;
    
    char *output = NULL;
    if (!knishio_shake256_hash(input, output_bits, &output)) return false;
    
    bool result = (strcmp(output, expected_hex) == 0);
    knishio_free(output);
    
    return result;
}