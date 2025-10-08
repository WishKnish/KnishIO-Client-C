/**
 * @file response_continu_id.c
 * @brief ContinuID response implementation
 * 
 * Complete implementation of ContinuID response handling compatible with
 * JavaScript SDK ResponseContinuId functionality.
 */

#include "knishio/response/response_continu_id.h"
#include "knishio/json.h"
#include "knishio/json/parser.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include <string.h>
#include <stdlib.h>

/* Forward declarations */
static bool parse_continu_id_data(knishio_json_t *json, knishio_continu_id_info_t *identity);
static void free_continu_id_info(knishio_continu_id_info_t *identity);

/* ========== ContinuID Response Implementation ========== */

knishio_response_continu_id_t* knishio_response_continu_id_create(knishio_query_t *query,
                                                                  knishio_json_t *json) {
    if (!query || !json) {
        return NULL;
    }
    
    /* Allocate response */
    knishio_response_continu_id_t *response = knishio_calloc(1, sizeof(knishio_response_continu_id_t));
    if (!response) {
        return NULL;
    }
    
    /* Initialize base response */
    memset(&response->base, 0, sizeof(knishio_response_t));
    if (!knishio_response_init(&response->base)) {
        knishio_free(response);
        return NULL;
    }
    response->base.query = query;
    response->base.origin_response = json;
    
    bool success = false;
    knishio_json_t *data_json = NULL;
    
    /* Get ContinuId data from response */
    if (knishio_json_get_object(json, "data", &data_json) != KNISHIO_SUCCESS || !data_json) {
        goto cleanup;
    }
    
    knishio_json_t *continuid_json = NULL;
    if (knishio_json_get_object(data_json, "ContinuId", &continuid_json) != KNISHIO_SUCCESS || !continuid_json) {
        /* Empty result is valid */
        response->identity_count = 0;
        response->identities = NULL;
        success = true;
        goto cleanup;
    }
    
    /* Check if it's an array or single object */
    if (knishio_json_is_array(continuid_json)) {
        /* Array of identities */
        size_t array_size = knishio_json_array_size(continuid_json);
        if (array_size > 0) {
            response->identities = knishio_calloc(array_size, sizeof(knishio_continu_id_info_t));
            if (!response->identities) {
                goto cleanup;
            }
            
            response->identity_count = array_size;
            
            /* Parse each identity */
            for (size_t i = 0; i < array_size; i++) {
                knishio_json_t *item = knishio_json_array_get(continuid_json, i);
                if (!item || !parse_continu_id_data(item, &response->identities[i])) {
                    goto cleanup;
                }
            }
        }
    } else {
        /* Single identity object */
        response->identities = knishio_calloc(1, sizeof(knishio_continu_id_info_t));
        if (!response->identities) {
            goto cleanup;
        }
        
        response->identity_count = 1;
        
        if (!parse_continu_id_data(continuid_json, &response->identities[0])) {
            goto cleanup;
        }
    }
    
    /* Set primary bundle if available */
    if (response->identity_count > 0 && response->identities[0].bundle_hash) {
        response->primary_bundle = knishio_strdup(response->identities[0].bundle_hash);
    }
    
    response->identities_cached = true;
    success = true;
    
cleanup:
    if (!success) {
        knishio_response_continu_id_free(response);
        response = NULL;
    }
    
    return response;
}

void knishio_response_continu_id_free(knishio_response_continu_id_t *response) {
    if (!response) {
        return;
    }
    
    /* Free identities array */
    if (response->identities) {
        for (size_t i = 0; i < response->identity_count; i++) {
            free_continu_id_info(&response->identities[i]);
        }
        knishio_free(response->identities);
    }
    
    knishio_free(response->primary_bundle);
    knishio_free(response->requested_slug);
    
    /* Free base response */
    knishio_response_free(&response->base);
    
    knishio_free(response);
}

knishio_continu_id_info_t* knishio_response_continu_id_get_identities(knishio_response_continu_id_t *response,
                                                                      size_t *count) {
    if (!response || !count) {
        return NULL;
    }
    
    *count = response->identity_count;
    return response->identities;
}

knishio_continu_id_info_t* knishio_response_continu_id_get_identity(knishio_response_continu_id_t *response,
                                                                    size_t index) {
    if (!response || index >= response->identity_count) {
        return NULL;
    }
    
    return &response->identities[index];
}

size_t knishio_response_continu_id_count(knishio_response_continu_id_t *response) {
    if (!response) {
        return 0;
    }
    
    return response->identity_count;
}

knishio_continu_id_info_t* knishio_response_continu_id_get_primary(knishio_response_continu_id_t *response) {
    if (!response || response->identity_count == 0) {
        return NULL;
    }
    
    /* First identity is considered primary */
    return &response->identities[0];
}

const char* knishio_response_continu_id_get_primary_bundle(knishio_response_continu_id_t *response) {
    if (!response) {
        return NULL;
    }
    
    return response->primary_bundle;
}

const char* knishio_response_continu_id_get_token_slug(knishio_response_continu_id_t *response) {
    if (!response) {
        return NULL;
    }
    
    return response->requested_slug;
}

bool knishio_response_continu_id_has_identities(knishio_response_continu_id_t *response) {
    return response && response->identity_count > 0;
}

knishio_continu_id_info_t* knishio_response_continu_id_find_by_bundle(knishio_response_continu_id_t *response,
                                                                      const char *bundle_hash) {
    if (!response || !bundle_hash) {
        return NULL;
    }
    
    for (size_t i = 0; i < response->identity_count; i++) {
        if (response->identities[i].bundle_hash && 
            strcmp(response->identities[i].bundle_hash, bundle_hash) == 0) {
            return &response->identities[i];
        }
    }
    
    return NULL;
}

knishio_continu_id_info_t* knishio_response_continu_id_find_by_address(knishio_response_continu_id_t *response,
                                                                       const char *address) {
    if (!response || !address) {
        return NULL;
    }
    
    for (size_t i = 0; i < response->identity_count; i++) {
        if (response->identities[i].address && 
            strcmp(response->identities[i].address, address) == 0) {
            return &response->identities[i];
        }
    }
    
    return NULL;
}

/* ========== Identity Information Accessors ========== */

const char* knishio_continu_id_info_get_bundle_hash(knishio_continu_id_info_t *identity) {
    if (!identity) {
        return NULL;
    }
    
    return identity->bundle_hash;
}

const char* knishio_continu_id_info_get_position(knishio_continu_id_info_t *identity) {
    if (!identity) {
        return NULL;
    }
    
    return identity->position;
}

const char* knishio_continu_id_info_get_address(knishio_continu_id_info_t *identity) {
    if (!identity) {
        return NULL;
    }
    
    return identity->address;
}

const char* knishio_continu_id_info_get_pubkey(knishio_continu_id_info_t *identity) {
    if (!identity) {
        return NULL;
    }
    
    return identity->pubkey;
}

const char* knishio_continu_id_info_get_created_at(knishio_continu_id_info_t *identity) {
    if (!identity) {
        return NULL;
    }
    
    return identity->created_at;
}

knishio_json_t* knishio_continu_id_info_get_metadata(knishio_continu_id_info_t *identity) {
    if (!identity) {
        return NULL;
    }
    
    return identity->metadata;
}

bool knishio_continu_id_info_get_balance(knishio_continu_id_info_t *identity, double *balance) {
    if (!identity || !balance) {
        return false;
    }
    
    if (identity->has_balance) {
        *balance = identity->balance;
        return true;
    }
    
    return false;
}

const char* knishio_continu_id_info_get_metadata_string(knishio_continu_id_info_t *identity,
                                                        const char *key) {
    if (!identity || !key || !identity->metadata) {
        return NULL;
    }
    
    return knishio_json_get_string(identity->metadata, key);
}

bool knishio_continu_id_info_get_metadata_number(knishio_continu_id_info_t *identity,
                                                 const char *key,
                                                 double *value) {
    if (!identity || !key || !value || !identity->metadata) {
        return false;
    }
    
    return knishio_json_get_number(identity->metadata, key, value);
}

/* ========== Iterator Functions ========== */

bool knishio_response_continu_id_foreach(knishio_response_continu_id_t *response,
                                         knishio_continu_id_iterator_fn callback,
                                         void *user_data) {
    if (!response || !callback) {
        return false;
    }
    
    for (size_t i = 0; i < response->identity_count; i++) {
        if (!callback(i, &response->identities[i], user_data)) {
            return false; /* Callback requested stop */
        }
    }
    
    return true;
}

/* ========== Memory Management Helpers ========== */

void knishio_continu_id_info_free(knishio_continu_id_info_t *identity) {
    if (!identity) {
        return;
    }
    
    free_continu_id_info(identity);
}

void knishio_continu_id_info_free_array(knishio_continu_id_info_t *identities, size_t count) {
    if (!identities) {
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        free_continu_id_info(&identities[i]);
    }
    
    knishio_free(identities);
}

/* ========== Conversion Functions ========== */

knishio_response_t* knishio_response_continu_id_to_base(knishio_response_continu_id_t *continu_id_response) {
    if (!continu_id_response) {
        return NULL;
    }
    
    return &continu_id_response->base;
}

knishio_response_continu_id_t* knishio_response_continu_id_from_base(knishio_response_t *base_response) {
    if (!base_response) {
        return NULL;
    }
    
    /* Check if this is actually a ContinuID response */
    /* This is a simple check - in a real implementation you'd want a more robust type system */
    return (knishio_response_continu_id_t*)base_response;
}

/* ========== Helper Functions ========== */

static bool parse_continu_id_data(knishio_json_t *json, knishio_continu_id_info_t *identity) {
    if (!json || !identity) {
        return false;
    }
    
    /* Initialize identity */
    memset(identity, 0, sizeof(knishio_continu_id_info_t));
    
    /* Parse bundle hash */
    const char *bundle_hash = knishio_json_get_string(json, "bundleHash");
    if (bundle_hash) {
        identity->bundle_hash = knishio_strdup(bundle_hash);
        if (!identity->bundle_hash) {
            return false;
        }
    }
    
    /* Parse position */
    const char *position = knishio_json_get_string(json, "position");
    if (position) {
        identity->position = knishio_strdup(position);
        if (!identity->position) {
            free_continu_id_info(identity);
            return false;
        }
    }
    
    /* Parse address */
    const char *address = knishio_json_get_string(json, "address");
    if (address) {
        identity->address = knishio_strdup(address);
        if (!identity->address) {
            free_continu_id_info(identity);
            return false;
        }
    }
    
    /* Parse public key */
    const char *pubkey = knishio_json_get_string(json, "pubkey");
    if (pubkey) {
        identity->pubkey = knishio_strdup(pubkey);
        if (!identity->pubkey) {
            free_continu_id_info(identity);
            return false;
        }
    }
    
    /* Parse created at */
    const char *created_at = knishio_json_get_string(json, "createdAt");
    if (created_at) {
        identity->created_at = knishio_strdup(created_at);
        if (!identity->created_at) {
            free_continu_id_info(identity);
            return false;
        }
    }
    
    /* Parse balance */
    if (knishio_json_get_number(json, "amount", &identity->balance)) {
        identity->has_balance = true;
    }
    
    /* Parse metadata (if present) */
    knishio_json_t *metadata = NULL;
    if (knishio_json_get_object(json, "metadata", &metadata) == KNISHIO_SUCCESS && metadata) {
        identity->metadata = knishio_json_duplicate(metadata);
        if (!identity->metadata) {
            free_continu_id_info(identity);
            return false;
        }
    }
    
    return true;
}

static void free_continu_id_info(knishio_continu_id_info_t *identity) {
    if (!identity) {
        return;
    }
    
    knishio_free(identity->bundle_hash);
    knishio_free(identity->position);
    knishio_free(identity->address);
    knishio_free(identity->pubkey);
    knishio_free(identity->created_at);
    
    if (identity->metadata) {
        knishio_json_free(identity->metadata);
    }
    
    memset(identity, 0, sizeof(knishio_continu_id_info_t));
}