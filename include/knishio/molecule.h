#ifndef KNISHIO_MOLECULE_H
#define KNISHIO_MOLECULE_H

/**
 * @file molecule.h
 * @brief Molecule operations for KnishIO SDK
 * 
 * Implements JavaScript-compatible molecule management for post-blockchain DLT.
 * Molecules are collections of atoms that are accepted or rejected together.
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
typedef struct knishio_wallet knishio_wallet_t;
typedef struct knishio_molecule knishio_molecule_t;

/* Constants */
#define KNISHIO_MOLECULAR_HASH_LENGTH 64      /**< Molecular hash length in hex chars */
#define KNISHIO_MAX_ATOMS_PER_MOLECULE 256    /**< Maximum atoms per molecule */
#define KNISHIO_CELL_SLUG_DELIMITER "."       /**< Cell slug delimiter */

/* Molecule status enumeration */
typedef enum {
    KNISHIO_MOLECULE_STATUS_UNKNOWN = 0,
    KNISHIO_MOLECULE_STATUS_PENDING,
    KNISHIO_MOLECULE_STATUS_ACCEPTED,
    KNISHIO_MOLECULE_STATUS_REJECTED,
    KNISHIO_MOLECULE_STATUS_FAILED
} knishio_molecule_status_t;

/* Molecule structure with C17 static assertions */
struct knishio_molecule {
    /* Core properties */
    char* secret;                    /**< Secret for signing */
    char* bundle;                    /**< Bundle hash */
    char* molecular_hash;            /**< Molecular hash (64 hex chars) */
    char* cell_slug;                 /**< Cell identifier */
    char* cell_slug_origin;          /**< Original cell slug */
    char* version;                   /**< Protocol version */
    
    /* Timing */
    time_t created_at;               /**< Creation timestamp */
    
    /* Status */
    knishio_molecule_status_t status; /**< Current molecule status */
    
    /* Wallets */
    knishio_wallet_t* source_wallet;    /**< Source wallet */
    knishio_wallet_t* remainder_wallet; /**< Remainder wallet */
    
    /* Atoms management */
    knishio_atom_t** atoms;          /**< Array of atom pointers */
    size_t atom_count;               /**< Current number of atoms */
    size_t atom_capacity;            /**< Allocated capacity for atoms */
    
    /* Internal state */
    bool is_signed;                  /**< Whether molecule is signed */
    bool is_verified;                /**< Whether molecule is verified */
};

/* Static assertions for C17 safety */
_Static_assert(sizeof(knishio_molecule_status_t) <= sizeof(int),
    "Molecule status enum must fit in int");
_Static_assert(KNISHIO_MAX_ATOMS_PER_MOLECULE > 0 && KNISHIO_MAX_ATOMS_PER_MOLECULE <= 1024,
    "Maximum atoms per molecule must be reasonable");

/* Molecule lifecycle */

/**
 * @brief Create a new molecule
 * @param molecule Output molecule pointer
 * @param secret Secret for signing (can be NULL)
 * @param bundle Bundle hash (can be NULL)
 * @param source_wallet Source wallet (can be NULL)
 * @param remainder_wallet Remainder wallet (can be NULL)
 * @param cell_slug Cell identifier (can be NULL)
 * @param version Protocol version (can be NULL)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_create(
    knishio_molecule_t** molecule,
    const char* secret,
    const char* bundle,
    knishio_wallet_t* source_wallet,
    knishio_wallet_t* remainder_wallet,
    const char* cell_slug,
    const char* version
);

/**
 * @brief Free molecule and all associated resources
 * @param molecule Molecule to free
 */
void knishio_molecule_free(knishio_molecule_t* molecule);

/* Atom management */

/**
 * @brief Add atom to molecule
 * @param molecule Target molecule
 * @param atom Atom to add (molecule takes ownership)
 * @return KNISHIO_SUCCESS on success, error code on failure
 * 
 * Note: Adding an atom resets the molecular hash
 */
knishio_error_t knishio_molecule_add_atom(
    knishio_molecule_t* molecule, 
    knishio_atom_t* atom
);

/**
 * @brief Get atom by index
 * @param molecule Source molecule
 * @param index Atom index
 * @return Atom pointer or NULL if index invalid
 */
knishio_atom_t* knishio_molecule_get_atom(
    const knishio_molecule_t* molecule, 
    size_t index
);

/**
 * @brief Filter atoms by isotope type
 * @param molecule Source molecule
 * @param isotope Isotope to filter by (e.g., "V", "C", "M")
 * @param filtered_atoms Output array of atom pointers
 * @param filtered_count Output count of filtered atoms
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_filter_atoms_by_isotope(
    const knishio_molecule_t* molecule,
    const char* isotope,
    knishio_atom_t*** filtered_atoms,
    size_t* filtered_count
);

/**
 * @brief Generate next atomic index for this molecule
 * @param molecule Source molecule
 * @return Next available atom index
 */
size_t knishio_molecule_generate_next_index(const knishio_molecule_t* molecule);

/* Molecule operations */

/**
 * @brief Sign the molecule
 * @param molecule Molecule to sign
 * @param bundle Bundle hash (NULL to derive from secret)
 * @param anonymous Whether to sign anonymously
 * @param compressed Whether to use compressed signature
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_sign(
    knishio_molecule_t* molecule,
    const char* bundle,
    bool anonymous,
    bool compressed
);

/**
 * @brief Verify molecule integrity
 * @param molecule Molecule to verify
 * @param sender_wallet Sender wallet for verification (can be NULL)
 * @return KNISHIO_SUCCESS if valid, error code if invalid
 */
knishio_error_t knishio_molecule_check(
    const knishio_molecule_t* molecule,
    const knishio_wallet_t* sender_wallet
);

/**
 * @brief Generate molecular hash from atom composition
 * @param molecule Molecule to hash
 * @return KNISHIO_SUCCESS on success, error code on failure
 * 
 * Updates molecule->molecular_hash with computed 64-character hex hash
 */
knishio_error_t knishio_molecule_generate_hash(knishio_molecule_t* molecule);

/* Utility functions */

/**
 * @brief Convert molecule to JSON string
 * @param molecule Source molecule
 * @param json_output Output JSON string (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_to_json(
    const knishio_molecule_t* molecule,
    char** json_output
);

/**
 * @brief Create molecule from JSON string
 * @param json_input JSON string
 * @param molecule Output molecule
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_from_json(
    const char* json_input,
    knishio_molecule_t** molecule
);

/**
 * @brief Get cell slug delimiter
 * @return Cell slug delimiter string
 */
const char* knishio_molecule_get_cell_slug_delimiter(void);

/**
 * @brief Enumerate molecular hash to numeric array
 * @param molecular_hash 64-character molecular hash
 * @param enumerated_output Output numeric array (caller must free)
 * @param output_length Output array length
 * @return KNISHIO_SUCCESS on success, error code on failure
 * 
 * Converts hex hash to Base-17 enumeration for normalization
 */
knishio_error_t knishio_molecule_enumerate_hash(
    const char* molecular_hash,
    int** enumerated_output,
    size_t* output_length
);

/**
 * @brief Normalize enumerated molecular hash
 * @param enumerated_array Input enumerated array
 * @param array_length Array length
 * @param normalized_output Output normalized array (caller must free)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_normalize_hash(
    const int* enumerated_array,
    size_t array_length,
    int** normalized_output
);

/* High-level molecule operations for JavaScript SDK compatibility */

/**
 * @brief Initialize metadata molecule (matches JavaScript SDK initMeta)
 * Adds M-isotope atom with metadata and I-isotope atom for ContinuID
 * @param molecule Molecule to initialize
 * @param meta_type Metadata type
 * @param meta_id Metadata identifier
 * @param meta_keys Array of metadata keys
 * @param meta_values Array of metadata values
 * @param meta_count Number of metadata key-value pairs
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_init_meta(
    knishio_molecule_t* molecule,
    const char* meta_type,
    const char* meta_id,
    const char** meta_keys,
    const char** meta_values,
    size_t meta_count
);

/**
 * @brief Initialize value transfer molecule (matches JavaScript SDK initValue)
 * Adds V-isotope atoms: source debit, recipient credit, remainder (UTXO pattern)
 * @param molecule Molecule to initialize
 * @param recipient_wallet Recipient wallet
 * @param amount Transfer amount
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_init_value(
    knishio_molecule_t* molecule,
    knishio_wallet_t* recipient_wallet,
    int amount
);

/**
 * @brief Initialize a MULTI-recipient value molecule (multi-recipient sibling of init_value).
 * One source debits its FULL balance to fund N recipients (each its own amount + stackable units)
 * plus a remainder back to the sender. recipient_wallets is parallel to amounts. Conserves:
 * -balance + Sum(amounts) + (balance - Sum) == 0.
 * @param molecule Molecule to initialize (source_wallet + remainder_wallet set)
 * @param recipient_wallets Array of recipient wallets
 * @param amounts Array of per-recipient amounts (parallel to recipient_wallets)
 * @param recipient_count Number of recipients
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_init_values(
    knishio_molecule_t* molecule,
    knishio_wallet_t** recipient_wallets,
    const int* amounts,
    size_t recipient_count
);

/**
 * @brief Initialize a token-burn molecule (canonical 3 V-atoms, zero-sum).
 * Source debits full balance; the burn-target atom credits `amount` to the all-zeros burn
 * bundle (empty position/address, metaType 'walletBundle', metaId all-zeros = destruction);
 * the remainder returns (balance - amount) to the source identity. Pure V (no ContinuID).
 * @param molecule Molecule (must have source_wallet [with balance] + remainder_wallet set)
 * @param amount Amount to burn (integer)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_init_burn(
    knishio_molecule_t* molecule,
    int amount
);

/**
 * @brief Initialize a token-creation molecule (matches JavaScript SDK initTokenCreation)
 * Adds a C-isotope atom issuing a new token + an I-isotope ContinuID atom. The C-atom is
 * signed by molecule->source_wallet; its meta is the user token meta FIRST, then the 7
 * prefixed wallet* keys for the recipient wallet (setMetaWallet order).
 * @param molecule Molecule (must have source_wallet + remainder_wallet set)
 * @param recipient_wallet Wallet receiving the new token
 * @param amount Initial supply (string; the C-atom value)
 * @param token_meta_keys Array of user token-meta keys (name/fungibility/supply/decimals)
 * @param token_meta_values Array of user token-meta values
 * @param token_meta_count Number of user token-meta pairs
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_init_token_creation(
    knishio_molecule_t* molecule,
    const knishio_wallet_t* recipient_wallet,
    const char* amount,
    const char** token_meta_keys,
    const char** token_meta_values,
    size_t token_meta_count
);

/**
 * @brief Initialize a wallet-creation molecule (matches JavaScript SDK initWalletCreation)
 * Adds a C-isotope atom (metaType "wallet") defining a new wallet on the ledger + an I-isotope
 * ContinuID atom. The C-atom is signed by molecule->source_wallet; its meta is the optional
 * leading atom_meta FIRST (e.g. shadowWalletClaim), then the 7 prefixed wallet* keys.
 * @param molecule Molecule (must have source_wallet + remainder_wallet set)
 * @param wallet The new wallet being defined
 * @param atom_meta_keys Optional leading meta keys (may be NULL)
 * @param atom_meta_values Optional leading meta values (may be NULL)
 * @param atom_meta_count Number of leading meta pairs (0 for plain wallet creation)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_init_wallet_creation(
    knishio_molecule_t* molecule,
    const knishio_wallet_t* wallet,
    const char** atom_meta_keys,
    const char** atom_meta_values,
    size_t atom_meta_count
);

/**
 * @brief Initialize a shadow-wallet-claim molecule (matches JavaScript SDK initShadowWalletClaim)
 * Prepends a shadowWalletClaim=1 meta key, then delegates to init_wallet_creation.
 * @param molecule Molecule (must have source_wallet + remainder_wallet set)
 * @param wallet The shadow wallet being claimed
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_init_shadow_wallet_claim(
    knishio_molecule_t* molecule,
    const knishio_wallet_t* wallet
);

/**
 * @brief Initialize an authorization molecule (matches JavaScript SDK initAuthorization)
 * Adds a U-isotope atom (signed by molecule->source_wallet; no metaType/metaId; meta =
 * [encrypt, pubkey, characters] in JS order) + an I-isotope ContinuID atom. Used to request
 * a bundle-scoped auth token; the validator extracts the pubkey from the U-atom's walletAddress
 * and issues a JWT (U-isotope is OTS-exempt, but the molecular hash is still verified).
 * @param molecule Molecule (must have source_wallet [token "AUTH"] + remainder_wallet set)
 * @param encrypt Whether the session requests encrypted communications (meta "encrypt")
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_molecule_init_authorization(
    knishio_molecule_t* molecule,
    bool encrypt
);

/**
 * @brief Simplified wallet creation (matches JavaScript SDK Wallet.create pattern)
 * @param wallet Output wallet pointer
 * @param secret Wallet secret
 * @param token Token type
 * @param position Wallet position
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_wallet_create_simple(
    knishio_wallet_t** wallet,
    const char* secret,
    const char* token,
    const char* position
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_MOLECULE_H */
