#ifndef KNISHIO_ERROR_H
#define KNISHIO_ERROR_H

/**
 * @file error.h
 * @brief Consolidated error definitions for KnishIO SDK
 * 
 * Simple error handling following JS SDK's KISS principle.
 * Maps JavaScript exceptions to C error codes.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Include the full error context system which has all error codes */
#include "knishio/error/context.h"

/* Re-export the main error type for convenience */
#ifndef KNISHIO_ERROR_T_DEFINED
typedef enum knishio_error knishio_error_t;
#endif

/* Common error checking macros */
#define KNISHIO_IS_SUCCESS(x) ((x) == KNISHIO_SUCCESS)
#define KNISHIO_IS_ERROR(x) ((x) != KNISHIO_SUCCESS)

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_ERROR_H */
