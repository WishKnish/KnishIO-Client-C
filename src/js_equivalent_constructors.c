/**
 * @file js_equivalent_constructors.c
 * @brief Implementation of JavaScript SDK equivalent constructor functions
 * 
 * Provides constructor functions that accept configuration structs,
 * bridging the gap between JS SDK object constructors and C SDK functions.
 */

#include "knishio/js_equivalent_constructors.h"
#include "knishio/wallet.h"
#include "knishio/molecule.h"
#include "knishio/atom.h"
#include <string.h>

/**
 * Create wallet from config struct - TRUE JavaScript SDK equivalent
 */
knishio_error_t knishio_wallet_create_from_config(
    knishio_wallet_t** wallet,
    const knishio_wallet_config_t* config
) {
    if (!wallet || !config) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    // Call the full parameter function that exists in the SDK
    bool success = knishio_wallet_create_from_params(
        wallet,
        config->secret,      // secret
        config->bundle,      // bundle  
        config->token,       // token
        NULL,                // address (not in config - auto-generated)
        config->position,    // position
        config->batch_id,    // batch_id
        config->characters   // characters
    );
    
    return success ? KNISHIO_SUCCESS : KNISHIO_ERROR_WALLET_CREDENTIAL;
}

/**
 * Create molecule from config struct - TRUE JavaScript SDK equivalent  
 */
knishio_error_t knishio_molecule_create_from_config(
    knishio_molecule_t** molecule,
    const knishio_molecule_config_t* config
) {
    if (!molecule || !config) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    // Call the existing molecule create function with all parameters
    return knishio_molecule_create(
        molecule,
        config->secret,           // secret
        config->bundle,           // bundle
        config->source_wallet,    // source_wallet
        config->remainder_wallet, // remainder_wallet
        config->cell_slug,        // cell_slug
        config->version           // version
    );
}

/**
 * Create atom from config struct - TRUE JavaScript SDK equivalent
 */
knishio_error_t knishio_atom_create_from_config(
    knishio_atom_t** atom,
    const knishio_atom_config_t* config
) {
    if (!atom || !config) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    // Convert string isotope to enum if needed
    knishio_isotope_t isotope;
    if (strcmp(config->isotope, "C") == 0) {
        isotope = KNISHIO_ISOTOPE_C;
    } else if (strcmp(config->isotope, "V") == 0) {
        isotope = KNISHIO_ISOTOPE_V;
    } else if (strcmp(config->isotope, "M") == 0) {
        isotope = KNISHIO_ISOTOPE_M;
    } else if (strcmp(config->isotope, "U") == 0) {
        isotope = KNISHIO_ISOTOPE_U;
    } else if (strcmp(config->isotope, "I") == 0) {
        isotope = KNISHIO_ISOTOPE_I;
    } else {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    // Check if metadata is provided
    if (config->meta_type || config->meta_id || (config->meta && config->meta_count > 0)) {
        // Use the full metadata-enabled constructor
        return knishio_atom_create_with_meta(
            atom,
            config->position,       // position
            config->wallet_address, // wallet_address
            isotope,               // isotope
            config->token,         // token
            config->value,         // value
            config->batch_id,      // batch_id
            config->meta_type,     // meta_type
            config->meta_id,       // meta_id
            (knishio_meta_t**)config->meta, // meta (cast needed)
            config->meta_count     // meta_count
        );
    } else {
        // Use the basic constructor without metadata
        return knishio_atom_create(
            atom,
            config->position,       // position
            config->wallet_address, // wallet_address
            isotope,               // isotope
            config->token,         // token
            config->value,         // value
            config->batch_id       // batch_id
        );
    }
}