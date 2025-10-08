#ifndef KNISHIO_ERROR_CONTEXT_H
#define KNISHIO_ERROR_CONTEXT_H

/**
 * @file context.h
 * @brief Error handling and context system for KnishIO SDK
 * 
 * Provides comprehensive error handling that maps JavaScript exceptions
 * to C error codes with detailed context information for debugging.
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Include the main knishio error enum */
#ifndef KNISHIO_ERROR_T_DEFINED
#define KNISHIO_ERROR_T_DEFINED
typedef enum {
    KNISHIO_SUCCESS = 0,
    
    /* General errors */
    KNISHIO_ERROR_INVALID_ARGS = -1,
    KNISHIO_ERROR_MEMORY = -2,
    KNISHIO_ERROR_NULL_POINTER = -3,
    KNISHIO_ERROR_NOT_IMPLEMENTED = -4,
    KNISHIO_ERROR_INVALID_STATE = -5,
    
    /* Authentication errors */
    KNISHIO_ERROR_AUTH = -100,
    KNISHIO_ERROR_UNAUTHENTICATED = -101,
    KNISHIO_ERROR_AUTHORIZATION_REJECTED = -102,
    KNISHIO_ERROR_WALLET_CREDENTIAL = -103,
    
    /* Network errors */
    KNISHIO_ERROR_NETWORK = -200,
    KNISHIO_ERROR_INVALID_RESPONSE = -201,
    KNISHIO_ERROR_TIMEOUT = -202,
    KNISHIO_ERROR_HTTP_INIT = -203,
    KNISHIO_ERROR_HTTP_REQUEST = -204,
    
    /* Cryptographic errors */
    KNISHIO_ERROR_CRYPTO = -300,
    KNISHIO_ERROR_SIGNATURE_MALFORMED = -301,
    KNISHIO_ERROR_SIGNATURE_MISMATCH = -302,
    KNISHIO_ERROR_MOLECULAR_HASH_MISMATCH = -303,
    KNISHIO_ERROR_MOLECULAR_HASH_MISSING = -304,
    KNISHIO_ERROR_DECRYPTION_KEY = -305,
    
    /* Transaction errors */
    KNISHIO_ERROR_BALANCE_INSUFFICIENT = -400,
    KNISHIO_ERROR_NEGATIVE_AMOUNT = -401,
    KNISHIO_ERROR_TRANSFER_BALANCE = -402,
    KNISHIO_ERROR_TRANSFER_MALFORMED = -403,
    KNISHIO_ERROR_TRANSFER_MISMATCHED = -404,
    KNISHIO_ERROR_TRANSFER_REMAINDER = -405,
    KNISHIO_ERROR_TRANSFER_TO_SELF = -406,
    KNISHIO_ERROR_TRANSFER_UNBALANCED = -407,
    
    /* Atom/Molecule errors */
    KNISHIO_ERROR_ATOM_INDEX = -500,
    KNISHIO_ERROR_ATOMS_MISSING = -501,
    KNISHIO_ERROR_BATCH_ID = -502,
    KNISHIO_ERROR_STACKABLE_UNIT_AMOUNT = -503,
    KNISHIO_ERROR_STACKABLE_UNIT_DECIMALS = -504,
    
    /* Metadata errors */
    KNISHIO_ERROR_META_MISSING = -600,
    KNISHIO_ERROR_POLICY_INVALID = -601,
    KNISHIO_ERROR_WRONG_TOKEN_TYPE = -602,
    
    /* Wallet errors */
    KNISHIO_ERROR_WALLET_SHADOW = -700,
    KNISHIO_ERROR_CODE = -701,
    
    /* Policy engine errors */
    KNISHIO_ERROR_POLICY_ENGINE_NOT_INITIALIZED = -800,
    KNISHIO_ERROR_POLICY_NOT_FOUND = -801,
    KNISHIO_ERROR_RULE_INVALID = -802,
    KNISHIO_ERROR_CONDITION_INVALID = -803,
    KNISHIO_ERROR_CALLBACK_INVALID = -804,
    KNISHIO_ERROR_POLICY_EVALUATION_FAILED = -805,
    KNISHIO_ERROR_ACCESS_DENIED = -806,
    
    /* JSON parsing errors */
    KNISHIO_ERROR_JSON_PARSE = -900,
    KNISHIO_ERROR_INVALID_JSON = -901,
    KNISHIO_ERROR_WALLET_MISMATCH = -902
} knishio_error_t;
#endif

/**
 * @brief Error context structure for detailed error reporting
 */
typedef struct knishio_error_context {
    knishio_error_t code;                    /**< Error code */
    char *message;                           /**< Error message */
    char *file;                              /**< Source file where error occurred */
    int line;                                /**< Line number where error occurred */
    char *function;                          /**< Function where error occurred */
    struct knishio_error_context *cause;    /**< Underlying cause (chained errors) */
    void *user_data;                         /**< User-defined data */
    bool owns_strings;                       /**< True if context owns string memory */
} knishio_error_context_t;

/**
 * @brief Error callback function type
 */
typedef void (*knishio_error_handler_t)(const knishio_error_context_t *context, void *user_data);

/* Error context management */

/**
 * @brief Create a new error context
 * @param code Error code
 * @param message Error message (copied if non-NULL)
 * @return Error context or NULL on failure
 */
knishio_error_context_t* knishio_error_context_create(knishio_error_t code, const char *message);

/**
 * @brief Create error context with source location
 * @param code Error code
 * @param message Error message (copied if non-NULL)
 * @param file Source file name
 * @param line Line number
 * @param function Function name
 * @return Error context or NULL on failure
 */
knishio_error_context_t* knishio_error_context_create_detailed(
    knishio_error_t code,
    const char *message,
    const char *file,
    int line,
    const char *function
);

/**
 * @brief Clone an error context
 * @param ctx Error context to clone
 * @return Cloned context or NULL on failure
 */
knishio_error_context_t* knishio_error_context_clone(const knishio_error_context_t *ctx);

/**
 * @brief Destroy error context and free memory
 * @param ctx Error context
 */
void knishio_error_context_destroy(knishio_error_context_t *ctx);

/* Error context modification */

/**
 * @brief Set error message
 * @param ctx Error context
 * @param message New message (copied)
 * @return 0 on success, -1 on failure
 */
int knishio_error_context_set_message(knishio_error_context_t *ctx, const char *message);

/**
 * @brief Set source location
 * @param ctx Error context
 * @param file Source file
 * @param line Line number
 * @param function Function name
 * @return 0 on success, -1 on failure
 */
int knishio_error_context_set_location(knishio_error_context_t *ctx, const char *file, int line, const char *function);

/**
 * @brief Chain error contexts (set cause)
 * @param ctx Error context
 * @param cause Underlying cause (takes ownership)
 * @return 0 on success, -1 on failure
 */
int knishio_error_context_set_cause(knishio_error_context_t *ctx, knishio_error_context_t *cause);

/**
 * @brief Set user data
 * @param ctx Error context
 * @param user_data User data pointer
 */
void knishio_error_context_set_user_data(knishio_error_context_t *ctx, void *user_data);

/* Error context inspection */

/**
 * @brief Get error code
 * @param ctx Error context
 * @return Error code
 */
knishio_error_t knishio_error_context_get_code(const knishio_error_context_t *ctx);

/**
 * @brief Get error message
 * @param ctx Error context
 * @return Error message (may be NULL)
 */
const char* knishio_error_context_get_message(const knishio_error_context_t *ctx);

/**
 * @brief Get source file
 * @param ctx Error context
 * @return Source file (may be NULL)
 */
const char* knishio_error_context_get_file(const knishio_error_context_t *ctx);

/**
 * @brief Get line number
 * @param ctx Error context
 * @return Line number (0 if not set)
 */
int knishio_error_context_get_line(const knishio_error_context_t *ctx);

/**
 * @brief Get function name
 * @param ctx Error context
 * @return Function name (may be NULL)
 */
const char* knishio_error_context_get_function(const knishio_error_context_t *ctx);

/**
 * @brief Get underlying cause
 * @param ctx Error context
 * @return Underlying cause (may be NULL)
 */
const knishio_error_context_t* knishio_error_context_get_cause(const knishio_error_context_t *ctx);

/**
 * @brief Get user data
 * @param ctx Error context
 * @return User data pointer (may be NULL)
 */
void* knishio_error_context_get_user_data(const knishio_error_context_t *ctx);

/* Error formatting and reporting */

/**
 * @brief Format error context as string
 * @param ctx Error context
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written (excluding null terminator)
 */
int knishio_error_context_format(const knishio_error_context_t *ctx, char *buffer, size_t buffer_size);

/**
 * @brief Print error context to stderr
 * @param ctx Error context
 */
void knishio_error_context_print(const knishio_error_context_t *ctx);

/**
 * @brief Print full error chain to stderr
 * @param ctx Error context
 */
void knishio_error_context_print_chain(const knishio_error_context_t *ctx);

/* Global error handling */

/**
 * @brief Set global error handler
 * @param handler Error handler function
 * @param user_data User data passed to handler
 */
void knishio_set_error_handler(knishio_error_handler_t handler, void *user_data);

/**
 * @brief Get current global error handler
 * @param user_data Output for user data (may be NULL)
 * @return Current error handler (may be NULL)
 */
knishio_error_handler_t knishio_get_error_handler(void **user_data);

/**
 * @brief Trigger global error handler
 * @param ctx Error context
 */
void knishio_trigger_error_handler(const knishio_error_context_t *ctx);

/* Convenience macros for error creation with location */

#define KNISHIO_ERROR_CREATE(code, message) \
    knishio_error_context_create_detailed(code, message, __FILE__, __LINE__, __func__)

#define KNISHIO_ERROR_CREATE_SIMPLE(code) \
    knishio_error_context_create_detailed(code, NULL, __FILE__, __LINE__, __func__)

/* Error string conversion */

/**
 * @brief Convert error code to string
 * @param error Error code
 * @return Error string (never NULL)
 */
const char* knishio_error_to_string(knishio_error_t error);

/**
 * @brief Check if error code indicates success
 * @param error Error code
 * @return true if success, false otherwise
 */
static inline bool knishio_error_is_success(knishio_error_t error) {
    return error == KNISHIO_SUCCESS;
}

/**
 * @brief Check if error code indicates failure
 * @param error Error code
 * @return true if failure, false otherwise
 */
static inline bool knishio_error_is_failure(knishio_error_t error) {
    return error != KNISHIO_SUCCESS;
}

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_ERROR_CONTEXT_H */

