/**
 * @file policy_meta.c
 * @brief PolicyMeta implementation for KnishIO C SDK
 *
 * Equivalent to the JavaScript PolicyMeta class (PolicyMeta.js).
 */

#include "knishio/policy_meta.h"
#include "knishio/knishio.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct knishio_policy_meta {
    cJSON* policy;                    /**< Normalized policy object */
};

/* A JavaScript array index: "0" or a digit string without a leading zero, below 2^32 - 1. */
static bool js_array_index(const char* key, unsigned long long* value) {
    if (!key || !key[0] || (key[0] == '0' && key[1]) || strlen(key) > 10) {
        return false;
    }
    unsigned long long v = 0;
    for (const char* p = key; *p; p++) {
        if (*p < '0' || *p > '9') return false;
        v = v * 10 + (unsigned long long)(*p - '0');
    }
    if (v >= 4294967295ULL) return false;
    *value = v;
    return true;
}
typedef struct {
    cJSON* item;
    bool indexed;
    unsigned long long index;
    size_t position;
} js_member_t;

static int compare_js_members(const void* a, const void* b) {
    const js_member_t* x = a;
    const js_member_t* y = b;
    if (x->indexed != y->indexed) return x->indexed ? -1 : 1;
    if (x->indexed && x->index != y->index) return x->index < y->index ? -1 : 1;
    return x->position < y->position ? -1 : (x->position > y->position ? 1 : 0);
}

void knishio_json_js_order(cJSON* item) {
    if (!item) return;
    size_t n = 0;
    for (cJSON* child = item->child; child; child = child->next) {
        knishio_json_js_order(child);
        n++;
    }
    if (!cJSON_IsObject(item) || n < 2) return;

    js_member_t* members = malloc(n * sizeof *members);
    if (!members) return;
    size_t i = 0;
    for (cJSON* child = item->child; child; child = child->next, i++) {
        members[i].item = child;
        members[i].index = 0;
        members[i].indexed = js_array_index(child->string, &members[i].index);
        members[i].position = i;
    }
    qsort(members, n, sizeof *members, compare_js_members);

    /* Relink in the new order; cJSON keeps the list tail in child->prev. */
    item->child = members[0].item;
    for (i = 0; i < n; i++) {
        members[i].item->next = (i + 1 < n) ? members[i + 1].item : NULL;
        members[i].item->prev = (i > 0) ? members[i - 1].item : members[n - 1].item;
    }
    free(members);
}

/* JavaScript truthiness of a JSON value */
static bool js_truthy(const cJSON* v) {
    if (!v || cJSON_IsNull(v) || cJSON_IsFalse(v)) return false;
    if (cJSON_IsNumber(v)) return v->valuedouble != 0;
    if (cJSON_IsString(v)) return v->valuestring && v->valuestring[0];
    return true;
}

knishio_policy_meta_t* knishio_policy_meta_create(
    const cJSON* policy,
    const char** meta_keys,
    size_t meta_key_count
) {
    knishio_policy_meta_t* policy_meta = calloc(1, sizeof(knishio_policy_meta_t));
    if (!policy_meta) {
        return NULL;
    }
    policy_meta->policy = knishio_policy_normalize_json(policy);
    if (!policy_meta->policy
        || knishio_policy_fill_defaults(policy_meta->policy, meta_keys, meta_key_count) != KNISHIO_SUCCESS) {
        knishio_policy_meta_free(policy_meta);
        return NULL;
    }
    return policy_meta;
}

/* PolicyMeta.normalizePolicy(): for each non-null "read"/"write" member, in the policy's own
 * order, a new object with that member's entries (Object.entries: an array contributes its
 * indexes as keys; any other value contributes nothing). */
cJSON* knishio_policy_normalize_json(const cJSON* policy) {
    cJSON* policy_meta = cJSON_CreateObject();
    if (!policy_meta || !cJSON_IsObject(policy)) {
        return policy_meta;
    }
    const cJSON* member = NULL;
    cJSON_ArrayForEach(member, policy) {
        if (!member->string || cJSON_IsNull(member)
            || (strcmp(member->string, "read") != 0 && strcmp(member->string, "write") != 0)) {
            continue;
        }
        cJSON* section = cJSON_CreateObject();
        if (!section) {
            cJSON_Delete(policy_meta);
            return NULL;
        }
        const cJSON* entry = NULL;
        size_t index = 0;
        cJSON_ArrayForEach(entry, member) {
            char index_key[24];
            const char* key = entry->string;
            if (cJSON_IsArray(member)) {
                snprintf(index_key, sizeof index_key, "%zu", index++);
                key = index_key;
            }
            cJSON* copy = key ? cJSON_Duplicate(entry, true) : NULL;
            if (key && (!copy || !cJSON_AddItemToObject(section, key, copy))) {
                cJSON_Delete(copy);
                cJSON_Delete(section);
                cJSON_Delete(policy_meta);
                return NULL;
            }
        }
        /* A repeated member replaces the earlier one in place, as a JS object key does. */
        if (cJSON_GetObjectItemCaseSensitive(policy_meta, member->string)) {
            cJSON_ReplaceItemInObjectCaseSensitive(policy_meta, member->string, section);
        } else if (!cJSON_AddItemToObject(policy_meta, member->string, section)) {
            cJSON_Delete(section);
            cJSON_Delete(policy_meta);
            return NULL;
        }
    }
    return policy_meta;
}

/* PolicyMeta.fillDefault(): ensure "read" and "write" exist, then give every meta key whose
 * entry is missing or falsy the default (write: ["self"], except characters and pubkey;
 * read: ["all"]). An existing falsy entry is replaced where it stands. */
knishio_error_t knishio_policy_fill_defaults(
    cJSON* policy_json,
    const char** meta_keys,
    size_t meta_key_count
) {
    if (!policy_json || (meta_key_count > 0 && !meta_keys)) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    const char* policy_types[] = { "read", "write" };
    for (size_t t = 0; t < 2; t++) {
        const bool is_write = t == 1;
        cJSON* section = cJSON_GetObjectItemCaseSensitive(policy_json, policy_types[t]);
        if (!section) {
            section = cJSON_AddObjectToObject(policy_json, policy_types[t]);
            if (!section) {
                return KNISHIO_ERROR_MEMORY;
            }
        }
        if (!cJSON_IsObject(section)) {
            continue;   /* JS assigns properties on a primitive without effect */
        }
        for (size_t i = 0; i < meta_key_count; i++) {
            const char* key = meta_keys[i];
            if (!key || js_truthy(cJSON_GetObjectItemCaseSensitive(section, key))) {
                continue;
            }
            const bool self = is_write && strcmp(key, "characters") != 0 && strcmp(key, "pubkey") != 0;
            cJSON* value = cJSON_CreateArray();
            if (!value || !cJSON_AddItemToArray(value, cJSON_CreateString(self ? "self" : "all"))) {
                cJSON_Delete(value);
                return KNISHIO_ERROR_MEMORY;
            }
            if (cJSON_GetObjectItemCaseSensitive(section, key)) {
                cJSON_ReplaceItemInObjectCaseSensitive(section, key, value);
            } else if (!cJSON_AddItemToObject(section, key, value)) {
                cJSON_Delete(value);
                return KNISHIO_ERROR_MEMORY;
            }
        }
    }
    return KNISHIO_SUCCESS;
}

cJSON* knishio_policy_meta_get(const knishio_policy_meta_t* policy_meta) {
    if (!policy_meta || !policy_meta->policy) {
        return NULL;
    }
    return cJSON_Duplicate(policy_meta->policy, true);
}

char* knishio_policy_meta_to_json(const knishio_policy_meta_t* policy_meta) {
    if (!policy_meta || !policy_meta->policy) {
        return NULL;
    }
    knishio_json_js_order(policy_meta->policy);
    return cJSON_PrintUnformatted(policy_meta->policy);
}
/**
 * @brief Free PolicyMeta instance
 */
void knishio_policy_meta_free(knishio_policy_meta_t* policy_meta) {
    if (!policy_meta) return;
    
    if (policy_meta->policy) {
        cJSON_Delete(policy_meta->policy);
    }
    free(policy_meta);
}

/* Utility functions for policy operations */

/**
 * @brief Check if wallet has read access to meta key
 */
bool knishio_policy_meta_check_read_access(
    const knishio_policy_meta_t* policy_meta,
    const char* meta_key,
    const char* wallet_address
) {
    if (!policy_meta || !meta_key || !wallet_address) {
        return false;
    }
    
    cJSON* read_policy = cJSON_GetObjectItem(policy_meta->policy, "read");
    if (!read_policy) {
        return true; /* Default allow */
    }
    
    cJSON* key_policy = cJSON_GetObjectItem(read_policy, meta_key);
    if (!key_policy || !cJSON_IsArray(key_policy)) {
        return true; /* Default allow */
    }
    
    /* Check permissions */
    cJSON* permission;
    cJSON_ArrayForEach(permission, key_policy) {
        if (cJSON_IsString(permission)) {
            const char* perm_str = cJSON_GetStringValue(permission);
            if (strcmp(perm_str, "all") == 0 ||
                strcmp(perm_str, wallet_address) == 0) {
                return true;
            }
        }
    }
    
    return false;
}

/**
 * @brief Check if wallet has write access to meta key
 */
bool knishio_policy_meta_check_write_access(
    const knishio_policy_meta_t* policy_meta,
    const char* meta_key,
    const char* wallet_address
) {
    if (!policy_meta || !meta_key || !wallet_address) {
        return false;
    }
    
    cJSON* write_policy = cJSON_GetObjectItem(policy_meta->policy, "write");
    if (!write_policy) {
        return false; /* Default deny for write */
    }
    
    cJSON* key_policy = cJSON_GetObjectItem(write_policy, meta_key);
    if (!key_policy || !cJSON_IsArray(key_policy)) {
        return false; /* Default deny for write */
    }
    
    /* Check permissions */
    cJSON* permission;
    cJSON_ArrayForEach(permission, key_policy) {
        if (cJSON_IsString(permission)) {
            const char* perm_str = cJSON_GetStringValue(permission);
            if (strcmp(perm_str, "all") == 0 ||
                strcmp(perm_str, "self") == 0 ||
                strcmp(perm_str, wallet_address) == 0) {
                return true;
            }
        }
    }
    
    return false;
}

/**
 * @brief Merge two policy objects
 */
cJSON* knishio_policy_meta_merge_policies(cJSON* base_policy, cJSON* override_policy) {
    if (!base_policy && !override_policy) {
        return NULL;
    }
    
    if (!base_policy) {
        return cJSON_Duplicate(override_policy, true);
    }
    
    if (!override_policy) {
        return cJSON_Duplicate(base_policy, true);
    }
    
    cJSON* merged = cJSON_Duplicate(base_policy, true);
    if (!merged) {
        return NULL;
    }
    
    /* Merge read and write policies */
    const char* policy_types[] = { "read", "write" };
    
    for (size_t i = 0; i < 2; i++) {
        const char* policy_type = policy_types[i];
        
        cJSON* base_section = cJSON_GetObjectItem(merged, policy_type);
        cJSON* override_section = cJSON_GetObjectItem(override_policy, policy_type);
        
        if (override_section && cJSON_IsObject(override_section)) {
            if (!base_section) {
                cJSON_AddItemToObject(merged, policy_type, 
                                    cJSON_Duplicate(override_section, true));
            } else {
                /* Merge individual keys */
                cJSON* key_item;
                cJSON_ArrayForEach(key_item, override_section) {
                    if (key_item->string) {
                        cJSON_DeleteItemFromObject(base_section, key_item->string);
                        cJSON_AddItemToObject(base_section, key_item->string,
                                            cJSON_Duplicate(key_item, true));
                    }
                }
            }
        }
    }
    
    return merged;
}