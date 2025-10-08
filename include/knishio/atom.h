#ifndef KNISHIO_ATOM_H
#define KNISHIO_ATOM_H

/**
 * @file atom.h
 * @brief Atom operations for KnishIO SDK
 * 
 * Implements JavaScript-compatible atomic operations for post-blockchain DLT.
 * Atoms are smallest transactional units performing single, monodirectional actions.
 * Features 2025 C17 best practices with static assertions and type safety.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "knishio/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_atom knishio_atom_t;
typedef struct knishio_meta knishio_meta_t;

/* Constants */
#define KNISHIO_POSITION_LENGTH 64        /**< Position length in hex characters */
#define KNISHIO_ADDRESS_LENGTH 64         /**< Wallet address length in hex characters */
#define KNISHIO_BATCH_ID_LENGTH 64        /**< Batch ID length */
#define KNISHIO_OTS_FRAGMENT_LENGTH 512   /**< OTS fragment length */
#define KNISHIO_MAX_TOKEN_NAME_LENGTH 64  /**< Maximum token name length */
#define KNISHIO_MAX_META_TYPE_LENGTH 64   /**< Maximum meta type length */
#define KNISHIO_MAX_META_ID_LENGTH 128    /**< Maximum meta ID length */

/* Isotope types enumeration - based on JS SDK */
typedef enum {
    KNISHIO_ISOTOPE_UNKNOWN = 0,  /**< Unknown isotope */
    KNISHIO_ISOTOPE_V,            /**< Value/transfer isotope */
    KNISHIO_ISOTOPE_C,            /**< Create isotope (wallet/token creation) */
    KNISHIO_ISOTOPE_M,            /**< Meta isotope (metadata operations) */
    KNISHIO_ISOTOPE_U,            /**< User isotope */
    KNISHIO_ISOTOPE_I,            /**< Identity isotope */
    KNISHIO_ISOTOPE_R,            /**< Remainder isotope */
    KNISHIO_ISOTOPE_T,            /**< Token isotope */
    KNISHIO_ISOTOPE_L,            /**< Link isotope */
    KNISHIO_ISOTOPE_S,            /**< Shadow isotope */
    KNISHIO_ISOTOPE_F             /**< Fusion isotope */
} knishio_isotope_t;

/* Atom structure with C17 static assertions */
struct knishio_atom {
    /* Core identifying properties */
    char* position;              /**< 64-char hex wallet position */
    char* wallet_address;        /**< 64-char hex wallet address */
    knishio_isotope_t isotope;   /**< Isotope type */
    char* token;                 /**< Token identifier */
    char* value;                 /**< Transaction value (as string) */
    char* batch_id;              /**< Batch identifier */
    
    /* Metadata properties */
    char* meta_type;             /**< Metadata type */
    char* meta_id;               /**< Metadata identifier */
    knishio_meta_t** meta;       /**< Array of metadata objects */
    size_t meta_count;           /**< Number of metadata objects */
    
    /* Cryptographic properties */
    char* ots_fragment;          /**< One-time signature fragment */
    
    /* System properties */
    int index;                   /**< Atom index within molecule */
    char* version;               /**< Protocol version */
    time_t created_at;           /**< Creation timestamp */
    
    /* Internal state */
    bool is_validated;           /**< Whether atom has been validated */
};

/* Static assertions for C17 safety */
_Static_assert(sizeof(knishio_isotope_t) <= sizeof(int),
    "Isotope enum must fit in int");
_Static_assert(KNISHIO_POSITION_LENGTH == 64,
    "Position length must match JS SDK constant");
_Static_assert(KNISHIO_ADDRESS_LENGTH == 64,
    "Address length must match JS SDK constant");

/* Atom lifecycle */

/**
 * @brief Create a new atom
 * @param atom Output atom pointer
 * @param position 64-char hex wallet position
 * @param wallet_address 64-char hex wallet address
 * @param isotope Isotope type
 * @param token Token identifier
 * @param value Transaction value (can be NULL)
 * @param batch_id Batch identifier (can be NULL)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_create(
    knishio_atom_t** atom,
    const char* position,
    const char* wallet_address,
    knishio_isotope_t isotope,
    const char* token,
    const char* value,
    const char* batch_id
);

/**
 * @brief Create atom with metadata
 * @param atom Output atom pointer
 * @param position 64-char hex wallet position
 * @param wallet_address 64-char hex wallet address
 * @param isotope Isotope type
 * @param token Token identifier
 * @param value Transaction value (can be NULL)
 * @param batch_id Batch identifier (can be NULL)
 * @param meta_type Metadata type (can be NULL)
 * @param meta_id Metadata identifier (can be NULL)
 * @param meta Array of metadata objects (can be NULL)
 * @param meta_count Number of metadata objects
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_create_with_meta(
    knishio_atom_t** atom,
    const char* position,
    const char* wallet_address,
    knishio_isotope_t isotope,
    const char* token,
    const char* value,
    const char* batch_id,
    const char* meta_type,
    const char* meta_id,
    knishio_meta_t** meta,
    size_t meta_count
);

/**
 * @brief Free atom and all associated resources
 * @param atom Atom to free
 */
void knishio_atom_free(knishio_atom_t* atom);

/* Atom properties */

/**
 * @brief Get atom position
 * @param atom Source atom
 * @return Position string or NULL
 */
const char* knishio_atom_get_position(const knishio_atom_t* atom);

/**
 * @brief Get atom wallet address
 * @param atom Source atom
 * @return Wallet address string or NULL
 */
const char* knishio_atom_get_wallet_address(const knishio_atom_t* atom);

/**
 * @brief Get atom isotope
 * @param atom Source atom
 * @return Isotope type
 */
knishio_isotope_t knishio_atom_get_isotope(const knishio_atom_t* atom);

/**
 * @brief Get atom isotope as string
 * @param atom Source atom
 * @return Isotope string ("V", "C", "M", etc.) or NULL
 */
const char* knishio_atom_get_isotope_string(const knishio_atom_t* atom);

/**
 * @brief Get atom token
 * @param atom Source atom
 * @return Token string or NULL
 */
const char* knishio_atom_get_token(const knishio_atom_t* atom);

/**
 * @brief Get atom value
 * @param atom Source atom
 * @return Value string or NULL
 */
const char* knishio_atom_get_value(const knishio_atom_t* atom);

/**
 * @brief Get atom batch ID
 * @param atom Source atom
 * @return Batch ID string or NULL
 */
const char* knishio_atom_get_batch_id(const knishio_atom_t* atom);

/**
 * @brief Get atom index
 * @param atom Source atom
 * @return Atom index within molecule
 */
int knishio_atom_get_index(const knishio_atom_t* atom);

/* Atom setters */

/**
 * @brief Set atom index
 * @param atom Target atom
 * @param index New index value
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_set_index(knishio_atom_t* atom, int index);

/**
 * @brief Set atom version
 * @param atom Target atom
 * @param version Version string
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_set_version(knishio_atom_t* atom, const char* version);

/**
 * @brief Set OTS fragment
 * @param atom Target atom
 * @param ots_fragment OTS fragment string
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_set_ots_fragment(knishio_atom_t* atom, const char* ots_fragment);

/**
 * @brief Get OTS fragment from atom
 * @param atom Source atom
 * @return OTS fragment string or NULL
 */
const char* knishio_atom_get_ots_fragment(const knishio_atom_t* atom);

/* Metadata management */

/**
 * @brief Add metadata to atom
 * @param atom Target atom
 * @param meta Metadata object to add
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_add_meta(knishio_atom_t* atom, knishio_meta_t* meta);

/**
 * @brief Get metadata count
 * @param atom Source atom
 * @return Number of metadata objects
 */
size_t knishio_atom_get_meta_count(const knishio_atom_t* atom);

/**
 * @brief Get metadata by index
 * @param atom Source atom
 * @param index Metadata index
 * @return Metadata object or NULL if index invalid
 */
knishio_meta_t* knishio_atom_get_meta(const knishio_atom_t* atom, size_t index);

/* Validation */

/**
 * @brief Validate atom structure and properties
 * @param atom Atom to validate
 * @return KNISHIO_SUCCESS if valid, error code if invalid
 */
knishio_error_t knishio_atom_validate(knishio_atom_t* atom);

/**
 * @brief Check if atom is valid for specific isotope rules
 * @param atom Atom to check
 * @param isotope Expected isotope type
 * @return True if valid for isotope, false otherwise
 */
bool knishio_atom_is_valid_for_isotope(const knishio_atom_t* atom, knishio_isotope_t isotope);

/* Serialization */

/**
 * @brief Convert atom to JSON string
 * @param atom Source atom
 * @param json_output Output JSON string (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_to_json(
    const knishio_atom_t* atom,
    char** json_output
);

/**
 * @brief Create atom from JSON string
 * @param json_input JSON string
 * @param atom Output atom
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_from_json(
    const char* json_input,
    knishio_atom_t** atom
);

/* Utility functions */

/**
 * @brief Convert isotope enum to string
 * @param isotope Isotope type
 * @return Isotope string ("V", "C", "M", etc.) or NULL for unknown
 */
const char* knishio_isotope_to_string(knishio_isotope_t isotope);

/**
 * @brief Convert isotope string to enum
 * @param isotope_str Isotope string
 * @return Isotope type or KNISHIO_ISOTOPE_UNKNOWN if invalid
 */
knishio_isotope_t knishio_isotope_from_string(const char* isotope_str);

/**
 * @brief Get hashable properties for atom
 * @param properties Output array of property names (caller must free)
 * @param property_count Output number of properties
 * @return KNISHIO_SUCCESS on success, error code on failure
 * 
 * Returns the list of properties that should be included in atom hashing
 */
knishio_error_t knishio_atom_get_hashable_properties(
    char*** properties,
    size_t* property_count
);

/**
 * @brief Sort atoms array by index
 * @param atoms Array of atom pointers
 * @param atom_count Number of atoms
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_atom_sort_by_index(
    knishio_atom_t** atoms,
    size_t atom_count
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_ATOM_H */
