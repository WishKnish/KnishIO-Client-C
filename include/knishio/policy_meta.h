/**
 * @file policy_meta.h
 * @brief PolicyMeta: the read/write policy an atom carries in its "policy" meta (JS PolicyMeta)
 */
#ifndef KNISHIO_POLICY_META_H
#define KNISHIO_POLICY_META_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>

#include "knishio/error/context.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief A normalized policy with defaults for every meta key (opaque) */
typedef struct knishio_policy_meta knishio_policy_meta_t;

/**
 * @brief new PolicyMeta(policy, metaKeys)
 *
 * Keeps the policy's non-null "read" and "write" members in their order, adds whichever of
 * the two is missing, and gives every meta key without a truthy entry the default ["self"]
 * (write; ["all"] for characters and pubkey) or ["all"] (read). JS AtomMeta passes the INDEXES
 * of its meta entries ("0", "1", ...) as metaKeys, not their names.
 *
 * @param policy Policy object (not modified); NULL counts as {}
 * @param meta_keys Meta keys; may be NULL when meta_key_count is 0
 * @param meta_key_count Number of meta keys
 * @return New PolicyMeta, or NULL on allocation failure
 */
knishio_policy_meta_t* knishio_policy_meta_create(
    const cJSON* policy,
    const char** meta_keys,
    size_t meta_key_count
);

/** @brief PolicyMeta.normalizePolicy(): a new object with the non-null read/write members */
cJSON* knishio_policy_normalize_json(const cJSON* policy);

/** @brief PolicyMeta.fillDefault(metaKeys) on a normalized policy, in place */
knishio_error_t knishio_policy_fill_defaults(
    cJSON* policy_json,
    const char** meta_keys,
    size_t meta_key_count
);

/** @brief policyMeta.get(): a copy of the policy object (caller deletes) */
cJSON* knishio_policy_meta_get(const knishio_policy_meta_t* policy_meta);

/**
 * @brief policyMeta.toJson(): JSON.stringify of the policy (caller frees with cJSON_free)
 *
 * Object members print in JavaScript property order (see knishio_json_js_order).
 */
char* knishio_policy_meta_to_json(const knishio_policy_meta_t* policy_meta);

/** @brief Free a PolicyMeta */
void knishio_policy_meta_free(knishio_policy_meta_t* policy_meta);

/** @brief Whether `wallet_address` may read `meta_key` ("all" or the address listed) */
bool knishio_policy_meta_check_read_access(
    const knishio_policy_meta_t* policy_meta,
    const char* meta_key,
    const char* wallet_address
);

/** @brief Whether `wallet_address` may write `meta_key` ("all", "self" or the address listed) */
bool knishio_policy_meta_check_write_access(
    const knishio_policy_meta_t* policy_meta,
    const char* meta_key,
    const char* wallet_address
);

/** @brief Merge two policies; members of `override_policy` replace those of `base_policy` */
cJSON* knishio_policy_meta_merge_policies(cJSON* base_policy, cJSON* override_policy);

/**
 * @brief Reorder every object in `item` (recursively) into JavaScript property order
 *
 * JavaScript enumerates (and JSON.stringify prints) array-index keys ("0", "1", ... below
 * 2^32 - 1) first in ascending numeric order, then the other keys in insertion order. cJSON
 * keeps insertion order only; call this before printing JSON that must equal JS bytes.
 */
void knishio_json_js_order(cJSON* item);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_POLICY_META_H */
