/**
 * @file client_auth.c
 * @brief Client-level authentication integration implementation
 * 
 * Provides automatic authentication management similar to JavaScript SDK
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "knishio/client_auth.h"
#include "knishio/client.h"
#include "knishio/operations/auth.h"
#include "knishio/auth_token.h"
#include "knishio/utils/string.h"
#include "knishio/json/builder.h"
#include "client_internal.h"

/* Internal helper functions */
static knishio_error_t knishio_client_get_auth_state(
    knishio_client_t* client,
    knishio_client_auth_state_t** state
);
static void knishio_client_trigger_auth_event(
    knishio_client_t* client,
    const char* event_type
);
static char* knishio_strdup_safe(const char* str);

/* Configure client authentication */
knishio_error_t knishio_client_configure_auth(
    knishio_client_t* client,
    const knishio_client_auth_config_t* config
) {
    if (!client || !config) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_client_auth_state_t* state = NULL;
    knishio_error_t error = knishio_client_get_auth_state(client, &state);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Update configuration */
    if (config->secret) {
        if (state->secret) {
            free(state->secret);
        }
        state->secret = knishio_strdup_safe(config->secret);
    }

    if (config->cell_slug) {
        if (state->cell_slug) {
            free(state->cell_slug);
        }
        state->cell_slug = knishio_strdup_safe(config->cell_slug);
    }

    state->encrypt = config->encrypt;
    state->auto_refresh = config->auto_refresh;
    state->refresh_threshold_ms = config->refresh_threshold_ms > 0 
        ? config->refresh_threshold_ms 
        : 300000; /* 5 minutes default */

    return KNISHIO_SUCCESS;
}

/* Authenticate client with profile credentials */
knishio_error_t knishio_client_authenticate(
    knishio_client_t* client,
    const char* secret,
    bool encrypt
) {
    if (!client || !secret) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    /* Configure authentication */
    knishio_client_auth_config_t config = {
        .secret = (char*)secret,
        .encrypt = encrypt,
        .auto_refresh = true,
        .refresh_threshold_ms = 300000 /* 5 minutes */
    };

    knishio_error_t error = knishio_client_configure_auth(client, &config);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Request profile authentication token */
    knishio_request_profile_auth_token_params_t params = {
        .secret = secret,
        .encrypt = encrypt
    };

    knishio_request_profile_auth_token_result_t* result = NULL;
    error = knishio_client_request_profile_auth_token(client, &params, &result);
    
    if (error != KNISHIO_SUCCESS || !result || !result->success) {
        if (result) {
            knishio_request_profile_auth_token_result_free(result);
        }
        knishio_client_trigger_auth_event(client, "failed");
        return error != KNISHIO_SUCCESS ? error : KNISHIO_ERROR_AUTH_FAILED;
    }

    /* Create and set auth token */
    knishio_auth_token_config_t token_config = {
        .token = result->token,
        .expires_at = time(NULL) + 3600, /* 1 hour default */
        .encrypt = encrypt,
        .pubkey = NULL
    };

    knishio_auth_token_t* auth_token = NULL;
    error = knishio_auth_token_create(&auth_token, &token_config);
    
    if (error == KNISHIO_SUCCESS) {
        error = knishio_client_set_auth_token(client, auth_token);
        if (error == KNISHIO_SUCCESS) {
            knishio_client_trigger_auth_event(client, "authenticated");
        }
    }

    knishio_request_profile_auth_token_result_free(result);

    if (error != KNISHIO_SUCCESS && auth_token) {
        knishio_auth_token_cleanup(auth_token);
    }

    return error;
}

/* Authenticate client as guest */
knishio_error_t knishio_client_authenticate_guest(
    knishio_client_t* client,
    const char* cell_slug,
    bool encrypt
) {
    if (!client || !cell_slug) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    /* Configure authentication */
    knishio_client_auth_config_t config = {
        .cell_slug = (char*)cell_slug,
        .encrypt = encrypt,
        .auto_refresh = true,
        .refresh_threshold_ms = 300000 /* 5 minutes */
    };

    knishio_error_t error = knishio_client_configure_auth(client, &config);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Request guest authentication token */
    knishio_request_guest_auth_token_params_t params = {
        .cell_slug = cell_slug,
        .encrypt = encrypt
    };

    knishio_request_guest_auth_token_result_t* result = NULL;
    error = knishio_client_request_guest_auth_token(client, &params, &result);
    
    if (error != KNISHIO_SUCCESS || !result || !result->success) {
        if (result) {
            knishio_request_guest_auth_token_result_free(result);
        }
        knishio_client_trigger_auth_event(client, "failed");
        return error != KNISHIO_SUCCESS ? error : KNISHIO_ERROR_AUTH_FAILED;
    }

    /* Create and set auth token */
    knishio_auth_token_config_t token_config = {
        .token = result->token,
        .expires_at = time(NULL) + 3600, /* 1 hour default */
        .encrypt = encrypt,
        .pubkey = NULL
    };

    knishio_auth_token_t* auth_token = NULL;
    error = knishio_auth_token_create(&auth_token, &token_config);
    
    if (error == KNISHIO_SUCCESS) {
        error = knishio_client_set_auth_token(client, auth_token);
        if (error == KNISHIO_SUCCESS) {
            knishio_client_trigger_auth_event(client, "authenticated");
        }
    }

    knishio_request_guest_auth_token_result_free(result);

    if (error != KNISHIO_SUCCESS && auth_token) {
        knishio_auth_token_cleanup(auth_token);
    }

    return error;
}

/* Check if client is authenticated */
bool knishio_client_is_authenticated(const knishio_client_t* client) {
    if (!client) {
        return false;
    }

    knishio_client_auth_state_t* state = NULL;
    if (knishio_client_get_auth_state((knishio_client_t*)client, &state) != KNISHIO_SUCCESS) {
        return false;
    }

    if (!state->current_token) {
        return false;
    }

    return !knishio_auth_token_is_expired(state->current_token);
}

/* Get current authentication token */
knishio_auth_token_t* knishio_client_get_auth_token(const knishio_client_t* client) {
    if (!client) {
        return NULL;
    }

    knishio_client_auth_state_t* state = NULL;
    if (knishio_client_get_auth_state((knishio_client_t*)client, &state) != KNISHIO_SUCCESS) {
        return NULL;
    }

    return state->current_token;
}

/* Set authentication token */
knishio_error_t knishio_client_set_auth_token(
    knishio_client_t* client,
    knishio_auth_token_t* auth_token
) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_client_auth_state_t* state = NULL;
    knishio_error_t error = knishio_client_get_auth_state(client, &state);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Clean up previous token */
    if (state->current_token) {
        knishio_auth_token_cleanup(state->current_token);
    }

    state->current_token = auth_token;
    return KNISHIO_SUCCESS;
}

/* Clear authentication token */
knishio_error_t knishio_client_clear_auth(knishio_client_t* client) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_client_auth_state_t* state = NULL;
    knishio_error_t error = knishio_client_get_auth_state(client, &state);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    if (state->current_token) {
        knishio_auth_token_cleanup(state->current_token);
        state->current_token = NULL;
    }

    return KNISHIO_SUCCESS;
}

/* Refresh authentication token */
knishio_error_t knishio_client_refresh_auth(knishio_client_t* client) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_client_auth_state_t* state = NULL;
    knishio_error_t error = knishio_client_get_auth_state(client, &state);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    if (!state->current_token) {
        return KNISHIO_ERROR_AUTH_NO_TOKEN;
    }

    /* Determine refresh method based on available credentials */
    if (state->secret) {
        /* Profile authentication refresh */
        error = knishio_client_authenticate(client, state->secret, state->encrypt);
    } else if (state->cell_slug) {
        /* Guest authentication refresh */
        error = knishio_client_authenticate_guest(client, state->cell_slug, state->encrypt);
    } else {
        return KNISHIO_ERROR_AUTH_NO_CREDENTIALS;
    }

    if (error == KNISHIO_SUCCESS) {
        knishio_client_trigger_auth_event(client, "refreshed");
    } else {
        knishio_client_trigger_auth_event(client, "failed");
    }

    return error;
}

/* Check and refresh auth token if needed */
knishio_error_t knishio_client_ensure_auth(knishio_client_t* client) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_client_auth_state_t* state = NULL;
    knishio_error_t error = knishio_client_get_auth_state(client, &state);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    if (!state->current_token) {
        return KNISHIO_ERROR_AUTH_NO_TOKEN;
    }

    /* Check if token needs refresh */
    if (!state->auto_refresh) {
        return KNISHIO_SUCCESS;
    }

    int64_t expire_interval = knishio_auth_token_get_expire_interval(state->current_token);
    if (expire_interval < state->refresh_threshold_ms) {
        /* Token is expired or about to expire, refresh it */
        knishio_client_trigger_auth_event(client, "expired");
        return knishio_client_refresh_auth(client);
    }

    return KNISHIO_SUCCESS;
}

/* Get authentication headers for GraphQL request */
knishio_error_t knishio_client_get_auth_headers(
    const knishio_client_t* client,
    char** headers
) {
    if (!client || !headers) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_client_auth_state_t* state = NULL;
    knishio_error_t error = knishio_client_get_auth_state((knishio_client_t*)client, &state);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    if (!state->current_token) {
        *headers = NULL;
        return KNISHIO_SUCCESS;
    }

    /* Get auth data from token */
    char* auth_data_json = NULL;
    error = knishio_auth_token_get_auth_data(state->current_token, &auth_data_json);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Build authorization header */
    size_t header_len = strlen("Authorization: Bearer ") + strlen(auth_data_json) + 1;
    *headers = malloc(header_len);
    if (!*headers) {
        free(auth_data_json);
        return KNISHIO_ERROR_MEMORY;
    }

    snprintf(*headers, header_len, "Authorization: Bearer %s", auth_data_json);
    free(auth_data_json);

    return KNISHIO_SUCCESS;
}

/* Save authentication token to snapshot */
knishio_error_t knishio_client_save_auth_snapshot(
    const knishio_client_t* client,
    char** snapshot_json
) {
    if (!client || !snapshot_json) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_client_auth_state_t* state = NULL;
    knishio_error_t error = knishio_client_get_auth_state((knishio_client_t*)client, &state);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    if (!state->current_token) {
        return KNISHIO_ERROR_AUTH_NO_TOKEN;
    }

    return knishio_auth_token_get_snapshot(state->current_token, snapshot_json);
}

/* Restore authentication token from snapshot */
knishio_error_t knishio_client_restore_auth_snapshot(
    knishio_client_t* client,
    const char* snapshot_json,
    const char* secret
) {
    if (!client || !snapshot_json || !secret) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_auth_token_t* auth_token = NULL;
    knishio_error_t error = knishio_auth_token_restore(&auth_token, snapshot_json, secret);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    error = knishio_client_set_auth_token(client, auth_token);
    if (error != KNISHIO_SUCCESS) {
        knishio_auth_token_cleanup(auth_token);
        return error;
    }

    /* Update stored secret */
    knishio_client_auth_state_t* state = NULL;
    error = knishio_client_get_auth_state(client, &state);
    if (error == KNISHIO_SUCCESS) {
        if (state->secret) {
            free(state->secret);
        }
        state->secret = knishio_strdup_safe(secret);
    }

    knishio_client_trigger_auth_event(client, "authenticated");
    return KNISHIO_SUCCESS;
}

/* Get current user secret */
const char* knishio_client_get_secret(const knishio_client_t* client) {
    if (!client) {
        return NULL;
    }

    knishio_client_auth_state_t* state = NULL;
    if (knishio_client_get_auth_state((knishio_client_t*)client, &state) != KNISHIO_SUCCESS) {
        return NULL;
    }

    return state->secret;
}

/* Check if secret is available */
bool knishio_client_has_secret(const knishio_client_t* client) {
    const char* secret = knishio_client_get_secret(client);
    return secret != NULL && strlen(secret) > 0;
}

/* Register authentication event callback */
knishio_error_t knishio_client_register_auth_callback(
    knishio_client_t* client,
    knishio_auth_event_callback_t callback,
    void* user_data
) {
    if (!client || !callback) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_client_auth_state_t* state = NULL;
    knishio_error_t error = knishio_client_get_auth_state(client, &state);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    state->callback = callback;
    state->callback_user_data = user_data;

    return KNISHIO_SUCCESS;
}

/* Unregister authentication event callback */
knishio_error_t knishio_client_unregister_auth_callback(knishio_client_t* client) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_client_auth_state_t* state = NULL;
    knishio_error_t error = knishio_client_get_auth_state(client, &state);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    state->callback = NULL;
    state->callback_user_data = NULL;

    return KNISHIO_SUCCESS;
}

/* Internal helper functions */

static knishio_error_t knishio_client_get_auth_state(
    knishio_client_t* client,
    knishio_client_auth_state_t** state
) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Access the authentication state from the client structure */
    *state = &client->auth_state;
    return KNISHIO_SUCCESS;
}

static void knishio_client_trigger_auth_event(
    knishio_client_t* client,
    const char* event_type
) {
    if (!client || !event_type) {
        return;
    }

    knishio_client_auth_state_t* state = NULL;
    if (knishio_client_get_auth_state(client, &state) != KNISHIO_SUCCESS) {
        return;
    }

    if (state->callback) {
        state->callback(client, event_type, state->callback_user_data);
    }
}

static char* knishio_strdup_safe(const char* str) {
    if (!str) {
        return NULL;
    }
    
    size_t len = strlen(str);
    char* copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    
    memcpy(copy, str, len + 1);
    return copy;
}