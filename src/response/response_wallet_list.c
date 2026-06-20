/**
 * @file response_wallet_list.c
 * @brief Wallet list response implementation placeholder
 */

#include "knishio/response/response_wallet_list.h"
#include "knishio/utils/memory.h"
#include "knishio/json/parser.h"
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

    /* Stackable (NFT) token units (forward-compat; the validator's tokenUnits resolver is a stub
     * returning [] until gap SDK-001 lands, so this populates only when the response carries units).
     * Canonical wire form per unit: { id, name, metas }; metas stored as a JSON string (default "{}"). */
    knishio_json_t *units = knishio_json_object_get(wallet_data, "tokenUnits");
    size_t units_count = units ? knishio_json_array_size(units) : 0;
    if (units_count > 0) {
        wallet->token_units = knishio_calloc(units_count, sizeof(knishio_token_unit_t));
        if (wallet->token_units) {
            for (size_t i = 0; i < units_count; i++) {
                knishio_json_t *u = knishio_json_array_get(units, i);
                if (!u) {
                    continue;
                }
                knishio_json_t *id = knishio_json_object_get(u, "id");
                knishio_json_t *name = knishio_json_object_get(u, "name");
                knishio_json_t *metas = knishio_json_object_get(u, "metas");
                const char *id_s = id ? knishio_json_get_string(id) : NULL;
                const char *name_s = name ? knishio_json_get_string(name) : NULL;
                knishio_token_unit_t *slot = &wallet->token_units[wallet->token_unit_count];
                slot->id = id_s ? knishio_strdup(id_s) : NULL;
                slot->name = name_s ? knishio_strdup(name_s) : NULL;
                /* metas may arrive as a JSON string or an object; store as a JSON string. */
                char *metas_json = NULL;
                if (metas) {
                    const char *ms = knishio_json_get_string(metas);
                    metas_json = ms ? knishio_strdup(ms) : knishio_json_serialize(metas, false);
                }
                slot->metas_json = metas_json ? metas_json : knishio_strdup("{}");
                wallet->token_unit_count++;
            }
        }
    }

    return wallet;
}

/* knishio_wallet_free removed: the canonical definition lives in wallet.c
 * (this placeholder was a duplicate symbol that macOS ld64 rejects). */