/**
 * @file check_molecule.c
 * @brief Complete molecule validation implementation for KnishIO C SDK
 *
 * This is THE molecule verifier. It is the sole definition of knishio_molecule_check.
 *
 * History worth keeping, because it nearly repeated: this file spent its life excluded
 * from the build. It and src/molecule.c both defined knishio_molecule_check, macOS ld64
 * rejected the duplicate, and the resolution was to drop this — the complete one — from
 * CMakeLists. What shipped instead was a ~110-line subset that verified the molecular
 * hash and the V-sum and carried the comment "Skip complex OTS verification for now", so
 * the SDK accepted any molecule whose hash was internally consistent, forged signature
 * and all. Nothing detected that, because the only verifier tests called the subset.
 *
 * Two things were wrong with this file when it was restored, both invisible for the same
 * reason — it had never executed:
 *   - check_molecular_hash reimplemented atom sorting and hashing by hand instead of
 *     calling knishio_molecule_generate_hash. It disagreed with the canonical hasher and
 *     rejected every molecule, valid or not. It now delegates.
 *   - 49 printf("DEBUG …") calls sat outside #if KNISHIO_DEBUG_MODE, which would have
 *     corrupted the self-test's JSON output on stdout.
 *
 * Implements JavaScript CheckMolecule.js functionality exactly for complete
 * cross-SDK compatibility. Provides comprehensive molecular validation including:
 * - Molecular hash verification
 * - One-time signature (OTS) verification  
 * - All isotope validation (V, M, I, R, C, T, U, P, A, B, F)
 * - ContinuID validation
 * - Batch ID validation
 *
 * Every rule runs over a view of the atoms sorted by index, as JS sorts them (Atom.sortAtoms):
 * "the first atom" and "a non-primary atom" mean index order, never the order a peer's JSON
 * happened to list them in.
 *
 * Without a sender wallet, isotopeV() applies V-only conservation and nothing more, as in every
 * SDK: the validator and knishio_verify_from_server_data() check served transfers with no sender
 * wallet, and a transfer's last V atom is its remainder, which is normally non-zero. (Older JS
 * releases threw TransferRemainderException for that remainder; the JS SDK no longer does.)
 */

#include "knishio/molecule.h"
#include "knishio/atom.h"
#include "knishio/utils/memory.h"
#include "knishio/wallet.h"
#include "knishio/crypto/shake256.h"
#include "knishio/utils/encoding.h"
#include "knishio/error.h"
#include "../kcore_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "knishio/meta.h"
#include <math.h>
#include <errno.h>
#include <strings.h>

#include <cjson/cJSON.h>

/* Forward declarations for internal functions */
static bool check_molecular_hash(const knishio_molecule_t* molecule);
static knishio_error_t check_ots(const knishio_molecule_t* molecule);
static bool check_batch_id(const knishio_molecule_t* molecule);
static bool check_continuid(const knishio_molecule_t* molecule);
static knishio_error_t check_isotope_m(const knishio_molecule_t* molecule);
static knishio_error_t check_isotope_t(const knishio_molecule_t* molecule);
static bool check_isotope_c(const knishio_molecule_t* molecule);
static bool check_isotope_u(const knishio_molecule_t* molecule);
static bool check_isotope_i(const knishio_molecule_t* molecule);
static knishio_error_t check_isotope_r(const knishio_molecule_t* molecule);
static knishio_error_t check_isotope_p(const knishio_molecule_t* molecule);
static knishio_error_t check_isotope_a(const knishio_molecule_t* molecule);
static knishio_error_t check_isotope_v(const knishio_molecule_t* molecule, const knishio_wallet_t* sender_wallet);

static int compare_atom_index(const void* a, const void* b) {
    const int ia = (*(const knishio_atom_t* const*)a)->index;
    const int ib = (*(const knishio_atom_t* const*)b)->index;
    return (ia > ib) - (ia < ib);
}

/* The aggregated meta value for key (JS Meta.aggregateMeta: a later entry wins). *present is set
 * when the key exists at all, even with a NULL value. */
static const char* meta_value(const knishio_atom_t* atom, const char* key, bool* present) {
    const char* found = NULL;
    bool seen = false;
    for (size_t i = 0; i < atom->meta_count; i++) {
        const knishio_meta_t* m = atom->meta ? atom->meta[i] : NULL;
        if (m && m->key && strcmp(m->key, key) == 0) {
            found = m->value;
            seen = true;
        }
    }
    if (present) *present = seen;
    return found;
}

/* JS truthiness of a meta value or atom field: present and not "". */
static bool truthy(const char* s) {
    return s && s[0] != '\0';
}

/* A value as JS Number() reads it: NULL, "" or whitespace is 0; otherwise a decimal number
 * filling the whole string (surrounding whitespace allowed). Returns false where JS gives NaN;
 * non-finite values are refused too. */
static bool js_number(const char* s, double* out) {
    if (!s) {
        *out = 0.0;
        return true;
    }
    const char* p = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (*p == '\0') {
        *out = 0.0;
        return true;
    }
    errno = 0;
    char* end = NULL;
    const double v = strtod(p, &end);
    if (end == p || errno == ERANGE || !isfinite(v)) {
        return false;
    }
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v') end++;
    if (*end != '\0') {
        return false;
    }
    *out = v;
    return true;
}

/* JS strict equality of two nullable strings. */
static bool same_string(const char* a, const char* b) {
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

/**
 * @brief Complete molecule verification (matches JavaScript CheckMolecule.verify exactly)
 *
 * Each check returns its OWN error code rather than a blanket KNISHIO_ERROR_INVALID_STATE.
 * This is not cosmetic: a caller — or a negative test — that only sees "rejected" cannot
 * tell which check fired, so a test written to prove conservation works keeps passing when
 * an unrelated check starts rejecting for an unrelated reason. The codes are the ones
 * already declared in knishio/error/context.h.
 */
knishio_error_t knishio_molecule_check(
    const knishio_molecule_t* molecule,
    const knishio_wallet_t* sender_wallet
) {
    if (!molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    if (molecule->atom_count == 0 || !molecule->atoms) {
        return KNISHIO_ERROR_ATOMS_MISSING;
    }

    if (!molecule->molecular_hash) {
        return KNISHIO_ERROR_MOLECULAR_HASH_MISSING;
    }

    for (size_t i = 0; i < molecule->atom_count; i++) {
        if (!molecule->atoms[i]) {
            return KNISHIO_ERROR_ATOMS_MISSING;
        }
    }

    /* Every check reads the atoms in index order (JS Atom.sortAtoms), on a view: the caller's
     * array order is left as it is. */
    knishio_atom_t** sorted = malloc(sizeof(knishio_atom_t*) * molecule->atom_count);
    if (!sorted) {
        return KNISHIO_ERROR_MEMORY;
    }
    memcpy(sorted, molecule->atoms, sizeof(knishio_atom_t*) * molecule->atom_count);
    qsort(sorted, molecule->atom_count, sizeof(knishio_atom_t*), compare_atom_index);
    knishio_molecule_t view = *molecule;
    view.atoms = sorted;

    /* Order matches JavaScript CheckMolecule.verify. */
    knishio_error_t err = KNISHIO_SUCCESS;
    if (!check_molecular_hash(&view)) {
        err = KNISHIO_ERROR_MOLECULAR_HASH_MISMATCH;
    } else if ((err = check_ots(&view)) != KNISHIO_SUCCESS) {
        /* check_ots's own code */
    } else if (!check_batch_id(&view)) {
        err = KNISHIO_ERROR_BATCH_ID;
    } else if (!check_continuid(&view)) {
        /* A USER-signed molecule without an I atom: JS CheckMolecule.continuId() throws
         * AtomsMissingException, and validator 0.6.1 answers "AtomsMissing" — same code here. */
        err = KNISHIO_ERROR_ATOMS_MISSING;
    } else if ((err = check_isotope_m(&view)) != KNISHIO_SUCCESS
               || (err = check_isotope_t(&view)) != KNISHIO_SUCCESS) {
        /* the rule's own code */
    } else if (!check_isotope_c(&view) || !check_isotope_u(&view) || !check_isotope_i(&view)) {
        err = KNISHIO_ERROR_INVALID_STATE;
    } else {
        /* R, P, A, then B/F and V (each reports which way it failed: an unbalanced sum and a
         * malformed B/F meta shape are different defects, and the negative vectors target them
         * separately). */
        if ((err = check_isotope_r(&view)) == KNISHIO_SUCCESS
            && (err = check_isotope_p(&view)) == KNISHIO_SUCCESS
            && (err = check_isotope_a(&view)) == KNISHIO_SUCCESS) {
            err = check_isotope_v(&view, sender_wallet);
        }
    }

    free(sorted);
    return err;
}


/**
 * @brief Verify molecular hash matches atom composition
 * Matches JavaScript CheckMolecule.molecularHash() exactly
 */
static bool check_molecular_hash(const knishio_molecule_t* molecule) {
    if (!molecule->molecular_hash || strlen(molecule->molecular_hash) != 64) {
#if KNISHIO_DEBUG_MODE
        printf("DEBUG check_molecular_hash: Invalid molecular hash format\n");
#endif
        return false;
    }

    if (!molecule->atoms || molecule->atom_count == 0) {
#if KNISHIO_DEBUG_MODE
        printf("DEBUG check_molecular_hash: No atoms to hash\n");
#endif
        return false;
    }

    /* Recompute with the canonical hasher — the SAME function knishio_molecule_sign uses.
     *
     * This deliberately does NOT reimplement atom sorting and hashable-value concatenation.
     * It used to: ~95 lines that bubble-sorted the atoms and built the hash input by hand.
     * That third implementation disagreed with the canonical one and rejected 100% of
     * molecules, valid and invalid alike — which no test caught, because this whole file
     * was excluded from the build. A hash has exactly one definition; anything that needs
     * it calls knishio_molecule_generate_hash. */
    knishio_molecule_t temp = *molecule;
    temp.molecular_hash = NULL;

    if (knishio_molecule_generate_hash(&temp) != KNISHIO_SUCCESS) {
#if KNISHIO_DEBUG_MODE
        printf("DEBUG check_molecular_hash: Failed to regenerate hash\n");
#endif
        return false;
    }

    const bool hash_matches = (strcmp(temp.molecular_hash, molecule->molecular_hash) == 0);
    knishio_free(temp.molecular_hash);

#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_molecular_hash: %s\n", hash_matches ? "MATCH" : "MISMATCH");
#endif
    return hash_matches;
}

/**
 * @brief Verify one-time signature
 * Matches JavaScript CheckMolecule.ots(): OTS v1 (2048 hex, 16 chains) or v2 (8192 hex,
 * 64 chains), as hex or base64.
 */
static knishio_error_t check_ots(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_ots: Starting OTS verification\n");
#endif

    /* The declared hash must be exactly 64 base-17 characters before any chain work */
    if (!knishio_molecule_hash_is_signable(molecule->molecular_hash)) {
        return KNISHIO_ERROR_SIGNATURE_MALFORMED;
    }

    /* Get normalized hash (matches JavaScript molecule.normalizedHash()) */
    int* enumerated = NULL;
    size_t hash_length = 0;
    knishio_error_t error = knishio_molecule_enumerate_hash(molecule->molecular_hash, &enumerated, &hash_length);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* normalize_hash allocates a fresh array, so the enumerated one is freed separately */
    int* normalized_hash = NULL;
    error = knishio_molecule_normalize_hash(enumerated, hash_length, &normalized_hash);
    free(enumerated);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Rebuild OTS from all atoms; a missing otsFragment contributes nothing. The buffer is
     * sized from the actual fragments. */
    size_t ots_len = 0;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = knishio_molecule_get_atom(molecule, i);
        if (atom && atom->ots_fragment) {
            ots_len += strlen(atom->ots_fragment);
        }
    }

    char* ots = malloc(ots_len + 1);
    if (!ots) {
        free(normalized_hash);
        return KNISHIO_ERROR_MEMORY;
    }

    ots[0] = '\0';
    size_t ots_used = 0;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = knishio_molecule_get_atom(molecule, i);
        if (atom && atom->ots_fragment) {
            size_t n = strlen(atom->ots_fragment);
            memcpy(ots + ots_used, atom->ots_fragment, n);
            ots_used += n;
            ots[ots_used] = '\0';
        }
    }

#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_ots: Rebuilt OTS length: %zu\n", ots_used);
#endif

    /* 2048 or 8192 characters are hex; anything else is the compressed (base64) form, and the
     * decoded length decides the version */
    size_t sig_len = ots_used;
    if (sig_len != KNISHIO_PRIVKEY_LENGTH && sig_len != KNISHIO_PRIVKEY_LENGTH_V2) {
        char* hex_ots = NULL;
        if (!knishio_base64_to_hex(ots, &hex_ots)) {
            free(ots);
            free(normalized_hash);
            return KNISHIO_ERROR_SIGNATURE_MALFORMED;
        }
        free(ots);
        ots = hex_ots;
        sig_len = strlen(ots);

#if KNISHIO_DEBUG_MODE
        printf("DEBUG check_ots: Decompressed OTS length: %zu\n", sig_len);
#endif

        if (sig_len != KNISHIO_PRIVKEY_LENGTH && sig_len != KNISHIO_PRIVKEY_LENGTH_V2) {
            free(ots);
            free(normalized_hash);
            return KNISHIO_ERROR_SIGNATURE_MALFORMED;
        }
    }

    /* Segments of 128 characters: 16 (v1) or 64 (v2), chain i advanced 8 + normalized[i] steps */
    const size_t chunk_count = sig_len / 128;
    char* key_fragments = malloc(sig_len + 1);
    if (!key_fragments) {
        free(ots);
        free(normalized_hash);
        return KNISHIO_ERROR_MEMORY;
    }
    key_fragments[0] = '\0';

    /* With kcore linked, an eligible OTS (lowercase hex, every count 0..64) runs all chains in
     * one call. A peer's OTS that is not lowercase hex runs the loop below, so the verdict
     * never depends on whether kcore is linked. */
    bool chains_done = false;
#ifdef KNISHIO_HAVE_KCORE
    if (hash_length >= chunk_count) {
        int counts[KNISHIO_KCORE_WOTS_MAX_CHUNKS];
        for (size_t i = 0; i < chunk_count; i++) {
            counts[i] = 8 + normalized_hash[i];
        }
        chains_done = knishio_kcore_wots_chains(ots, chunk_count, counts, key_fragments);
    }
#endif
    error = KNISHIO_SUCCESS;
    for (size_t index = 0; !chains_done && error == KNISHIO_SUCCESS && index < chunk_count; index++) {
        char* working_chunk = knishio_strndup(ots + (index * 128), 128);
        if (!working_chunk) {
            error = KNISHIO_ERROR_MEMORY;
            break;
        }

        const int iterations = 8 + normalized_hash[index];
        for (int iter = 0; iter < iterations; iter++) {
            char* new_hash = NULL;
            if (!knishio_shake256_hash(working_chunk, 512, &new_hash)) {
                error = KNISHIO_ERROR_CRYPTO;
                break;
            }
            free(working_chunk);
            working_chunk = new_hash;
        }

        strcat(key_fragments, working_chunk);
        free(working_chunk);
    }
    free(ots);
    free(normalized_hash);

    /* Digest, then the address it resolves to (matches JavaScript) */
    char* digest = NULL;
    char* final_address = NULL;
    if (error == KNISHIO_SUCCESS &&
        (!knishio_shake256_hash(key_fragments, 8192, &digest) ||
         !knishio_shake256_hash(digest, 256, &final_address))) {
        error = KNISHIO_ERROR_CRYPTO;
    }
    free(key_fragments);
    free(digest);

    /* The recovered address must be the address the first atom claims */
    if (error == KNISHIO_SUCCESS) {
        const knishio_atom_t* signing_atom = knishio_molecule_get_atom(molecule, 0);
        if (!signing_atom || !signing_atom->wallet_address ||
            strcmp(final_address, signing_atom->wallet_address) != 0) {
            error = KNISHIO_ERROR_SIGNATURE_MISMATCH;
        }
#if KNISHIO_DEBUG_MODE
        printf("DEBUG check_ots: Recovered address: %s, result %d\n", final_address, (int)error);
#endif
    }
    free(final_address);

    return error;
}

knishio_error_t knishio_molecule_check_ots(const knishio_molecule_t* molecule) {
    if (!molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (molecule->atom_count == 0 || !molecule->atoms) {
        return KNISHIO_ERROR_ATOMS_MISSING;
    }
    for (size_t i = 0; i < molecule->atom_count; i++) {
        if (!molecule->atoms[i]) {
            return KNISHIO_ERROR_ATOMS_MISSING;
        }
    }
    /* Fragments concatenate, and atoms[0] signs, in index order (as knishio_molecule_check). */
    knishio_atom_t** sorted = malloc(sizeof(knishio_atom_t*) * molecule->atom_count);
    if (!sorted) {
        return KNISHIO_ERROR_MEMORY;
    }
    memcpy(sorted, molecule->atoms, sizeof(knishio_atom_t*) * molecule->atom_count);
    qsort(sorted, molecule->atom_count, sizeof(knishio_atom_t*), compare_atom_index);
    knishio_molecule_t view = *molecule;
    view.atoms = sorted;
    const knishio_error_t err = check_ots(&view);
    free(sorted);
    return err;
}

/**
 * @brief Check batch ID validation (matches JavaScript batchId())
 */
static bool check_batch_id(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_batch_id: Starting batch ID validation\n");
#endif
    
    if (!molecule || molecule->atom_count == 0) {
        /* JavaScript throws BatchIdException if no atoms */
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_batch_id: FAIL - No atoms in molecule\n");
        #endif
        return false;
    }
    
    const knishio_atom_t* signing_atom = knishio_molecule_get_atom(molecule, 0);
    if (!signing_atom) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_batch_id: FAIL - No signing atom\n");
        #endif
        return false;
    }
    
    /* Only validate if signing atom is V isotope with batch ID */
    if (signing_atom->isotope == KNISHIO_ISOTOPE_V && signing_atom->batch_id) {
        /* Get all V isotope atoms */
        knishio_atom_t** v_atoms = NULL;
        size_t v_count = 0;
        
        if (knishio_molecule_filter_atoms_by_isotope(molecule, "V", &v_atoms, &v_count) != KNISHIO_SUCCESS) {
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_batch_id: FAIL - Could not filter V atoms\n");
            #endif
            return false;
        }
        
        if (v_count == 0) {
            free(v_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_batch_id: FAIL - No V atoms found\n");
            #endif
            return false;
        }
        
        /* Check remainder atom (last V atom) has same batch ID */
        const knishio_atom_t* remainder_atom = v_atoms[v_count - 1];
        if (!remainder_atom->batch_id || strcmp(signing_atom->batch_id, remainder_atom->batch_id) != 0) {
            free(v_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_batch_id: FAIL - Signing and remainder batch IDs don't match\n");
            #endif
            return false;
        }
        
        /* All V atoms must have batch ID */
        for (size_t i = 0; i < v_count; i++) {
            if (!v_atoms[i]->batch_id) {
                free(v_atoms);
                #if KNISHIO_DEBUG_MODE
                printf("DEBUG check_batch_id: FAIL - V atom %zu missing batch ID\n", i);
                #endif
                return false;
            }
        }
        
        free(v_atoms);
    }
    
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_batch_id: PASS\n");
    #endif
    return true;
}

/**
 * @brief Check ContinuID requirements (matches JavaScript continuId())  
 */
static bool check_continuid(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_continuid: Starting ContinuID validation\n");
#endif
    
    if (!molecule || molecule->atom_count == 0) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_continuid: PASS - No atoms to check\n");
        #endif
        return true;
    }
    
    const knishio_atom_t* first_atom = knishio_molecule_get_atom(molecule, 0);
    if (!first_atom) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_continuid: PASS - No first atom\n");
        #endif
        return true;
    }
    
    /* Check if first atom token is 'USER' and molecule has I isotope atoms */
    if (first_atom->token && strcmp(first_atom->token, "USER") == 0) {
        /* Get all I isotope atoms */
        knishio_atom_t** i_atoms = NULL;
        size_t i_count = 0;
        
        if (knishio_molecule_filter_atoms_by_isotope(molecule, "I", &i_atoms, &i_count) != KNISHIO_SUCCESS) {
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_continuid: FAIL - Could not filter I atoms\n");
            #endif
            return false;
        }
        
        if (i_count < 1) {
            free(i_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_continuid: FAIL - USER token requires ContinuID atom (I isotope)\n");
            #endif
            return false;
        }
        
        free(i_atoms);
    }
    
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_continuid: PASS\n");
    #endif
    return true;
}

/* JS Wallet.isBundleHash: 64 characters of [0-9A-Fa-f]. */
static bool is_bundle_hash(const char* s) {
    size_t n = 0;
    for (; s[n]; n++) {
        const char c = s[n];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return n == 64;
}

/**
 * @brief Check M-isotope atoms (matches JavaScript isotopeM())
 *
 * Meta present and token USER (KNISHIO_ERROR_META_MISSING, as before). A readPolicy/writePolicy
 * meta is a JSON object; every entry other than readPolicy/writePolicy must name a meta key of
 * the atom and list only bundle hashes, "all" or "self" (KNISHIO_ERROR_POLICY_INVALID).
 */
static knishio_error_t check_isotope_m(const knishio_molecule_t* molecule) {
    static const char* const policy_keys[] = { "readPolicy", "writePolicy" };

    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = molecule->atoms[i];
        if (atom->isotope != KNISHIO_ISOTOPE_M) {
            continue;
        }
        if (!atom->meta || atom->meta_count < 1) {
            return KNISHIO_ERROR_META_MISSING;
        }
        if (!same_string(atom->token, "USER")) {
            return KNISHIO_ERROR_META_MISSING;
        }

        for (size_t k = 0; k < 2; k++) {
            const char* text = meta_value(atom, policy_keys[k], NULL);
            if (!truthy(text)) {
                continue;
            }
            cJSON* policy = cJSON_Parse(text);
            bool valid = cJSON_IsObject(policy);
            const cJSON* entry = NULL;
            if (valid) {
                cJSON_ArrayForEach(entry, policy) {
                    if (strcmp(entry->string, "readPolicy") == 0 || strcmp(entry->string, "writePolicy") == 0) {
                        continue;
                    }
                    bool present = false;
                    meta_value(atom, entry->string, &present);
                    if (!present || !cJSON_IsArray(entry)) {
                        valid = false;
                        break;
                    }
                    const cJSON* v = NULL;
                    cJSON_ArrayForEach(v, entry) {
                        const char* s = cJSON_IsString(v) ? v->valuestring : NULL;
                        if (!s || !(is_bundle_hash(s) || strcmp(s, "all") == 0 || strcmp(s, "self") == 0)) {
                            valid = false;
                            break;
                        }
                    }
                    if (!valid) {
                        break;
                    }
                }
            }
            cJSON_Delete(policy);
            if (!valid) {
                return KNISHIO_ERROR_POLICY_INVALID;
            }
        }
    }
    return KNISHIO_SUCCESS;
}

/**
 * @brief Check T-isotope atoms (matches JavaScript isotopeT())
 *
 * A metaType of "wallet" (any case) needs truthy position and bundle meta, and every T atom a
 * truthy token meta (KNISHIO_ERROR_META_MISSING); then token USER and index 0
 * (KNISHIO_ERROR_INVALID_STATE, as before).
 */
static knishio_error_t check_isotope_t(const knishio_molecule_t* molecule) {
    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = molecule->atoms[i];
        if (atom->isotope != KNISHIO_ISOTOPE_T) {
            continue;
        }
        if (atom->meta_type && strcasecmp(atom->meta_type, "wallet") == 0
            && (!truthy(meta_value(atom, "position", NULL)) || !truthy(meta_value(atom, "bundle", NULL)))) {
            return KNISHIO_ERROR_META_MISSING;
        }
        if (!truthy(meta_value(atom, "token", NULL))) {
            return KNISHIO_ERROR_META_MISSING;
        }
        if (!same_string(atom->token, "USER") || atom->index != 0) {
            return KNISHIO_ERROR_INVALID_STATE;
        }
    }
    return KNISHIO_SUCCESS;
}

/**
 * @brief Check C-isotope atoms (matches JavaScript isotopeC())
 */
static bool check_isotope_c(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_c: Starting C-isotope validation\n");
#endif
    
    /* Get all C isotope atoms */
    knishio_atom_t** c_atoms = NULL;
    size_t c_count = 0;
    
    if (knishio_molecule_filter_atoms_by_isotope(molecule, "C", &c_atoms, &c_count) != KNISHIO_SUCCESS) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_c: FAIL - Could not filter C atoms\n");
        #endif
        return false;
    }
    
    /* No C atoms is valid */
    if (c_count == 0) {
        free(c_atoms);
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_c: PASS - No C atoms to validate\n");
        #endif
        return true;
    }
    
    /* Validate each C atom */
    for (size_t i = 0; i < c_count; i++) {
        const knishio_atom_t* atom = c_atoms[i];
        
        /* Must have USER token */
        if (!atom->token || strcmp(atom->token, "USER") != 0) {
            free(c_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_c: FAIL - C atom token must be USER, got: %s\n", atom->token ? atom->token : "NULL");
            #endif
            return false;
        }
        
        /* Must have index equal to 0 */
        if (atom->index != 0) {
            free(c_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_c: FAIL - C atom index must be 0, got: %d\n", atom->index);
            #endif
            return false;
        }
    }
    
    free(c_atoms);
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_c: PASS\n");
    #endif
    return true;
}

/**
 * @brief Check U-isotope atoms (matches JavaScript isotopeU())
 */
static bool check_isotope_u(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_u: Starting U-isotope validation\n");
#endif
    
    /* Get all U isotope atoms */
    knishio_atom_t** u_atoms = NULL;
    size_t u_count = 0;
    
    if (knishio_molecule_filter_atoms_by_isotope(molecule, "U", &u_atoms, &u_count) != KNISHIO_SUCCESS) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_u: FAIL - Could not filter U atoms\n");
        #endif
        return false;
    }
    
    /* No U atoms is valid */
    if (u_count == 0) {
        free(u_atoms);
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_u: PASS - No U atoms to validate\n");
        #endif
        return true;
    }
    
    /* Validate each U atom */
    for (size_t i = 0; i < u_count; i++) {
        const knishio_atom_t* atom = u_atoms[i];
        
        /* Must be signed by an AUTH wallet (first login) or the identity's USER wallet at its
         * ContinuID pointer (a returning login the validator marks proven). */
        if (!atom->token || (strcmp(atom->token, "AUTH") != 0 && strcmp(atom->token, "USER") != 0)) {
            free(u_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_u: FAIL - U atom token must be AUTH or USER, got: %s\n", atom->token ? atom->token : "NULL");
            #endif
            return false;
        }
        
        /* Must have index equal to 0 */
        if (atom->index != 0) {
            free(u_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_u: FAIL - U atom index must be 0, got: %d\n", atom->index);
            #endif
            return false;
        }
    }
    
    free(u_atoms);
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_u: PASS\n");
    #endif
    return true;
}

/**
 * @brief Check I-isotope atoms (matches JavaScript isotopeI())
 */
static bool check_isotope_i(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_i: Starting I-isotope validation\n");
#endif
    
    /* Get all I isotope atoms */
    knishio_atom_t** i_atoms = NULL;
    size_t i_count = 0;
    
    if (knishio_molecule_filter_atoms_by_isotope(molecule, "I", &i_atoms, &i_count) != KNISHIO_SUCCESS) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_i: FAIL - Could not filter I atoms\n");
        #endif
        return false;
    }
    
    /* No I atoms is valid */
    if (i_count == 0) {
        free(i_atoms);
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_i: PASS - No I atoms to validate\n");
        #endif
        return true;
    }
    
    /* Validate each I atom */
    for (size_t i = 0; i < i_count; i++) {
        const knishio_atom_t* atom = i_atoms[i];
        
        /* Must have USER token */
        if (!atom->token || strcmp(atom->token, "USER") != 0) {
            free(i_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_i: FAIL - I atom token must be USER, got: %s\n", atom->token ? atom->token : "NULL");
            #endif
            return false;
        }
        
        /* Must have non-zero index */
        if (atom->index == 0) {
            free(i_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_i: FAIL - I atom index must be non-zero, got: %d\n", atom->index);
            #endif
            return false;
        }
    }
    
    free(i_atoms);
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_i: PASS\n");
    #endif
    return true;
}

/* JS truthiness of a parsed JSON value: absent, null, false, 0 and "" are falsy; [] and {} are not. */
static bool cjson_truthy(const cJSON* v) {
    return v && !cJSON_IsNull(v) && !cJSON_IsFalse(v)
        && !(cJSON_IsNumber(v) && v->valuedouble == 0)
        && !(cJSON_IsString(v) && v->valuestring[0] == '\0');
}

/* JS Rule.toObject() on one parsed rule: condition and callback must be truthy (MetaMissing);
 * every condition needs truthy key, value and comparison and every callback a truthy action
 * (RuleArgumentException), and a truthy callback amount must be numeric (CodeException); both
 * of those are KNISHIO_ERROR_RULE_INVALID here. */
static knishio_error_t check_rule_item(const cJSON* item) {
    const cJSON* condition = cJSON_IsObject(item) ? cJSON_GetObjectItemCaseSensitive(item, "condition") : NULL;
    const cJSON* callback = cJSON_IsObject(item) ? cJSON_GetObjectItemCaseSensitive(item, "callback") : NULL;
    if (!cjson_truthy(condition) || !cjson_truthy(callback)) {
        return KNISHIO_ERROR_META_MISSING;
    }
    if (!cJSON_IsArray(condition) || !cJSON_IsArray(callback)) {
        return KNISHIO_ERROR_RULE_INVALID;
    }
    const cJSON* c = NULL;
    cJSON_ArrayForEach(c, condition) {
        if (!cJSON_IsObject(c)
            || !cjson_truthy(cJSON_GetObjectItemCaseSensitive(c, "key"))
            || !cjson_truthy(cJSON_GetObjectItemCaseSensitive(c, "value"))
            || !cjson_truthy(cJSON_GetObjectItemCaseSensitive(c, "comparison"))) {
            return KNISHIO_ERROR_RULE_INVALID;
        }
    }
    cJSON_ArrayForEach(c, callback) {
        if (!cJSON_IsObject(c) || !cjson_truthy(cJSON_GetObjectItemCaseSensitive(c, "action"))) {
            return KNISHIO_ERROR_RULE_INVALID;
        }
        const cJSON* amount = cJSON_GetObjectItemCaseSensitive(c, "amount");
        if (cjson_truthy(amount) && !cJSON_IsNumber(amount)) {
            /* JS isNumeric: a string with non-blank content that Number() reads */
            double ignored = 0;
            const char* s = cJSON_IsString(amount) ? amount->valuestring : NULL;
            while (s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
            if (!s || *s == '\0' || !js_number(amount->valuestring, &ignored)) {
                return KNISHIO_ERROR_RULE_INVALID;
            }
        }
    }
    return KNISHIO_SUCCESS;
}

/**
 * @brief Check R-isotope atoms (matches JavaScript isotopeR())
 *
 * A policy meta may only carry read/write; a rule meta is a non-empty JSON array of rules, each
 * with a condition and a callback list (Rule.toObject).
 */
static knishio_error_t check_isotope_r(const knishio_molecule_t* molecule) {
    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = molecule->atoms[i];
        if (atom->isotope != KNISHIO_ISOTOPE_R) {
            continue;
        }

        const char* policy_text = meta_value(atom, "policy", NULL);
        if (truthy(policy_text)) {
            cJSON* policy = cJSON_Parse(policy_text);
            if (!policy) {
                return KNISHIO_ERROR_POLICY_INVALID;
            }
            bool mixed = !cJSON_IsObject(policy);
            const cJSON* entry = NULL;
            cJSON_ArrayForEach(entry, policy) {
                if (!entry->string || (strcmp(entry->string, "read") != 0 && strcmp(entry->string, "write") != 0)) {
                    mixed = true;
                }
            }
            cJSON_Delete(policy);
            if (mixed) {
                return KNISHIO_ERROR_META_MISSING;   /* "Mixing rules with politics!" */
            }
        }

        const char* rule_text = meta_value(atom, "rule", NULL);
        if (truthy(rule_text)) {
            cJSON* rules = cJSON_Parse(rule_text);
            if (!rules) {
                return KNISHIO_ERROR_RULE_INVALID;
            }
            knishio_error_t err = cJSON_IsArray(rules) ? KNISHIO_SUCCESS : KNISHIO_ERROR_META_MISSING;
            const cJSON* item = NULL;
            if (err == KNISHIO_SUCCESS) {
                cJSON_ArrayForEach(item, rules) {
                    if ((err = check_rule_item(item)) != KNISHIO_SUCCESS) {
                        break;
                    }
                }
            }
            if (err == KNISHIO_SUCCESS && cJSON_GetArraySize(rules) < 1) {
                err = KNISHIO_ERROR_META_MISSING;    /* "No rules!" */
            }
            cJSON_Delete(rules);
            if (err != KNISHIO_SUCCESS) {
                return err;
            }
        }
    }
    return KNISHIO_SUCCESS;
}

/** @brief Check P-isotope (peering) atoms (matches JavaScript isotopeP()) */
static knishio_error_t check_isotope_p(const knishio_molecule_t* molecule) {
    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = molecule->atoms[i];
        if (atom->isotope != KNISHIO_ISOTOPE_P) {
            continue;
        }
        if (!same_string(atom->token, "USER")) {
            return KNISHIO_ERROR_WRONG_TOKEN_TYPE;
        }
        if (!truthy(meta_value(atom, "peerHost", NULL))) {
            return KNISHIO_ERROR_META_MISSING;
        }
    }
    return KNISHIO_SUCCESS;
}

/** @brief Check A-isotope (append request) atoms (matches JavaScript isotopeA()) */
static knishio_error_t check_isotope_a(const knishio_molecule_t* molecule) {
    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = molecule->atoms[i];
        if (atom->isotope != KNISHIO_ISOTOPE_A) {
            continue;
        }
        if (!same_string(atom->token, "USER")) {
            return KNISHIO_ERROR_WRONG_TOKEN_TYPE;
        }
        if (!truthy(atom->meta_type) || !truthy(atom->meta_id)
            || !truthy(meta_value(atom, "action", NULL))) {
            return KNISHIO_ERROR_META_MISSING;
        }
    }
    return KNISHIO_SUCCESS;
}

/* B or F atoms (JavaScript isotopeB()/isotopeF(), CheckMolecule.js:410-501): each carries
 * metaType "walletBundle" and a non-empty metaId, and a numeric value (F: not negative); when
 * the molecule has V atoms, the V values then these atoms' values sum to exactly zero.
 *
 * This is what check_isotope_v() relies on when it skips the V-only sum. The two are a single
 * unit: shipping the skip without this check is not a relaxed rule, it is a deleted one. That
 * exact mistake was found live in three sibling SDKs, each accepting molecules that created or
 * destroyed value while passing every positive test. */
static knishio_error_t check_cross_isotope(const knishio_molecule_t* molecule, knishio_isotope_t isotope) {
    bool any_v = false;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = molecule->atoms[i];
        if (atom->isotope == KNISHIO_ISOTOPE_V) {
            any_v = true;
        }
        if (atom->isotope != isotope) {
            continue;
        }
        /* Empty is rejected, but "0" is a legitimate metaId, so test the string, not its value. */
        if (!same_string(atom->meta_type, "walletBundle") || !truthy(atom->meta_id)) {
            return KNISHIO_ERROR_META_MISSING;
        }
        double value = 0;
        if (!js_number(atom->value, &value)) {
            return KNISHIO_ERROR_TRANSFER_MALFORMED;
        }
        if (isotope == KNISHIO_ISOTOPE_F && value < 0) {
            return KNISHIO_ERROR_TRANSFER_MALFORMED;
        }
    }
    if (!any_v) {
        return KNISHIO_SUCCESS;
    }

    /* JS sums [...vAtoms, ...crossAtoms] and skips a value that is NaN (the V rule rejects it). */
    double sum = 0;
    for (int pass = 0; pass < 2; pass++) {
        const knishio_isotope_t want = pass == 0 ? KNISHIO_ISOTOPE_V : isotope;
        for (size_t i = 0; i < molecule->atom_count; i++) {
            double value = 0;
            if (molecule->atoms[i]->isotope == want && js_number(molecule->atoms[i]->value, &value)) {
                sum += value;
            }
        }
    }
    return sum == 0 ? KNISHIO_SUCCESS : KNISHIO_ERROR_TRANSFER_UNBALANCED;
}

/**
 * @brief Check V-isotope atoms (matches JavaScript CheckMolecule.isotopeV(), with isotopeB()
 *        and isotopeF() run first as JS verify() does)
 *
 * Every V atom: a numeric value (TRANSFER_MALFORMED), atoms[0]'s token (TRANSFER_MISMATCHED);
 * after the first atom, not negative (TRANSFER_MALFORMED) and not atoms[0]'s address
 * (TRANSFER_TO_SELF). Without B/F atoms the V values sum to exactly zero (TRANSFER_UNBALANCED),
 * in the same double arithmetic JS uses. With a sender wallet, its balance plus atoms[0]'s value
 * must not be negative (TRANSFER_BALANCE) and, without B/F atoms, must equal that sum
 * (TRANSFER_REMAINDER). A 2-atom V transfer signed by a V atom takes JS's short path: same
 * token, non-negative end atom, the two values summing to zero.
 */
static knishio_error_t check_isotope_v(const knishio_molecule_t* molecule, const knishio_wallet_t* sender_wallet) {
    bool has_b = false;
    bool has_f = false;
    size_t v_count = 0;
    const knishio_atom_t* end_atom = NULL;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* a = molecule->atoms[i];
        has_b = has_b || a->isotope == KNISHIO_ISOTOPE_B;
        has_f = has_f || a->isotope == KNISHIO_ISOTOPE_F;
        if (a->isotope == KNISHIO_ISOTOPE_V) {
            v_count++;
            end_atom = a;
        }
    }

    knishio_error_t err = KNISHIO_SUCCESS;
    if (has_b && (err = check_cross_isotope(molecule, KNISHIO_ISOTOPE_B)) != KNISHIO_SUCCESS) {
        return err;
    }
    if (has_f && (err = check_cross_isotope(molecule, KNISHIO_ISOTOPE_F)) != KNISHIO_SUCCESS) {
        return err;
    }
    if (v_count == 0) {
        return KNISHIO_SUCCESS;
    }

    const bool has_cross_isotope = has_b || has_f;
    const knishio_atom_t* first_atom = molecule->atoms[0];

    if (!has_cross_isotope && first_atom->isotope == KNISHIO_ISOTOPE_V && v_count == 2) {
        double first_value = 0;
        double end_value = 0;
        if (!same_string(first_atom->token, end_atom->token)) {
            return KNISHIO_ERROR_TRANSFER_MISMATCHED;
        }
        if (!js_number(end_atom->value, &end_value) || end_value < 0) {
            return KNISHIO_ERROR_TRANSFER_MALFORMED;
        }
        if (!js_number(first_atom->value, &first_value) || first_value + end_value != 0) {
            return KNISHIO_ERROR_TRANSFER_UNBALANCED;
        }
        return KNISHIO_SUCCESS;
    }

    double sum = 0;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = molecule->atoms[i];
        if (atom->isotope != KNISHIO_ISOTOPE_V) {
            continue;
        }
        double value = 0;
        if (!js_number(atom->value, &value)) {
            return KNISHIO_ERROR_TRANSFER_MALFORMED;
        }
        if (!same_string(atom->token, first_atom->token)) {
            return KNISHIO_ERROR_TRANSFER_MISMATCHED;
        }
        if (i > 0) {
            if (value < 0) {
                return KNISHIO_ERROR_TRANSFER_MALFORMED;
            }
            if (same_string(atom->wallet_address, first_atom->wallet_address)) {
                return KNISHIO_ERROR_TRANSFER_TO_SELF;
            }
        }
        sum += value;
    }

    if (!has_cross_isotope && sum != 0) {
        return KNISHIO_ERROR_TRANSFER_UNBALANCED;
    }

    if (sender_wallet) {
        double first_value = 0;
        if (!js_number(first_atom->value, &first_value)) {
            return KNISHIO_ERROR_TRANSFER_MALFORMED;
        }
        const double remainder = sender_wallet->balance + first_value;
        if (remainder < 0) {
            return KNISHIO_ERROR_TRANSFER_BALANCE;
        }
        if (!has_cross_isotope && remainder != sum) {
            return KNISHIO_ERROR_TRANSFER_REMAINDER;
        }
    }

    return KNISHIO_SUCCESS;
}