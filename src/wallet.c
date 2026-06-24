#include "knishio/wallet.h"
#include "knishio/crypto/shake256.h"
#include "knishio/crypto/bigint.h"
#include "knishio/crypto/mlkem768.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/utils/security.h"
#include "knishio/utils/encoding.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Legacy function definitions updated to support ContinuID structures */

/* Secret and Bundle Generation */

bool knishio_generate_secret(const char* seed, size_t length, char** secret) {
    if (seed == NULL || secret == NULL) {
        return false;
    }
    
    /* Default length=2048 for backward compatibility */
    if (length == 0) length = 2048;
    
    /* Canonical JS formula (post-2026-06 cross-SDK alignment): `length` is the
     * number of hex chars wanted; outputLen = length*4 bits (= length/2 bytes).
     * length=2048 -> 2048 hex secret; length=128 -> 64-byte (d||z) ML-KEM seed. */
    size_t output_bits = length * 4;
    
    return knishio_shake256_hash(seed, output_bits, secret);
}

/* Updated to support new signature with token and position parameters */
bool knishio_generate_bundle_hash(const char* secret, const char* token, 
                                  const char* position, char** bundle_hash) {
    if (secret == NULL || bundle_hash == NULL) {
        return false;
    }
    
    /* Validate secret format and length */
    if (!knishio_validate_hex(secret, KNISHIO_SECRET_LENGTH)) {
        return false;
    }
    
    /* Validate optional parameters if provided */
    if (token && !knishio_validate_token(token)) {
        return false;
    }
    
    if (position && !knishio_validate_position(position)) {
        return false;
    }
    
    /* Generate 64-char bundle hash from secret using SHAKE256 */
    /* JS: SHAKE256(secret) → 256 bits → 64 hex chars */
    return knishio_shake256_hash(secret, 256, bundle_hash);
}

/* Legacy overload for backward compatibility */
bool knishio_generate_bundle_hash_legacy(const char* secret, char** bundle) {
    return knishio_generate_bundle_hash(secret, NULL, NULL, bundle);
}

/* Position Generation */

bool knishio_generate_position(char** position) {
    if (position == NULL) {
        return false;
    }
    
    /* Generate random 64-char hex position */
    return knishio_random_hex_string(KNISHIO_POSITION_LENGTH, NULL, position);
}

bool knishio_use_fixed_position(char** position) {
    if (position == NULL) {
        return false;
    }
    
    /* Use fixed position for deterministic testing */
    *position = knishio_strdup(KNISHIO_FIXED_POSITION);
    return (*position != NULL);
}

/* Wallet Key Derivation */

bool knishio_generate_wallet_key(const char* secret, const char* token, 
                                const char* position, char** private_key) {
    if (secret == NULL || position == NULL || private_key == NULL) {
        return false;
    }
    
    /* Validate input lengths */
    if (strlen(secret) != KNISHIO_SECRET_LENGTH || 
        strlen(position) != KNISHIO_POSITION_LENGTH) {
        return false;
    }
    
    bool success = false;
    char* indexed_key_hex = NULL;
    char* intermediate_hash = NULL;
    knishio_shake256_ctx_t intermediate_ctx;
    knishio_shake256_ctx_t final_ctx;
    
    /* Step 1: indexed_key = BigInt(secret) + BigInt(position) */
    if (!knishio_wallet_key_from_hex(secret, position, &indexed_key_hex)) {
        goto cleanup;
    }
    
    /* Step 2: intermediate = SHAKE256(indexed_key + token) → 8192 bits */
    if (!knishio_shake256_init(&intermediate_ctx)) {
        goto cleanup;
    }
    
    /* Update with indexed key (as hex string) */
    if (!knishio_shake256_update_string(&intermediate_ctx, indexed_key_hex)) {
        goto cleanup;
    }
    
    /* Update with token if provided */
    if (token != NULL && strlen(token) > 0) {
        if (!knishio_shake256_update_string(&intermediate_ctx, token)) {
            goto cleanup;
        }
    }
    
    /* Finalize and get intermediate hash */
    if (!knishio_shake256_final(&intermediate_ctx)) {
        goto cleanup;
    }
    
    if (!knishio_shake256_squeeze_hex(&intermediate_ctx, 8192, &intermediate_hash)) {
        goto cleanup;
    }
    
    /* Step 3: private_key = SHAKE256(intermediate) → 8192 bits → 2048 hex chars */
    if (!knishio_shake256_init(&final_ctx)) {
        goto cleanup;
    }
    
    if (!knishio_shake256_update_string(&final_ctx, intermediate_hash)) {
        goto cleanup;
    }
    
    if (!knishio_shake256_final(&final_ctx)) {
        goto cleanup;
    }
    
    if (!knishio_shake256_squeeze_hex(&final_ctx, 8192, private_key)) {
        goto cleanup;
    }
    
    success = true;
    
cleanup:
    knishio_shake256_cleanup(&intermediate_ctx);
    knishio_shake256_cleanup(&final_ctx);
    
    if (indexed_key_hex != NULL) {
        knishio_free(indexed_key_hex);
    }
    
    if (intermediate_hash != NULL) {
        knishio_free(intermediate_hash);
    }
    
    return success;
}

/* Address Generation */

bool knishio_generate_address(const char* private_key, char** address) {
    if (private_key == NULL || address == NULL) {
        return false;
    }
    
    /* Validate private key length */
    if (strlen(private_key) != KNISHIO_PRIVKEY_LENGTH) {
        return false;
    }
    
    bool success = false;
    char** fragments = NULL;
    size_t fragment_count = 0;
    knishio_shake256_ctx_t digest_ctx;
    knishio_shake256_ctx_t output_ctx;
    char* final_digest = NULL;
    
    /* Step 1: Split key into 16 fragments of 128 chars each */
    if (!knishio_chunk_string(private_key, 128, &fragments, &fragment_count)) {
        goto cleanup;
    }
    
    if (fragment_count != 16) {
        goto cleanup;
    }
    
    /* Initialize digest context for accumulating all processed fragments */
    if (!knishio_shake256_init(&digest_ctx)) {
        goto cleanup;
    }
    
    /* Step 2: For each fragment, hash it 16 times iteratively */
    for (size_t i = 0; i < fragment_count; i++) {
        char* working_fragment = knishio_strdup(fragments[i]);
        if (working_fragment == NULL) {
            goto cleanup;
        }
        
        /* Hash the fragment 16 times */
        for (int iteration = 0; iteration < 16; iteration++) {
            knishio_shake256_ctx_t working_ctx;
            char* new_fragment = NULL;
            
            if (!knishio_shake256_init(&working_ctx)) {
                knishio_free(working_fragment);
                goto cleanup;
            }
            
            if (!knishio_shake256_update_string(&working_ctx, working_fragment)) {
                knishio_shake256_cleanup(&working_ctx);
                knishio_free(working_fragment);
                goto cleanup;
            }
            
            if (!knishio_shake256_final(&working_ctx)) {
                knishio_shake256_cleanup(&working_ctx);
                knishio_free(working_fragment);
                goto cleanup;
            }
            
            /* Get 512-bit (128 hex char) output for next iteration */
            if (!knishio_shake256_squeeze_hex(&working_ctx, 512, &new_fragment)) {
                knishio_shake256_cleanup(&working_ctx);
                knishio_free(working_fragment);
                goto cleanup;
            }
            
            knishio_shake256_cleanup(&working_ctx);
            knishio_free(working_fragment);
            working_fragment = new_fragment;
        }
        
        /* Step 3: Add final fragment to digest */
        if (!knishio_shake256_update_string(&digest_ctx, working_fragment)) {
            knishio_free(working_fragment);
            goto cleanup;
        }
        
        knishio_free(working_fragment);
    }
    
    /* Get the accumulated digest */
    if (!knishio_shake256_final(&digest_ctx)) {
        goto cleanup;
    }
    
    if (!knishio_shake256_squeeze_hex(&digest_ctx, 8192, &final_digest)) {
        goto cleanup;
    }
    
    /* Step 4: Final hash to produce 64-char address */
    if (!knishio_shake256_init(&output_ctx)) {
        goto cleanup;
    }
    
    if (!knishio_shake256_update_string(&output_ctx, final_digest)) {
        goto cleanup;
    }
    
    if (!knishio_shake256_final(&output_ctx)) {
        goto cleanup;
    }
    
    if (!knishio_shake256_squeeze_hex(&output_ctx, 256, address)) {
        goto cleanup;
    }
    
    success = true;
    
cleanup:
    knishio_shake256_cleanup(&digest_ctx);
    knishio_shake256_cleanup(&output_ctx);
    
    if (fragments != NULL) {
        for (size_t i = 0; i < fragment_count; i++) {
            if (fragments[i] != NULL) {
                knishio_free(fragments[i]);
            }
        }
        knishio_free(fragments);
    }
    
    if (final_digest != NULL) {
        knishio_free(final_digest);
    }
    
    return success;
}

/* Updated Wallet Management with ContinuID structures */

bool knishio_wallet_create(knishio_wallet_t** wallet, const char* seed, 
                          const char* token, const char* position) {
    if (wallet == NULL || seed == NULL || token == NULL) {
        return false;
    }
    
    /* Generate secret from seed */
    char* secret = NULL;
    if (!knishio_generate_secret(seed, 2048, &secret)) {
        return false;
    }
    
    /* Use new ContinuID-aware wallet creation */
    bool result = knishio_wallet_create_with_continuid(wallet, secret, token, position);
    
    /* Clean up temporary secret */
    knishio_secure_zero(secret, strlen(secret));
    knishio_free(secret);
    
    return result;
}

bool knishio_wallet_from_secret(knishio_wallet_t** wallet, const char* secret,
                               const char* token, const char* position) {
    if (wallet == NULL || secret == NULL || token == NULL) {
        return false;
    }
    
    /* Use new ContinuID-aware wallet creation */
    return knishio_wallet_create_with_continuid(wallet, secret, token, position);
}

void knishio_wallet_cleanup(knishio_wallet_t* wallet) {
    if (wallet == NULL) {
        return;
    }
    
    /* Securely zero and free sensitive data */
    if (wallet->secret != NULL) {
        knishio_secure_zero(wallet->secret, strlen(wallet->secret));
        knishio_free(wallet->secret);
        wallet->secret = NULL;
    }
    
    if (wallet->private_key != NULL) {
        knishio_secure_zero(wallet->private_key, strlen(wallet->private_key));
        knishio_free(wallet->private_key);
        wallet->private_key = NULL;
    }
    
    /* Free ML-KEM private key bytes */
    if (wallet->privkey_bytes != NULL) {
        knishio_secure_zero(wallet->privkey_bytes, wallet->privkey_bytes_len);
        knishio_free(wallet->privkey_bytes);
        wallet->privkey_bytes = NULL;
        wallet->privkey_bytes_len = 0;
    }
    
    /* Free other fields */
    knishio_free(wallet->bundle_hash);
    knishio_free(wallet->position);
    knishio_free(wallet->address);
    knishio_free(wallet->token);
    knishio_free(wallet->pubkey);
    knishio_free(wallet->batch_id);
    knishio_free(wallet->characters);
    knishio_free(wallet->created_at);
    
    /* Free token units */
    if (wallet->token_units != NULL) {
        for (size_t i = 0; i < wallet->token_unit_count; i++) {
            knishio_free(wallet->token_units[i].id);
            knishio_free(wallet->token_units[i].name);
            knishio_free(wallet->token_units[i].metas_json);
            knishio_free(wallet->token_units[i].token_slug);
            knishio_free(wallet->token_units[i].batch_id);
            knishio_free(wallet->token_units[i].created_at);
        }
        knishio_free(wallet->token_units);
        wallet->token_units = NULL;
        wallet->token_unit_count = 0;
    }
    
    /* Free trade rates */
    if (wallet->trade_rates.tokens != NULL) {
        for (size_t i = 0; i < wallet->trade_rates.count; i++) {
            knishio_free(wallet->trade_rates.tokens[i]);
        }
        knishio_free(wallet->trade_rates.tokens);
        knishio_free(wallet->trade_rates.rates);
        wallet->trade_rates.tokens = NULL;
        wallet->trade_rates.rates = NULL;
        wallet->trade_rates.count = 0;
    }
    
    /* Free molecules */
    if (wallet->molecules.hashes != NULL) {
        for (size_t i = 0; i < wallet->molecules.count; i++) {
            knishio_free(wallet->molecules.hashes[i]);
            /* Note: molecules are not freed here as they may be shared */
        }
        knishio_free(wallet->molecules.hashes);
        knishio_free(wallet->molecules.molecules);
        wallet->molecules.hashes = NULL;
        wallet->molecules.molecules = NULL;
        wallet->molecules.count = 0;
    }
    
    /* Free position sequence */
    if (wallet->sequence != NULL) {
        knishio_position_sequence_free(wallet->sequence);
        wallet->sequence = NULL;
    }
    
    /* Zero the structure but don't free it (caller's responsibility) */
    wallet->bundle_hash = NULL;
    wallet->position = NULL;
    wallet->address = NULL;
    wallet->token = NULL;
    wallet->pubkey = NULL;
    wallet->batch_id = NULL;
    wallet->characters = NULL;
    wallet->created_at = NULL;
    wallet->balance = 0.0;
    wallet->is_shadow = false;
    wallet->initialized = false;
}

void knishio_wallet_free(knishio_wallet_t* wallet) {
    if (wallet == NULL) {
        return;
    }

    knishio_wallet_cleanup(wallet);
    knishio_free(wallet);
}

/* Token-unit helpers (stackable / NFT support) */

/* Deep-copy a token unit (each wallet owns its own strings; aliasing would double-free). */
static void tu_copy(knishio_token_unit_t* dst, const knishio_token_unit_t* src) {
    dst->id = src->id ? knishio_strdup(src->id) : NULL;
    dst->name = src->name ? knishio_strdup(src->name) : NULL;
    dst->metas_json = src->metas_json ? knishio_strdup(src->metas_json) : NULL;
    dst->token_slug = src->token_slug ? knishio_strdup(src->token_slug) : NULL;
    dst->batch_id = src->batch_id ? knishio_strdup(src->batch_id) : NULL;
    dst->created_at = src->created_at ? knishio_strdup(src->created_at) : NULL;
    dst->amount = src->amount;
    dst->is_shadow = src->is_shadow;
}

/* Free a token-unit array (entries' strings + the array). */
static void tu_free_array(knishio_token_unit_t* units, size_t count) {
    if (units == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        knishio_free(units[i].id);
        knishio_free(units[i].name);
        knishio_free(units[i].metas_json);
        knishio_free(units[i].token_slug);
        knishio_free(units[i].batch_id);
        knishio_free(units[i].created_at);
    }
    knishio_free(units);
}

static bool tu_id_in_units(const char* id, char** units, size_t unit_count) {
    if (id == NULL) {
        return false;
    }
    for (size_t i = 0; i < unit_count; i++) {
        if (units[i] && strcmp(id, units[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Partition the source wallet's tokenUnits across source (SENT), recipient (SENT) and remainder
 * (KEPT), mirroring the Rust/Python SDK split. The SENT set = units whose id is in `units`; the
 * KEPT set = the rest. Crucially, the ORIGINAL units are partitioned BEFORE the source is
 * reassigned, and every target gets independent (strdup'd) copies — each wallet frees its own. */
bool knishio_wallet_split_units(knishio_wallet_t *source_wallet,
                                char **units,
                                size_t unit_count,
                                knishio_wallet_t *remainder_wallet,
                                knishio_wallet_t *recipient_wallet) {
    if (source_wallet == NULL || remainder_wallet == NULL) {
        return false;
    }
    if (units == NULL || unit_count == 0) {
        return true; /* nothing to split */
    }

    size_t sent_n = 0, kept_n = 0;
    for (size_t i = 0; i < source_wallet->token_unit_count; i++) {
        if (tu_id_in_units(source_wallet->token_units[i].id, units, unit_count)) {
            sent_n++;
        } else {
            kept_n++;
        }
    }

    knishio_token_unit_t* sent = (sent_n > 0) ? knishio_calloc(sent_n, sizeof(knishio_token_unit_t)) : NULL;
    knishio_token_unit_t* sent_recipient = (recipient_wallet != NULL && sent_n > 0)
        ? knishio_calloc(sent_n, sizeof(knishio_token_unit_t)) : NULL;
    knishio_token_unit_t* kept = (kept_n > 0) ? knishio_calloc(kept_n, sizeof(knishio_token_unit_t)) : NULL;
    if ((sent_n > 0 && sent == NULL) ||
        (recipient_wallet != NULL && sent_n > 0 && sent_recipient == NULL) ||
        (kept_n > 0 && kept == NULL)) {
        tu_free_array(sent, sent_n);
        tu_free_array(sent_recipient, sent_n);
        tu_free_array(kept, kept_n);
        return false;
    }

    size_t si = 0, ki = 0;
    for (size_t i = 0; i < source_wallet->token_unit_count; i++) {
        const knishio_token_unit_t* u = &source_wallet->token_units[i];
        if (tu_id_in_units(u->id, units, unit_count)) {
            tu_copy(&sent[si], u);
            if (sent_recipient) {
                tu_copy(&sent_recipient[si], u);
            }
            si++;
        } else {
            tu_copy(&kept[ki], u);
            ki++;
        }
    }

    /* Read complete — now free the originals + any pre-existing units on the targets, then assign. */
    tu_free_array(source_wallet->token_units, source_wallet->token_unit_count);
    source_wallet->token_units = sent;
    source_wallet->token_unit_count = sent_n;

    tu_free_array(remainder_wallet->token_units, remainder_wallet->token_unit_count);
    remainder_wallet->token_units = kept;
    remainder_wallet->token_unit_count = kept_n;

    if (recipient_wallet != NULL) {
        tu_free_array(recipient_wallet->token_units, recipient_wallet->token_unit_count);
        recipient_wallet->token_units = sent_recipient;
        recipient_wallet->token_unit_count = sent_n;
    }

    return true;
}

bool knishio_wallet_split_units_multi(knishio_wallet_t *source_wallet,
                                      const char ***recipient_unit_lists,
                                      const size_t *recipient_unit_counts,
                                      knishio_wallet_t **recipient_wallets,
                                      size_t recipient_count,
                                      knishio_wallet_t *remainder_wallet) {
    if (source_wallet == NULL || remainder_wallet == NULL ||
        recipient_unit_lists == NULL || recipient_unit_counts == NULL ||
        recipient_wallets == NULL) {
        return false;
    }

    /* Any units to send? (else fungible -> no-op) */
    bool any_sent = false;
    for (size_t r = 0; r < recipient_count; r++) {
        if (recipient_unit_counts[r] > 0) { any_sent = true; break; }
    }
    if (!any_sent) {
        return true;
    }

    /* 1. Each recipient gets its own subset (deep-copied from the source's units; the source is
     *    not modified yet — step 2 re-partitions it after all subsets are copied). */
    for (size_t r = 0; r < recipient_count; r++) {
        knishio_wallet_t *rw = recipient_wallets[r];
        if (rw == NULL) {
            return false;
        }
        char **ids = (char **) recipient_unit_lists[r];
        size_t idc = recipient_unit_counts[r];
        size_t n = 0;
        for (size_t i = 0; i < source_wallet->token_unit_count; i++) {
            if (tu_id_in_units(source_wallet->token_units[i].id, ids, idc)) {
                n++;
            }
        }
        knishio_token_unit_t *subset = (n > 0) ? knishio_calloc(n, sizeof(knishio_token_unit_t)) : NULL;
        if (n > 0 && subset == NULL) {
            return false;
        }
        size_t si = 0;
        for (size_t i = 0; i < source_wallet->token_unit_count; i++) {
            if (tu_id_in_units(source_wallet->token_units[i].id, ids, idc)) {
                tu_copy(&subset[si++], &source_wallet->token_units[i]);
            }
        }
        tu_free_array(rw->token_units, rw->token_unit_count);
        rw->token_units = subset;
        rw->token_unit_count = n;
    }

    /* 2. Partition the source: SENT union (id in ANY recipient list) -> source; KEPT -> remainder. */
    size_t sent_n = 0, kept_n = 0;
    for (size_t i = 0; i < source_wallet->token_unit_count; i++) {
        bool sent = false;
        for (size_t r = 0; r < recipient_count; r++) {
            if (tu_id_in_units(source_wallet->token_units[i].id,
                               (char **) recipient_unit_lists[r], recipient_unit_counts[r])) {
                sent = true; break;
            }
        }
        if (sent) sent_n++; else kept_n++;
    }
    knishio_token_unit_t *sent_units = (sent_n > 0) ? knishio_calloc(sent_n, sizeof(knishio_token_unit_t)) : NULL;
    knishio_token_unit_t *kept_units = (kept_n > 0) ? knishio_calloc(kept_n, sizeof(knishio_token_unit_t)) : NULL;
    if ((sent_n > 0 && sent_units == NULL) || (kept_n > 0 && kept_units == NULL)) {
        tu_free_array(sent_units, sent_n);
        tu_free_array(kept_units, kept_n);
        return false;
    }
    size_t sii = 0, kii = 0;
    for (size_t i = 0; i < source_wallet->token_unit_count; i++) {
        const knishio_token_unit_t *u = &source_wallet->token_units[i];
        bool sent = false;
        for (size_t r = 0; r < recipient_count; r++) {
            if (tu_id_in_units(u->id, (char **) recipient_unit_lists[r], recipient_unit_counts[r])) {
                sent = true; break;
            }
        }
        if (sent) tu_copy(&sent_units[sii++], u);
        else tu_copy(&kept_units[kii++], u);
    }

    tu_free_array(source_wallet->token_units, source_wallet->token_unit_count);
    source_wallet->token_units = sent_units;
    source_wallet->token_unit_count = sent_n;

    tu_free_array(remainder_wallet->token_units, remainder_wallet->token_unit_count);
    remainder_wallet->token_units = kept_units;
    remainder_wallet->token_unit_count = kept_n;

    return true;
}

/* Utility Functions */

bool knishio_random_hex_string(size_t length, const char* charset, char** output) {
    if (output == NULL || length == 0) {
        return false;
    }
    
    /* Use default hex charset if not provided */
    const char* chars = charset ? charset : "0123456789abcdef";
    size_t charset_len = strlen(chars);
    
    /* Allocate output string */
    *output = knishio_malloc(length + 1);
    if (*output == NULL) {
        return false;
    }
    
    /* Seed random number generator (only once per process) */
    static int seeded = 0;
    if (!seeded) {
        // KNOWN: non-CSPRNG seed for the position/charset salt generator (positions are public
        // salt, not the secret). Hardening to a CSPRNG (cross-SDK parity with JS/Rust) is a
        // separate security follow-up.
        // NOLINTNEXTLINE(bugprone-random-generator-seed)
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
    
    /* Generate random hex string */
    for (size_t i = 0; i < length; i++) {
        (*output)[i] = chars[rand() % charset_len];
    }
    (*output)[length] = '\0';
    
    return true;
}

bool knishio_chunk_string(const char* input, size_t chunk_size, 
                         char*** chunks, size_t* chunk_count) {
    if (input == NULL || chunks == NULL || chunk_count == NULL || chunk_size == 0) {
        return false;
    }
    
    size_t input_len = strlen(input);
    *chunk_count = (input_len + chunk_size - 1) / chunk_size;
    
    /* Allocate array of chunk pointers */
    *chunks = knishio_calloc(*chunk_count, sizeof(char*));
    if (*chunks == NULL) {
        return false;
    }
    
    /* Create chunks */
    for (size_t i = 0; i < *chunk_count; i++) {
        size_t offset = i * chunk_size;
        size_t len = (offset + chunk_size <= input_len) ? chunk_size : (input_len - offset);
        
        (*chunks)[i] = knishio_malloc(len + 1);
        if ((*chunks)[i] == NULL) {
            /* Clean up on failure */
            for (size_t j = 0; j < i; j++) {
                knishio_free((*chunks)[j]);
            }
            knishio_free(*chunks);
            *chunks = NULL;
            *chunk_count = 0;
            return false;
        }
        
        memcpy((*chunks)[i], input + offset, len);
        (*chunks)[i][len] = '\0';
    }
    
    return true;
}

/* Test Functions */

bool knishio_wallet_self_test(void) {
    /* Test vector: alice-test-seed-2025 */
    const char* seed = "alice-test-seed-2025";
    const char* expected_bundle = "a06e74f7c0ccb8b28b8864468dc404c5e4e116ed2f3bd197320395369000cc7b";
    const char* expected_address = "c6802a455002eee377179a768240128449723e91b06ffd821935b0fe702b248b";
    
    return knishio_wallet_test_vector(seed, expected_bundle, expected_address, 
                                     "TEST", KNISHIO_FIXED_POSITION);
}

bool knishio_wallet_test_vector(const char* seed, const char* expected_bundle,
                               const char* expected_address, const char* token,
                               const char* position) {
    if (seed == NULL || expected_bundle == NULL || expected_address == NULL) {
        return false;
    }
    
    knishio_wallet_t* wallet = NULL;
    bool success = false;
    
    /* Create wallet from seed */
    if (!knishio_wallet_create(&wallet, seed, token, position)) {
        goto cleanup;
    }
    
    /* Verify bundle hash */
    if (strcmp(wallet->bundle_hash, expected_bundle) != 0) {
#if KNISHIO_DEBUG_MODE
        printf("Bundle mismatch:\n  Expected: %s\n  Got:      %s\n",
               expected_bundle, wallet->bundle_hash);
#endif
        goto cleanup;
    }

    /* Verify address */
    if (strcmp(wallet->address, expected_address) != 0) {
#if KNISHIO_DEBUG_MODE
        printf("Address mismatch:\n  Expected: %s\n  Got:      %s\n",
               expected_address, wallet->address);
#endif
        goto cleanup;
    }
    
    success = true;
    
cleanup:
    if (wallet != NULL) {
        knishio_wallet_free(wallet);
    }
    
    return success;
}

/* Wallet property accessor functions */

const char* knishio_wallet_get_characters(knishio_wallet_t *wallet) {
    if (wallet == NULL) {
        return NULL;
    }
    return wallet->characters;
}

const char* knishio_wallet_get_position(knishio_wallet_t *wallet) {
    if (wallet == NULL) {
        return NULL;
    }
    return wallet->position;
}

const char* knishio_wallet_get_address(knishio_wallet_t *wallet) {
    if (wallet == NULL) {
        return NULL;
    }
    return wallet->address;
}

/* Missing functions implementation for JavaScript SDK parity */

/**
 * @brief Generate random position with specified parameters
 */
bool knishio_generate_random_position(size_t salt_length,
                                      const char *charset,
                                      char **position) {
    if (!position || !charset || salt_length == 0) {
        return false;
    }
    
    *position = malloc(salt_length + 1);
    if (!*position) {
        return false;
    }
    
    /* Generate random position using secure random */
    size_t charset_len = strlen(charset);
    for (size_t i = 0; i < salt_length; i++) {
        (*position)[i] = charset[rand() % charset_len];
    }
    (*position)[salt_length] = '\0';
    
    return true;
}

/**
 * @brief Free position sequence (stub for compatibility)
 */
void knishio_position_sequence_free(knishio_position_sequence_t *sequence) {
    if (sequence) {
        /* Implementation would free position sequence structure */
        free(sequence);
    }
}

/**
 * @brief Create wallet from parameters (full implementation)
 */
/**
 * @brief Derive the wallet's ML-KEM768 keypair from its private key and store the
 *        base64 public key in wallet->pubkey. Mirrors JS Wallet.initializeMLKEM:
 *        seed = generateSecret(key, 128) -> 64 bytes -> FIPS-203 keygen -> base64.
 */
bool knishio_wallet_initialize_mlkem(knishio_wallet_t *wallet) {
    if (!wallet || !wallet->private_key) {
        return false;
    }

    /* JS: const seedHex = generateSecret(this.key, 128) -> 128 hex chars = 64 bytes */
    char *seed_hex = NULL;
    if (!knishio_generate_secret(wallet->private_key, 128, &seed_hex)) {
        return false;
    }
    if (!seed_hex || strlen(seed_hex) != 128) {
        free(seed_hex);
        return false;
    }

    /* Convert 128 hex chars -> the full 64-byte (d||z) seed (no truncation) */
    uint8_t seed[64];
    for (int i = 0; i < 64; i++) {
        char pair[3] = { seed_hex[i * 2], seed_hex[i * 2 + 1], '\0' };
        seed[i] = (uint8_t)strtol(pair, NULL, 16);
    }
    free(seed_hex);

    /* Deterministic ML-KEM768 keygen (FIPS-203, mlkem-native) */
    knishio_mlkem768_keypair_t keypair;
    if (knishio_mlkem768_keypair_from_seed(&keypair, seed, sizeof(seed)) != KNISHIO_SUCCESS) {
        memset(seed, 0, sizeof(seed));
        return false;
    }
    memset(seed, 0, sizeof(seed));

    /* base64-encode the 1184-byte public key (JS serializes the ML-KEM pubkey as base64) */
    char *pubkey_b64 = NULL;
    if (!knishio_base64_encode(keypair.public_key, KNISHIO_PUBKEY_LENGTH, &pubkey_b64)) {
        return false;
    }

    knishio_free(wallet->pubkey);
    wallet->pubkey = pubkey_b64;
    return true;
}

bool knishio_wallet_create_from_params(knishio_wallet_t **wallet,
                                       const char *secret,
                                       const char *bundle,
                                       const char *token,
                                       const char *address,
                                       const char *position,
                                       const char *batch_id,
                                       const char *characters) {
    if (!wallet || !secret || !token || !position) {
        return false;
    }
    
    /* Allocate wallet structure */
    *wallet = knishio_malloc(sizeof(knishio_wallet_t));
    if (!*wallet) {
        return false;
    }
    
    knishio_wallet_t *w = *wallet;
    memset(w, 0, sizeof(knishio_wallet_t));
    
    /* Set basic properties */
    w->secret = knishio_strdup(secret);
    w->token = knishio_strdup(token);
    w->position = knishio_strdup(position);
    w->characters = knishio_strdup(characters ? characters : "BASE64");
    
    if (bundle) {
        w->bundle_hash = knishio_strdup(bundle);
    } else {
        /* Generate bundle hash if not provided (matches JavaScript SDK) */
        char *generated_bundle = NULL;
        if (knishio_generate_bundle_hash(secret, NULL, NULL, &generated_bundle)) {
            w->bundle_hash = generated_bundle;
        }
    }
    
    if (address) {
        w->address = knishio_strdup(address);
    }
    
    /* Generate and store private key (required for ML-KEM768) */
    char *private_key = NULL;
    if (knishio_generate_wallet_key(secret, token, position, &private_key)) {
        w->private_key = private_key; // Store in wallet structure
        
        if (!address) {
            /* Generate address if not provided */
            char *generated_address = NULL;
            if (knishio_generate_address(private_key, &generated_address)) {
                w->address = generated_address;
            }
        }

        /* Initialize ML-KEM768 keypair (JS Wallet calls initializeMLKEM after key/address)
         * so every wallet carries a deterministic base64 pubkey for the ContinuID I-atom. */
        knishio_wallet_initialize_mlkem(w);
    }

    if (batch_id) {
        w->batch_id = knishio_strdup(batch_id);
    }
    
    w->balance = 0.0;
    w->initialized = true;
    w->created_at = knishio_strdup("1756533000000"); /* Placeholder timestamp */
    
    return true;
}

/**
 * @brief Create wallet with ContinuID (JavaScript SDK compatibility)
 */
bool knishio_wallet_create_with_continuid(knishio_wallet_t **wallet,
                                          const char *secret,
                                          const char *token,
                                          const char *position) {
    if (!wallet || !secret || !token || !position) {
        return false;
    }
    
    /* Use the comprehensive create_from_params function */
    return knishio_wallet_create_from_params(wallet, secret, NULL, token, NULL, position, NULL, "BASE64");
}
