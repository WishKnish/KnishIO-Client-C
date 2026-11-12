#ifndef KNISHIO_JSON_SERIALIZERS_H
#define KNISHIO_JSON_SERIALIZERS_H

/**
 * @file serializers.h
 * @brief Type-safe JSON serialization for KnishIO data structures
 * 
 * Provides serialization and deserialization functions for all KnishIO SDK
 * data structures, ensuring compatibility with the JavaScript SDK JSON format.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "knishio/json/parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Include error definitions */
#include "knishio/error/context.h"

/* Forward declarations */
typedef struct knishio_atom knishio_atom_t;
typedef struct knishio_molecule knishio_molecule_t;
typedef struct knishio_wallet knishio_wallet_t;
typedef struct knishio_meta knishio_meta_t;

/* Atom serialization */

/**
 * @brief Convert atom to JSON object
 * @param atom Source atom
 * @return JSON object (caller must free) or NULL on error
 */
knishio_json_t* knishio_atom_to_json_obj(const knishio_atom_t* atom);

/**
 * @brief Create atom from JSON object
 *
 * Parses with knishio_atom_from_json(), the inverse of knishio_atom_to_json(): every field that
 * function emits is read, and an atom it would reject is rejected here.
 * @param json JSON object containing atom data
 * @param atom Output atom, NULL on failure; release it with knishio_atom_free_deep()
 * @return KNISHIO_SUCCESS on success, KNISHIO_ERROR_INVALID_ARGS if json is not an object,
 *         otherwise knishio_atom_from_json()'s error
 */
knishio_error_t knishio_atom_from_json_obj(const knishio_json_t* json, knishio_atom_t** atom);

/**
 * @brief Convert atom to JSON string
 * @param atom Source atom
 * @param json_string Output JSON string (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_to_json_string(const knishio_atom_t* atom, char** json_string);

/**
 * @brief Create atom from JSON string
 *
 * Equivalent to knishio_atom_from_json().
 * @param json_string JSON string containing atom data
 * @param atom Output atom, NULL on failure; release it with knishio_atom_free_deep()
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_from_json_string(const char* json_string, knishio_atom_t** atom);

/* Meta serialization */

/**
 * @brief Convert meta object to JSON
 * @param meta Source meta object
 * @return JSON object (caller must free) or NULL on error
 */
knishio_json_t* knishio_meta_to_json_obj(const knishio_meta_t* meta);

/**
 * @brief Create meta object from JSON
 * @param json JSON object containing meta data
 * @param meta Output meta object (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_meta_from_json_obj(const knishio_json_t* json, knishio_meta_t** meta);

/* Wallet serialization */

/**
 * @brief Convert wallet to JSON object
 * @param wallet Source wallet
 * @return JSON object (caller must free) or NULL on error
 */
knishio_json_t* knishio_wallet_to_json_obj(const knishio_wallet_t* wallet);

/**
 * @brief Create wallet from JSON object
 * @param json JSON object containing wallet data
 * @param wallet Output wallet (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_wallet_from_json_obj(const knishio_json_t* json, knishio_wallet_t** wallet);

/* Molecule serialization */

/**
 * @brief Convert molecule to JSON object
 * @param molecule Source molecule
 * @return JSON object (caller must free) or NULL on error
 */
knishio_json_t* knishio_molecule_to_json_obj(const knishio_molecule_t* molecule);

/**
 * @brief Create molecule from JSON object
 *
 * Parses with knishio_molecule_from_json(), the inverse of knishio_molecule_to_json().
 * @param json JSON object containing molecule data
 * @param molecule Output molecule, NULL on failure; it owns its atoms and their meta, so
 *        release it with knishio_molecule_free_deep()
 * @return KNISHIO_SUCCESS on success, KNISHIO_ERROR_INVALID_ARGS if json is not an object,
 *         otherwise knishio_molecule_from_json()'s error
 */
knishio_error_t knishio_molecule_from_json_obj(const knishio_json_t* json, knishio_molecule_t** molecule);

/* GraphQL response handling */

/**
 * @brief Extract data field from GraphQL response
 * @param response GraphQL response JSON
 * @return Data JSON object (caller must free) or NULL if not found
 */
knishio_json_t* knishio_json_extract_data(const knishio_json_t* response);

/**
 * @brief Extract errors field from GraphQL response
 * @param response GraphQL response JSON
 * @return Errors array (caller must free) or NULL if not found
 */
knishio_json_t* knishio_json_extract_errors(const knishio_json_t* response);

/**
 * @brief Check if GraphQL response has errors
 * @param response GraphQL response JSON
 * @return True if response contains errors, false otherwise
 */
bool knishio_json_has_errors(const knishio_json_t* response);

/**
 * @brief Get first error message from GraphQL response
 * @param response GraphQL response JSON
 * @return Error message string (caller must free) or NULL if no errors
 */
char* knishio_json_get_first_error_message(const knishio_json_t* response);

/* Array helper functions */

/**
 * @brief Create JSON array from atom array
 * @param atoms Array of atom pointers
 * @param count Number of atoms
 * @return JSON array (caller must free) or NULL on error
 */
knishio_json_t* knishio_json_create_atom_array(knishio_atom_t** atoms, size_t count);

/**
 * @brief Parse JSON array into atom array
 *
 * Each element is parsed with knishio_atom_from_json_obj(). The array is all or nothing: if any
 * element is missing or fails to parse, that element's error is returned
 * (KNISHIO_ERROR_INVALID_JSON for a missing element). On every failure, argument errors
 * included, *atoms is NULL and *count is 0.
 * @param json JSON array containing atoms
 * @param atoms Output atom array; release each atom with knishio_atom_free_deep(), then the
 *        array with knishio_free()
 * @param count Output atom count
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_json_parse_atom_array(const knishio_json_t* json, knishio_atom_t*** atoms, size_t* count);

/* GraphQL query/mutation building */

/* Validation helpers */

/* Utility functions for specific KnishIO types */

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_JSON_SERIALIZERS_H */
