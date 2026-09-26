#include "knishio/knishio.h"
#include "knishio/auth_token.h"
#include "knishio/client_auth.h"
#include "knishio/client_ops.h"
#include "knishio/wallet.h"
#include "knishio/http.h"
#include "knishio/graphql.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"
#include "knishio/crypto/cipher_hash.h"
#include "knishio/utils/memory.h"
#include "client_internal.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* cJSON (same include guard as src/json/parser.c) for the PQ-transport wrap/unwrap. */
#ifdef __has_include
  #if __has_include(<cjson/cJSON.h>)
    #include <cjson/cJSON.h>
  #elif __has_include(<cJSON.h>)
    #include <cJSON.h>
  #endif
#else
  #include <cjson/cJSON.h>
#endif

/* PQ-transport (Phase E): whether an outgoing operation should be wrapped in CipherHash. Bypass
 * (plaintext): __schema/ContinuId queries, the AccessToken mutation, and the U-isotope
 * ProposeMolecule (auth bootstrap). Mirrors the validator/other-SDK bypass set. The validator
 * keys its bypass on the root field (ContinuId); the SDK's own ContinuId operation is named
 * QueryContinuId (operations/wallet.c), and the profile login sends it before it has keys. */
static bool cipher_should_encrypt(const knishio_graphql_operation_t* op) {
    if (!op || !op->name) {
        return true;
    }
    if (strcmp(op->name, "__schema") == 0 || strcmp(op->name, "ContinuId") == 0
        || strcmp(op->name, "QueryContinuId") == 0) {
        return false;
    }
    if (strcmp(op->name, "AccessToken") == 0) {
        return false;
    }
    if (strcmp(op->name, "ProposeMolecule") == 0 && op->variables_json) {
        cJSON* v = cJSON_Parse(op->variables_json);
        bool is_u = false;
        if (v) {
            cJSON* mol = cJSON_GetObjectItem(v, "molecule");
            cJSON* atoms = mol ? cJSON_GetObjectItem(mol, "atoms") : NULL;
            cJSON* a0 = (atoms && cJSON_IsArray(atoms)) ? cJSON_GetArrayItem(atoms, 0) : NULL;
            cJSON* iso = a0 ? cJSON_GetObjectItem(a0, "isotope") : NULL;
            if (cJSON_IsString(iso) && strcmp(cJSON_GetStringValue(iso), "U") == 0) {
                is_u = true;
            }
            cJSON_Delete(v);
        }
        if (is_u) {
            return false;
        }
    }
    return true;
}

/* Client management implementations */
knishio_error_t knishio_client_create(knishio_client_t **client, const knishio_client_config_t *config) {
    if (client == NULL || config == NULL) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (config->mlkem_parameter_set != 0 &&
        config->mlkem_parameter_set != 1024 &&
        config->mlkem_parameter_set != 768) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    knishio_client_t *new_client = knishio_malloc(sizeof(knishio_client_t));
    if (new_client == NULL) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    // Initialize fields
    new_client->uri = config->uri ? knishio_strdup(config->uri) : NULL;
    new_client->cell_slug = config->cell_slug ? knishio_strdup(config->cell_slug) : NULL;
    new_client->auth_token = NULL;
    new_client->insecure_tls = config->insecure_tls;
    new_client->initialized = false;
    new_client->cipher_enabled = false;
    new_client->cipher_server_pubkey = NULL;
    new_client->cipher_wallet = NULL;
    new_client->mlkem_parameter_set = (config->mlkem_parameter_set == 768) ? 768 : 1024;
    knishio_wallet_set_default_mlkem_param(new_client->mlkem_parameter_set == 768 ? KNISHIO_MLKEM_768 : KNISHIO_MLKEM_1024);
    // Initialize authentication state
    memset(&new_client->auth_state, 0, sizeof(knishio_client_auth_state_t));
    new_client->auth_state.refresh_threshold_ms = 300000; // 5 minutes default
    
    // Create HTTP client
    knishio_error_t http_result = knishio_http_client_create(&new_client->http_client, config->uri);
    if (http_result != KNISHIO_SUCCESS) {
        knishio_client_destroy(new_client);
        return http_result;
    }

    /* Opt-in insecure TLS (dev/self-signed validators); default keeps verification ON */
    if (config->insecure_tls) {
        knishio_http_client_set_ssl_verify(new_client->http_client, false);
    }

    new_client->initialized = true;
    *client = new_client;
    return KNISHIO_SUCCESS;
}

void knishio_client_destroy(knishio_client_t *client) {
    if (client == NULL) {
        return;
    }
    
    // Clean up HTTP client
    if (client->http_client) {
        knishio_http_client_free(client->http_client);
    }
    
    // Clean up auth token
    if (client->auth_token) {
        knishio_auth_token_cleanup(client->auth_token);
    }
    
    // Clean up authentication state
    if (client->auth_state.current_token) {
        knishio_auth_token_cleanup(client->auth_state.current_token);
    }
    if (client->auth_state.secret) {
        knishio_secure_free(client->auth_state.secret, strlen(client->auth_state.secret));
        client->auth_state.secret = NULL;
    }
    if (client->auth_state.bundle_hash) {
        free(client->auth_state.bundle_hash);
        client->auth_state.bundle_hash = NULL;
    }
    if (client->auth_state.storage_label) {
        free(client->auth_state.storage_label);
        client->auth_state.storage_label = NULL;
    }
    if (client->auth_state.storage_passphrase) {
        knishio_secure_free(client->auth_state.storage_passphrase, strlen(client->auth_state.storage_passphrase));
        client->auth_state.storage_passphrase = NULL;
    }
    if (client->auth_state.storage_recovery_passphrase) {
        knishio_secure_free(client->auth_state.storage_recovery_passphrase, strlen(client->auth_state.storage_recovery_passphrase));
        client->auth_state.storage_recovery_passphrase = NULL;
    }
    if (client->auth_state.cell_slug) {
        knishio_free(client->auth_state.cell_slug);
    }
    
    // Clean up strings
    knishio_free(client->uri);
    knishio_free(client->cell_slug);

    // Clean up PQ-transport cipher context
    if (client->cipher_server_pubkey) knishio_free(client->cipher_server_pubkey);
    if (client->cipher_wallet) knishio_wallet_free(client->cipher_wallet);

    // Free the client structure
    knishio_free(client);
}

/* Authentication function implementations */

/* 
 * Legacy authentication function - DEPRECATED
 * Use knishio_client_authenticate() or knishio_client_authenticate_guest() instead
 */
knishio_error_t knishio_client_request_auth_token(knishio_client_t *client,
                                                  knishio_wallet_t *wallet,
                                                  knishio_auth_token_t **auth_token) {
    if (!client || !wallet || !auth_token) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!client->initialized || !client->http_client) {
        return KNISHIO_ERROR_INVALID_STATE; // Client not properly initialized
    }
    
    /* This function is deprecated and should not be used.
     * Use the new authentication system:
     * - knishio_client_authenticate() for profile authentication
     * - knishio_client_authenticate_guest() for guest authentication
     */
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
}

knishio_error_t knishio_client_set_auth_token(knishio_client_t *client,
                                              knishio_auth_token_t *auth_token) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!client->initialized || !client->http_client) {
        return KNISHIO_ERROR_INVALID_STATE; // Client not properly initialized
    }
    
    // Clear existing auth token
    if (client->auth_token) {
        knishio_auth_token_cleanup(client->auth_token);
        client->auth_token = NULL;
    }
    
    // Set new auth token
    client->auth_token = auth_token;
    
    // Update HTTP client with auth token
    if (auth_token) {
        const char *token = knishio_auth_token_get_token(auth_token);
        const char *pubkey = knishio_auth_token_get_pubkey(auth_token);
        
        if (token) {
            knishio_error_t result = knishio_http_client_set_auth_token(client->http_client, token);
            if (result != KNISHIO_SUCCESS) {
                return result;
            }
        }
        
        if (pubkey) {
            knishio_error_t result = knishio_http_client_set_auth_pubkey(client->http_client, pubkey);
            if (result != KNISHIO_SUCCESS) {
                return result;
            }
        }
    }
    
    return KNISHIO_SUCCESS;
}

knishio_auth_token_t* knishio_client_get_auth_token(const knishio_client_t* client) {
    if (!client) {
        return NULL;
    }

    return client->auth_token;
}

const char* knishio_client_get_cell_slug(const knishio_client_t* client) {
    return client ? client->cell_slug : NULL;
}

knishio_error_t knishio_client_clear_auth(knishio_client_t *client) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    // Clear auth token
    if (client->auth_token) {
        knishio_auth_token_cleanup(client->auth_token);
        client->auth_token = NULL;
    }
    
    // Clear HTTP client auth
    if (client->http_client) {
        knishio_http_client_clear_auth(client->http_client);
    }

    return KNISHIO_SUCCESS;
}

/* Build a proper graphql client from this client (its own http_client to client->uri + the client's
 * auth token), submit the operation, and free the graphql client. Replaces the prior
 * (knishio_graphql_client_t*)client cast in the create ops — knishio_client_t and
 * knishio_graphql_client_t are different structs, so the cast never propagated the auth token to
 * the X-Auth-Token header (and was UB). knishio_graphql_client_create makes its OWN http_client to
 * client->uri, so client->http_client is untouched and graphql_client_free is safe. */
knishio_error_t knishio_client_execute_graphql(
    knishio_client_t* client,
    const knishio_graphql_operation_t* operation,
    knishio_graphql_response_t** response
) {
    if (!client || !operation || !response) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (!client->uri) {
        return KNISHIO_ERROR_INVALID_STATE;
    }

    knishio_graphql_client_t* gql = NULL;
    knishio_error_t error = knishio_graphql_client_create(&gql, client->uri, client->cell_slug);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* The per-op gql client makes its own http_client (verify_ssl=true default) — inherit the
     * client's insecure-TLS opt-in so it can reach a self-signed dev validator (slice 2b-i). */
    if (client->insecure_tls && gql->http_client) {
        knishio_http_client_set_ssl_verify(gql->http_client, false);
    }

    /* Propagate the client's auth token (slice 2a: manually-set; the full JWT flow is slice 2b).
     * graphql_client_set_auth_token pushes it into the http client's X-Auth-Token header. */
    if (client->auth_token) {
        knishio_graphql_client_set_auth_token(gql, client->auth_token);
    }

    /* PQ-transport Phase E: wrap the operation in the ML-KEM CipherHash envelope when encryption is
     * enabled + the operation isn't bypassed. Encrypt the FULL body string (the validator recovers
     * it as a JSON string value and parses the inner request).
     *
     * Decide the bypass FIRST, then demand the keys: a bypassed operation (`__schema`, `ContinuId`,
     * `AccessToken`, U-isotope `ProposeMolecule`) must still go out in plaintext or the auth
     * bootstrap would deadlock encrypting to a server pubkey it has not learned yet. Anything else
     * on an encryption-enabled client fails closed — both when the transport keys are missing and
     * when the envelope cannot be built — rather than silently downgrading to plaintext (matches
     * PHP Cipher.php / Kotlin HttpClient). */
    bool encrypted_request = false;
    char* body = NULL;
    char* envelope = NULL;
    char* cipher_vars = NULL;
    knishio_graphql_operation_t cipher_op;
    const knishio_graphql_operation_t* exec_op = operation;

    if (client->cipher_enabled && cipher_should_encrypt(operation)) {
        if (!client->cipher_wallet || !client->cipher_server_pubkey
            || client->cipher_server_pubkey[0] == '\0') {
            /* Authorized wallet / server public key missing — never send this in the clear.
             * An empty advertised key counts as missing (parity with JS/TS `!serverPubkey`). */
            knishio_graphql_client_free(gql);
            return KNISHIO_ERROR_INVALID_STATE;
        }
        cJSON* body_json = cJSON_CreateObject();
        if (body_json) {
            cJSON_AddStringToObject(body_json, "query", operation->query ? operation->query : "");
            if (operation->variables_json) {
                cJSON* v = cJSON_Parse(operation->variables_json);
                cJSON_AddItemToObject(body_json, "variables", v ? v : cJSON_CreateObject());
            }
            body = cJSON_PrintUnformatted(body_json);
            cJSON_Delete(body_json);
        }
        if (!body) {
            knishio_graphql_client_free(gql);
            return KNISHIO_ERROR_CRYPTO;
        }
        if (knishio_cipher_hash_encrypt(body, client->cipher_server_pubkey, &envelope) != KNISHIO_SUCCESS
            || !envelope) {
            free(body);
            if (envelope) free(envelope);
            knishio_graphql_client_free(gql);
            return KNISHIO_ERROR_CRYPTO;
        }
        cJSON* cv = cJSON_CreateObject();
        if (cv) {
            cJSON_AddStringToObject(cv, "Hash", envelope);
            cipher_vars = cJSON_PrintUnformatted(cv);
            cJSON_Delete(cv);
        }
        if (!cipher_vars) {
            free(body);
            free(envelope);
            knishio_graphql_client_free(gql);
            return KNISHIO_ERROR_CRYPTO;
        }
        cipher_op.name = "CipherHash";
        cipher_op.query = KNISHIO_CIPHER_HASH_QUERY;
        cipher_op.variables_json = cipher_vars;
        cipher_op.requires_auth = operation->requires_auth;
        cipher_op.is_mutation = false;  /* CipherHash is a query op */
        exec_op = &cipher_op;
        encrypted_request = true;
    }

    error = knishio_graphql_execute(gql, exec_op, response);

    /* Decrypt the CipherHash response envelope back to the inner GraphQL response JSON (replaces
     * response->data for the normal downstream parse). The validator encrypts the response OBJECT,
     * so the decrypted plaintext is the inner response JSON directly (no JSON-decode). */
    if (error == KNISHIO_SUCCESS && encrypted_request && *response && (*response)->data) {
        cJSON* root = cJSON_Parse((*response)->data);
        if (root) {
            cJSON* data = cJSON_GetObjectItem(root, "data");
            cJSON* ch = data ? cJSON_GetObjectItem(data, "CipherHash") : NULL;
            cJSON* hash = ch ? cJSON_GetObjectItem(ch, "hash") : NULL;
            if (cJSON_IsString(hash)) {
                char* inner = NULL;
                if (knishio_cipher_hash_decrypt(cJSON_GetStringValue(hash), client->cipher_wallet,
                                                &inner) == KNISHIO_SUCCESS && inner) {
                    knishio_free((*response)->data);
                    (*response)->data = inner;  /* ownership transferred (malloc'd) */
                }
            }
            cJSON_Delete(root);
        }
    }

    if (body) free(body);
    if (envelope) free(envelope);
    if (cipher_vars) free(cipher_vars);

    knishio_graphql_client_free(gql);
    return error;
}

knishio_error_t knishio_client_set_cipher_context(knishio_client_t* client,
                                                  const char* server_pubkey_b64,
                                                  const knishio_wallet_t* source_wallet) {
    if (!client || !server_pubkey_b64 || !source_wallet) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    /* Replace any prior context. */
    if (client->cipher_server_pubkey) { knishio_free(client->cipher_server_pubkey); client->cipher_server_pubkey = NULL; }
    if (client->cipher_wallet)        { knishio_wallet_free(client->cipher_wallet);  client->cipher_wallet = NULL; }

    client->cipher_server_pubkey = knishio_strdup(server_pubkey_b64);
    if (!client->cipher_server_pubkey) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Own a copy of the auth signing wallet's identity (AUTH, or USER for a login signed from the
     * ContinuID pointer). Re-deriving from (secret, token, position) is
     * deterministic, so the copy's ML-KEM keypair is byte-identical to the source wallet's — and
     * unlike a bare pubkey/privkey pair it also carries the KnishIO key the transport needs to
     * derive this wallet's OTHER ML-KEM identity for a legacy inbound envelope. */
    if (!source_wallet->secret || !source_wallet->token || !source_wallet->position) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    knishio_error_t error = knishio_wallet_create_simple(&client->cipher_wallet,
                                                         source_wallet->secret,
                                                         source_wallet->token,
                                                         source_wallet->position);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    if (knishio_wallet_get_mlkem_param(client->cipher_wallet)
        != knishio_wallet_get_mlkem_param(source_wallet)) {
        knishio_wallet_set_mlkem_param(client->cipher_wallet,
                                       knishio_wallet_get_mlkem_param(source_wallet));
    }
    if (!client->cipher_wallet->pubkey || !client->cipher_wallet->privkey_bytes) {
        knishio_wallet_free(client->cipher_wallet);
        client->cipher_wallet = NULL;
        return KNISHIO_ERROR_CRYPTO;
    }
    return KNISHIO_SUCCESS;
}

void knishio_client_set_encryption(knishio_client_t* client, bool encrypt) {
    if (client) {
        client->cipher_enabled = encrypt;
    }
}

void knishio_client_switch_encryption(knishio_client_t* client, bool encrypt) {
    if (client) {
        client->cipher_enabled = encrypt;
    }
}
int knishio_client_get_mlkem_parameter_set(const knishio_client_t* client) {
    if (!client) return 1024;
    return (client->mlkem_parameter_set == 768) ? 768 : 1024;
}

void knishio_client_set_mlkem_parameter_set(knishio_client_t* client, int param_set) {
    if (client) {
        client->mlkem_parameter_set = (param_set == 768) ? 768 : 1024;
    }
}