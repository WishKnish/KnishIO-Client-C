#ifndef KNISHIO_ATOM_INTERNAL_H
#define KNISHIO_ATOM_INTERNAL_H

/**
 * @file atom_internal.h
 * @brief Atom JSON parsing shared by knishio_atom_from_json(), knishio_molecule_from_json() and
 *        knishio_verify_from_server_data()
 *
 * Not part of the public API: the prototype exposes cJSON, which the public headers do not.
 */

/* Include cJSON properly */
#ifdef __has_include
  #if __has_include(<cjson/cJSON.h>)
    #include <cjson/cJSON.h>
  #elif __has_include(<cJSON.h>)
    #include <cJSON.h>
  #endif
#else
  #include <cjson/cJSON.h>
#endif

#include "knishio/atom.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read an optional string member: a string gives *out (borrowed from json), null or
 *        absent gives NULL.
 * @return false if the member exists with any other JSON type (malformed input)
 */
bool knishio_cjson_optional_string(const cJSON *json, const char *key, const char **out);

/**
 * @brief Parse a wire timestamp: 1-18 decimal digits of milliseconds (fits int64 unchecked).
 * @return false for anything else
 */
bool knishio_cjson_millis(const char *text, long long *ms);

/**
 * @brief Format a JSON number the way JavaScript's String(n) does (shortest round-trip digits;
 *        exponent form below 1e-6 and from 1e21).
 * @param value Finite number
 * @param out Output buffer (32 bytes is enough)
 * @param size Size of out
 * @return false for a non-finite value or a buffer that is too small
 */
bool knishio_json_number_to_string(double value, char *out, size_t size);

/** @brief The createdAt an atom hashes, in milliseconds. */
static inline long long knishio_atom_created_at_ms(const knishio_atom_t *atom) {
    return atom->created_at_ms > 0 ? (long long)atom->created_at_ms : (long long)atom->created_at * 1000LL;
}

/**
 * @brief Build an atom from one parsed atom JSON object (the inverse of knishio_atom_to_json)
 *
 * Field rules are documented at knishio_molecule_from_json() in include/knishio/molecule.h.
 * On failure *atom is NULL and every partial allocation has been released; on success the
 * caller owns the atom and its meta (release with knishio_atom_free_deep()).
 *
 * @param json Atom object
 * @param atom Receives the atom
 * @return KNISHIO_SUCCESS, KNISHIO_ERROR_INVALID_ARGS (NULL argument), KNISHIO_ERROR_INVALID_JSON
 *         (wrong shape or unrepresentable value) or KNISHIO_ERROR_MEMORY
 */
knishio_error_t knishio_atom_from_cjson(const cJSON *json, knishio_atom_t **atom);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_ATOM_INTERNAL_H */
