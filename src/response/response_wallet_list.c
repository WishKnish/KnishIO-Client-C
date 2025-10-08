/**
 * @file response_wallet_list.c
 * @brief Wallet list response implementation placeholder
 */

#include "knishio/response/response_wallet_list.h"
#include "knishio/utils/memory.h"
#include <stdlib.h>

/**
 * @brief Convert wallet data to client wallet (placeholder)
 */
knishio_wallet_t* knishio_response_wallet_list_to_client_wallet(knishio_json_t *wallet_data,
                                                                const char *secret) {
    (void)secret; // Suppress unused parameter warning
    
    if (!wallet_data) {
        return NULL;
    }

    knishio_wallet_t *wallet = knishio_calloc(1, sizeof(knishio_wallet_t));
    if (!wallet) {
        return NULL;
    }

    // Extract basic wallet data from JSON
    const char *bundle_hash = knishio_json_get_string_path(wallet_data, "bundleHash");
    if (bundle_hash) {
        wallet->bundle_hash = knishio_strdup(bundle_hash);
    }

    const char *token_slug = knishio_json_get_string_path(wallet_data, "tokenSlug");
    if (token_slug) {
        wallet->token = knishio_strdup(token_slug);
    }

    const char *address = knishio_json_get_string_path(wallet_data, "address");
    if (address) {
        wallet->address = knishio_strdup(address);
    }

    const char *position = knishio_json_get_string_path(wallet_data, "position");
    if (position) {
        wallet->position = knishio_strdup(position);
    }

    const char *pubkey = knishio_json_get_string_path(wallet_data, "pubkey");
    if (pubkey) {
        wallet->pubkey = knishio_strdup(pubkey);
    }

    const char *created_at = knishio_json_get_string_path(wallet_data, "createdAt");
    if (created_at) {
        wallet->created_at = knishio_strdup(created_at);
    }

    double amount;
    if (knishio_json_get_number_path(wallet_data, "amount", &amount)) {
        wallet->balance = amount;
    }

    return wallet;
}

/**
 * @brief Free wallet (placeholder)
 */
void knishio_wallet_free(knishio_wallet_t *wallet) {
    if (!wallet) {
        return;
    }

    knishio_free(wallet->bundle_hash);
    knishio_free(wallet->token);
    knishio_free(wallet->address);
    knishio_free(wallet->position);
    knishio_free(wallet->pubkey);
    knishio_free(wallet->created_at);
    knishio_free(wallet);
}