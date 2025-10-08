#include "knishio/error/context.h"
#include "knishio/utils/memory.h"
#include <stdio.h>
#include <string.h>

/* Global error handler */
static knishio_error_handler_t g_error_handler = NULL;
static void *g_error_handler_data = NULL;

/* Error message lookup table */
static const struct {
    knishio_error_t code;
    const char *message;
} error_messages[] = {
    {KNISHIO_SUCCESS, "Success"},
    
    /* General errors */
    {KNISHIO_ERROR_INVALID_ARGS, "Invalid arguments"},
    {KNISHIO_ERROR_MEMORY, "Memory allocation failed"},
    {KNISHIO_ERROR_NULL_POINTER, "Null pointer access"},
    {KNISHIO_ERROR_NOT_IMPLEMENTED, "Feature not implemented"},
    
    /* Authentication errors */
    {KNISHIO_ERROR_AUTH, "Authentication failed"},
    {KNISHIO_ERROR_UNAUTHENTICATED, "Unauthenticated access"},
    {KNISHIO_ERROR_AUTHORIZATION_REJECTED, "Authorization rejected"},
    {KNISHIO_ERROR_WALLET_CREDENTIAL, "Wallet credential error"},
    
    /* Network errors */
    {KNISHIO_ERROR_NETWORK, "Network error"},
    {KNISHIO_ERROR_INVALID_RESPONSE, "Invalid response"},
    {KNISHIO_ERROR_TIMEOUT, "Operation timeout"},
    
    /* Cryptographic errors */
    {KNISHIO_ERROR_CRYPTO, "Cryptographic error"},
    {KNISHIO_ERROR_SIGNATURE_MALFORMED, "Malformed signature"},
    {KNISHIO_ERROR_SIGNATURE_MISMATCH, "Signature mismatch"},
    {KNISHIO_ERROR_MOLECULAR_HASH_MISMATCH, "Molecular hash mismatch"},
    {KNISHIO_ERROR_MOLECULAR_HASH_MISSING, "Molecular hash missing"},
    {KNISHIO_ERROR_DECRYPTION_KEY, "Decryption key error"},
    
    /* Transaction errors */
    {KNISHIO_ERROR_BALANCE_INSUFFICIENT, "Insufficient balance"},
    {KNISHIO_ERROR_NEGATIVE_AMOUNT, "Negative amount"},
    {KNISHIO_ERROR_TRANSFER_BALANCE, "Transfer balance error"},
    {KNISHIO_ERROR_TRANSFER_MALFORMED, "Malformed transfer"},
    {KNISHIO_ERROR_TRANSFER_MISMATCHED, "Transfer mismatch"},
    {KNISHIO_ERROR_TRANSFER_REMAINDER, "Transfer remainder error"},
    {KNISHIO_ERROR_TRANSFER_TO_SELF, "Transfer to self"},
    {KNISHIO_ERROR_TRANSFER_UNBALANCED, "Unbalanced transfer"},
    
    /* Atom/Molecule errors */
    {KNISHIO_ERROR_ATOM_INDEX, "Atom index error"},
    {KNISHIO_ERROR_ATOMS_MISSING, "Atoms missing"},
    {KNISHIO_ERROR_BATCH_ID, "Batch ID error"},
    {KNISHIO_ERROR_STACKABLE_UNIT_AMOUNT, "Stackable unit amount error"},
    {KNISHIO_ERROR_STACKABLE_UNIT_DECIMALS, "Stackable unit decimals error"},
    
    /* Metadata errors */
    {KNISHIO_ERROR_META_MISSING, "Metadata missing"},
    {KNISHIO_ERROR_POLICY_INVALID, "Invalid policy"},
    {KNISHIO_ERROR_WRONG_TOKEN_TYPE, "Wrong token type"},
    
    /* Wallet errors */
    {KNISHIO_ERROR_WALLET_SHADOW, "Wallet shadow error"},
    {KNISHIO_ERROR_CODE, "Code error"}
};

static const size_t error_messages_count = sizeof(error_messages) / sizeof(error_messages[0]);

/* Error context management */

knishio_error_context_t* knishio_error_context_create(knishio_error_t code, const char *message) {
    knishio_error_context_t *ctx = knishio_malloc(sizeof(knishio_error_context_t));
    if (ctx == NULL) {
        return NULL;
    }
    
    ctx->code = code;
    ctx->message = message ? knishio_strdup(message) : NULL;
    ctx->file = NULL;
    ctx->line = 0;
    ctx->function = NULL;
    ctx->cause = NULL;
    ctx->user_data = NULL;
    ctx->owns_strings = true;
    
    return ctx;
}

knishio_error_context_t* knishio_error_context_create_detailed(
    knishio_error_t code,
    const char *message,
    const char *file,
    int line,
    const char *function) {
    
    knishio_error_context_t *ctx = knishio_error_context_create(code, message);
    if (ctx == NULL) {
        return NULL;
    }
    
    ctx->file = file ? knishio_strdup(file) : NULL;
    ctx->line = line;
    ctx->function = function ? knishio_strdup(function) : NULL;
    
    return ctx;
}

knishio_error_context_t* knishio_error_context_clone(const knishio_error_context_t *ctx) {
    if (ctx == NULL) {
        return NULL;
    }
    
    knishio_error_context_t *clone = knishio_error_context_create_detailed(
        ctx->code,
        ctx->message,
        ctx->file,
        ctx->line,
        ctx->function
    );
    
    if (clone == NULL) {
        return NULL;
    }
    
    /* Clone cause chain */
    if (ctx->cause != NULL) {
        clone->cause = knishio_error_context_clone(ctx->cause);
    }
    
    clone->user_data = ctx->user_data;
    
    return clone;
}

void knishio_error_context_destroy(knishio_error_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    if (ctx->owns_strings) {
        knishio_free(ctx->message);
        knishio_free(ctx->file);
        knishio_free(ctx->function);
    }
    
    if (ctx->cause != NULL) {
        knishio_error_context_destroy(ctx->cause);
    }
    
    knishio_free(ctx);
}

/* Error context modification */

int knishio_error_context_set_message(knishio_error_context_t *ctx, const char *message) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (ctx->owns_strings) {
        knishio_free(ctx->message);
    }
    
    ctx->message = message ? knishio_strdup(message) : NULL;
    ctx->owns_strings = true;
    
    return 0;
}

int knishio_error_context_set_location(knishio_error_context_t *ctx, const char *file, int line, const char *function) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (ctx->owns_strings) {
        knishio_free(ctx->file);
        knishio_free(ctx->function);
    }
    
    ctx->file = file ? knishio_strdup(file) : NULL;
    ctx->line = line;
    ctx->function = function ? knishio_strdup(function) : NULL;
    ctx->owns_strings = true;
    
    return 0;
}

int knishio_error_context_set_cause(knishio_error_context_t *ctx, knishio_error_context_t *cause) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (ctx->cause != NULL) {
        knishio_error_context_destroy(ctx->cause);
    }
    
    ctx->cause = cause;
    return 0;
}

void knishio_error_context_set_user_data(knishio_error_context_t *ctx, void *user_data) {
    if (ctx != NULL) {
        ctx->user_data = user_data;
    }
}

/* Error context accessors */

knishio_error_t knishio_error_context_get_code(const knishio_error_context_t *ctx) {
    return ctx ? ctx->code : KNISHIO_ERROR_NULL_POINTER;
}

const char* knishio_error_context_get_message(const knishio_error_context_t *ctx) {
    if (ctx == NULL) {
        return "";
    }
    
    if (ctx->message != NULL) {
        return ctx->message;
    }
    
    /* Fall back to default message for error code */
    return knishio_error_to_string(ctx->code);
}

const char* knishio_error_context_get_file(const knishio_error_context_t *ctx) {
    return ctx ? ctx->file : NULL;
}

int knishio_error_context_get_line(const knishio_error_context_t *ctx) {
    return ctx ? ctx->line : 0;
}

const char* knishio_error_context_get_function(const knishio_error_context_t *ctx) {
    return ctx ? ctx->function : NULL;
}

const knishio_error_context_t* knishio_error_context_get_cause(const knishio_error_context_t *ctx) {
    return ctx ? ctx->cause : NULL;
}

void* knishio_error_context_get_user_data(const knishio_error_context_t *ctx) {
    return ctx ? ctx->user_data : NULL;
}

/* Error reporting */

int knishio_error_context_format(const knishio_error_context_t *ctx, char *buffer, size_t buffer_size) {
    if (ctx == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }
    
    const char *message = knishio_error_context_get_message(ctx);
    int offset = 0;
    
    /* Format main error */
    offset += snprintf(buffer + offset, buffer_size - offset, 
                      "Error %d: %s", ctx->code, message);
    
    if (ctx->file && ctx->function) {
        offset += snprintf(buffer + offset, buffer_size - offset,
                          " at %s:%d in %s()", ctx->file, ctx->line, ctx->function);
    } else if (ctx->file) {
        offset += snprintf(buffer + offset, buffer_size - offset,
                          " at %s:%d", ctx->file, ctx->line);
    } else if (ctx->function) {
        offset += snprintf(buffer + offset, buffer_size - offset,
                          " in %s()", ctx->function);
    }
    
    return offset;
}

void knishio_error_context_print(const knishio_error_context_t *ctx) {
    char buffer[1024];
    int result = knishio_error_context_format(ctx, buffer, sizeof(buffer));
    if (result >= 0) {
        fprintf(stderr, "%s\n", buffer);
    } else {
        fprintf(stderr, "Failed to format error context\n");
    }
}

void knishio_error_context_print_chain(const knishio_error_context_t *ctx) {
    const knishio_error_context_t *current = ctx;
    while (current != NULL) {
        knishio_error_context_print(current);
        current = current->cause;
        if (current != NULL) {
            fprintf(stderr, "  Caused by: ");
        }
    }
}

/* Global error handling */

void knishio_set_error_handler(knishio_error_handler_t handler, void *user_data) {
    g_error_handler = handler;
    g_error_handler_data = user_data;
}

knishio_error_handler_t knishio_get_error_handler(void **user_data) {
    if (user_data != NULL) {
        *user_data = g_error_handler_data;
    }
    return g_error_handler;
}

void knishio_trigger_error_handler(const knishio_error_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    if (g_error_handler != NULL) {
        g_error_handler(ctx, g_error_handler_data);
    } else {
        /* Default: print to stderr */
        knishio_error_context_print_chain(ctx);
    }
}

/* Error utility functions */

const char* knishio_error_to_string(knishio_error_t code) {
    for (size_t i = 0; i < error_messages_count; i++) {
        if (error_messages[i].code == code) {
            return error_messages[i].message;
        }
    }
    
    return "Unknown error";
}

bool knishio_error_is_category(knishio_error_t code, int category_base) {
    if (code >= 0) {
        return false; /* Success codes */
    }
    
    int code_category = (code / 100) * 100;
    return code_category == category_base;
}