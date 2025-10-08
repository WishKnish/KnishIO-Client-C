/**
 * @file exceptions.c
 * @brief Essential exception implementation for KnishIO C SDK
 * 
 * Implements JavaScript-compatible exception handling with C17 best practices.
 * KISS design covering 80% of error scenarios with minimal complexity.
 * Simple KISS design following ultrathink methodology.
 */

#include "knishio/exceptions.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Internal helper function declarations */
static knishio_error_t copy_exception_string(char** dest, const char* src);
static void free_exception_string(char** field);

/* Exception lifecycle functions */

knishio_error_t knishio_exception_create(
    knishio_exception_t** exception,
    knishio_exception_type_t type,
    const char* message
) {
    if (!exception) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate exception structure */
    knishio_exception_t* ex = knishio_malloc(sizeof(knishio_exception_t));
    if (!ex) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Initialize all fields to safe defaults */
    memset(ex, 0, sizeof(knishio_exception_t));
    ex->type = type;
    ex->error_code = (int)type;

    /* Copy message if provided */
    if (message) {
        knishio_error_t error = copy_exception_string(&ex->message, message);
        if (error != KNISHIO_SUCCESS) {
            knishio_free(ex);
            return error;
        }
    }

    *exception = ex;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_exception_create_detailed(
    knishio_exception_t** exception,
    knishio_exception_type_t type,
    const char* message,
    const char* details
) {
    knishio_error_t error = knishio_exception_create(exception, type, message);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Copy details if provided */
    if (details) {
        error = copy_exception_string(&(*exception)->details, details);
        if (error != KNISHIO_SUCCESS) {
            knishio_exception_free(*exception);
            *exception = NULL;
            return error;
        }
    }

    return KNISHIO_SUCCESS;
}

void knishio_exception_free(knishio_exception_t* exception) {
    if (!exception) {
        return;
    }

    /* Free string fields */
    free_exception_string(&exception->message);
    free_exception_string(&exception->details);

    /* Free the exception structure itself */
    knishio_free(exception);
}

/* Exception property functions */

knishio_exception_type_t knishio_exception_get_type(const knishio_exception_t* exception) {
    return exception ? exception->type : 0;
}

const char* knishio_exception_get_message(const knishio_exception_t* exception) {
    return exception ? exception->message : NULL;
}

const char* knishio_exception_get_details(const knishio_exception_t* exception) {
    return exception ? exception->details : NULL;
}

int knishio_exception_get_error_code(const knishio_exception_t* exception) {
    return exception ? exception->error_code : 0;
}

/* Convenience factory functions for the 5 essential exceptions */

knishio_error_t knishio_exception_invalid_response(
    knishio_exception_t** exception,
    const char* message
) {
    const char* default_message = "Invalid or malformed response received";
    return knishio_exception_create(
        exception,
        KNISHIO_EXCEPTION_INVALID_RESPONSE,
        message ? message : default_message
    );
}

knishio_error_t knishio_exception_unauthenticated(
    knishio_exception_t** exception,
    const char* message
) {
    const char* default_message = "Authentication required to perform this operation";
    return knishio_exception_create(
        exception,
        KNISHIO_EXCEPTION_UNAUTHENTICATED,
        message ? message : default_message
    );
}

knishio_error_t knishio_exception_atoms_missing(
    knishio_exception_t** exception,
    const char* message
) {
    const char* default_message = "Required atoms are missing from the molecule";
    return knishio_exception_create(
        exception,
        KNISHIO_EXCEPTION_ATOMS_MISSING,
        message ? message : default_message
    );
}

knishio_error_t knishio_exception_balance_insufficient(
    knishio_exception_t** exception,
    const char* balance_required,
    const char* balance_available
) {
    const char* default_message = "Insufficient balance for this operation";
    
    /* Build detailed message if balances provided */
    if (balance_required && balance_available) {
        char details_buffer[512];
        snprintf(details_buffer, sizeof(details_buffer),
            "Required: %s, Available: %s", balance_required, balance_available);
        
        return knishio_exception_create_detailed(
            exception,
            KNISHIO_EXCEPTION_BALANCE_INSUFFICIENT,
            default_message,
            details_buffer
        );
    }

    return knishio_exception_create(
        exception,
        KNISHIO_EXCEPTION_BALANCE_INSUFFICIENT,
        default_message
    );
}

knishio_error_t knishio_exception_signature_mismatch(
    knishio_exception_t** exception,
    const char* expected_signature,
    const char* actual_signature
) {
    const char* default_message = "Signature validation failed";
    
    /* Build detailed message if signatures provided */
    if (expected_signature && actual_signature) {
        char details_buffer[1024];
        snprintf(details_buffer, sizeof(details_buffer),
            "Expected: %.200s, Actual: %.200s", expected_signature, actual_signature);
        
        return knishio_exception_create_detailed(
            exception,
            KNISHIO_EXCEPTION_SIGNATURE_MISMATCH,
            default_message,
            details_buffer
        );
    }

    return knishio_exception_create(
        exception,
        KNISHIO_EXCEPTION_SIGNATURE_MISMATCH,
        default_message
    );
}

/* Utility functions */

const char* knishio_exception_type_to_string(knishio_exception_type_t type) {
    switch (type) {
        case KNISHIO_EXCEPTION_INVALID_RESPONSE:
            return "InvalidResponseException";
        case KNISHIO_EXCEPTION_UNAUTHENTICATED:
            return "UnauthenticatedException";
        case KNISHIO_EXCEPTION_ATOMS_MISSING:
            return "AtomsMissingException";
        case KNISHIO_EXCEPTION_BALANCE_INSUFFICIENT:
            return "BalanceInsufficientException";
        case KNISHIO_EXCEPTION_SIGNATURE_MISMATCH:
            return "SignatureMismatchException";
        default:
            return "UnknownException";
    }
}

bool knishio_exception_is_type(const knishio_exception_t* exception, knishio_exception_type_t type) {
    return exception && exception->type == type;
}

/* Internal helper functions */

static knishio_error_t copy_exception_string(char** dest, const char* src) {
    if (!dest || !src) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    size_t len = strlen(src);
    char* copy = knishio_malloc(len + 1);
    if (!copy) {
        return KNISHIO_ERROR_MEMORY;
    }

    strcpy(copy, src);
    *dest = copy;
    return KNISHIO_SUCCESS;
}

static void free_exception_string(char** field) {
    if (field && *field) {
        knishio_free(*field);
        *field = NULL;
    }
}