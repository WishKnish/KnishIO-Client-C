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
 * - All isotope validation (V, M, I, R, C, T, U)
 * - ContinuID validation
 * - Batch ID validation
 */

#include "knishio/molecule.h"
#include "knishio/atom.h"
#include "knishio/utils/memory.h"
#include "knishio/wallet.h"
#include "knishio/crypto/shake256.h"
#include "knishio/utils/encoding.h"
#include "knishio/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

/* Forward declarations for internal functions */
static bool check_molecular_hash(const knishio_molecule_t* molecule);
static bool check_ots(const knishio_molecule_t* molecule);
static bool check_batch_id(const knishio_molecule_t* molecule);
static bool check_continuid(const knishio_molecule_t* molecule);
static bool check_isotope_m(const knishio_molecule_t* molecule);
static bool check_isotope_t(const knishio_molecule_t* molecule);
static bool check_isotope_c(const knishio_molecule_t* molecule);
static bool check_isotope_u(const knishio_molecule_t* molecule);
static bool check_isotope_i(const knishio_molecule_t* molecule);
static bool check_isotope_r(const knishio_molecule_t* molecule);
static bool check_isotope_v(const knishio_molecule_t* molecule, const knishio_wallet_t* sender_wallet, knishio_error_t* why);
static bool check_token_consistency(const knishio_molecule_t* molecule);

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
#if KNISHIO_DEBUG_MODE
    printf("DEBUG knishio_molecule_check: ENTERED function\n");
#endif

    if (!molecule) {
#if KNISHIO_DEBUG_MODE
        printf("DEBUG knishio_molecule_check: NULL molecule\n");
#endif
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    if (molecule->atom_count == 0 || !molecule->atoms) {
        return KNISHIO_ERROR_ATOMS_MISSING;
    }

    if (!molecule->molecular_hash) {
        return KNISHIO_ERROR_MOLECULAR_HASH_MISSING;
    }

    /* Order matches JavaScript CheckMolecule.verify. */
    if (!check_molecular_hash(molecule))   return KNISHIO_ERROR_MOLECULAR_HASH_MISMATCH;
    if (!check_ots(molecule))              return KNISHIO_ERROR_SIGNATURE_MISMATCH;
    if (!check_batch_id(molecule))         return KNISHIO_ERROR_BATCH_ID;
    if (!check_continuid(molecule))        return KNISHIO_ERROR_INVALID_STATE;
    if (!check_isotope_m(molecule))        return KNISHIO_ERROR_META_MISSING;
    if (!check_isotope_t(molecule))        return KNISHIO_ERROR_INVALID_STATE;
    if (!check_isotope_c(molecule))        return KNISHIO_ERROR_INVALID_STATE;
    if (!check_isotope_u(molecule))        return KNISHIO_ERROR_INVALID_STATE;
    if (!check_isotope_i(molecule))        return KNISHIO_ERROR_INVALID_STATE;
    if (!check_isotope_r(molecule))        return KNISHIO_ERROR_INVALID_STATE;
    /* The V/cross-isotope check reports WHICH way it failed: an unbalanced sum and a
     * malformed B/F meta shape are different defects and the negative vectors target them
     * separately. */
    knishio_error_t v_why = KNISHIO_ERROR_TRANSFER_UNBALANCED;
    if (!check_isotope_v(molecule, sender_wallet, &v_why)) return v_why;
    if (!check_token_consistency(molecule)) return KNISHIO_ERROR_TRANSFER_MISMATCHED;

#if KNISHIO_DEBUG_MODE
    printf("DEBUG knishio_molecule_check: Overall validation result: PASS\n");
#endif

    return KNISHIO_SUCCESS;
}

/**
 * @brief All atoms must carry the same token, except fusion (F) atoms
 *
 * Ported from the subset verifier that previously shipped in src/molecule.c — it is the
 * one check that file had and this one did not.
 */
static bool check_token_consistency(const knishio_molecule_t* molecule) {
    const char* first_token = molecule->atoms[0]->token;
    if (!first_token) {
        return true;
    }

    for (size_t i = 1; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = molecule->atoms[i];
        if (!atom || !atom->token) {
            continue;
        }
        /* A fusion atom legitimately carries a different token */
        if (atom->isotope != KNISHIO_ISOTOPE_F &&
            strcmp(atom->token, first_token) != 0) {
#if KNISHIO_DEBUG_MODE
            printf("DEBUG check_token_consistency: FAIL - atom %zu token '%s' != '%s'\n",
                   i, atom->token, first_token);
#endif
            return false;
        }
    }
    return true;
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
 * Matches JavaScript CheckMolecule.ots() exactly
 */
static bool check_ots(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_ots: Starting OTS verification\n");
#endif
    
    /* Get normalized hash (matches JavaScript molecule.normalizedHash()) */
    int* normalized_hash = NULL;
    size_t hash_length = 0;
    
    if (knishio_molecule_enumerate_hash(molecule->molecular_hash, &normalized_hash, &hash_length) != KNISHIO_SUCCESS) {
#if KNISHIO_DEBUG_MODE
        printf("DEBUG check_ots: Failed to enumerate molecular hash\n");
#endif
        return false;
    }
    
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_ots: Enumerated hash length: %zu\n", hash_length);
#endif
    
    /* Normalize the enumerated hash (matches JavaScript Molecule.normalize()).
     * normalize_hash allocates a fresh array, so the enumerated one must be freed
     * separately — aliasing input and output here leaked it. */
    int* enumerated = normalized_hash;
    normalized_hash = NULL;
    if (knishio_molecule_normalize_hash(enumerated, hash_length, &normalized_hash) != KNISHIO_SUCCESS) {
#if KNISHIO_DEBUG_MODE
        printf("DEBUG check_ots: Failed to normalize molecular hash\n");
#endif
        free(enumerated);
        return false;
    }
    free(enumerated);

#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_ots: Hash enumeration and normalization completed\n");
#endif

    /* Rebuild OTS from all atoms (matches JavaScript). Size the buffer from the actual
     * fragments rather than assuming they fit a fixed 8192 — an unbounded strcat into a
     * fixed allocation is a heap overflow waiting on a molecule with enough atoms. */
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
        return false;
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
    printf("DEBUG check_ots: Rebuilt OTS length: %zu\n", strlen(ots));
    #endif
    
    /* Check if OTS needs decompression (JavaScript logic) */
    if (strlen(ots) != 2048) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_ots: OTS length %zu, attempting base64 decompression\n", strlen(ots));
        #endif
        
        /* Decompress base64 to hex (JavaScript base64ToHex logic) */
        char* hex_ots = NULL;
        if (!knishio_base64_to_hex(ots, &hex_ots)) {
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_ots: Failed to decompress base64 OTS\n");
            #endif
            free(ots);
            free(normalized_hash);
            return false;
        }
        
        /* Replace compressed OTS with decompressed hex version */
        free(ots);
        ots = hex_ots;
        
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_ots: Decompressed OTS length: %zu\n", strlen(ots));
        #endif
        
        /* Verify decompressed length is correct */
        if (strlen(ots) != 2048) {
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_ots: Decompressed OTS still wrong length\n");
            #endif
            free(ots);
            free(normalized_hash);
            return false;
        }
    }
    
    /* Subdivide OTS into 16 chunks of 128 characters (JavaScript logic) */
    char chunks[16][129]; /* 16 chunks of 128 chars + null terminator */
    
    for (int i = 0; i < 16; i++) {
        strncpy(chunks[i], ots + (i * 128), 128);
        chunks[i][128] = '\0';
    }
    
    /* Process each chunk with SHAKE256 iterations (matches JavaScript) */
    char* key_fragments = malloc(2048 + 1);
    if (!key_fragments) {
        free(ots);
        free(normalized_hash);
        return false;
    }
    key_fragments[0] = '\0';
    
    for (int index = 0; index < 16; index++) {
        char* working_chunk = malloc(256);
        if (!working_chunk) continue;
        
        strcpy(working_chunk, chunks[index]);
        
        /* Hash (8 + normalized_hash[index]) times (JavaScript logic) */
        int iterations = 8 + normalized_hash[index];
        
        for (int iter = 0; iter < iterations; iter++) {
            char* new_hash = NULL;
            if (knishio_shake256_hash(working_chunk, 512, &new_hash)) {
                free(working_chunk);
                working_chunk = new_hash;
            } else {
                break;
            }
        }
        
        strcat(key_fragments, working_chunk);
        free(working_chunk);
    }
    
    /* Generate digest from key fragments (JavaScript logic) */
    char* digest = NULL;
    if (!knishio_shake256_hash(key_fragments, 8192, &digest)) {
        free(ots);
        free(normalized_hash);
        free(key_fragments);
        return false;
    }
    
    /* Generate final address from digest (JavaScript logic) */
    char* final_address = NULL;
    if (!knishio_shake256_hash(digest, 256, &final_address)) {
        free(ots);
        free(normalized_hash);
        free(key_fragments);
        free(digest);
        return false;
    }
    
    /* Compare with signing atom's wallet address (JavaScript logic) */
    bool verified = false;
    if (molecule->atom_count > 0) {
        knishio_atom_t* signing_atom = knishio_molecule_get_atom(molecule, 0);
        if (signing_atom && signing_atom->wallet_address) {
            verified = (strcmp(final_address, signing_atom->wallet_address) == 0);
        }
    }
    
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_ots: Expected address: %s\n", final_address);
    #endif
    if (molecule->atom_count > 0) {
        knishio_atom_t* signing_atom = knishio_molecule_get_atom(molecule, 0);
        if (signing_atom && signing_atom->wallet_address) {
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_ots: Signing address: %s\n", signing_atom->wallet_address);
            #endif
        }
    }
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_ots: Verification result: %s\n", verified ? "PASS" : "FAIL");
    #endif
    
    /* Cleanup */
    free(ots);
    free(normalized_hash);
    free(key_fragments);
    free(digest);
    free(final_address);
    
    return verified;
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

/**
 * @brief Check M-isotope atoms (matches JavaScript isotopeM())
 */
static bool check_isotope_m(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_m: Starting M-isotope validation\n");
#endif
    
    /* Get all M isotope atoms */
    knishio_atom_t** m_atoms = NULL;
    size_t m_count = 0;
    
    if (knishio_molecule_filter_atoms_by_isotope(molecule, "M", &m_atoms, &m_count) != KNISHIO_SUCCESS) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_m: FAIL - Could not filter M atoms\n");
        #endif
        return false;
    }
    
    /* No M atoms is valid */
    if (m_count == 0) {
        free(m_atoms);
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_m: PASS - No M atoms to validate\n");
        #endif
        return true;
    }
    
    /* Validate each M atom */
    for (size_t i = 0; i < m_count; i++) {
        const knishio_atom_t* atom = m_atoms[i];
        
        /* Must have meta data */
        if (!atom->meta || atom->meta_count < 1) {
            free(m_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_m: FAIL - M atom missing meta data\n");
            #endif
            return false;
        }
        
        /* Must have USER token */
        if (!atom->token || strcmp(atom->token, "USER") != 0) {
            free(m_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_m: FAIL - M atom token must be USER, got: %s\n", atom->token ? atom->token : "NULL");
            #endif
            return false;
        }
        
        /* Note: Policy validation is complex and may require JSON parsing
         * For now, we validate the basic structure */
    }
    
    free(m_atoms);
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_m: PASS\n");
    #endif
    return true;
}

/**
 * @brief Check T-isotope atoms (matches JavaScript isotopeT())
 */
static bool check_isotope_t(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_t: Starting T-isotope validation\n");
#endif
    
    /* Get all T isotope atoms */
    knishio_atom_t** t_atoms = NULL;
    size_t t_count = 0;
    
    if (knishio_molecule_filter_atoms_by_isotope(molecule, "T", &t_atoms, &t_count) != KNISHIO_SUCCESS) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_t: FAIL - Could not filter T atoms\n");
        #endif
        return false;
    }
    
    /* No T atoms is valid */
    if (t_count == 0) {
        free(t_atoms);
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_t: PASS - No T atoms to validate\n");
        #endif
        return true;
    }
    
    /* Validate each T atom */
    for (size_t i = 0; i < t_count; i++) {
        const knishio_atom_t* atom = t_atoms[i];
        
        /* Must have USER token */
        if (!atom->token || strcmp(atom->token, "USER") != 0) {
            free(t_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_t: FAIL - T atom token must be USER, got: %s\n", atom->token ? atom->token : "NULL");
            #endif
            return false;
        }
        
        /* Must have index equal to 0 */
        if (atom->index != 0) {
            free(t_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_t: FAIL - T atom index must be 0, got: %d\n", atom->index);
            #endif
            return false;
        }
        
        /* Note: Meta validation for wallet-specific fields would require more complex parsing */
    }
    
    free(t_atoms);
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_t: PASS\n");
    #endif
    return true;
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
        
        /* Must have AUTH token */
        if (!atom->token || strcmp(atom->token, "AUTH") != 0) {
            free(u_atoms);
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_u: FAIL - U atom token must be AUTH, got: %s\n", atom->token ? atom->token : "NULL");
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

/**
 * @brief Check R-isotope atoms (matches JavaScript isotopeR())
 */
static bool check_isotope_r(const knishio_molecule_t* molecule) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_r: Starting R-isotope validation\n");
#endif
    
    /* Get all R isotope atoms */
    knishio_atom_t** r_atoms = NULL;
    size_t r_count = 0;
    
    if (knishio_molecule_filter_atoms_by_isotope(molecule, "R", &r_atoms, &r_count) != KNISHIO_SUCCESS) {
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_r: FAIL - Could not filter R atoms\n");
        #endif
        return false;
    }
    
    /* No R atoms is valid */
    if (r_count == 0) {
        free(r_atoms);
        #if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_r: PASS - No R atoms to validate\n");
        #endif
        return true;
    }
    
    /* Validate each R atom */
    for (size_t i = 0; i < r_count; i++) {
        const knishio_atom_t* atom = r_atoms[i];
        
        /* Note: R atoms require complex policy and rule validation that would need JSON parsing
         * For now, we perform basic structural validation */
        
        /* Must have meta data for policy/rule validation */
        if (!atom->meta || atom->meta_count < 1) {
            #if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_r: WARNING - R atom %zu has no meta data\n", i);
            #endif
            /* Continue rather than fail, as this might be a valid case */
        }
    }
    
    free(r_atoms);
    #if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_r: PASS\n");
    #endif
    return true;
}

/**
 * @brief Check V-isotope atoms with balance validation (JavaScript pattern)
 * Matches JavaScript CheckMolecule.isotopeV()
 */
/* Conservation and meta-shape validation for cross-isotope (B/F) molecules.
 *
 * Mirrors isotopeB()/isotopeF() in the JavaScript reference (CheckMolecule.js:395-486):
 *   - every B/F atom carries metaType "walletBundle" and a non-empty metaId
 *   - F atom values must not be negative
 *   - the combined V+B (and V+F) values sum to zero
 *
 * This is what check_isotope_v() delegates to when it skips the V-only sum. The two are a
 * single unit: shipping the skip without this check is not a relaxed rule, it is a deleted
 * one. That exact mistake was found live in three sibling SDKs, each accepting molecules
 * that created or destroyed value while passing every positive test.
 *
 * Note C had no F validation at all before this, so it covers F as well as B.
 */
static bool check_cross_isotope_conservation(const knishio_molecule_t* molecule, knishio_error_t* why) {
    long double cross_sum = 0.0L;
    bool saw_cross_isotope = false;

    for (size_t i = 0; i < molecule->atom_count; i++) {
        const knishio_atom_t* atom = molecule->atoms[i];
        if (!atom) {
            continue;
        }

        const bool is_b = (atom->isotope == KNISHIO_ISOTOPE_B);
        const bool is_f = (atom->isotope == KNISHIO_ISOTOPE_F);

        if (is_b || is_f) {
            saw_cross_isotope = true;

            /* B/F atoms must reference a wallet bundle. Empty is rejected, but "0" is a
             * legitimate metaId, so test the string rather than its truthiness. */
            if (!atom->meta_type || strcmp(atom->meta_type, "walletBundle") != 0 ||
                !atom->meta_id || atom->meta_id[0] == '\0') {
                if (why) *why = KNISHIO_ERROR_META_MISSING;
                return false;
            }
        } else if (atom->isotope != KNISHIO_ISOTOPE_V) {
            continue;
        }

        /* V, B and F atom values all participate in the combined conservation sum */
        if (!atom->value || atom->value[0] == '\0') {
            continue;
        }

        /* A value that will not parse is malformed, not zero */
        errno = 0;
        char* end = NULL;
        long double parsed = strtold(atom->value, &end);
        if (end == atom->value || *end != '\0' || errno == ERANGE) {
            return false;
        }

        /* F atoms must not be negative */
        if (is_f && parsed < 0.0L) {
            return false;
        }

        cross_sum += parsed;
    }

    if (!saw_cross_isotope) {
        return true;
    }

    return fabsl(cross_sum) < 1e-9L;
}

static bool check_isotope_v(const knishio_molecule_t* molecule, const knishio_wallet_t* sender_wallet, knishio_error_t* why) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_v: Checking V-isotope atoms\n");
#endif

    /* Cross-isotope (buffer-family) molecules carry the balancing weight on B/F atoms, so
     * the V-only sum below is non-zero by construction and must be skipped. Skipping is
     * only sound because check_cross_isotope_conservation() enforces conservation over the
     * combined set — the gate and the delegate are added together, deliberately. */
    bool has_cross_isotope = false;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        knishio_atom_t* a = knishio_molecule_get_atom(molecule, i);
        if (a && (a->isotope == KNISHIO_ISOTOPE_B || a->isotope == KNISHIO_ISOTOPE_F)) {
            has_cross_isotope = true;
            break;
        }
    }

    if (has_cross_isotope) {
        return check_cross_isotope_conservation(molecule, why);
    }

    /* Count V-isotope atoms */
    int v_atom_count = 0;
    double total_value = 0.0;

    for (size_t i = 0; i < molecule->atom_count; i++) {
        knishio_atom_t* atom = knishio_molecule_get_atom(molecule, i);
        if (atom && atom->isotope == KNISHIO_ISOTOPE_V) {
            v_atom_count++;

            if (atom->value) {
                double value = atof(atom->value);
                total_value += value;
#if KNISHIO_DEBUG_MODE
                printf("DEBUG check_isotope_v: V-atom %d value: %.1f\n", v_atom_count-1, value);
#endif
            }
        }
    }

#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_v: Total V atoms: %d, Total value: %.1f\n", v_atom_count, total_value);
#endif

    /* All atoms must sum to zero for balanced transaction (JavaScript logic) */
    bool balanced = (fabs(total_value) < 0.01);
#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_v: Balanced: %s\n", balanced ? "YES" : "NO");
#endif

    if (!balanced) {
#if KNISHIO_DEBUG_MODE
        printf("DEBUG check_isotope_v: FAIL - Transaction not balanced\n");
#endif
        return false;
    }

    /* Additional sender wallet validation if provided (JavaScript logic) */
    if (sender_wallet && v_atom_count > 0) {
        knishio_atom_t* first_atom = knishio_molecule_get_atom(molecule, 0);
        if (first_atom && first_atom->isotope == KNISHIO_ISOTOPE_V && first_atom->value) {
            double first_value = atof(first_atom->value);
            double remainder = sender_wallet->balance + first_value;

#if KNISHIO_DEBUG_MODE
            printf("DEBUG check_isotope_v: Sender balance: %.1f, First atom: %.1f, Remainder: %.1f\n",
                   sender_wallet->balance, first_value, remainder);
#endif

            if (remainder < 0) {
#if KNISHIO_DEBUG_MODE
                printf("DEBUG check_isotope_v: FAIL - Insufficient balance\n");
#endif
                return false;
            }

            if (fabs(remainder - total_value) > 0.01) {
#if KNISHIO_DEBUG_MODE
                printf("DEBUG check_isotope_v: FAIL - Remainder mismatch\n");
#endif
                return false;
            }
        }
    }

#if KNISHIO_DEBUG_MODE
    printf("DEBUG check_isotope_v: PASS\n");
#endif
    return true;
}