/**
 * @file logging.c
 * @brief Minimal logging implementation for KnishIO SDK
 * 
 * Following JS SDK KISS principle - simple logging without complexity
 */

#include "knishio/utils/logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* Global logging configuration */
static knishio_log_level_t g_log_level = KNISHIO_LOG_ERROR;
static bool g_logging_enabled = false;

/* Initialize logging system */
void knishio_logging_init(void) {
    g_logging_enabled = true;
    g_log_level = KNISHIO_LOG_INFO;
}

/* Set log level */
void knishio_logging_set_level(knishio_log_level_t level) {
    g_log_level = level;
}

/* Set logging enabled state */
void knishio_logging_set_enabled(bool enabled) {
    g_logging_enabled = enabled;
}

/* Main logging function - minimal implementation following JS SDK pattern */
void knishio_log(knishio_log_level_t level, const char *format, ...) {
    if (!g_logging_enabled || level > g_log_level) {
        return;
    }
    
    /* Log level prefixes */
    const char *prefix = "";
    switch (level) {
        case KNISHIO_LOG_ERROR:
            prefix = "[ERROR]";
            break;
        case KNISHIO_LOG_WARN:
            prefix = "[WARN] ";
            break;
        case KNISHIO_LOG_INFO:
            prefix = "[INFO] ";
            break;
        case KNISHIO_LOG_DEBUG:
            prefix = "[DEBUG]";
            break;
        case KNISHIO_LOG_TRACE:
            prefix = "[TRACE]";
            break;
    }
    
    /* Print log message */
    fprintf(stderr, "%s ", prefix);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

/* Cleanup logging system */
void knishio_logging_cleanup(void) {
    g_logging_enabled = false;
}
