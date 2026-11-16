/**
 * @file response_wallet_list.c
 * @brief Wallet list response implementation placeholder
 */

#include "knishio/response/response_wallet_list.h"
#include "knishio/utils/memory.h"
#include "knishio/json/parser.h"
#include <stdlib.h>
#include <string.h>

/* JS truthiness of a JSON value: null, false, 0 and "" are falsy. */
static bool json_js_falsy(const knishio_json_t *v) {
    double d = 0;
    bool b = false;
    switch (knishio_json_get_type(v)) {
        case KNISHIO_JSON_NULL:
            return true;
        case KNISHIO_JSON_BOOL:
            return knishio_json_get_bool(v, &b) && !b;
        case KNISHIO_JSON_NUMBER:
            return knishio_json_get_number(v, &d) && (d == 0 || d != d);
        case KNISHIO_JSON_STRING: {
            const char *s = knishio_json_get_string(v);
            return !s || !s[0];
        }
        default:
            return false;
    }
}

/* A unit's metas as JS TokenUnit.createFromGraphQL reads them, stored as a JSON string:
 * `data.metas || {}`, then a non-empty string is JSON-parsed and a falsy result becomes {}.
 * So absent, null, false, 0, "" and the strings "null", "false", "0" give "{}"; an object,
 * array or other value is kept. A string that is not JSON is kept as given (JS throws there;
 * the molecule builder falls back to {} for it). */
static char *metas_like_js(const knishio_json_t *metas) {
    if (!metas || json_js_falsy(metas)) {
        return knishio_strdup("{}");
    }
    if (knishio_json_get_type(metas) == KNISHIO_JSON_STRING) {
        const char *ms = knishio_json_get_string(metas);
        knishio_json_t *parsed = knishio_json_parse(ms, NULL);
        bool falsy = parsed && json_js_falsy(parsed);
        knishio_json_free(parsed);
        return knishio_strdup(falsy ? "{}" : ms);
    }
    return knishio_json_serialize(metas, false);
}

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

    const char *batch_id = knishio_json_get_string_path(wallet_data, "batchId");
    if (batch_id && batch_id[0]) {
        wallet->batch_id = knishio_strdup(batch_id);
    }

    const char *created_at = knishio_json_get_string_path(wallet_data, "createdAt");
    if (created_at) {
        wallet->created_at = knishio_strdup(created_at);
    }

    /* The validator returns Balance.amount as a GraphQL String (Wallet.amount() -> &str), so try the
     * string form first (strtod); fall back to a numeric amount for other shapes. (Was number-only,
     * which left balance=0 against this validator.) */
    const char *amount_str = knishio_json_get_string_path(wallet_data, "amount");
    double amount;
    if (amount_str) {
        wallet->balance = strtod(amount_str, NULL);
    } else if (knishio_json_get_number_path(wallet_data, "amount", &amount)) {
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
                /* metas may arrive as a JSON string or an object; store as a JSON string, read as
                 * JS TokenUnit.createFromGraphQL reads it (null metas -> {}). */
                char *metas_json = metas_like_js(metas);
                slot->metas_json = metas_json ? metas_json : knishio_strdup("{}");
                wallet->token_unit_count++;
                knishio_json_free(metas);
                knishio_json_free(name);
                knishio_json_free(id);
                knishio_json_free(u);
            }
        }
    }
    knishio_json_free(units);

    return wallet;
}

/* knishio_wallet_free removed: the canonical definition lives in wallet.c
 * (this placeholder was a duplicate symbol that macOS ld64 rejects). */