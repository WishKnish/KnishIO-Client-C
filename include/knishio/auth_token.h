#ifndef KNISHIO_AUTH_TOKEN_H
#define KNISHIO_AUTH_TOKEN_H

/**
 * @file auth_token.h
 * @brief Authentication token management for KnishIO SDK
 * 
 * Provides 100% compatibility with JavaScript SDK AuthToken class.
 * All methods map directly to their JavaScript equivalents for 
 * seamless cross-platform authentication integration.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "knishio/error/context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_wallet knishio_wallet_t;
typedef struct knishio_auth_token knishio_auth_token_t;

/**
 * @brief Authentication token structure
 * Maps directly to JavaScript AuthToken class properties
 */
struct knishio_auth_token {
    char* token;                    /**< Authentication token string */
    int64_t expires_at;             /**< Expiration timestamp (Unix seconds) */
    bool encrypt;                   /**< Encryption flag */
    char* pubkey;                   /**< Public key for authentication */
    knishio_wallet_t* wallet;       /**< Associated wallet reference */
};

/**
 * @brief AuthToken configuration structure
 * Used for creating new AuthToken instances
 */
typedef struct {
    const char* token;              /**< Token string */
    int64_t expires_at;             /**< Expiration timestamp */
    bool encrypt;                   /**< Encryption flag */
    const char* pubkey;             /**< Public key */
} knishio_auth_token_config_t;

/**
 * @brief Snapshot structure for AuthToken serialization
 * Maps to JavaScript getSnapshot() return value
 */
typedef struct {
    char* token;                    /**< Token string */
    int64_t expires_at;             /**< Expiration timestamp */
    char* pubkey;                   /**< Public key */
    bool encrypt;                   /**< Encryption flag */
    struct {
        char* position;             /**< Wallet position */
        char* characters;           /**< Wallet characters */
    } wallet;
} knishio_auth_token_snapshot_t;

/**
 * @brief Authentication data structure for GraphQL
 * Maps to JavaScript getAuthData() return value
 */
typedef struct {
    char* token;                    /**< Token string */
    char* pubkey;                   /**< Public key */
    knishio_wallet_t* wallet;       /**< Wallet reference */
} knishio_auth_token_auth_data_t;

/* AuthToken Creation and Management */

/**
 * @brief Create new AuthToken instance
 * Equivalent to JavaScript: new AuthToken({ token, expiresAt, encrypt, pubkey })
 * 
 * @param auth_token Output AuthToken instance (allocated, must be freed)
 * @param config AuthToken configuration
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_auth_token_create(knishio_auth_token_t** auth_token,
                                          const knishio_auth_token_config_t* config);

/**
 * @brief Create AuthToken with associated wallet
 * Equivalent to JavaScript: AuthToken.create(data, wallet)
 * 
 * @param auth_token Output AuthToken instance (allocated, must be freed)
 * @param config AuthToken configuration
 * @param wallet Wallet to associate with token
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_auth_token_create_with_wallet(knishio_auth_token_t** auth_token,
                                                      const knishio_auth_token_config_t* config,
                                                      knishio_wallet_t* wallet);

/**
 * @brief Restore AuthToken from snapshot
 * Equivalent to JavaScript: AuthToken.restore(snapshot, secret)
 * 
 * @param auth_token Output AuthToken instance (allocated, must be freed)
 * @param snapshot_json JSON snapshot string from getSnapshot()
 * @param secret Secret for wallet restoration
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_auth_token_restore(knishio_auth_token_t** auth_token,
                                           const char* snapshot_json,
                                           const char* secret);

/**
 * @brief Clean up AuthToken resources
 * Frees all allocated memory for the AuthToken
 * 
 * @param auth_token AuthToken to clean up
 */
void knishio_auth_token_cleanup(knishio_auth_token_t* auth_token);

/* Wallet Management */

/**
 * @brief Set wallet for AuthToken
 * Equivalent to JavaScript: authToken.setWallet(wallet)
 * 
 * @param auth_token AuthToken instance
 * @param wallet Wallet to associate
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_auth_token_set_wallet(knishio_auth_token_t* auth_token,
                                              knishio_wallet_t* wallet);

/**
 * @brief Get wallet from AuthToken
 * Equivalent to JavaScript: authToken.getWallet()
 * 
 * @param auth_token AuthToken instance
 * @return Wallet reference or NULL if not set
 */
knishio_wallet_t* knishio_auth_token_get_wallet(const knishio_auth_token_t* auth_token);

/* Property Accessors */

/**
 * @brief Get token string
 * Equivalent to JavaScript: authToken.getToken()
 * 
 * @param auth_token AuthToken instance
 * @return Token string or NULL if not set
 */
const char* knishio_auth_token_get_token(const knishio_auth_token_t* auth_token);

/**
 * @brief Get public key
 * Equivalent to JavaScript: authToken.getPubkey()
 * 
 * @param auth_token AuthToken instance
 * @return Public key string or NULL if not set
 */
const char* knishio_auth_token_get_pubkey(const knishio_auth_token_t* auth_token);

/**
 * @brief Get expiration interval in milliseconds
 * Equivalent to JavaScript: authToken.getExpireInterval()
 * Algorithm: (expires_at * 1000) - Date.now()
 * 
 * @param auth_token AuthToken instance
 * @return Milliseconds until expiration, negative if expired
 */
int64_t knishio_auth_token_get_expire_interval(const knishio_auth_token_t* auth_token);

/**
 * @brief Check if token is expired
 * Equivalent to JavaScript: authToken.isExpired()
 * 
 * @param auth_token AuthToken instance
 * @return True if expired, false if valid
 */
bool knishio_auth_token_is_expired(const knishio_auth_token_t* auth_token);

/* Serialization */

/**
 * @brief Get AuthToken snapshot for serialization
 * Equivalent to JavaScript: authToken.getSnapshot()
 * Returns JSON string with all AuthToken data for persistence
 * 
 * @param auth_token AuthToken instance
 * @param snapshot_json Output JSON string (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_auth_token_get_snapshot(const knishio_auth_token_t* auth_token,
                                                char** snapshot_json);

/**
 * @brief Get authentication data for GraphQL client
 * Equivalent to JavaScript: authToken.getAuthData()
 * Returns JSON string: { token: string, pubkey: string, wallet: Wallet }
 * 
 * @param auth_token AuthToken instance
 * @param auth_data_json Output JSON string (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_auth_token_get_auth_data(const knishio_auth_token_t* auth_token,
                                                 char** auth_data_json);

/* Snapshot Management */

/**
 * @brief Create snapshot structure from AuthToken
 * Helper function for internal snapshot operations
 * 
 * @param auth_token AuthToken instance
 * @param snapshot Output snapshot structure (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_auth_token_create_snapshot(const knishio_auth_token_t* auth_token,
                                                   knishio_auth_token_snapshot_t** snapshot);

/**
 * @brief Clean up snapshot structure
 * Frees all allocated memory for the snapshot
 * 
 * @param snapshot Snapshot to clean up
 */
void knishio_auth_token_snapshot_cleanup(knishio_auth_token_snapshot_t* snapshot);

/* Authentication Data Management */

/**
 * @brief Create authentication data structure
 * Helper function for getAuthData() implementation
 * 
 * @param auth_token AuthToken instance
 * @param auth_data Output auth data structure (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_auth_token_create_auth_data(const knishio_auth_token_t* auth_token,
                                                    knishio_auth_token_auth_data_t** auth_data);

/**
 * @brief Clean up authentication data structure
 * Frees all allocated memory for the auth data
 * 
 * @param auth_data Auth data to clean up
 */
void knishio_auth_token_auth_data_cleanup(knishio_auth_token_auth_data_t* auth_data);

/* Utility Functions */

/**
 * @brief Get current Unix timestamp in milliseconds
 * Used for expiration calculations
 * 
 * @return Current timestamp in milliseconds
 */
int64_t knishio_auth_token_current_timestamp_ms(void);

/**
 * @brief Validate AuthToken configuration
 * Checks if config has required fields
 * 
 * @param config Configuration to validate
 * @return True if valid, false if invalid
 */
bool knishio_auth_token_validate_config(const knishio_auth_token_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_AUTH_TOKEN_H */
