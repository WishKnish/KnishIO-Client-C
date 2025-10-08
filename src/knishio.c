#include "knishio/knishio.h"
#include "knishio/error/context.h"
#include <string.h>

/* Global initialization state */
static bool g_initialized = false;

/* Version functions */
const char* knishio_version(void) {
    return KNISHIO_VERSION_STRING;
}

/* Initialization functions */
knishio_error_t knishio_init(void) {
    if (g_initialized) {
        return KNISHIO_SUCCESS;
    }
    
    /* Initialize any global state here */
    
    g_initialized = true;
    return KNISHIO_SUCCESS;
}

void knishio_cleanup(void) {
    if (!g_initialized) {
        return;
    }
    
    /* Cleanup any global state here */
    
    g_initialized = false;
}

/* Error utility function - implemented in error/context.c */