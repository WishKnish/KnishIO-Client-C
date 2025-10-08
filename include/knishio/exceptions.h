#ifndef KNISHIO_EXCEPTIONS_H
#define KNISHIO_EXCEPTIONS_H

/**
 * @file exceptions.h
 * @brief Essential exception types for KnishIO C SDK
 * 
 * Implements the 5 most critical exception types covering 80% of error scenarios.
 * KISS design with simple error codes and message handling.
 * JavaScript SDK compatible error semantics.
 */

#include "knishio/knishio.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Essential exception types (80/20 rule coverage) */
typedef enum {
    KNISHIO_EXCEPTION_INVALID_RESPONSE = 1000,    /**< Invalid or malformed response */
    KNISHIO_EXCEPTION_UNAUTHENTICATED = 1001,     /**< Authentication required */
    KNISHIO_EXCEPTION_ATOMS_MISSING = 1002,       /**< Required atoms missing */
    KNISHIO_EXCEPTION_BALANCE_INSUFFICIENT = 1003, /**< Insufficient balance */
    KNISHIO_EXCEPTION_SIGNATURE_MISMATCH = 1004   /**< Signature validation failed */
} knishio_exception_type_t;

/* Exception structure */
typedef struct knishio_exception {
    knishio_exception_type_t type;     /**< Exception type */
    char* message;                     /**< Error message (allocated) */
    int error_code;                    /**< Numeric error code */
    char* details;                     /**< Additional details (optional) */
} knishio_exception_t;

/* Exception lifecycle */

/**
 * @brief Create exception with message
 * @param exception Output exception pointer
 * @param type Exception type
 * @param message Error message
 * @return Success or error code
 */
knishio_error_t knishio_exception_create(
    knishio_exception_t** exception,
    knishio_exception_type_t type,
    const char* message
);

/**
 * @brief Create exception with details
 * @param exception Output exception pointer
 * @param type Exception type
 * @param message Error message
 * @param details Additional details
 * @return Success or error code
 */
knishio_error_t knishio_exception_create_detailed(
    knishio_exception_t** exception,
    knishio_exception_type_t type,
    const char* message,
    const char* details
);

/**
 * @brief Free exception
 * @param exception Exception to free
 */
void knishio_exception_free(knishio_exception_t* exception);

/* Exception properties */

/**
 * @brief Get exception type
 * @param exception Exception instance
 * @return Exception type
 */
knishio_exception_type_t knishio_exception_get_type(const knishio_exception_t* exception);

/**
 * @brief Get exception message
 * @param exception Exception instance
 * @return Error message (do not free)
 */
const char* knishio_exception_get_message(const knishio_exception_t* exception);

/**
 * @brief Get exception details
 * @param exception Exception instance
 * @return Details string or NULL (do not free)
 */
const char* knishio_exception_get_details(const knishio_exception_t* exception);

/**
 * @brief Get exception error code
 * @param exception Exception instance
 * @return Numeric error code
 */
int knishio_exception_get_error_code(const knishio_exception_t* exception);

/* Convenience factory functions for the 5 essential exceptions */

/**
 * @brief Create InvalidResponseException
 * @param exception Output exception pointer
 * @param message Error message (optional, will use default if NULL)
 * @return Success or error code
 */
knishio_error_t knishio_exception_invalid_response(
    knishio_exception_t** exception,
    const char* message
);

/**
 * @brief Create UnauthenticatedException
 * @param exception Output exception pointer
 * @param message Error message (optional, will use default if NULL)
 * @return Success or error code
 */
knishio_error_t knishio_exception_unauthenticated(
    knishio_exception_t** exception,
    const char* message
);

/**
 * @brief Create AtomsMissingException
 * @param exception Output exception pointer
 * @param message Error message (optional, will use default if NULL)
 * @return Success or error code
 */
knishio_error_t knishio_exception_atoms_missing(
    knishio_exception_t** exception,
    const char* message
);

/**
 * @brief Create BalanceInsufficientException
 * @param exception Output exception pointer
 * @param balance_required Required balance (optional)
 * @param balance_available Available balance (optional)
 * @return Success or error code
 */
knishio_error_t knishio_exception_balance_insufficient(
    knishio_exception_t** exception,
    const char* balance_required,
    const char* balance_available
);

/**
 * @brief Create SignatureMismatchException
 * @param exception Output exception pointer
 * @param expected_signature Expected signature (optional)
 * @param actual_signature Actual signature (optional)
 * @return Success or error code
 */
knishio_error_t knishio_exception_signature_mismatch(
    knishio_exception_t** exception,
    const char* expected_signature,
    const char* actual_signature
);

/* Utility functions */

/**
 * @brief Convert exception type to string
 * @param type Exception type
 * @return String representation
 */
const char* knishio_exception_type_to_string(knishio_exception_type_t type);

/**
 * @brief Check if exception is of specific type
 * @param exception Exception to check
 * @param type Type to check against
 * @return True if matches type
 */
bool knishio_exception_is_type(const knishio_exception_t* exception, knishio_exception_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_EXCEPTIONS_H */
