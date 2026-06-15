#ifndef KNISHIO_H
#define KNISHIO_H

/**
 * @file knishio.h
 * @brief Main KnishIO C Client SDK header
 * 
 * This header provides the complete API for the KnishIO C Client SDK,
 * offering 100% compatibility with the JavaScript SDK implementation
 * for post-blockchain distributed ledger technology.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define KNISHIO_VERSION_MAJOR 0
#define KNISHIO_VERSION_MINOR 8
#define KNISHIO_VERSION_PATCH 0
#define KNISHIO_VERSION_STRING "0.8.0"

/* Forward declarations for opaque types */
typedef struct knishio_client knishio_client_t;
typedef struct knishio_wallet knishio_wallet_t;
typedef struct knishio_molecule knishio_molecule_t;
typedef struct knishio_atom knishio_atom_t;
typedef struct knishio_meta knishio_meta_t;
typedef struct knishio_auth_token knishio_auth_token_t;
typedef struct knishio_token_unit knishio_token_unit_t;
typedef struct knishio_response knishio_response_t;
typedef struct knishio_query knishio_query_t;
typedef struct knishio_mutation knishio_mutation_t;

/* Utility types */
typedef struct knishio_string knishio_string_t;
typedef struct knishio_array knishio_array_t;
typedef struct knishio_map knishio_map_t;
typedef struct knishio_error_context knishio_error_context_t;

/* Include error context first to define knishio_error_t */
#include "knishio/error/context.h"

/**
 * @brief Callback types for asynchronous operations
 */
typedef void (*knishio_async_callback_t)(knishio_error_t error, const char *result, void *user_data);
typedef void (*knishio_error_callback_t)(knishio_error_t error, const char *message, void *user_data);
typedef void (*knishio_subscription_callback_t)(const char *data, void *user_data);

/**
 * @brief Client configuration structure
 */
typedef struct {
    const char *uri;           /**< GraphQL endpoint URI */
    const char *cell_slug;     /**< Cell identifier */
    void *client;              /**< HTTP client instance */
    void *socket;              /**< WebSocket instance */
    int server_sdk_version;    /**< Server SDK version */
    bool logging;              /**< Enable logging */
} knishio_client_config_t;

/**
 * @brief Wallet configuration structure
 */
typedef struct {
    const char *secret;        /**< Wallet secret */
    const char *bundle;        /**< Bundle hash */
    const char *token;         /**< Token type */
    const char *position;      /**< Position salt */
    const char *batch_id;      /**< Batch identifier */
    const char *characters;    /**< Character set */
} knishio_wallet_config_t;

/**
 * @brief Molecule configuration structure
 */
typedef struct {
    const char *secret;              /**< Secret for signing */
    const char *bundle;              /**< Bundle hash */
    knishio_wallet_t *source_wallet; /**< Source wallet */
    knishio_wallet_t *remainder_wallet; /**< Remainder wallet */
    const char *cell_slug;           /**< Cell identifier */
    const char *version;             /**< Protocol version */
} knishio_molecule_config_t;

/**
 * @brief Atom configuration structure
 */
typedef struct {
    const char *position;        /**< Wallet position */
    const char *wallet_address;  /**< Wallet address */
    const char *isotope;         /**< Transaction type */
    const char *token;           /**< Token type */
    const char *value;           /**< Transaction value */
    const char *batch_id;        /**< Batch identifier */
    const char *meta_type;       /**< Metadata type */
    const char *meta_id;         /**< Metadata identifier */
    const char **meta;           /**< Metadata array */
    size_t meta_count;           /**< Metadata count */
    int index;                   /**< Atom index */
} knishio_atom_config_t;

/* Include specific component headers */
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/utils/array.h"
#include "knishio/utils/map.h"
#include "knishio/crypto/shake256.h"
#include "knishio/crypto/mlkem768.h"
#include "knishio/meta.h"
#include "knishio/exceptions.h"
#include "knishio/http.h"
#include "knishio/graphql.h"
#include "knishio/client.h"
#include "knishio/wallet.h"
#include "knishio/auth_token.h"
#include "knishio/molecule.h"
#include "knishio/atom.h"

/* High-level client operations for full JS SDK alignment */
#include "knishio/client_ops.h"

/**
 * @brief Get version string
 * @return Version string
 */
const char* knishio_version(void);

/**
 * @brief Initialize the KnishIO SDK
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_init(void);

/**
 * @brief Cleanup the KnishIO SDK
 */
void knishio_cleanup(void);

/**
 * @brief Get error message for error code
 * @param error Error code
 * @return Human-readable error message
 */
const char* knishio_error_message(knishio_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_H */
