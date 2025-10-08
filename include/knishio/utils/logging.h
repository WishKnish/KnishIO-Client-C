#ifndef KNISHIO_UTILS_LOGGING_H
#define KNISHIO_UTILS_LOGGING_H

/**
 * @file logging.h
 * @brief Minimal logging utilities for KnishIO SDK
 * 
 * Following JS SDK KISS principle - only 8 log calls in entire SDK
 */

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log levels */
typedef enum {
    KNISHIO_LOG_ERROR = 0,
    KNISHIO_LOG_WARN = 1,
    KNISHIO_LOG_INFO = 2,
    KNISHIO_LOG_DEBUG = 3,
    KNISHIO_LOG_TRACE = 4
} knishio_log_level_t;

/* Main logging function */
void knishio_log(knishio_log_level_t level, const char *format, ...);

/* Initialize logging system */
void knishio_logging_init(void);

/* Set log level */
void knishio_logging_set_level(knishio_log_level_t level);

/* Enable/disable logging */
void knishio_logging_set_enabled(bool enabled);

/* Cleanup logging system */
void knishio_logging_cleanup(void);

/* Convenience macros - minimal usage following JS SDK pattern */
#define KNISHIO_LOG_ERROR(...) knishio_log(KNISHIO_LOG_ERROR, __VA_ARGS__)
#define KNISHIO_LOG_WARN(...)  knishio_log(KNISHIO_LOG_WARN, __VA_ARGS__)
#define KNISHIO_LOG_INFO(...)  knishio_log(KNISHIO_LOG_INFO, __VA_ARGS__)
#define KNISHIO_LOG_DEBUG(...) knishio_log(KNISHIO_LOG_DEBUG, __VA_ARGS__)
#define KNISHIO_LOG_TRACE(...) knishio_log(KNISHIO_LOG_TRACE, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_UTILS_LOGGING_H */