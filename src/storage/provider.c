#include "knishio/storage/provider.h"
#include "knishio/storage/envelope.h"
#include "knishio/utils/memory.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    knishio_storage_backend_t *backend;
    char *default_passphrase;
    size_t default_passphrase_len;
} aes_gcm_provider_state_t;

static const char *resolve_passphrase(const aes_gcm_provider_state_t *state, const knishio_storage_options_t *options) {
    if (options && options->passphrase && options->passphrase[0] != '\0') {
        return options->passphrase;
    }
    if (state && state->default_passphrase && state->default_passphrase[0] != '\0') {
        return state->default_passphrase;
    }
    return NULL;
}

static const char *aes_gcm_provider_type(const knishio_secret_storage_provider_t *provider) {
    (void)provider;
    return "aes-gcm";
}

static bool aes_gcm_is_hardware_backed(const knishio_secret_storage_provider_t *provider) {
    (void)provider;
    return false;
}

static knishio_error_t aes_gcm_store_secret(
    knishio_secret_storage_provider_t *provider,
    const char *bundle_hash,
    const char *secret,
    size_t secret_len,
    const knishio_storage_options_t *options
) {
    if (!provider || !provider->user_data) return KNISHIO_ERROR_NULL_POINTER;
    aes_gcm_provider_state_t *state = (aes_gcm_provider_state_t *)provider->user_data;
    const char *pass = resolve_passphrase(state, options);
    if (!pass) return KNISHIO_ERROR_INVALID_ARGS;
    return knishio_envelope_store_secret(state->backend, bundle_hash, secret, secret_len, pass, options);
}

static knishio_error_t aes_gcm_retrieve_secret(
    knishio_secret_storage_provider_t *provider,
    const char *bundle_hash,
    const knishio_storage_options_t *options,
    char **plaintext_out,
    size_t *plaintext_len_out
) {
    if (!provider || !provider->user_data) return KNISHIO_ERROR_NULL_POINTER;
    aes_gcm_provider_state_t *state = (aes_gcm_provider_state_t *)provider->user_data;
    const char *pass = resolve_passphrase(state, options);
    if (!pass) return KNISHIO_ERROR_INVALID_ARGS;
    return knishio_envelope_retrieve_secret(state->backend, bundle_hash, pass, options, plaintext_out, plaintext_len_out);
}

static knishio_error_t aes_gcm_delete_secret(
    knishio_secret_storage_provider_t *provider,
    const char *bundle_hash,
    bool *existed_out
) {
    if (!provider || !provider->user_data) return KNISHIO_ERROR_NULL_POINTER;
    aes_gcm_provider_state_t *state = (aes_gcm_provider_state_t *)provider->user_data;
    return knishio_envelope_delete_secret(state->backend, bundle_hash, existed_out);
}

static knishio_error_t aes_gcm_has_secret(
    knishio_secret_storage_provider_t *provider,
    const char *bundle_hash,
    bool *exists_out
) {
    if (!provider || !provider->user_data) return KNISHIO_ERROR_NULL_POINTER;
    aes_gcm_provider_state_t *state = (aes_gcm_provider_state_t *)provider->user_data;
    return knishio_envelope_has_secret(state->backend, bundle_hash, exists_out);
}

static knishio_error_t aes_gcm_list_secrets(
    knishio_secret_storage_provider_t *provider,
    knishio_secret_metadata_t **metadata_list_out,
    size_t *count_out
) {
    if (!provider || !provider->user_data) return KNISHIO_ERROR_NULL_POINTER;
    aes_gcm_provider_state_t *state = (aes_gcm_provider_state_t *)provider->user_data;
    return knishio_envelope_list_secrets(state->backend, metadata_list_out, count_out);
}

static knishio_error_t aes_gcm_recover_secret(
    knishio_secret_storage_provider_t *provider,
    const char *bundle_hash,
    const char *recovery_passphrase,
    const knishio_storage_options_t *options
) {
    if (!provider || !provider->user_data) return KNISHIO_ERROR_NULL_POINTER;
    aes_gcm_provider_state_t *state = (aes_gcm_provider_state_t *)provider->user_data;
    const char *new_pass = resolve_passphrase(state, options);
    if (!new_pass) return KNISHIO_ERROR_INVALID_ARGS;
    return knishio_envelope_recover_secret(state->backend, bundle_hash, recovery_passphrase, new_pass, options);
}

static void aes_gcm_free_provider(knishio_secret_storage_provider_t *provider) {
    if (!provider) return;
    if (provider->user_data) {
        aes_gcm_provider_state_t *state = (aes_gcm_provider_state_t *)provider->user_data;
        if (state->default_passphrase) {
            knishio_secure_free(state->default_passphrase, state->default_passphrase_len);
            state->default_passphrase = NULL;
        }
        free(state);
        provider->user_data = NULL;
    }
    free(provider);
}

knishio_error_t knishio_aes_gcm_secret_storage_provider_create(
    knishio_storage_backend_t *backend,
    const char *default_passphrase,
    knishio_secret_storage_provider_t **provider_out
) {
    if (!backend || !provider_out) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_secret_storage_provider_t *provider = (knishio_secret_storage_provider_t *)calloc(1, sizeof(knishio_secret_storage_provider_t));
    if (!provider) return KNISHIO_ERROR_MEMORY;

    aes_gcm_provider_state_t *state = (aes_gcm_provider_state_t *)calloc(1, sizeof(aes_gcm_provider_state_t));
    if (!state) {
        free(provider);
        return KNISHIO_ERROR_MEMORY;
    }

    state->backend = backend;
    if (default_passphrase) {
        state->default_passphrase = strdup(default_passphrase);
        if (!state->default_passphrase) {
            free(state);
            free(provider);
            return KNISHIO_ERROR_MEMORY;
        }
        state->default_passphrase_len = strlen(default_passphrase);
    }

    provider->user_data = state;
    provider->provider_type = aes_gcm_provider_type;
    provider->is_hardware_backed = aes_gcm_is_hardware_backed;
    provider->store_secret = aes_gcm_store_secret;
    provider->retrieve_secret = aes_gcm_retrieve_secret;
    provider->delete_secret = aes_gcm_delete_secret;
    provider->has_secret = aes_gcm_has_secret;
    provider->list_secrets = aes_gcm_list_secrets;
    provider->recover_secret = aes_gcm_recover_secret;
    provider->free_provider = aes_gcm_free_provider;

    *provider_out = provider;
    return KNISHIO_SUCCESS;
}

void knishio_secret_storage_provider_free(knishio_secret_storage_provider_t *provider) {
    if (!provider) return;
    if (provider->free_provider) {
        provider->free_provider(provider);
    } else {
        free(provider);
    }
}
