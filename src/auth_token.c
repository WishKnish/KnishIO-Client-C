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
#include "knishio/utils/encoding.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"

/* cJSON (same include guard as src/json/parser.c) for the nested snapshot object. */
#ifdef __has_include
  #if __has_include(<cjson/cJSON.h>)
    #include <cjson/cJSON.h>
  #elif __has_include(<cJSON.h>)
    #include <cJSON.h>
  #endif
#else
  #include <cjson/cJSON.h>
#endif

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
    /* The bound wallet's token: USER for a login signed from the ContinuID pointer. Snapshots
     * written before it was recorded came from AUTH-signed logins. */
    const char* wallet_token_str = knishio_json_get_string_path(json, "wallet.token");
    if (!wallet_token_str || !wallet_token_str[0]) {
        wallet_token_str = "AUTH";
    }
    
    int64_t expires_at = 0;
    knishio_json_get_number_path(json, "expiresAt", (double*)&expires_at);
    
    bool encrypt = false;
    knishio_json_get_bool_path(json, "encrypt", &encrypt);
    
    if (!token_str || !position_str || !characters_str) {
        knishio_json_free(json);
        return KNISHIO_ERROR_INVALID_JSON;
    }
    
    /* Resolve the wallet's ML-KEM parameter set — three tiers, identical in every SDK:
     *   1. the snapshot carries it explicitly            -> use it
     *   2. else decode `pubkey`: 1184 raw bytes -> 768, 1568 -> 1024
     *   3. else                                          -> 768
     * Tier 3 is deliberately NOT the constructor default: a snapshot with neither an explicit
     * field nor a recognisable key can only have come from a pre-bump build, and every pre-bump
     * build was ML-KEM-768-only. Falling back to the default is precisely the defect. */
    knishio_mlkem_param_t resolved_param = KNISHIO_MLKEM_768;
    double snapshot_param = 0.0;
    if (knishio_json_get_number_path(json, "wallet.mlKemParameterSet", &snapshot_param)
        && ((int)snapshot_param == 1024 || (int)snapshot_param == 768)) {
        resolved_param = ((int)snapshot_param == 1024) ? KNISHIO_MLKEM_1024 : KNISHIO_MLKEM_768;
    } else if (pubkey_str) {
        unsigned char* pubkey_raw = NULL;
        size_t pubkey_raw_len = 0;
        if (knishio_base64_decode(pubkey_str, &pubkey_raw, &pubkey_raw_len)) {
            if (pubkey_raw_len == KNISHIO_PUBKEY_LENGTH_1024) {
                resolved_param = KNISHIO_MLKEM_1024;
            }
            free(pubkey_raw);
        }
    }

    /* Create wallet from snapshot data */
    knishio_wallet_t* wallet = NULL;
    bool wallet_success = knishio_wallet_from_secret(&wallet, secret, wallet_token_str, position_str);
    knishio_error_t error = wallet_success ? KNISHIO_SUCCESS : KNISHIO_ERROR_INVALID_ARGS;
    if (error != KNISHIO_SUCCESS) {
        knishio_json_free(json);
        return error;
    }
    if (knishio_wallet_get_mlkem_param(wallet) != resolved_param
        && !knishio_wallet_set_mlkem_param(wallet, resolved_param)) {
        knishio_wallet_free(wallet);
        knishio_json_free(json);
        return KNISHIO_ERROR_CRYPTO;
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
    *snapshot_json = NULL;

    /* Built with cJSON directly: knishio_json_builder_start_object() DISCARDS the object under
     * construction, so the streaming builder cannot emit the nested `wallet` object this snapshot
     * shape requires — it used to serialize the wallet sub-object ALONE, dropping token/expiresAt/
     * pubkey/encrypt, and knishio_auth_token_restore() then rejected its own output. */
    cJSON* root = cJSON_CreateObject();
    cJSON* wallet = NULL;
    if (!root) {
        return KNISHIO_ERROR_MEMORY;
    }

    knishio_error_t error = KNISHIO_SUCCESS;
    if (!cJSON_AddStringToObject(root, "token", auth_token->token ? auth_token->token : "")
        || !cJSON_AddNumberToObject(root, "expiresAt", (double)auth_token->expires_at)
        || !cJSON_AddStringToObject(root, "pubkey", auth_token->pubkey ? auth_token->pubkey : "")
        || !cJSON_AddBoolToObject(root, "encrypt", auth_token->encrypt)) {
        error = KNISHIO_ERROR_MEMORY;
        goto done;
    }

    if (auth_token->wallet) {
        wallet = cJSON_CreateObject();
        if (!wallet) { error = KNISHIO_ERROR_MEMORY; goto done; }
        const char* position = knishio_wallet_get_position(auth_token->wallet);
        const char* characters = knishio_wallet_get_characters(auth_token->wallet);
        const char* wallet_token = auth_token->wallet->token;
        if (!cJSON_AddStringToObject(wallet, "position", position ? position : "")
            || !cJSON_AddStringToObject(wallet, "characters", characters ? characters : "")
            || !cJSON_AddStringToObject(wallet, "token", wallet_token ? wallet_token : "AUTH")
            /* Persist the parameter set beside position/characters so a restored session keeps the
             * set it authenticated with instead of taking the (now 1024) constructor default. */
            || !cJSON_AddNumberToObject(wallet, "mlKemParameterSet",
                                        (double)knishio_wallet_get_mlkem_param(auth_token->wallet))) {
            error = KNISHIO_ERROR_MEMORY;
            goto done;
        }
        cJSON_AddItemToObject(root, "wallet", wallet);
        wallet = NULL;  /* ownership moved into `root` */
    } else if (!cJSON_AddNullToObject(root, "wallet")) {
        error = KNISHIO_ERROR_MEMORY;
        goto done;
    }

    *snapshot_json = cJSON_PrintUnformatted(root);
    if (!*snapshot_json) {
        error = KNISHIO_ERROR_MEMORY;
    }

done:
    if (wallet) cJSON_Delete(wallet);
    cJSON_Delete(root);
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
        snap->wallet.mlkem_parameter_set = (int)knishio_wallet_get_mlkem_param(auth_token->wallet);
    } else {
        snap->wallet.position = NULL;
        snap->wallet.characters = NULL;
        snap->wallet.mlkem_parameter_set = 0;
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