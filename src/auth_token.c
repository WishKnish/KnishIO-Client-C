/**
 * @file auth_token.c
 * @brief AuthToken implementation - 100% JavaScript SDK compatible
 * 
 * Provides complete AuthToken functionality matching the JavaScript SDK
 * AuthToken class behavior for seamless cross-platform authentication.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "knishio/auth_token.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"

/* Internal helper functions */
static char* knishio_auth_token_strdup_safe(const char* str);
static int64_t knishio_auth_token_get_current_time_ms(void);

/* AuthToken Creation and Management */

knishio_error_t knishio_auth_token_create(knishio_auth_token_t** auth_token,
                                          const knishio_auth_token_config_t* config) {
    if (!auth_token || !config) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!knishio_auth_token_validate_config(config)) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    knishio_auth_token_t* token = calloc(1, sizeof(knishio_auth_token_t));
    if (!token) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    // Copy token string
    token->token = knishio_auth_token_strdup_safe(config->token);
    if (!token->token && config->token) {
        knishio_auth_token_cleanup(token);
        return KNISHIO_ERROR_MEMORY;
    }
    
    // Copy pubkey string
    token->pubkey = knishio_auth_token_strdup_safe(config->pubkey);
    if (!token->pubkey && config->pubkey) {
        knishio_auth_token_cleanup(token);
        return KNISHIO_ERROR_MEMORY;
    }
    
    // Set primitive fields
    token->expires_at = config->expires_at;
    token->encrypt = config->encrypt;
    token->wallet = NULL;
    
    *auth_token = token;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_auth_token_create_with_wallet(knishio_auth_token_t** auth_token,
                                                      const knishio_auth_token_config_t* config,
                                                      knishio_wallet_t* wallet) {
    knishio_error_t result = knishio_auth_token_create(auth_token, config);
    if (result != KNISHIO_SUCCESS) {
        return result;
    }
    
    result = knishio_auth_token_set_wallet(*auth_token, wallet);
    if (result != KNISHIO_SUCCESS) {
        knishio_auth_token_cleanup(*auth_token);
        *auth_token = NULL;
        return result;
    }
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_auth_token_restore(knishio_auth_token_t** auth_token,
                                           const char* snapshot_json,
                                           const char* secret) {
    if (!auth_token || !snapshot_json || !secret) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Parse JSON snapshot */
    char* error_msg = NULL;
    knishio_json_t* json = knishio_json_parse(snapshot_json, &error_msg);
    if (!json) {
        if (error_msg) {
            free(error_msg);
        }
        return KNISHIO_ERROR_JSON_PARSE;
    }
    
    /* Extract snapshot data */
    const char* token_str = knishio_json_get_string_path(json, "token");
    const char* pubkey_str = knishio_json_get_string_path(json, "pubkey");
    const char* position_str = knishio_json_get_string_path(json, "wallet.position");
    const char* characters_str = knishio_json_get_string_path(json, "wallet.characters");
    
    int64_t expires_at = 0;
    knishio_json_get_number_path(json, "expiresAt", (double*)&expires_at);
    
    bool encrypt = false;
    knishio_json_get_bool_path(json, "encrypt", &encrypt);
    
    if (!token_str || !position_str || !characters_str) {
        knishio_json_free(json);
        return KNISHIO_ERROR_INVALID_JSON;
    }
    
    /* Create wallet from snapshot data */
    knishio_wallet_t* wallet = NULL;
    bool wallet_success = knishio_wallet_from_secret(&wallet, secret, "AUTH", position_str);
    knishio_error_t error = wallet_success ? KNISHIO_SUCCESS : KNISHIO_ERROR_INVALID_ARGS;
    if (error != KNISHIO_SUCCESS) {
        knishio_json_free(json);
        return error;
    }
    
    /* Verify wallet characters match */
    const char* wallet_chars = knishio_wallet_get_characters(wallet);
    if (!wallet_chars || strcmp(wallet_chars, characters_str) != 0) {
        knishio_wallet_free(wallet);
        knishio_json_free(json);
        return KNISHIO_ERROR_WALLET_MISMATCH;
    }
    
    /* Create AuthToken */
    knishio_auth_token_config_t token_config = {
        .token = token_str,
        .expires_at = expires_at,
        .encrypt = encrypt,
        .pubkey = pubkey_str
    };
    
    error = knishio_auth_token_create_with_wallet(auth_token, &token_config, wallet);
    knishio_json_free(json);
    
    if (error != KNISHIO_SUCCESS) {
        knishio_wallet_free(wallet);
        return error;
    }
    
    return KNISHIO_SUCCESS;
}

void knishio_auth_token_cleanup(knishio_auth_token_t* auth_token) {
    if (!auth_token) return;
    
    // Free string fields
    if (auth_token->token) {
        free(auth_token->token);
        auth_token->token = NULL;
    }
    
    if (auth_token->pubkey) {
        free(auth_token->pubkey);
        auth_token->pubkey = NULL;
    }
    
    // Note: We don't free the wallet - it's managed externally
    auth_token->wallet = NULL;
    auth_token->expires_at = 0;
    auth_token->encrypt = false;
    
    // Free the structure itself
    free(auth_token);
}

/* Wallet Management */

knishio_error_t knishio_auth_token_set_wallet(knishio_auth_token_t* auth_token,
                                              knishio_wallet_t* wallet) {
    if (!auth_token) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    auth_token->wallet = wallet;
    return KNISHIO_SUCCESS;
}

knishio_wallet_t* knishio_auth_token_get_wallet(const knishio_auth_token_t* auth_token) {
    if (!auth_token) {
        return NULL;
    }
    
    return auth_token->wallet;
}

/* Property Accessors */

const char* knishio_auth_token_get_token(const knishio_auth_token_t* auth_token) {
    if (!auth_token) {
        return NULL;
    }
    
    return auth_token->token;
}

const char* knishio_auth_token_get_pubkey(const knishio_auth_token_t* auth_token) {
    if (!auth_token) {
        return NULL;
    }
    
    return auth_token->pubkey;
}

int64_t knishio_auth_token_get_expire_interval(const knishio_auth_token_t* auth_token) {
    if (!auth_token) {
        return -1;
    }
    
    // JavaScript algorithm: (this.$__expiresAt * 1000) - Date.now()
    int64_t current_time_ms = knishio_auth_token_get_current_time_ms();
    int64_t expire_time_ms = auth_token->expires_at * 1000;
    
    return expire_time_ms - current_time_ms;
}

bool knishio_auth_token_is_expired(const knishio_auth_token_t* auth_token) {
    if (!auth_token) {
        return true; // NULL token is considered expired
    }
    
    // JavaScript algorithm: !this.$__expiresAt || this.getExpireInterval() < 0
    if (auth_token->expires_at == 0) {
        return true;
    }
    
    return knishio_auth_token_get_expire_interval(auth_token) < 0;
}

/* Serialization */

knishio_error_t knishio_auth_token_get_snapshot(const knishio_auth_token_t* auth_token,
                                                char** snapshot_json) {
    if (!auth_token || !snapshot_json) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Create JSON builder for snapshot */
    knishio_json_builder_t* builder = knishio_json_builder_create();
    if (!builder) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Start object */
    knishio_error_t error = knishio_json_builder_start_object(builder);
    if (error != KNISHIO_SUCCESS) {
        knishio_json_builder_free(builder);
        return error;
    }
    
    /* Add token fields */
    error = knishio_json_builder_add_string(builder, "token", auth_token->token ? auth_token->token : "");
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_number(builder, "expiresAt", (double)auth_token->expires_at);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(builder, "pubkey", auth_token->pubkey ? auth_token->pubkey : "");
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_boolean(builder, "encrypt", auth_token->encrypt);
    }
    
    /* Add wallet object */
    if (error == KNISHIO_SUCCESS && auth_token->wallet) {
        error = knishio_json_builder_add_key(builder, "wallet");
        if (error == KNISHIO_SUCCESS) {
            error = knishio_json_builder_start_object(builder);
        }
        
        if (error == KNISHIO_SUCCESS) {
            const char* position = knishio_wallet_get_position(auth_token->wallet);
            error = knishio_json_builder_add_string(builder, "position", position ? position : "");
        }
        
        if (error == KNISHIO_SUCCESS) {
            const char* characters = knishio_wallet_get_characters(auth_token->wallet);
            error = knishio_json_builder_add_string(builder, "characters", characters ? characters : "");
        }
        
        if (error == KNISHIO_SUCCESS) {
            error = knishio_json_builder_end_object(builder);
        }
    } else if (error == KNISHIO_SUCCESS) {
        /* Add null wallet */
        error = knishio_json_builder_add_null(builder, "wallet");
    }
    
    /* End main object */
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_end_object(builder);
    }
    
    /* Build JSON string */
    if (error == KNISHIO_SUCCESS) {
        knishio_json_t *json_result = knishio_json_builder_build(builder);
        if (!json_result) {
            error = KNISHIO_ERROR_MEMORY;
        } else {
            *snapshot_json = knishio_json_builder_to_string(builder, false);
            knishio_json_free(json_result);
            error = *snapshot_json ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
        }
    }
    
    knishio_json_builder_free(builder);
    return error;
}

knishio_error_t knishio_auth_token_get_auth_data(const knishio_auth_token_t* auth_token,
                                                 char** auth_data_json) {
    if (!auth_token || !auth_data_json) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Create JSON builder for auth data */
    knishio_json_builder_t* builder = knishio_json_builder_create();
    if (!builder) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Start object */
    knishio_error_t error = knishio_json_builder_start_object(builder);
    if (error != KNISHIO_SUCCESS) {
        knishio_json_builder_free(builder);
        return error;
    }
    
    /* Add auth data fields */
    error = knishio_json_builder_add_string(builder, "token", auth_token->token ? auth_token->token : "");
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(builder, "pubkey", auth_token->pubkey ? auth_token->pubkey : "");
    }
    
    /* Add wallet reference (simplified) */
    if (error == KNISHIO_SUCCESS && auth_token->wallet) {
        error = knishio_json_builder_add_key(builder, "wallet");
        if (error == KNISHIO_SUCCESS) {
            error = knishio_json_builder_start_object(builder);
        }
        
        if (error == KNISHIO_SUCCESS) {
            const char* address = knishio_wallet_get_address(auth_token->wallet);
            error = knishio_json_builder_add_string(builder, "address", address ? address : "");
        }
        
        if (error == KNISHIO_SUCCESS) {
            const char* position = knishio_wallet_get_position(auth_token->wallet);
            error = knishio_json_builder_add_string(builder, "position", position ? position : "");
        }
        
        if (error == KNISHIO_SUCCESS) {
            error = knishio_json_builder_end_object(builder);
        }
    } else if (error == KNISHIO_SUCCESS) {
        /* Add null wallet */
        error = knishio_json_builder_add_null(builder, "wallet");
    }
    
    /* End main object */
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_end_object(builder);
    }
    
    /* Build JSON string */
    if (error == KNISHIO_SUCCESS) {
        knishio_json_t *json_result = knishio_json_builder_build(builder);
        if (!json_result) {
            error = KNISHIO_ERROR_MEMORY;
        } else {
            *auth_data_json = knishio_json_builder_to_string(builder, false);
            knishio_json_free(json_result);
            error = *auth_data_json ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
        }
    }
    
    knishio_json_builder_free(builder);
    return error;
}

/* Snapshot Management */

knishio_error_t knishio_auth_token_create_snapshot(const knishio_auth_token_t* auth_token,
                                                   knishio_auth_token_snapshot_t** snapshot) {
    if (!auth_token || !snapshot) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    knishio_auth_token_snapshot_t* snap = calloc(1, sizeof(knishio_auth_token_snapshot_t));
    if (!snap) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    // Copy basic fields
    snap->token = knishio_auth_token_strdup_safe(auth_token->token);
    snap->pubkey = knishio_auth_token_strdup_safe(auth_token->pubkey);
    snap->expires_at = auth_token->expires_at;
    snap->encrypt = auth_token->encrypt;
    
    // Copy wallet data if present
    if (auth_token->wallet) {
        const char* position = knishio_wallet_get_position(auth_token->wallet);
        const char* characters = knishio_wallet_get_characters(auth_token->wallet);
        
        snap->wallet.position = knishio_auth_token_strdup_safe(position);
        snap->wallet.characters = knishio_auth_token_strdup_safe(characters);
    } else {
        snap->wallet.position = NULL;
        snap->wallet.characters = NULL;
    }
    
    *snapshot = snap;
    return KNISHIO_SUCCESS;
}

void knishio_auth_token_snapshot_cleanup(knishio_auth_token_snapshot_t* snapshot) {
    if (!snapshot) return;
    
    if (snapshot->token) {
        free(snapshot->token);
    }
    if (snapshot->pubkey) {
        free(snapshot->pubkey);
    }
    if (snapshot->wallet.position) {
        free(snapshot->wallet.position);
    }
    if (snapshot->wallet.characters) {
        free(snapshot->wallet.characters);
    }
    
    free(snapshot);
}

/* Authentication Data Management */

knishio_error_t knishio_auth_token_create_auth_data(const knishio_auth_token_t* auth_token,
                                                    knishio_auth_token_auth_data_t** auth_data) {
    if (!auth_token || !auth_data) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    knishio_auth_token_auth_data_t* data = calloc(1, sizeof(knishio_auth_token_auth_data_t));
    if (!data) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    // Copy token and pubkey
    data->token = knishio_auth_token_strdup_safe(auth_token->token);
    data->pubkey = knishio_auth_token_strdup_safe(auth_token->pubkey);
    data->wallet = auth_token->wallet; // Reference, not copy
    
    *auth_data = data;
    return KNISHIO_SUCCESS;
}

void knishio_auth_token_auth_data_cleanup(knishio_auth_token_auth_data_t* auth_data) {
    if (!auth_data) return;
    
    if (auth_data->token) {
        free(auth_data->token);
    }
    if (auth_data->pubkey) {
        free(auth_data->pubkey);
    }
    
    // Don't free wallet - it's a reference
    auth_data->wallet = NULL;
    
    free(auth_data);
}

/* Utility Functions */

int64_t knishio_auth_token_current_timestamp_ms(void) {
    return knishio_auth_token_get_current_time_ms();
}

bool knishio_auth_token_validate_config(const knishio_auth_token_config_t* config) {
    if (!config) {
        return false;
    }
    
    // At minimum, we need a token
    if (!config->token || strlen(config->token) == 0) {
        return false;
    }
    
    return true;
}

/* Internal helper functions */

static char* knishio_auth_token_strdup_safe(const char* str) {
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

static int64_t knishio_auth_token_get_current_time_ms(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return 0; // Error case
    }
    
    return (int64_t)(tv.tv_sec) * 1000 + (int64_t)(tv.tv_usec) / 1000;
}