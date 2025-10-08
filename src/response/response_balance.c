/**
 * @file response_balance.c
 * @brief Response for balance queries implementation
 * 
 * Implements balance response functionality compatible with the
 * JavaScript SDK's ResponseBalance class.
 */

#include "knishio/response/response_balance.h"
#include "knishio/response/response_wallet_list.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/json/parser.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create balance response
 */
knishio_response_balance_t* knishio_response_balance_create(knishio_query_t *query,
                                                            knishio_json_t *json) {
    if (!json) {
        return NULL;
    }

    knishio_response_balance_t *response = knishio_calloc(1, sizeof(knishio_response_balance_t));
    if (!response) {
        return NULL;
    }

    // Initialize base response
    knishio_response_t *base = knishio_response_create(query, json, "data.Balance");
    if (!base) {
        knishio_free(response);
        return NULL;
    }

    // Copy base response data
    memcpy(&response->base, base, sizeof(knishio_response_t));
    knishio_free(base); // Free the temporary base structure

    response->wallet = NULL;
    response->wallet_cached = false;

    return response;
}

/**
 * @brief Free balance response
 */
void knishio_response_balance_free(knishio_response_balance_t *response) {
    if (!response) {
        return;
    }

    // Free wallet if cached
    if (response->wallet) {
        knishio_wallet_free(response->wallet);
    }

    // Free base response
    knishio_response_free(&response->base);
    knishio_free(response);
}

/**
 * @brief Get wallet with balance
 */
knishio_wallet_t* knishio_response_balance_get_wallet(knishio_response_balance_t *response) {
    if (!response) {
        return NULL;
    }

    if (response->wallet_cached) {
        return response->wallet;
    }

    // Get wallet data from response
    knishio_json_t *wallet_data = knishio_response_data(&response->base);
    if (!wallet_data) {
        return NULL;
    }

    // Check if we have the required fields
    const char *bundle_hash = knishio_json_get_string_path(wallet_data, "bundleHash");
    const char *token_slug = knishio_json_get_string_path(wallet_data, "tokenSlug");

    if (!bundle_hash || !token_slug) {
        return NULL;
    }

    // Convert wallet data to client wallet
    response->wallet = knishio_response_wallet_list_to_client_wallet(wallet_data, NULL);
    response->wallet_cached = true;

    return response->wallet;
}

/**
 * @brief Get balance amount
 */
bool knishio_response_balance_get_amount(knishio_response_balance_t *response, double *amount) {
    if (!response || !amount) {
        return false;
    }

    knishio_json_t *data = knishio_response_data(&response->base);
    if (!data) {
        return false;
    }

    return knishio_json_get_number_path(data, "amount", amount);
}

/**
 * @brief Get token slug
 */
const char* knishio_response_balance_get_token_slug(knishio_response_balance_t *response) {
    if (!response) {
        return NULL;
    }

    return knishio_response_get_string(&response->base, "tokenSlug");
}

/**
 * @brief Get wallet bundle hash
 */
const char* knishio_response_balance_get_bundle_hash(knishio_response_balance_t *response) {
    if (!response) {
        return NULL;
    }

    return knishio_response_get_string(&response->base, "bundleHash");
}

/**
 * @brief Get wallet address
 */
const char* knishio_response_balance_get_address(knishio_response_balance_t *response) {
    if (!response) {
        return NULL;
    }

    return knishio_response_get_string(&response->base, "address");
}

/**
 * @brief Get wallet position
 */
const char* knishio_response_balance_get_position(knishio_response_balance_t *response) {
    if (!response) {
        return NULL;
    }

    return knishio_response_get_string(&response->base, "position");
}

/**
 * @brief Get wallet public key
 */
const char* knishio_response_balance_get_pubkey(knishio_response_balance_t *response) {
    if (!response) {
        return NULL;
    }

    return knishio_response_get_string(&response->base, "pubkey");
}

/**
 * @brief Get wallet creation timestamp
 */
const char* knishio_response_balance_get_created_at(knishio_response_balance_t *response) {
    if (!response) {
        return NULL;
    }

    return knishio_response_get_string(&response->base, "createdAt");
}

/**
 * @brief Check if response has valid balance data
 */
bool knishio_response_balance_has_data(knishio_response_balance_t *response) {
    if (!response) {
        return false;
    }

    knishio_json_t *data = knishio_response_data(&response->base);
    if (!data) {
        return false;
    }

    // Check for required fields
    const char *bundle_hash = knishio_json_get_string_path(data, "bundleHash");
    const char *token_slug = knishio_json_get_string_path(data, "tokenSlug");

    return (bundle_hash != NULL && token_slug != NULL);
}

/**
 * @brief Get token information if available
 */
bool knishio_response_balance_get_token_info(knishio_response_balance_t *response,
                                             const char **token_name,
                                             double *token_amount,
                                             double *token_supply,
                                             bool *fungibility) {
    if (!response) {
        return false;
    }

    knishio_json_t *data = knishio_response_data(&response->base);
    if (!data) {
        return false;
    }

    knishio_json_t *token_obj = knishio_json_object_get(data, "token");
    if (!token_obj) {
        return false;
    }

    bool found_any = false;

    if (token_name) {
        const char *name = knishio_json_get_string_path(token_obj, "name");
        if (name) {
            *token_name = name;
            found_any = true;
        } else {
            *token_name = NULL;
        }
    }

    if (token_amount) {
        if (knishio_json_get_number_path(token_obj, "amount", token_amount)) {
            found_any = true;
        } else {
            *token_amount = 0.0;
        }
    }

    if (token_supply) {
        if (knishio_json_get_number_path(token_obj, "supply", token_supply)) {
            found_any = true;
        } else {
            *token_supply = 0.0;
        }
    }

    if (fungibility) {
        if (knishio_json_get_bool_path(token_obj, "fungibility", fungibility)) {
            found_any = true;
        } else {
            *fungibility = false;
        }
    }

    return found_any;
}

/**
 * @brief Get batch ID if available
 */
const char* knishio_response_balance_get_batch_id(knishio_response_balance_t *response) {
    if (!response) {
        return NULL;
    }

    return knishio_response_get_string(&response->base, "batchId");
}

/**
 * @brief Get wallet characters if available
 */
const char* knishio_response_balance_get_characters(knishio_response_balance_t *response) {
    if (!response) {
        return NULL;
    }

    return knishio_response_get_string(&response->base, "characters");
}

/* Conversion functions */

/**
 * @brief Convert to base response
 */
knishio_response_t* knishio_response_balance_to_base(knishio_response_balance_t *balance_response) {
    return balance_response ? &balance_response->base : NULL;
}

/**
 * @brief Convert from base response
 */
knishio_response_balance_t* knishio_response_balance_from_base(knishio_response_t *base_response) {
    if (!base_response) {
        return NULL;
    }

    // This is a simple cast - in a production system, you'd want to verify the type
    return (knishio_response_balance_t*)base_response;
}