#ifndef KNISHIO_WALLET_H
#define KNISHIO_WALLET_H

/**
 * @file wallet.h
 * @brief Complete KnishIO wallet and ContinuID implementation
 * 
 * Provides full wallet management with ContinuID identity system:
 * - Position sequence generation and validation
 * - Bundle hash calculation and verification  
 * - Wallet chain management and traversal
 * - Identity relay race mechanics
 * - Remainder wallet generation
 * - Multi-identity support
 * 
 * Matches JavaScript SDK Wallet.js functionality exactly.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define KNISHIO_SECRET_LENGTH       2048    /**< Secret string length (2048 hex, cross-SDK canonical) */
#define KNISHIO_BUNDLE_LENGTH       64      /**< Bundle hash length */
#define KNISHIO_POSITION_LENGTH     64      /**< Position string length */
#define KNISHIO_ADDRESS_LENGTH      64      /**< Address string length */
#define KNISHIO_PRIVKEY_LENGTH      2048    /**< Private key length */
#define KNISHIO_PUBKEY_LENGTH       1184    /**< Public key length (ML-KEM768) */
#define KNISHIO_FIXED_POSITION      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
#define KNISHIO_POSITION_CHARSET    "abcdef0123456789"
#define KNISHIO_DEFAULT_TOKEN       "USER"

/* Forward declarations */
typedef struct knishio_wallet knishio_wallet_t;
typedef struct knishio_wallet_bundle knishio_wallet_bundle_t;
typedef struct knishio_position_sequence knishio_position_sequence_t;
typedef struct knishio_continu_id knishio_continu_id_t;

/* Token unit structure. Canonical cross-SDK fields are (id, name, metas); the wire format
 * emitted on V-atoms is JSON `[[id, name, metas], ...]` (matches JS/Rust/Python to_data()).
 * `metas_json` holds the metas object as a JSON string (default "{}"). The legacy fields below
 * are unpopulated/unused today (kept to avoid churn). */
typedef struct knishio_token_unit {
    char *id;                       /**< Token unit ID */
    char *name;                     /**< Token unit name (canonical) */
    char *metas_json;               /**< Token unit metas as a JSON object string (canonical; default "{}") */
    char *token_slug;               /**< Token identifier (legacy, unused) */
    double amount;                  /**< Unit amount (legacy, unused) */
    char *batch_id;                 /**< Batch identifier (legacy, unused) */
    char *created_at;               /**< Creation timestamp (legacy, unused) */
    bool is_shadow;                 /**< Shadow unit flag (legacy, unused) */
} knishio_token_unit_t;

/**
 * @brief Complete wallet structure with ContinuID support
 */
struct knishio_wallet {
    /* Core identity */
    char *secret;                   /**< Master secret (2048 chars) */
    char *bundle_hash;              /**< Bundle identity hash */
    char *token;                    /**< Token slug */
    char *position;                 /**< Position in wallet chain */
    
    /* Cryptographic keys */
    char *private_key;              /**< Private key (2048 chars) */
    char *address;                  /**< Wallet address (64 chars) */
    char *pubkey;                   /**< ML-KEM768 public key */
    uint8_t *privkey_bytes;         /**< ML-KEM768 private key bytes */
    size_t privkey_bytes_len;       /**< Private key bytes length */
    
    /* Wallet state */
    double balance;                 /**< Current balance */
    knishio_token_unit_t *token_units;  /**< Array of token units */
    size_t token_unit_count;        /**< Number of token units */
    
    /* ContinuID chain */
    char *batch_id;                 /**< Batch identifier */
    char *characters;               /**< Character set (BASE64, etc) */
    char *created_at;               /**< Creation timestamp */
    bool is_shadow;                 /**< Shadow wallet flag */
    bool initialized;               /**< Initialization flag */
    
    /* Position sequence */
    knishio_position_sequence_t *sequence;  /**< Position sequence */
    
    /* Trade rates */
    struct {
        char **tokens;              /**< Token slugs */
        double *rates;              /**< Exchange rates */
        size_t count;               /**< Number of rates */
    } trade_rates;
    
    /* Molecules */
    struct {
        char **hashes;              /**< Molecular hashes */
        void **molecules;           /**< Molecule objects */
        size_t count;               /**< Number of molecules */
    } molecules;
};

/**
 * @brief Position sequence for wallet chain management
 */
struct knishio_position_sequence {
    char **positions;               /**< Array of positions */
    size_t count;                   /**< Number of positions */
    size_t capacity;                /**< Array capacity */
    char *current_position;         /**< Current active position */
    size_t current_index;           /**< Current position index */
    bool validated;                 /**< Sequence validation flag */
};

/**
 * @brief Wallet bundle for identity management
 */
struct knishio_wallet_bundle {
    char *secret;                   /**< Master secret */
    char *bundle_hash;              /**< Bundle identifier */
    char *token;                    /**< Default token */
    
    knishio_wallet_t **wallets;     /**< Array of wallets */
    size_t wallet_count;            /**< Number of wallets */
    size_t wallet_capacity;         /**< Wallet array capacity */
    
    knishio_position_sequence_t *sequence;  /**< Position sequence */
    char *current_position;         /**< Current position */
    
    /* Bundle metadata */
    char *created_at;               /**< Bundle creation timestamp */
    uint64_t transaction_count;     /**< Total transaction count */
    double total_balance;           /**< Total bundle balance */
};

/**
 * @brief ContinuID identity management
 */
struct knishio_continu_id {
    char *secret;                   /**< Master secret */
    knishio_wallet_bundle_t **bundles;  /**< Array of bundles */
    size_t bundle_count;            /**< Number of bundles */
    size_t bundle_capacity;         /**< Bundle array capacity */
    
    /* Identity relay race */
    struct {
        char **race_history;        /**< Position handoff history */
        size_t history_count;       /**< Number of handoffs */
        char *current_runner;       /**< Current active position */
        bool race_active;           /**< Race state flag */
    } relay_race;
    
    /* Multi-device support */
    struct {
        char **device_ids;          /**< Device identifiers */
        knishio_wallet_bundle_t **device_bundles;  /**< Device bundles */
        size_t device_count;        /**< Number of devices */
        bool *device_active;        /**< Device active flags */
    } devices;
};

/* Core ContinuID Functions */

/**
 * @brief Generate bundle hash from secret
 * @param secret Master secret (2048 chars)
 * @param token Token slug (optional)
 * @param position Starting position (optional, defaults to W0)
 * @param bundle_hash Output bundle hash (64 chars, allocated)
 * @return True on success, false on failure
 */
bool knishio_generate_bundle_hash(const char *secret, const char *token, 
                                  const char *position, char **bundle_hash);

/**
 * @brief Generate next position in sequence
 * @param current_position Current position
 * @param wallet_address Wallet address for entropy
 * @param next_position Output next position (allocated)
 * @return True on success, false on failure
 */
bool knishio_generate_next_position(const char *current_position, 
                                    const char *wallet_address,
                                    char **next_position);

/**
 * @brief Generate deterministic position sequence
 * @param secret Master secret
 * @param count Number of positions to generate
 * @param positions Output position array (allocated)
 * @return True on success, false on failure
 */
bool knishio_generate_position_sequence(const char *secret, size_t count,
                                        char ***positions);

/**
 * @brief Validate position sequence continuity
 * @param positions Array of positions
 * @param count Number of positions
 * @param secret Master secret for validation
 * @return True if sequence is valid, false otherwise
 */
bool knishio_validate_position_sequence(char **positions, size_t count,
                                        const char *secret);

/* Wallet Creation and Management */

/**
 * @brief Create wallet with ContinuID support
 * @param wallet Output wallet (allocated)
 * @param secret Master secret
 * @param token Token slug
 * @param position Position (optional, generates if NULL)
 * @return True on success, false on failure
 */
bool knishio_wallet_create_with_continuid(knishio_wallet_t **wallet,
                                          const char *secret,
                                          const char *token,
                                          const char *position);

/**
 * @brief Create wallet from existing parameters
 * @param wallet Output wallet (allocated)
 * @param secret Master secret (optional if bundle provided)
 * @param bundle Bundle hash (optional if secret provided)
 * @param token Token slug
 * @param address Wallet address (optional)
 * @param position Position (optional)
 * @param batch_id Batch ID (optional)
 * @param characters Character set (optional)
 * @return True on success, false on failure
 */
bool knishio_wallet_create_from_params(knishio_wallet_t **wallet,
                                       const char *secret,
                                       const char *bundle,
                                       const char *token,
                                       const char *address,
                                       const char *position,
                                       const char *batch_id,
                                       const char *characters);

/**
 * @brief Create remainder wallet for transfers
 * @param source_wallet Source wallet
 * @param remainder_wallet Output remainder wallet (allocated)
 * @return True on success, false on failure
 */
bool knishio_wallet_create_remainder(knishio_wallet_t *source_wallet,
                                     knishio_wallet_t **remainder_wallet);

/**
 * @brief Check if wallet is shadow wallet
 * @param wallet Wallet to check
 * @return True if shadow wallet, false otherwise
 */
bool knishio_wallet_is_shadow(knishio_wallet_t *wallet);

/**
 * @brief Initialize ML-KEM768 keys for wallet
 * @param wallet Wallet to initialize
 * @return True on success, false on failure
 */
bool knishio_wallet_initialize_mlkem(knishio_wallet_t *wallet);

/* Wallet Bundle Management */

/**
 * @brief Create wallet bundle
 * @param bundle Output bundle (allocated)
 * @param secret Master secret
 * @param token Default token
 * @return True on success, false on failure
 */
bool knishio_wallet_bundle_create(knishio_wallet_bundle_t **bundle,
                                  const char *secret,
                                  const char *token);

/**
 * @brief Add wallet to bundle
 * @param bundle Wallet bundle
 * @param position Position for new wallet (optional)
 * @param wallet Output wallet (allocated)
 * @return True on success, false on failure
 */
bool knishio_wallet_bundle_add_wallet(knishio_wallet_bundle_t *bundle,
                                      const char *position,
                                      knishio_wallet_t **wallet);

/**
 * @brief Get next position in bundle sequence
 * @param bundle Wallet bundle
 * @param next_position Output next position (allocated)
 * @return True on success, false on failure
 */
bool knishio_wallet_bundle_get_next_position(knishio_wallet_bundle_t *bundle,
                                             char **next_position);

/**
 * @brief Find wallet by position in bundle
 * @param bundle Wallet bundle
 * @param position Position to find
 * @return Wallet if found, NULL otherwise
 */
knishio_wallet_t *knishio_wallet_bundle_find_by_position(knishio_wallet_bundle_t *bundle,
                                                         const char *position);

/**
 * @brief Free wallet bundle
 * @param bundle Bundle to free
 */
void knishio_wallet_bundle_free(knishio_wallet_bundle_t *bundle);

/* ContinuID Management */

/**
 * @brief Create ContinuID identity system
 * @param continu_id Output ContinuID (allocated)
 * @param secret Master secret
 * @return True on success, false on failure
 */
bool knishio_continu_id_create(knishio_continu_id_t **continu_id,
                               const char *secret);

/**
 * @brief Add device to ContinuID
 * @param continu_id ContinuID system
 * @param device_id Device identifier
 * @param device_name Device name
 * @param device_bundle Output device bundle (allocated)
 * @return True on success, false on failure
 */
bool knishio_continu_id_add_device(knishio_continu_id_t *continu_id,
                                   const char *device_id,
                                   const char *device_name,
                                   knishio_wallet_bundle_t **device_bundle);

/**
 * @brief Start identity relay race
 * @param continu_id ContinuID system
 * @param initial_position Starting position
 * @param wallet Output initial wallet (allocated)
 * @return True on success, false on failure
 */
bool knishio_continu_id_start_relay_race(knishio_continu_id_t *continu_id,
                                         const char *initial_position,
                                         knishio_wallet_t **wallet);

/**
 * @brief Handoff position in relay race
 * @param continu_id ContinuID system
 * @param reason Handoff reason
 * @param new_wallet Output new wallet (allocated)
 * @param handoff_molecule Output handoff molecule (allocated)
 * @return True on success, false on failure
 */
bool knishio_continu_id_handoff_position(knishio_continu_id_t *continu_id,
                                         const char *reason,
                                         knishio_wallet_t **new_wallet,
                                         void **handoff_molecule);

/**
 * @brief Verify relay race continuity
 * @param continu_id ContinuID system
 * @param valid Output validity flag
 * @param error_reason Output error reason (allocated if invalid)
 * @return True on success, false on failure
 */
bool knishio_continu_id_verify_race_continuity(knishio_continu_id_t *continu_id,
                                               bool *valid,
                                               char **error_reason);

/**
 * @brief Free ContinuID system
 * @param continu_id ContinuID to free
 */
void knishio_continu_id_free(knishio_continu_id_t *continu_id);

/* Position Sequence Management */

/**
 * @brief Create position sequence
 * @param sequence Output sequence (allocated)
 * @param initial_capacity Initial capacity
 * @return True on success, false on failure
 */
bool knishio_position_sequence_create(knishio_position_sequence_t **sequence,
                                      size_t initial_capacity);

/**
 * @brief Add position to sequence
 * @param sequence Position sequence
 * @param position Position to add
 * @return True on success, false on failure
 */
bool knishio_position_sequence_add(knishio_position_sequence_t *sequence,
                                   const char *position);

/**
 * @brief Get current position from sequence
 * @param sequence Position sequence
 * @return Current position or NULL
 */
const char *knishio_position_sequence_get_current(knishio_position_sequence_t *sequence);

/**
 * @brief Move to next position in sequence
 * @param sequence Position sequence
 * @param next_position Output next position (allocated)
 * @return True on success, false on failure
 */
bool knishio_position_sequence_next(knishio_position_sequence_t *sequence,
                                    char **next_position);

/**
 * @brief Validate sequence integrity
 * @param sequence Position sequence
 * @param secret Master secret
 * @return True if valid, false otherwise
 */
bool knishio_position_sequence_validate(knishio_position_sequence_t *sequence,
                                        const char *secret);

/**
 * @brief Free position sequence
 * @param sequence Sequence to free
 */
void knishio_position_sequence_free(knishio_position_sequence_t *sequence);

/* Token Unit Management */

/**
 * @brief Split token units between wallets
 * @param source_wallet Source wallet
 * @param units Array of unit IDs to split
 * @param unit_count Number of units
 * @param remainder_wallet Remainder wallet
 * @param recipient_wallet Recipient wallet (optional)
 * @return True on success, false on failure
 */
bool knishio_wallet_split_units(knishio_wallet_t *source_wallet,
                                char **units,
                                size_t unit_count,
                                knishio_wallet_t *remainder_wallet,
                                knishio_wallet_t *recipient_wallet);

/**
 * @brief Get token units data
 * @param wallet Wallet
 * @param units_data Output units data (allocated)
 * @param count Output unit count
 * @return True on success, false on failure
 */
bool knishio_wallet_get_token_units_data(knishio_wallet_t *wallet,
                                         void **units_data,
                                         size_t *count);

/* Recovery and Verification */

/**
 * @brief Reconstruct bundle from secret
 * @param secret Master secret
 * @param token Token slug
 * @param bundle Output bundle (allocated)
 * @param bundle_hash Output bundle hash (allocated)
 * @param initial_wallet Output initial wallet (allocated)
 * @return True on success, false on failure
 */
bool knishio_bundle_reconstruct_from_secret(const char *secret,
                                            const char *token,
                                            knishio_wallet_bundle_t **bundle,
                                            char **bundle_hash,
                                            knishio_wallet_t **initial_wallet);

/**
 * @brief Verify bundle ownership
 * @param claimed_secret Claimed secret
 * @param bundle_hash Bundle hash to verify
 * @param position Position to check (optional)
 * @return True if secret generates bundle, false otherwise
 */
bool knishio_bundle_verify_ownership(const char *claimed_secret,
                                     const char *bundle_hash,
                                     const char *position);

/**
 * @brief Reconstruct identity from molecules
 * @param molecules Array of molecule data
 * @param molecule_count Number of molecules
 * @param positions Output positions array (allocated)
 * @param position_count Output position count
 * @param bundle_hash Output bundle hash (allocated)
 * @return True on success, false on failure
 */
bool knishio_bundle_reconstruct_from_molecules(void **molecules,
                                               size_t molecule_count,
                                               char ***positions,
                                               size_t *position_count,
                                               char **bundle_hash);

/* Utility Functions */

/**
 * @brief Check if string is bundle hash
 * @param maybe_bundle_hash String to check
 * @return True if bundle hash format, false otherwise
 */
bool knishio_wallet_is_bundle_hash(const char *maybe_bundle_hash);

/**
 * @brief Generate random position
 * @param salt_length Position length (default 64)
 * @param charset Character set (optional)
 * @param position Output position (allocated)
 * @return True on success, false on failure
 */
bool knishio_generate_random_position(size_t salt_length,
                                      const char *charset,
                                      char **position);

/**
 * @brief Generate deterministic salt for position
 * @param token Token slug
 * @param position Position identifier
 * @param salt Output salt (allocated)
 * @return True on success, false on failure
 */
bool knishio_generate_salt(const char *token, const char *position,
                           char **salt);

/* Legacy support functions */
bool knishio_generate_secret(const char *seed, size_t length, char **secret);
bool knishio_generate_position(char **position);
bool knishio_use_fixed_position(char **position);
bool knishio_generate_wallet_key(const char *secret, const char *token,
                                 const char *position, char **private_key);
bool knishio_generate_address(const char *private_key, char **address);
bool knishio_wallet_create(knishio_wallet_t **wallet, const char *seed,
                          const char *token, const char *position);
bool knishio_wallet_from_secret(knishio_wallet_t **wallet, const char *secret,
                                const char *token, const char *position);
void knishio_wallet_cleanup(knishio_wallet_t *wallet);
void knishio_wallet_free(knishio_wallet_t *wallet);

/* Wallet property accessors */

/**
 * @brief Get wallet characters
 * @param wallet Wallet instance
 * @return Wallet characters string or NULL on error
 */
const char* knishio_wallet_get_characters(knishio_wallet_t *wallet);

/**
 * @brief Get wallet position
 * @param wallet Wallet instance
 * @return Wallet position string or NULL on error
 */
const char* knishio_wallet_get_position(knishio_wallet_t *wallet);

/**
 * @brief Get wallet address
 * @param wallet Wallet instance
 * @return Wallet address string or NULL on error
 */
const char* knishio_wallet_get_address(knishio_wallet_t *wallet);

/* Test functions */
bool knishio_wallet_self_test(void);
bool knishio_wallet_test_vector(const char *seed, const char *expected_bundle,
                               const char *expected_address, const char *token,
                               const char *position);
bool knishio_continu_id_test_relay_race(void);
bool knishio_continu_id_test_position_sequence(void);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_WALLET_H */
