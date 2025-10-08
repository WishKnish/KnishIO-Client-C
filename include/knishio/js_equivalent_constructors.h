#ifndef KNISHIO_JS_EQUIVALENT_CONSTRUCTORS_H
#define KNISHIO_JS_EQUIVALENT_CONSTRUCTORS_H

/**
 * @file js_equivalent_constructors.h
 * @brief JavaScript SDK equivalent constructor functions
 * 
 * This header provides constructor functions that accept configuration structs,
 * providing true architectural equivalency with the JavaScript SDK's object
 * constructor patterns like new Wallet({...}), new Molecule({...}), etc.
 */

#include "knishio/knishio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create wallet from config struct (JavaScript SDK: new Wallet({...}))
 * 
 * Equivalent to:
 * const wallet = new Wallet({
 *   secret: config.secret,
 *   bundle: config.bundle,
 *   token: config.token,
 *   address: config.address,
 *   position: config.position,
 *   batchId: config.batch_id,
 *   characters: config.characters
 * })
 * 
 * @param wallet Output wallet pointer
 * @param config Wallet configuration (all fields optional except secret and token)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_wallet_create_from_config(
    knishio_wallet_t** wallet,
    const knishio_wallet_config_t* config
);

/**
 * @brief Create molecule from config struct (JavaScript SDK: new Molecule({...}))
 * 
 * Equivalent to:
 * const molecule = new Molecule({
 *   secret: config.secret,
 *   bundle: config.bundle, 
 *   sourceWallet: config.source_wallet,
 *   remainderWallet: config.remainder_wallet,
 *   cellSlug: config.cell_slug,
 *   version: config.version
 * })
 * 
 * @param molecule Output molecule pointer
 * @param config Molecule configuration (all fields optional except secret)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_create_from_config(
    knishio_molecule_t** molecule,
    const knishio_molecule_config_t* config
);

/**
 * @brief Create atom from config struct (JavaScript SDK: new Atom({...}))
 * 
 * Equivalent to:
 * const atom = new Atom({
 *   position: config.position,
 *   walletAddress: config.wallet_address,
 *   isotope: config.isotope,
 *   token: config.token,
 *   value: config.value,
 *   batchId: config.batch_id,
 *   metaType: config.meta_type,
 *   metaId: config.meta_id,
 *   meta: config.meta,
 *   index: auto_generated
 * })
 * 
 * @param atom Output atom pointer
 * @param config Atom configuration
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_create_from_config(
    knishio_atom_t** atom,
    const knishio_atom_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_JS_EQUIVALENT_CONSTRUCTORS_H */