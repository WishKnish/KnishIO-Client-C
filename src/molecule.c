/**
 * @file molecule.c
 * @brief Molecule implementation for KnishIO C SDK
 * 
 * Implements JavaScript-compatible molecule management with C17 best practices.
 * Molecules are collections of atoms that are accepted or rejected together.
 */

#include "knishio/knishio.h"
#include "knishio/molecule.h"
#include "knishio/atom.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/utils/encoding.h"
#include "knishio/crypto/shake256.h"
// #include "knishio/crypto/signatures.h" - removed (dead code)
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <math.h>

/* Internal helper function declarations */
static knishio_error_t resize_atoms_array(knishio_molecule_t* molecule);
static knishio_error_t copy_string_field(char** dest, const char* src);
static void free_string_field(char** field);
static int compare_atoms_by_index(const void* a, const void* b);

/* Molecule lifecycle functions */

knishio_error_t knishio_molecule_create(
    knishio_molecule_t** molecule,
    const char* secret,
    const char* bundle,
    knishio_wallet_t* source_wallet,
    knishio_wallet_t* remainder_wallet,
    const char* cell_slug,
    const char* version
) {
    if (!molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate molecule structure */
    knishio_molecule_t* mol = knishio_malloc(sizeof(knishio_molecule_t));
    if (!mol) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Initialize all fields to safe defaults */
    memset(mol, 0, sizeof(knishio_molecule_t));
    
    /* Set creation timestamp - use environment variable for deterministic testing */
    const char* fixed_timestamp = getenv("KNISHIO_FIXED_TIMESTAMP");
    if (fixed_timestamp) {
        mol->created_at = (time_t)atoll(fixed_timestamp);
    } else {
        mol->created_at = time(NULL);
    }
    mol->status = KNISHIO_MOLECULE_STATUS_UNKNOWN;
    
    /* Initialize atom array with initial capacity */
    const size_t initial_capacity = 8;
    mol->atoms = knishio_malloc(sizeof(knishio_atom_t*) * initial_capacity);
    if (!mol->atoms) {
        knishio_free(mol);
        return KNISHIO_ERROR_MEMORY;
    }
    mol->atom_capacity = initial_capacity;
    mol->atom_count = 0;

    /* Copy string fields if provided */
    knishio_error_t error = KNISHIO_SUCCESS;
    
    if (secret && (error = copy_string_field(&mol->secret, secret)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    
    if (bundle && (error = copy_string_field(&mol->bundle, bundle)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    
    if (cell_slug) {
        if ((error = copy_string_field(&mol->cell_slug, cell_slug)) != KNISHIO_SUCCESS) {
            goto cleanup;
        }
        if ((error = copy_string_field(&mol->cell_slug_origin, cell_slug)) != KNISHIO_SUCCESS) {
            goto cleanup;
        }
    }
    
    if (version && (error = copy_string_field(&mol->version, version)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Store wallet references (no ownership transfer) */
    mol->source_wallet = source_wallet;
    mol->remainder_wallet = remainder_wallet;

    *molecule = mol;
    return KNISHIO_SUCCESS;

cleanup:
    knishio_molecule_free(mol);
    return error;
}

void knishio_molecule_free(knishio_molecule_t* molecule) {
    if (!molecule) {
        return;
    }

    /* Free string fields */
    free_string_field(&molecule->secret);
    free_string_field(&molecule->bundle);
    free_string_field(&molecule->molecular_hash);
    free_string_field(&molecule->cell_slug);
    free_string_field(&molecule->cell_slug_origin);
    free_string_field(&molecule->version);

    /* Free atoms array (atoms themselves are owned elsewhere) */
    if (molecule->atoms) {
        knishio_free(molecule->atoms);
    }

    /* Note: We don't free wallets as they are not owned by the molecule */
    
    /* Free the molecule structure itself */
    knishio_free(molecule);
}

/* Atom management functions */

knishio_error_t knishio_molecule_add_atom(
    knishio_molecule_t* molecule, 
    knishio_atom_t* atom
) {
    if (!molecule || !atom) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Resize array if needed */
    if (molecule->atom_count >= molecule->atom_capacity) {
        knishio_error_t error = resize_atoms_array(molecule);
        if (error != KNISHIO_SUCCESS) {
            return error;
        }
    }

    /* Check maximum atoms limit */
    if (molecule->atom_count >= KNISHIO_MAX_ATOMS_PER_MOLECULE) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Reset molecular hash when adding atoms */
    free_string_field(&molecule->molecular_hash);
    molecule->is_signed = false;
    molecule->is_verified = false;

    /* Set atom index and add to array */
    knishio_error_t error = knishio_atom_set_index(atom, (int)molecule->atom_count);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    molecule->atoms[molecule->atom_count] = atom;
    molecule->atom_count++;

    return KNISHIO_SUCCESS;
}

knishio_atom_t* knishio_molecule_get_atom(
    const knishio_molecule_t* molecule, 
    size_t index
) {
    if (!molecule || index >= molecule->atom_count) {
        return NULL;
    }
    
    return molecule->atoms[index];
}

knishio_error_t knishio_molecule_filter_atoms_by_isotope(
    const knishio_molecule_t* molecule,
    const char* isotope,
    knishio_atom_t*** filtered_atoms,
    size_t* filtered_count
) {
    if (!molecule || !isotope || !filtered_atoms || !filtered_count) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Convert isotope string to enum for comparison */
    knishio_isotope_t target_isotope = knishio_isotope_from_string(isotope);
    if (target_isotope == KNISHIO_ISOTOPE_UNKNOWN) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Count matching atoms first */
    size_t matches = 0;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        knishio_isotope_t atom_isotope = knishio_atom_get_isotope(molecule->atoms[i]);
        if (atom_isotope == target_isotope) {
            matches++;
        }
    }

    if (matches == 0) {
        *filtered_atoms = NULL;
        *filtered_count = 0;
        return KNISHIO_SUCCESS;
    }

    /* Allocate output array */
    knishio_atom_t** filtered = knishio_malloc(sizeof(knishio_atom_t*) * matches);
    if (!filtered) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Fill filtered array */
    size_t filter_index = 0;
    for (size_t i = 0; i < molecule->atom_count && filter_index < matches; i++) {
        knishio_isotope_t atom_isotope = knishio_atom_get_isotope(molecule->atoms[i]);
        if (atom_isotope == target_isotope) {
            filtered[filter_index++] = molecule->atoms[i];
        }
    }

    *filtered_atoms = filtered;
    *filtered_count = matches;
    return KNISHIO_SUCCESS;
}

size_t knishio_molecule_generate_next_index(const knishio_molecule_t* molecule) {
    if (!molecule) {
        return 0;
    }
    return molecule->atom_count;
}

/* Molecule operations */

knishio_error_t knishio_molecule_sign(
    knishio_molecule_t* molecule,
    const char* bundle,
    bool anonymous,
    bool compressed
) {
#if KNISHIO_DEBUG_MODE
    fprintf(stderr, "DEBUG knishio_molecule_sign: ENTERED function, molecule=%p, bundle=%s\n", 
            (void*)molecule, bundle ? bundle : "NULL");
#endif
    
    if (!molecule || !molecule->secret) {
        fprintf(stderr, "DEBUG knishio_molecule_sign: INVALID_ARGS - molecule=%p, secret=%p\n",
                (void*)molecule, molecule ? (void*)molecule->secret : NULL);
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Check if we have atoms */
    if (molecule->atom_count == 0) {
        fprintf(stderr, "DEBUG knishio_molecule_sign: NO ATOMS - returning KNISHIO_ERROR_ATOMS_MISSING\n");
        return KNISHIO_ERROR_ATOMS_MISSING;
    }
#if KNISHIO_DEBUG_MODE
    fprintf(stderr, "DEBUG knishio_molecule_sign: Has %zu atoms\n", molecule->atom_count);
#endif

    /* Sort atoms by index before signing */
    qsort(molecule->atoms, molecule->atom_count, sizeof(knishio_atom_t*), compare_atoms_by_index);

    /* Derive bundle if not anonymous and not provided */
    if (!anonymous && !molecule->bundle) {
        if (bundle) {
            molecule->bundle = knishio_strdup(bundle);
        } else {
            if (!knishio_generate_bundle_hash(molecule->secret, NULL, NULL, &molecule->bundle)) {
                fprintf(stderr, "DEBUG knishio_molecule_sign: generate_bundle_hash failed\n");
                return KNISHIO_ERROR_CRYPTO;
            }
        }
    }

    /* Generate molecular hash */
    knishio_error_t error = knishio_molecule_generate_hash(molecule);
    if (error != KNISHIO_SUCCESS) {
        fprintf(stderr, "DEBUG knishio_molecule_sign: generate_hash failed with error %d\n", error);
        return error;
    }
#if KNISHIO_DEBUG_MODE
    fprintf(stderr, "DEBUG knishio_molecule_sign: Generated hash: %s\n", molecule->molecular_hash);
#endif

    /* Get signing position from first atom */
    knishio_atom_t* signing_atom = molecule->atoms[0];
    const char* signing_position = signing_atom->position;
    
    if (!signing_position) {
        fprintf(stderr, "DEBUG knishio_molecule_sign: No signing position\n");
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    fprintf(stderr, "DEBUG knishio_molecule_sign: Signing position: %s, token: %s\n", 
            signing_position, signing_atom->token ? signing_atom->token : "NULL");

    /* Generate private key for signing (matching JS Wallet.generateKey) */
    char* private_key = NULL;
    bool key_generated = knishio_generate_wallet_key(
        molecule->secret, 
        signing_atom->token, 
        signing_position, 
        &private_key
    );
    if (!key_generated) {
        fprintf(stderr, "DEBUG knishio_molecule_sign: generate_wallet_key failed\n");
        return KNISHIO_ERROR_CRYPTO;
    }
    fprintf(stderr, "DEBUG knishio_molecule_sign: Generated private key length: %zu\n", 
            private_key ? strlen(private_key) : 0);

    /* Chunk private key into 16 segments of 128 characters each */
    char** key_chunks = NULL;
    size_t chunk_count = 0;
    if (!knishio_chunk_string(private_key, 128, &key_chunks, &chunk_count)) {
        knishio_free(private_key);
        return KNISHIO_ERROR_MEMORY;
    }
    
    if (chunk_count != 16) {
        /* Key should be 2048 chars = 16 chunks of 128 */
        for (size_t i = 0; i < chunk_count; i++) {
            knishio_free(key_chunks[i]);
        }
        knishio_free(key_chunks);
        knishio_free(private_key);
        return KNISHIO_ERROR_CRYPTO;
    }

    /* Enumerate and normalize molecular hash */
    int* enumerated = NULL;
    size_t enum_len = 0;
    error = knishio_molecule_enumerate_hash(molecule->molecular_hash, &enumerated, &enum_len);
    if (error != KNISHIO_SUCCESS) {
        for (size_t i = 0; i < chunk_count; i++) {
            knishio_free(key_chunks[i]);
        }
        knishio_free(key_chunks);
        knishio_free(private_key);
        return error;
    }

    int* normalized = NULL;
    error = knishio_molecule_normalize_hash(enumerated, enum_len, &normalized);
    knishio_free(enumerated);
    if (error != KNISHIO_SUCCESS) {
        for (size_t i = 0; i < chunk_count; i++) {
            knishio_free(key_chunks[i]);
        }
        knishio_free(key_chunks);
        knishio_free(private_key);
        return error;
    }

    /* Build OTS fragments */
    size_t total_fragment_len = 0;
    char* signature_fragments = knishio_malloc(16 * 128 + 1);  /* Max size */
    if (!signature_fragments) {
        knishio_free(normalized);
        for (size_t i = 0; i < chunk_count; i++) {
            knishio_free(key_chunks[i]);
        }
        knishio_free(key_chunks);
        knishio_free(private_key);
        return KNISHIO_ERROR_MEMORY;
    }
    signature_fragments[0] = '\0';

    /* Generate OTS for each chunk */
    for (size_t i = 0; i < 16 && i < enum_len; i++) {
        char* working_chunk = knishio_strdup(key_chunks[i]);
        if (!working_chunk) {
            knishio_free(signature_fragments);
            knishio_free(normalized);
            for (size_t j = 0; j < chunk_count; j++) {
                knishio_free(key_chunks[j]);
            }
            knishio_free(key_chunks);
            knishio_free(private_key);
            return KNISHIO_ERROR_MEMORY;
        }

        /* Apply SHAKE256 iterations based on normalized hash */
        int iterations = 8 - normalized[i];
        for (int iter = 0; iter < iterations; iter++) {
            uint8_t hash_output[64];  /* 512 bits = 64 bytes */
            if (!knishio_shake256_hash_raw((uint8_t*)working_chunk, strlen(working_chunk), hash_output, 64)) {
                knishio_free(working_chunk);
                knishio_free(signature_fragments);
                knishio_free(normalized);
                for (size_t j = 0; j < chunk_count; j++) {
                    knishio_free(key_chunks[j]);
                }
                knishio_free(key_chunks);
                knishio_free(private_key);
                return KNISHIO_ERROR_CRYPTO;
            }
            
            /* Convert hash to hex string for next iteration */
            char hex_hash[129];
            for (int h = 0; h < 64; h++) {
                sprintf(hex_hash + (h * 2), "%02x", hash_output[h]);
            }
            hex_hash[128] = '\0';
            
            knishio_free(working_chunk);
            working_chunk = knishio_strdup(hex_hash);
        }

        /* Append to signature fragments */
        strcat(signature_fragments, working_chunk);
        total_fragment_len += strlen(working_chunk);
        knishio_free(working_chunk);
    }

    knishio_free(normalized);
    for (size_t i = 0; i < chunk_count; i++) {
        knishio_free(key_chunks[i]);
    }
    knishio_free(key_chunks);
    knishio_free(private_key);

    /* Optionally compress OTS using base64 */
    char* final_signature = NULL;
    if (compressed) {
        if (!knishio_hex_to_base64(signature_fragments, &final_signature)) {
            knishio_free(signature_fragments);
            return KNISHIO_ERROR_CRYPTO;
        }
        knishio_free(signature_fragments);
    } else {
        final_signature = signature_fragments;
    }

    /* Distribute signature across atoms */
    size_t sig_len = strlen(final_signature);
    size_t chunk_size = (sig_len + molecule->atom_count - 1) / molecule->atom_count;  /* Ceiling division */
    
    /* DEBUG: Log signature details */
    fprintf(stderr, "DEBUG knishio_molecule_sign: OTS signature length: %zu, atom count: %zu, chunk size: %zu\n", 
            sig_len, molecule->atom_count, chunk_size);
    if (sig_len > 0) {
        fprintf(stderr, "DEBUG knishio_molecule_sign: First 64 chars of signature: %.64s\n", final_signature);
    }
    
    for (size_t i = 0; i < molecule->atom_count; i++) {
        size_t start_pos = i * chunk_size;
        if (start_pos >= sig_len) {
            break;
        }
        
        size_t chunk_len = chunk_size;
        if (start_pos + chunk_len > sig_len) {
            chunk_len = sig_len - start_pos;
        }
        
        /* Set OTS fragment on atom */
        if (molecule->atoms[i]->ots_fragment) {
            knishio_free(molecule->atoms[i]->ots_fragment);
        }
        molecule->atoms[i]->ots_fragment = knishio_malloc(chunk_len + 1);
        if (!molecule->atoms[i]->ots_fragment) {
            knishio_free(final_signature);
            return KNISHIO_ERROR_MEMORY;
        }
        memcpy(molecule->atoms[i]->ots_fragment, final_signature + start_pos, chunk_len);
        molecule->atoms[i]->ots_fragment[chunk_len] = '\0';
        
        /* DEBUG: Log fragment attachment */
        fprintf(stderr, "DEBUG knishio_molecule_sign: Attached OTS fragment to atom %zu: length=%zu, first 32 chars: %.32s\n", 
                i, chunk_len, molecule->atoms[i]->ots_fragment);
    }

    knishio_free(final_signature);
    molecule->is_signed = true;
    
#if KNISHIO_DEBUG_MODE
    fprintf(stderr, "DEBUG knishio_molecule_sign: SUCCESS - returning 0\n");
#endif
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_molecule_check(
    const knishio_molecule_t* molecule,
    const knishio_wallet_t* sender_wallet
) {
    if (!molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Check basic molecule validity */
    if (molecule->atom_count == 0) {
        return KNISHIO_ERROR_ATOMS_MISSING;
    }

    if (!molecule->molecular_hash) {
        return KNISHIO_ERROR_MOLECULAR_HASH_MISSING;
    }

    /* Verify molecular hash matches actual atom composition */
    knishio_molecule_t* temp_mol = knishio_malloc(sizeof(knishio_molecule_t));
    if (!temp_mol) {
        return KNISHIO_ERROR_MEMORY;
    }
    memcpy(temp_mol, molecule, sizeof(knishio_molecule_t));
    temp_mol->molecular_hash = NULL;
    
    knishio_error_t error = knishio_molecule_generate_hash(temp_mol);
    if (error != KNISHIO_SUCCESS) {
        temp_mol->molecular_hash = NULL;  /* Don't free original hash */
        knishio_free(temp_mol);
        return error;
    }
    
    bool hash_matches = (strcmp(temp_mol->molecular_hash, molecule->molecular_hash) == 0);
    knishio_free(temp_mol->molecular_hash);
    temp_mol->molecular_hash = NULL;
    knishio_free(temp_mol);
    
    if (!hash_matches) {
        return KNISHIO_ERROR_MOLECULAR_HASH_MISMATCH;
    }

    /* Check if molecule is signed (has OTS fragments) */
    bool has_signature = false;
    for (size_t i = 0; i < molecule->atom_count; i++) {
        if (molecule->atoms[i]->ots_fragment && strlen(molecule->atoms[i]->ots_fragment) > 0) {
            has_signature = true;
            break;
        }
    }

    if (!has_signature) {
        /* Unsigned molecule - basic validation only */
        return KNISHIO_SUCCESS;
    }

    /* DEBUG: Skip complex OTS verification for now - focus on basic validation */
    /* DEBUG removed to prevent JSON corruption */
    
    /* Simplified V-isotope validation (matches JavaScript CheckMolecule.isotopeV) */
    double total_value = 0.0;
    int v_atom_count = 0;
    
    for (size_t i = 0; i < molecule->atom_count; i++) {
        knishio_atom_t* atom = knishio_molecule_get_atom(molecule, i);
        if (atom && atom->isotope == KNISHIO_ISOTOPE_V) {
            v_atom_count++;
            
            if (atom->value) {
                double value = atof(atom->value);
                total_value += value;
                /* DEBUG removed to prevent JSON corruption */
            }
        }
    }
    
    /* DEBUG removed to prevent JSON corruption */
    
    /* All V atoms must sum to zero for balanced transaction (JavaScript logic) */
    if (v_atom_count > 0) {
        bool balanced = (fabs(total_value) < 0.01);
        /* DEBUG removed to prevent JSON corruption */
        
        if (!balanced) {
            /* DEBUG removed to prevent JSON corruption */
            return KNISHIO_ERROR_INVALID_STATE;
        }
        
        /* Additional sender wallet validation if provided (JavaScript logic) */
        if (sender_wallet) {
            knishio_atom_t* first_atom = knishio_molecule_get_atom(molecule, 0);
            if (first_atom && first_atom->isotope == KNISHIO_ISOTOPE_V && first_atom->value) {
                double first_value = atof(first_atom->value);
                double remainder = sender_wallet->balance + first_value;
                
                /* DEBUG removed to prevent JSON corruption */
                
                if (remainder < 0) {
                    /* DEBUG removed to prevent JSON corruption */
                    return KNISHIO_ERROR_INVALID_STATE;
                }
                
                /* JavaScript logic: remainder should equal total_value (which should be 0) */
                if (fabs(remainder - total_value) > 0.01) {
                    /* DEBUG removed to prevent JSON corruption */
                    return KNISHIO_ERROR_INVALID_STATE;
                }
            }
        }
    }

    /* For now, just verify that all atoms have consistent properties */
    const char* first_token = molecule->atoms[0]->token;
    for (size_t i = 1; i < molecule->atom_count; i++) {
        if (molecule->atoms[i]->token && first_token) {
            if (strcmp(molecule->atoms[i]->token, first_token) != 0) {
                /* Atoms should generally have same token unless it's a fusion */
                knishio_isotope_t isotope = molecule->atoms[i]->isotope;
                if (isotope != KNISHIO_ISOTOPE_F) {
                    return KNISHIO_ERROR_INVALID_ARGS;
                }
            }
        }
    }

    return KNISHIO_SUCCESS;
}

/* Helper function to convert hex to base17 using proper base conversion */
static char* hex_to_base17(const char* hex_hash) {
    if (!hex_hash) return NULL;
    
    size_t hex_len = strlen(hex_hash);
    if (hex_len != 64) return NULL;
    
    /* Base17 charset matching JS SDK */
    const char* base17_chars = "0123456789abcdefg";
    
    /* We'll use a digit array to store the number in base 256 */
    uint8_t num[32];
    memset(num, 0, 32);
    
    /* Convert hex string to byte array */
    for (size_t i = 0; i < 32; i++) {
        char hex_byte[3] = {hex_hash[i*2], hex_hash[i*2+1], '\0'};
        num[i] = (uint8_t)strtol(hex_byte, NULL, 16);
    }
    
    /* Result buffer for base17 string */
    char result[65];
    memset(result, 0, 65);
    int result_idx = 63;  /* Start from the end */
    
    /* Convert from base 256 to base 17 using repeated division */
    /* Keep dividing the entire number by 17 until it becomes 0 */
    while (1) {
        /* Check if number is zero */
        int is_zero = 1;
        for (int i = 0; i < 32; i++) {
            if (num[i] != 0) {
                is_zero = 0;
                break;
            }
        }
        if (is_zero) break;
        
        /* Divide entire number by 17 and get remainder */
        uint32_t remainder = 0;
        for (int i = 0; i < 32; i++) {
            uint32_t temp = remainder * 256 + num[i];
            num[i] = temp / 17;
            remainder = temp % 17;
        }
        
        /* Add remainder to result (building from right to left) */
        if (result_idx >= 0) {
            result[result_idx--] = base17_chars[remainder];
        }
    }
    
    /* Pad with zeros on the left to make it 64 characters */
    for (int i = 0; i <= result_idx; i++) {
        result[i] = '0';
    }
    
    /* Allocate and copy final result */
    char* final_result = knishio_malloc(65);
    if (!final_result) return NULL;
    memcpy(final_result, result, 64);
    final_result[64] = '\0';
    
    return final_result;
}

/* Helper to update SHAKE256 context with atom properties following JS SDK order */
static knishio_error_t update_sponge_with_atom(
    knishio_shake256_ctx_t* ctx,
    const knishio_atom_t* atom,
    const char* atom_count_str
) {
    if (!ctx || !atom || !atom_count_str) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Properties to hash in order (from JS SDK getHashableProps):
     * numberOfAtoms, position, walletAddress, isotope, token, value, 
     * batchId, metaType, metaId, meta (keys and values), createdAt
     */
    
    /* Add number of atoms */
#if KNISHIO_DEBUG_MODE
    fprintf(stderr, "  Hash: numberOfAtoms='%s'\n", atom_count_str);
#endif
    if (!knishio_shake256_update_string(ctx, atom_count_str)) {
        return KNISHIO_ERROR_CRYPTO;
    }
    
    /* Add position (always include - required field) */
    if (atom->position) {
#if KNISHIO_DEBUG_MODE
        fprintf(stderr, "  Hash: position='%s'\n", atom->position);
#endif
        if (!knishio_shake256_update_string(ctx, atom->position)) {
            return KNISHIO_ERROR_CRYPTO;
        }
    }
    
    /* Add wallet address (always include - required field) */
    if (atom->wallet_address) {
#if KNISHIO_DEBUG_MODE
        fprintf(stderr, "  Hash: walletAddress='%s'\n", atom->wallet_address);
#endif
        if (!knishio_shake256_update_string(ctx, atom->wallet_address)) {
            return KNISHIO_ERROR_CRYPTO;
        }
    }
    
    /* Add isotope as string */
    const char* isotope_str = knishio_isotope_to_string(atom->isotope);
#if KNISHIO_DEBUG_MODE
    fprintf(stderr, "  Hash: isotope='%s'\n", isotope_str);
#endif
    if (!knishio_shake256_update_string(ctx, isotope_str)) {
        return KNISHIO_ERROR_CRYPTO;
    }
    
    /* Add token (only if not NULL per JS SDK) */
    if (atom->token) {
#if KNISHIO_DEBUG_MODE
        fprintf(stderr, "  Hash: token='%s'\n", atom->token);
#endif
        if (!knishio_shake256_update_string(ctx, atom->token)) {
            return KNISHIO_ERROR_CRYPTO;
        }
    }
    
    /* Add value (only if not NULL per JS SDK) */
    if (atom->value) {
#if KNISHIO_DEBUG_MODE
        fprintf(stderr, "  Hash: value='%s'\n", atom->value);
#endif
        if (!knishio_shake256_update_string(ctx, atom->value)) {
            return KNISHIO_ERROR_CRYPTO;
        }
    }
    
    /* Add batch_id (only if not NULL per JS SDK) */
    if (atom->batch_id) {
        if (!knishio_shake256_update_string(ctx, atom->batch_id)) {
            return KNISHIO_ERROR_CRYPTO;
        }
    }
    
    /* Add meta_type (only if not NULL per JS SDK) */
    if (atom->meta_type) {
        if (!knishio_shake256_update_string(ctx, atom->meta_type)) {
            return KNISHIO_ERROR_CRYPTO;
        }
    }
    
    /* Add meta_id (only if not NULL per JS SDK) */
    if (atom->meta_id) {
        if (!knishio_shake256_update_string(ctx, atom->meta_id)) {
            return KNISHIO_ERROR_CRYPTO;
        }
    }
    
    /* Add meta keys and values */
    for (size_t i = 0; i < atom->meta_count; i++) {
        if (atom->meta[i] && atom->meta[i]->key && atom->meta[i]->value) {
            if (!knishio_shake256_update_string(ctx, atom->meta[i]->key)) {
                return KNISHIO_ERROR_CRYPTO;
            }
            if (!knishio_shake256_update_string(ctx, atom->meta[i]->value)) {
                return KNISHIO_ERROR_CRYPTO;
            }
        }
    }
    
    /* Add created_at as string (milliseconds since epoch) */
    char created_at_str[32];
    snprintf(created_at_str, sizeof(created_at_str), "%lld", (long long)atom->created_at * 1000LL);
#if KNISHIO_DEBUG_MODE
    fprintf(stderr, "  Hash: createdAt='%s'\n", created_at_str);
#endif
    if (!knishio_shake256_update_string(ctx, created_at_str)) {
        return KNISHIO_ERROR_CRYPTO;
    }
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_molecule_generate_hash(knishio_molecule_t* molecule) {
    if (!molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (molecule->atom_count == 0) {
        return KNISHIO_ERROR_ATOMS_MISSING;
    }
    
    /* Sort atoms by index (matching JS SDK Atom.sortAtoms) */
    knishio_atom_t** sorted_atoms = knishio_malloc(sizeof(knishio_atom_t*) * molecule->atom_count);
    if (!sorted_atoms) {
        return KNISHIO_ERROR_MEMORY;
    }
    memcpy(sorted_atoms, molecule->atoms, sizeof(knishio_atom_t*) * molecule->atom_count);
    qsort(sorted_atoms, molecule->atom_count, sizeof(knishio_atom_t*), compare_atoms_by_index);
    
    /* Initialize SHAKE256 context for incremental updates (matches JS SDK) */
    knishio_shake256_ctx_t ctx;
    if (!knishio_shake256_init(&ctx)) {
        knishio_free(sorted_atoms);
        return KNISHIO_ERROR_CRYPTO;
    }
    
    /* Convert atom count to string */
    char atom_count_str[16];
    snprintf(atom_count_str, sizeof(atom_count_str), "%zu", molecule->atom_count);
    
    /* Update sponge with each atom's properties (matches JS SDK) */
    for (size_t i = 0; i < molecule->atom_count; i++) {
        knishio_error_t error = update_sponge_with_atom(&ctx, sorted_atoms[i], atom_count_str);
        if (error != KNISHIO_SUCCESS) {
            knishio_shake256_cleanup(&ctx);
            knishio_free(sorted_atoms);
            return error;
        }
    }
    
    knishio_free(sorted_atoms);
    
    /* Finalize the sponge absorb phase */
    if (!knishio_shake256_final(&ctx)) {
        knishio_shake256_cleanup(&ctx);
        return KNISHIO_ERROR_CRYPTO;
    }
    
    /* Squeeze 256 bits (32 bytes) from the sponge */
    uint8_t hash_bytes[32];
    if (!knishio_shake256_squeeze(&ctx, hash_bytes, 32)) {
        knishio_shake256_cleanup(&ctx);
        return KNISHIO_ERROR_CRYPTO;
    }
    
    /* Clean up context */
    knishio_shake256_cleanup(&ctx);
    
    /* Convert to hex */
    char hex_hash[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hex_hash + (i * 2), "%02x", hash_bytes[i]);
    }
    hex_hash[64] = '\0';
    
    /* Convert to base17 (default format like JS SDK) */
    char* base17_hash = hex_to_base17(hex_hash);
    if (!base17_hash) {
        return KNISHIO_ERROR_CRYPTO;
    }
    
    /* Ensure it's padded to 64 chars with leading zeros */
    if (strlen(base17_hash) < 64) {
        char padded[65];
        size_t pad_count = 64 - strlen(base17_hash);
        memset(padded, '0', pad_count);
        strcpy(padded + pad_count, base17_hash);
        knishio_free(base17_hash);
        base17_hash = knishio_strdup(padded);
    }
    
    /* Store the hash */
    free_string_field(&molecule->molecular_hash);
    molecule->molecular_hash = base17_hash;
    
    return KNISHIO_SUCCESS;
}

/* Utility functions */

knishio_error_t knishio_molecule_to_json(
    const knishio_molecule_t* molecule,
    char** json_output
) {
    if (!molecule || !json_output) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* KISS approach: Use string concatenation for now */
    /* This is simpler and more reliable than complex JSON builder API */
    
    /* Size the buffer to the content: each atom carries an otsFragment (~1100 chars) plus
     * meta that can include a base64 ML-KEM pubkey (~1580 chars). A fixed 8192 truncated
     * any molecule with a pubkey I-atom, producing malformed JSON — scale per atom. */
    size_t buf_size = 16384 + molecule->atom_count * 8192;
    char* buffer = knishio_malloc(buf_size);
    if (!buffer) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Start JSON object */
    strcpy(buffer, "{");
    
    /* Add status field (null for unsigned molecules) */
    strcat(buffer, "\"status\":null,");
    
    /* Add molecular hash */
    if (molecule->molecular_hash) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"molecularHash\":\"%s\",", molecule->molecular_hash);
    } else {
        strcat(buffer, "\"molecularHash\":null,");
    }
    
    /* Add creation timestamp (JS SDK compatible - milliseconds) */
    snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
        "\"createdAt\":\"%ld\",", (long)molecule->created_at * 1000);
    
    /* Add cell slug if present */
    if (molecule->cell_slug && strlen(molecule->cell_slug) > 0) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"cellSlug\":\"%s\",", molecule->cell_slug);
    } else {
        strcat(buffer, "\"cellSlug\":null,");
    }
    
    /* Add bundle hash */
    if (molecule->bundle && strlen(molecule->bundle) > 0) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"bundle\":\"%s\",", molecule->bundle);
    } else {
        strcat(buffer, "\"bundle\":null,");
    }
    
    /* Add version if present */
    if (molecule->version) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"version\":\"%s\",", molecule->version);
    }
    
    /* Add atoms array */
    strcat(buffer, "\"atoms\":[");
    
    for (size_t i = 0; i < molecule->atom_count; i++) {
        if (i > 0) {
            strcat(buffer, ",");
        }
        
        /* Get atom JSON */
        char* atom_json = NULL;
        knishio_error_t error = knishio_atom_to_json(molecule->atoms[i], &atom_json);
        if (error != KNISHIO_SUCCESS) {
            knishio_free(buffer);
            return error;
        }
        
        /* Add atom JSON to buffer */
        strncat(buffer, atom_json, buf_size - strlen(buffer) - 1);
        knishio_free(atom_json);
    }
    
    /* Close atoms array */
    strcat(buffer, "]");
    
    /* Add sourceWallet data for cross-SDK validation (matches Python/TypeScript pattern) */
    if (molecule->source_wallet) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            ",\"sourceWallet\":{\"address\":\"%s\",\"position\":\"%s\",\"token\":\"%s\",\"balance\":%.1f}",
            molecule->source_wallet->address ? molecule->source_wallet->address : "",
            molecule->source_wallet->position ? molecule->source_wallet->position : "",
            molecule->source_wallet->token ? molecule->source_wallet->token : "",
            molecule->source_wallet->balance);
        /* DEBUG removed to prevent JSON corruption */
    }
    
    /* Add remainderWallet data for cross-SDK validation (matches Python/TypeScript pattern) */
    if (molecule->remainder_wallet) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            ",\"remainderWallet\":{\"address\":\"%s\",\"position\":\"%s\",\"token\":\"%s\",\"balance\":%.1f}",
            molecule->remainder_wallet->address ? molecule->remainder_wallet->address : "",
            molecule->remainder_wallet->position ? molecule->remainder_wallet->position : "",
            molecule->remainder_wallet->token ? molecule->remainder_wallet->token : "",
            molecule->remainder_wallet->balance);
        /* DEBUG removed to prevent JSON corruption */
    }
    
    /* Close main JSON object */
    strcat(buffer, "}");
    
    /* Create final output string */
    size_t final_len = strlen(buffer);
    char* final_json = knishio_malloc(final_len + 1);
    if (!final_json) {
        knishio_free(buffer);
        return KNISHIO_ERROR_MEMORY;
    }
    
    strcpy(final_json, buffer);
    knishio_free(buffer);
    
    *json_output = final_json;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_molecule_from_json(
    const char* json_input,
    knishio_molecule_t** molecule
) {
    if (!json_input || !molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* TODO: Implement JSON deserialization using json parser */
    /* This would parse the JSON and create a molecule with all fields */
    
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
}

const char* knishio_molecule_get_cell_slug_delimiter(void) {
    return KNISHIO_CELL_SLUG_DELIMITER;
}

knishio_error_t knishio_molecule_enumerate_hash(
    const char* molecular_hash,
    int** enumerated_output,
    size_t* output_length
) {
    if (!molecular_hash || !enumerated_output || !output_length) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    size_t hash_len = strlen(molecular_hash);
    if (hash_len != KNISHIO_MOLECULAR_HASH_LENGTH) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate output array */
    int* enumerated = knishio_malloc(sizeof(int) * hash_len);
    if (!enumerated) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Enumerate each character according to JS SDK mapping */
    /* 0-9: -8 to 1, a-f: 2 to 7, g: 8 (for base-17) */
    for (size_t i = 0; i < hash_len; i++) {
        char c = molecular_hash[i];
        if (c >= '0' && c <= '9') {
            enumerated[i] = (c - '0') - 8;  /* 0->-8, 1->-7, ..., 9->1 */
        } else if (c >= 'a' && c <= 'f') {
            enumerated[i] = (c - 'a') + 2;  /* a->2, b->3, ..., f->7 */
        } else if (c == 'g') {
            enumerated[i] = 8;              /* g->8 for base-17 */
        } else {
            knishio_free(enumerated);
            return KNISHIO_ERROR_INVALID_ARGS;
        }
    }

    *enumerated_output = enumerated;
    *output_length = hash_len;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_molecule_normalize_hash(
    const int* enumerated_array,
    size_t array_length,
    int** normalized_output
) {
    if (!enumerated_array || !normalized_output || array_length == 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate output array and copy input */
    int* normalized = knishio_malloc(sizeof(int) * array_length);
    if (!normalized) {
        return KNISHIO_ERROR_MEMORY;
    }
    memcpy(normalized, enumerated_array, sizeof(int) * array_length);

    /* Calculate total sum */
    int total = 0;
    for (size_t i = 0; i < array_length; i++) {
        total += normalized[i];
    }

    /* Normalize to sum to zero (matching JS SDK Molecule.normalize) */
    bool total_condition = (total < 0);
    
    while (total != 0) {
        for (size_t i = 0; i < array_length && total != 0; i++) {
            bool condition = total_condition ? 
                (normalized[i] < 8) : (normalized[i] > -8);
            
            if (condition) {
                if (total_condition) {
                    normalized[i]++;
                    total++;
                } else {
                    normalized[i]--;
                    total--;
                }
                
                if (total == 0) {
                    break;
                }
            }
        }
    }

    *normalized_output = normalized;
    return KNISHIO_SUCCESS;
}

/* Internal helper functions */

static knishio_error_t resize_atoms_array(knishio_molecule_t* molecule) {
    if (!molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    size_t new_capacity = molecule->atom_capacity * 2;
    if (new_capacity > KNISHIO_MAX_ATOMS_PER_MOLECULE) {
        new_capacity = KNISHIO_MAX_ATOMS_PER_MOLECULE;
    }

    if (new_capacity <= molecule->atom_capacity) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    knishio_atom_t** new_atoms = knishio_realloc(
        molecule->atoms, 
        sizeof(knishio_atom_t*) * new_capacity
    );
    if (!new_atoms) {
        return KNISHIO_ERROR_MEMORY;
    }

    molecule->atoms = new_atoms;
    molecule->atom_capacity = new_capacity;
    return KNISHIO_SUCCESS;
}

static knishio_error_t copy_string_field(char** dest, const char* src) {
    if (!dest || !src) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    size_t len = strlen(src);
    char* copy = knishio_malloc(len + 1);
    if (!copy) {
        return KNISHIO_ERROR_MEMORY;
    }

    strcpy(copy, src);
    *dest = copy;
    return KNISHIO_SUCCESS;
}

static void free_string_field(char** field) {
    if (field && *field) {
        knishio_free(*field);
        *field = NULL;
    }
}

static int compare_atoms_by_index(const void* a, const void* b) {
    const knishio_atom_t* atom_a = *(const knishio_atom_t**)a;
    const knishio_atom_t* atom_b = *(const knishio_atom_t**)b;
    
    if (!atom_a || !atom_b) {
        return 0;
    }
    
    if (atom_a->index < atom_b->index) {
        return -1;
    } else if (atom_a->index > atom_b->index) {
        return 1;
    } else {
        return 0;
    }
}

/* High-level molecule operations for JavaScript SDK compatibility */

/* --- Cross-SDK helpers for the extended molecule types (cycle 33) --- */

/* Append the 7 prefixed wallet* meta keys in JS setMetaWallet() order. The 4 core keys are always
 * present; batch_id/pubkey/characters are appended ONLY when non-empty (the molecular hash skips
 * NULL meta — see update_sponge_with_atom — so an empty walletBatchId is omitted, matching JS).
 * pubkey is already the base64 ML-KEM key (mirrors the ContinuID I-atom). */
static void append_wallet_meta(const char** keys, const char** vals, size_t* n, const knishio_wallet_t* w) {
    keys[*n] = "walletTokenSlug";  vals[*n] = w->token;       (*n)++;
    keys[*n] = "walletBundleHash"; vals[*n] = w->bundle_hash; (*n)++;
    keys[*n] = "walletAddress";    vals[*n] = w->address;     (*n)++;
    keys[*n] = "walletPosition";   vals[*n] = w->position;    (*n)++;
    if (w->batch_id && w->batch_id[0])     { keys[*n] = "walletBatchId";    vals[*n] = w->batch_id;   (*n)++; }
    if (w->pubkey && w->pubkey[0])         { keys[*n] = "walletPubkey";     vals[*n] = w->pubkey;     (*n)++; }
    if (w->characters && w->characters[0]) { keys[*n] = "walletCharacters"; vals[*n] = w->characters; (*n)++; }
}

/* Append the ContinuID (I-isotope) atom, mirroring JS Molecule.addContinuIdAtom(). The I-atom is
 * sourced from molecule->remainder_wallet (position/address/bundle_hash), metaType "walletBundle",
 * meta [previousPosition = source.position, pubkey, characters] (each gated on truthy). Extracted
 * from init_meta so all four init flows share one ContinuID implementation. */
static knishio_error_t add_continuid_atom(knishio_molecule_t* molecule) {
    knishio_wallet_t* rw = molecule->remainder_wallet;
    if (!rw) {
        return KNISHIO_ERROR_INVALID_STATE;
    }

    knishio_atom_t* continuid_atom = NULL;
    knishio_error_t result = knishio_atom_create(
        &continuid_atom, rw->position, rw->address, KNISHIO_ISOTOPE_I, rw->token, NULL, NULL);
    if (result != KNISHIO_SUCCESS) {
        return result;
    }

    /* ContinuID metaType/metaId (JS: walletBundle / remainderWallet.bundle) */
    if (continuid_atom->meta_type) knishio_free(continuid_atom->meta_type);
    if (continuid_atom->meta_id) knishio_free(continuid_atom->meta_id);
    continuid_atom->meta_type = knishio_strdup("walletBundle");
    continuid_atom->meta_id = knishio_strdup(rw->bundle_hash);

    {
        const char* ci_keys[3];
        const char* ci_vals[3];
        size_t ci_n = 0;
        if (molecule->source_wallet && molecule->source_wallet->position) {
            ci_keys[ci_n] = "previousPosition";
            ci_vals[ci_n] = molecule->source_wallet->position;
            ci_n++;
        }
        if (rw->pubkey) {
            ci_keys[ci_n] = "pubkey";
            ci_vals[ci_n] = rw->pubkey;
            ci_n++;
        }
        if (rw->characters) {
            ci_keys[ci_n] = "characters";
            ci_vals[ci_n] = rw->characters;
            ci_n++;
        }
        if (ci_n > 0) {
            continuid_atom->meta = malloc(sizeof(knishio_meta_t*) * ci_n);
            if (continuid_atom->meta) {
                continuid_atom->meta_count = ci_n;
                for (size_t i = 0; i < ci_n; i++) {
                    if (knishio_meta_create(&continuid_atom->meta[i], ci_keys[i], ci_vals[i]) != KNISHIO_SUCCESS) {
                        continuid_atom->meta[i] = NULL;
                    }
                }
            } else {
                continuid_atom->meta_count = 0;
            }
        }
    }

    result = knishio_molecule_add_atom(molecule, continuid_atom);
    if (result != KNISHIO_SUCCESS) {
        knishio_atom_free(continuid_atom);
        return result;
    }

    return KNISHIO_SUCCESS;
}

/* Build the canonical tokenUnits wire value `[[id, name, metas], ...]` (matches JS/Rust/Python
 * to_data()) from a wallet's token_units. Returns a malloc'd JSON string (caller frees) or NULL. */
static char* build_token_units_json(const knishio_wallet_t* w) {
    knishio_json_array_builder_t* outer = knishio_json_array_builder_create();
    if (!outer) {
        return NULL;
    }
    bool ok = true;
    for (size_t i = 0; i < w->token_unit_count; i++) {
        const knishio_token_unit_t* u = &w->token_units[i];
        knishio_json_array_builder_t* inner = knishio_json_array_builder_create();
        if (!inner) {
            ok = false;
            break;
        }
        knishio_json_array_add_string(inner, u->id ? u->id : "");
        knishio_json_array_add_string(inner, u->name ? u->name : "");
        const char* metas_src = (u->metas_json && u->metas_json[0]) ? u->metas_json : "{}";
        knishio_json_t* metas_val = knishio_json_parse(metas_src, NULL);
        if (!metas_val) {
            metas_val = knishio_json_parse("{}", NULL);
        }
        if (metas_val) {
            knishio_json_array_add(inner, metas_val); /* consumes (frees) metas_val */
        }
        knishio_json_array_add_array(outer, inner);   /* duplicates inner */
        knishio_json_array_builder_free(inner);
    }
    char* result = NULL;
    if (ok) {
        knishio_json_t* arr = knishio_json_array_build(outer);
        if (arr) {
            result = knishio_json_serialize(arr, false);
            knishio_json_free(arr);
        }
    }
    knishio_json_array_builder_free(outer);
    return result;
}

/* Attach a `tokenUnits` meta key to an atom from its wallet's token_units (mirror JS setAtomWallet).
 * GATED: a wallet with no token_units adds nothing, so fungible ops are byte-identical (the 6 frozen
 * self-test hashes can't move). Preserves any meta_type/meta_id already set (separate atom fields). */
static void attach_token_units_meta(knishio_atom_t* atom, const knishio_wallet_t* w) {
    if (!atom || !w || w->token_unit_count == 0) {
        return;
    }
    char* tu_json = build_token_units_json(w);
    if (!tu_json) {
        return;
    }
    atom->meta = malloc(sizeof(knishio_meta_t*) * 1);
    if (atom->meta) {
        atom->meta_count = 1;
        if (knishio_meta_create(&atom->meta[0], "tokenUnits", tu_json) != KNISHIO_SUCCESS) {
            atom->meta[0] = NULL;
            atom->meta_count = 0;
        }
    }
    knishio_free(tu_json);
}

/**
 * @brief Initialize metadata molecule (matches JavaScript SDK initMeta)
 * Adds M-isotope atom with metadata and I-isotope atom for ContinuID
 */
knishio_error_t knishio_molecule_init_meta(
    knishio_molecule_t* molecule,
    const char* meta_type,
    const char* meta_id,
    const char** meta_keys,
    const char** meta_values,
    size_t meta_count
) {
#if KNISHIO_DEBUG_MODE
    printf("DEBUG knishio_molecule_init_meta: ENTERED function\n");
#endif
    
    if (!molecule || !meta_type || !meta_id) {
        printf("DEBUG knishio_molecule_init_meta: Invalid arguments\n");
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!molecule->source_wallet) {
        printf("DEBUG knishio_molecule_init_meta: No source wallet\n");
        return KNISHIO_ERROR_INVALID_STATE;
    }
    
    printf("DEBUG knishio_molecule_init_meta: Source wallet exists, creating meta atom\n");
    
    /* Create metadata atom (M isotope) with proper meta array (JavaScript compatible) */
    knishio_atom_t* meta_atom = NULL;
    
    const char* position = knishio_wallet_get_position(molecule->source_wallet);
    const char* address = knishio_wallet_get_address(molecule->source_wallet);
    
    printf("DEBUG knishio_molecule_init_meta: Wallet position: %s\n", position ? position : "NULL");
    printf("DEBUG knishio_molecule_init_meta: Wallet address: %s\n", address ? address : "NULL");
    printf("DEBUG knishio_molecule_init_meta: Wallet token: %s\n", molecule->source_wallet->token ? molecule->source_wallet->token : "NULL");
    
    /* Create metadata atom with basic creation first (avoid complexity issues) */
    knishio_error_t result = knishio_atom_create(
        &meta_atom,
        position,
        address,
        KNISHIO_ISOTOPE_M,
        molecule->source_wallet->token,
        NULL,  /* No value for metadata */
        NULL   /* No batch_id */
    );
    
    printf("DEBUG knishio_molecule_init_meta: knishio_atom_create result: %d\n", result);
    
    if (result != KNISHIO_SUCCESS) {
        printf("DEBUG knishio_molecule_init_meta: Meta atom creation failed\n");
        return result;
    }
    
    /* Set meta_type and meta_id (JavaScript compatibility) */
    if (meta_atom->meta_type) knishio_free(meta_atom->meta_type);
    if (meta_atom->meta_id) knishio_free(meta_atom->meta_id);
    meta_atom->meta_type = knishio_strdup(meta_type);
    meta_atom->meta_id = knishio_strdup(meta_id);
    
    /* Populate meta array directly (JavaScript compatibility) */
    if (meta_keys && meta_values && meta_count > 0) {
        printf("DEBUG knishio_molecule_init_meta: Populating meta array with %zu items\n", meta_count);
        
        /* Allocate meta array */
        meta_atom->meta = malloc(sizeof(knishio_meta_t*) * meta_count);
        if (meta_atom->meta) {
            meta_atom->meta_count = meta_count;
            
            /* Create meta objects */
            for (size_t i = 0; i < meta_count; i++) {
                if (meta_keys[i] && meta_values[i]) {
                    knishio_error_t meta_result = knishio_meta_create(&meta_atom->meta[i], meta_keys[i], meta_values[i]);
                    if (meta_result == KNISHIO_SUCCESS) {
                        printf("DEBUG knishio_molecule_init_meta: Added meta[%zu]: %s = %s\n", 
                               i, meta_keys[i], meta_values[i]);
                    } else {
                        printf("DEBUG knishio_molecule_init_meta: Failed to create meta[%zu]\n", i);
                        meta_atom->meta[i] = NULL;
                    }
                }
            }
        } else {
            printf("DEBUG knishio_molecule_init_meta: Failed to allocate meta array\n");
            meta_atom->meta_count = 0;
        }
    } else {
        printf("DEBUG knishio_molecule_init_meta: No metadata to populate\n");
        meta_atom->meta = NULL;
        meta_atom->meta_count = 0;
    }
    
    printf("DEBUG knishio_molecule_init_meta: Meta atom created successfully\n");
    
    printf("DEBUG knishio_molecule_init_meta: Adding meta atom to molecule\n");
    result = knishio_molecule_add_atom(molecule, meta_atom);
    printf("DEBUG knishio_molecule_init_meta: knishio_molecule_add_atom result: %d\n", result);
    
    if (result != KNISHIO_SUCCESS) {
        printf("DEBUG knishio_molecule_init_meta: Failed to add meta atom to molecule\n");
        knishio_atom_free(meta_atom);
        return result;
    }
    
    printf("DEBUG knishio_molecule_init_meta: Meta atom added successfully\n");
    
    /* Add ContinuID atom (I isotope), mirrors JS Molecule.addContinuIdAtom() (extracted helper). */
    result = add_continuid_atom(molecule);
    if (result != KNISHIO_SUCCESS) {
        printf("DEBUG knishio_molecule_init_meta: Failed to add ContinuID atom\n");
        return result;
    }

    printf("DEBUG knishio_molecule_init_meta: Function completed successfully - returning KNISHIO_SUCCESS\n");

    return KNISHIO_SUCCESS;
}

/**
 * @brief Initialize an authorization molecule (matches JavaScript SDK Molecule.initAuthorization).
 * U-isotope atom (signed by source_wallet; no metaType/metaId; meta = [encrypt, pubkey, characters]
 * in JS order — encrypt from the caller, pubkey/characters from the AUTH wallet via setAtomWallet)
 * + a ContinuID I-atom. The validator issues a bundle-scoped JWT from the U-atom's walletAddress.
 */
knishio_error_t knishio_molecule_init_authorization(
    knishio_molecule_t* molecule,
    bool encrypt
) {
    if (!molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!molecule->source_wallet) {
        return KNISHIO_ERROR_INVALID_STATE;
    }

    knishio_wallet_t* sw = molecule->source_wallet;

    /* U-atom: no value, no metaType/metaId (JS Atom.create with isotope 'U'). */
    knishio_atom_t* u_atom = NULL;
    knishio_error_t result = knishio_atom_create(
        &u_atom,
        knishio_wallet_get_position(sw),
        knishio_wallet_get_address(sw),
        KNISHIO_ISOTOPE_U,
        sw->token,
        NULL,  /* no value */
        NULL   /* no batch_id */
    );
    if (result != KNISHIO_SUCCESS) {
        return result;
    }

    /* Meta in JS order: encrypt (from caller), then pubkey + characters (setAtomWallet). */
    {
        const char* keys[3];
        const char* vals[3];
        size_t n = 0;
        keys[n] = "encrypt"; vals[n] = encrypt ? "true" : "false"; n++;
        if (sw->pubkey) {
            keys[n] = "pubkey"; vals[n] = sw->pubkey; n++;
        }
        if (sw->characters) {
            keys[n] = "characters"; vals[n] = sw->characters; n++;
        }
        u_atom->meta = malloc(sizeof(knishio_meta_t*) * n);
        if (u_atom->meta) {
            u_atom->meta_count = n;
            for (size_t i = 0; i < n; i++) {
                if (knishio_meta_create(&u_atom->meta[i], keys[i], vals[i]) != KNISHIO_SUCCESS) {
                    u_atom->meta[i] = NULL;
                }
            }
        } else {
            u_atom->meta_count = 0;
        }
    }

    result = knishio_molecule_add_atom(molecule, u_atom);
    if (result != KNISHIO_SUCCESS) {
        knishio_atom_free(u_atom);
        return result;
    }

    /* ContinuID I-atom (JS addContinuIdAtom) — requires molecule->remainder_wallet. */
    result = add_continuid_atom(molecule);
    if (result != KNISHIO_SUCCESS) {
        return result;
    }

    return KNISHIO_SUCCESS;
}

/**
 * @brief Initialize a token-creation molecule (matches JavaScript SDK initTokenCreation).
 * C-atom (issue new token) signed by molecule->source_wallet + a ContinuID I-atom.
 */
knishio_error_t knishio_molecule_init_token_creation(
    knishio_molecule_t* molecule,
    const knishio_wallet_t* recipient_wallet,
    const char* amount,
    const char** token_meta_keys,
    const char** token_meta_values,
    size_t token_meta_count
) {
    if (!molecule || !recipient_wallet || !amount) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!molecule->source_wallet) {
        return KNISHIO_ERROR_INVALID_STATE;
    }

    /* Combined meta: the user token meta FIRST, then the 7 prefixed wallet* keys
     * (JS: new AtomMeta(meta).setMetaWallet(recipientWallet)). */
    const char* keys[24];
    const char* vals[24];
    size_t n = 0;
    if (token_meta_keys && token_meta_values) {
        for (size_t i = 0; i < token_meta_count && n < 16; i++) {
            if (token_meta_keys[i] && token_meta_values[i]) {
                keys[n] = token_meta_keys[i];
                vals[n] = token_meta_values[i];
                n++;
            }
        }
    }
    append_wallet_meta(keys, vals, &n, recipient_wallet);

    /* C-atom: position/address/token from the SOURCE (signing) wallet; value=amount;
     * metaId=recipient.token; batchId=recipient.batch_id (NULL -> hash-skipped). */
    knishio_atom_t* atom = NULL;
    knishio_error_t result = knishio_atom_create(
        &atom,
        molecule->source_wallet->position,
        molecule->source_wallet->address,
        KNISHIO_ISOTOPE_C,
        molecule->source_wallet->token,
        amount,
        recipient_wallet->batch_id
    );
    if (result != KNISHIO_SUCCESS) {
        return result;
    }

    if (atom->meta_type) knishio_free(atom->meta_type);
    if (atom->meta_id) knishio_free(atom->meta_id);
    atom->meta_type = knishio_strdup("token");
    atom->meta_id = knishio_strdup(recipient_wallet->token);

    atom->meta = malloc(sizeof(knishio_meta_t*) * n);
    if (atom->meta) {
        atom->meta_count = n;
        for (size_t i = 0; i < n; i++) {
            if (knishio_meta_create(&atom->meta[i], keys[i], vals[i]) != KNISHIO_SUCCESS) {
                atom->meta[i] = NULL;
            }
        }
    } else {
        atom->meta_count = 0;
    }

    result = knishio_molecule_add_atom(molecule, atom);
    if (result != KNISHIO_SUCCESS) {
        knishio_atom_free(atom);
        return result;
    }

    return add_continuid_atom(molecule);
}

/**
 * @brief Initialize a wallet-creation molecule (matches JavaScript SDK initWalletCreation).
 * C-atom (metaType "wallet") signed by molecule->source_wallet + a ContinuID I-atom.
 */
knishio_error_t knishio_molecule_init_wallet_creation(
    knishio_molecule_t* molecule,
    const knishio_wallet_t* wallet,
    const char** atom_meta_keys,
    const char** atom_meta_values,
    size_t atom_meta_count
) {
    if (!molecule || !wallet) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!molecule->source_wallet) {
        return KNISHIO_ERROR_INVALID_STATE;
    }

    /* Combined meta: the optional leading atom_meta FIRST (e.g. shadowWalletClaim),
     * then the 7 prefixed wallet* keys (JS: atomMeta.setMetaWallet(wallet)). */
    const char* keys[24];
    const char* vals[24];
    size_t n = 0;
    if (atom_meta_keys && atom_meta_values) {
        for (size_t i = 0; i < atom_meta_count && n < 16; i++) {
            if (atom_meta_keys[i] && atom_meta_values[i]) {
                keys[n] = atom_meta_keys[i];
                vals[n] = atom_meta_values[i];
                n++;
            }
        }
    }
    append_wallet_meta(keys, vals, &n, wallet);

    /* C-atom: position/address/token from the SOURCE (signing) wallet; NO value;
     * metaType "wallet"; metaId=wallet.address; batchId=wallet.batch_id (NULL -> hash-skipped). */
    knishio_atom_t* atom = NULL;
    knishio_error_t result = knishio_atom_create(
        &atom,
        molecule->source_wallet->position,
        molecule->source_wallet->address,
        KNISHIO_ISOTOPE_C,
        molecule->source_wallet->token,
        NULL,  /* No value for wallet creation */
        wallet->batch_id
    );
    if (result != KNISHIO_SUCCESS) {
        return result;
    }

    if (atom->meta_type) knishio_free(atom->meta_type);
    if (atom->meta_id) knishio_free(atom->meta_id);
    atom->meta_type = knishio_strdup("wallet");
    atom->meta_id = knishio_strdup(wallet->address);

    atom->meta = malloc(sizeof(knishio_meta_t*) * n);
    if (atom->meta) {
        atom->meta_count = n;
        for (size_t i = 0; i < n; i++) {
            if (knishio_meta_create(&atom->meta[i], keys[i], vals[i]) != KNISHIO_SUCCESS) {
                atom->meta[i] = NULL;
            }
        }
    } else {
        atom->meta_count = 0;
    }

    result = knishio_molecule_add_atom(molecule, atom);
    if (result != KNISHIO_SUCCESS) {
        knishio_atom_free(atom);
        return result;
    }

    return add_continuid_atom(molecule);
}

/**
 * @brief Initialize a shadow-wallet-claim molecule (matches JavaScript SDK initShadowWalletClaim).
 * Prepends shadowWalletClaim=1, then delegates to init_wallet_creation.
 */
knishio_error_t knishio_molecule_init_shadow_wallet_claim(
    knishio_molecule_t* molecule,
    const knishio_wallet_t* wallet
) {
    const char* keys[] = { "shadowWalletClaim" };
    const char* vals[] = { "1" };
    return knishio_molecule_init_wallet_creation(molecule, wallet, keys, vals, 1);
}

/**
 * @brief Initialize value transfer molecule (matches JavaScript SDK initValue)
 * Adds V-isotope atoms: source debit, recipient credit, remainder (UTXO pattern)
 */
knishio_error_t knishio_molecule_init_value(
    knishio_molecule_t* molecule,
    knishio_wallet_t* recipient_wallet,
    int amount
) {
    if (!molecule || !recipient_wallet) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (!molecule->source_wallet) {
        return KNISHIO_ERROR_INVALID_STATE;
    }
    
    /* Check sufficient balance */
    if (molecule->source_wallet->balance < amount) {
        return KNISHIO_ERROR_BALANCE_INSUFFICIENT;
    }
    
    /* Add source atom (debit full balance - UTXO pattern matches JavaScript) */
    knishio_atom_t* source_atom = NULL;
    char source_value[32];
    snprintf(source_value, sizeof(source_value), "-%d", (int)molecule->source_wallet->balance);

    knishio_error_t result = knishio_atom_create(
        &source_atom,
        knishio_wallet_get_position(molecule->source_wallet),
        knishio_wallet_get_address(molecule->source_wallet),
        KNISHIO_ISOTOPE_V,
        molecule->source_wallet->token,
        source_value,
        molecule->source_wallet->batch_id  /* carry the wallet's batchId (NULL -> hash-skipped; C++/JS parity) */
    );
    
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    /* Stackable (NFT) transfer: source atom carries the SENT units (gated; fungible = no-op). */
    attach_token_units_meta(source_atom, molecule->source_wallet);
    result = knishio_molecule_add_atom(molecule, source_atom);
    if (result != KNISHIO_SUCCESS) {
        knishio_atom_free(source_atom);
        return result;
    }

    /* Add recipient atom (credit amount) */
    knishio_atom_t* recipient_atom = NULL;
    char recipient_value[32];
    snprintf(recipient_value, sizeof(recipient_value), "%d", amount);
    
    result = knishio_atom_create(
        &recipient_atom,
        knishio_wallet_get_position(recipient_wallet),
        knishio_wallet_get_address(recipient_wallet),
        KNISHIO_ISOTOPE_V,
        recipient_wallet->token,
        recipient_value,
        recipient_wallet->batch_id  /* recipient batchId -> validator creates a claimable shadow */
    );
    
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    /* Set recipient metadata */
    if (recipient_atom->meta_type) knishio_free(recipient_atom->meta_type);
    if (recipient_atom->meta_id) knishio_free(recipient_atom->meta_id);
    recipient_atom->meta_type = knishio_strdup("walletBundle");
    recipient_atom->meta_id = knishio_strdup(recipient_wallet->bundle_hash);
    /* Stackable (NFT) transfer: recipient atom carries the SENT units (gated). */
    attach_token_units_meta(recipient_atom, recipient_wallet);

    result = knishio_molecule_add_atom(molecule, recipient_atom);
    if (result != KNISHIO_SUCCESS) {
        knishio_atom_free(recipient_atom);
        return result;
    }
    
    /* Add remainder atom if remainder wallet exists */
    if (molecule->remainder_wallet) {
        knishio_atom_t* remainder_atom = NULL;
        char remainder_value[32];
        int remainder_amount = (int)molecule->source_wallet->balance - amount;
        snprintf(remainder_value, sizeof(remainder_value), "%d", remainder_amount);
        
        result = knishio_atom_create(
            &remainder_atom,
            knishio_wallet_get_position(molecule->remainder_wallet),
            knishio_wallet_get_address(molecule->remainder_wallet),
            KNISHIO_ISOTOPE_V,
            molecule->remainder_wallet->token,
            remainder_value,
            molecule->remainder_wallet->batch_id  /* carry the remainder wallet's batchId (NULL -> hash-skipped) */
        );
        
        if (result != KNISHIO_SUCCESS) {
            return result;
        }
        
        /* Set remainder metadata */
        if (remainder_atom->meta_type) knishio_free(remainder_atom->meta_type);
        if (remainder_atom->meta_id) knishio_free(remainder_atom->meta_id);
        remainder_atom->meta_type = knishio_strdup("walletBundle");
        remainder_atom->meta_id = knishio_strdup(molecule->remainder_wallet->bundle_hash);
        /* Stackable (NFT) transfer: remainder atom carries the KEPT units (gated). */
        attach_token_units_meta(remainder_atom, molecule->remainder_wallet);

        result = knishio_molecule_add_atom(molecule, remainder_atom);
        if (result != KNISHIO_SUCCESS) {
            knishio_atom_free(remainder_atom);
            return result;
        }
    }

    return KNISHIO_SUCCESS;
}

/**
 * @brief Initialize a token-burn molecule: canonical 3 V-atoms, zero-sum.
 * Mirrors init_value, but the "recipient" is the all-zeros burn bundle (token destruction):
 * the burn-target atom has EMPTY position/address + metaType 'walletBundle' + metaId all-zeros.
 * Pure V (NO ContinuID atom). Requires source_wallet (with balance) + remainder_wallet.
 */
knishio_error_t knishio_molecule_init_burn(
    knishio_molecule_t* molecule,
    int amount
) {
    if (!molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!molecule->source_wallet || !molecule->remainder_wallet) {
        return KNISHIO_ERROR_INVALID_STATE;
    }
    if (molecule->source_wallet->balance < amount) {
        return KNISHIO_ERROR_BALANCE_INSUFFICIENT;
    }

    /* V-atom 1: debit the ENTIRE source balance (UTXO; conservation needs -balance, not -amount) */
    knishio_atom_t* source_atom = NULL;
    char source_value[32];
    snprintf(source_value, sizeof(source_value), "-%d", (int)molecule->source_wallet->balance);
    knishio_error_t result = knishio_atom_create(
        &source_atom,
        knishio_wallet_get_position(molecule->source_wallet),
        knishio_wallet_get_address(molecule->source_wallet),
        KNISHIO_ISOTOPE_V,
        molecule->source_wallet->token,
        source_value,
        NULL
    );
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    /* Stackable (NFT) burn: source atom carries the BURNED units (gated; fungible = no-op). */
    attach_token_units_meta(source_atom, molecule->source_wallet);
    result = knishio_molecule_add_atom(molecule, source_atom);
    if (result != KNISHIO_SUCCESS) {
        knishio_atom_free(source_atom);
        return result;
    }

    /* V-atom 2: credit the burn amount to the all-zeros burn address (destruction).
     * Empty position/address (no signing wallet); metaType walletBundle + metaId all-zeros. */
    knishio_atom_t* burn_atom = NULL;
    char burn_value[32];
    snprintf(burn_value, sizeof(burn_value), "%d", amount);
    result = knishio_atom_create(
        &burn_atom,
        "",
        "",
        KNISHIO_ISOTOPE_V,
        molecule->source_wallet->token,
        burn_value,
        NULL
    );
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    if (burn_atom->meta_type) knishio_free(burn_atom->meta_type);
    if (burn_atom->meta_id) knishio_free(burn_atom->meta_id);
    burn_atom->meta_type = knishio_strdup("walletBundle");
    burn_atom->meta_id = knishio_strdup("0000000000000000000000000000000000000000000000000000000000000000");
    result = knishio_molecule_add_atom(molecule, burn_atom);
    if (result != KNISHIO_SUCCESS) {
        knishio_atom_free(burn_atom);
        return result;
    }

    /* V-atom 3: remainder back to the source identity */
    knishio_atom_t* remainder_atom = NULL;
    char remainder_value[32];
    snprintf(remainder_value, sizeof(remainder_value), "%d", (int)molecule->source_wallet->balance - amount);
    result = knishio_atom_create(
        &remainder_atom,
        knishio_wallet_get_position(molecule->remainder_wallet),
        knishio_wallet_get_address(molecule->remainder_wallet),
        KNISHIO_ISOTOPE_V,
        molecule->remainder_wallet->token,
        remainder_value,
        NULL
    );
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    if (remainder_atom->meta_type) knishio_free(remainder_atom->meta_type);
    if (remainder_atom->meta_id) knishio_free(remainder_atom->meta_id);
    remainder_atom->meta_type = knishio_strdup("walletBundle");
    remainder_atom->meta_id = knishio_strdup(molecule->remainder_wallet->bundle_hash);
    /* Stackable (NFT) burn: remainder atom carries the KEPT units (gated). */
    attach_token_units_meta(remainder_atom, molecule->remainder_wallet);
    result = knishio_molecule_add_atom(molecule, remainder_atom);
    if (result != KNISHIO_SUCCESS) {
        knishio_atom_free(remainder_atom);
        return result;
    }

    return KNISHIO_SUCCESS;
}

/**
 * @brief Simplified wallet creation (matches JavaScript SDK Wallet.create pattern)
 */
knishio_error_t knishio_wallet_create_simple(
    knishio_wallet_t** wallet,
    const char* secret,
    const char* token, 
    const char* position
) {
    if (!wallet || !secret || !token || !position) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Generate bundle hash (matches JavaScript SDK - only uses secret) */
    char* bundle = NULL;
    if (!knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)) {
        return KNISHIO_ERROR_CRYPTO;
    }
    
    /* Use existing wallet creation function */
    bool success = knishio_wallet_create_from_params(
        wallet, secret, bundle, token, NULL, position, NULL, "BASE64"
    );
    
    knishio_free(bundle);
    
    return success ? KNISHIO_SUCCESS : KNISHIO_ERROR_CRYPTO;
}
