/**
 * @file policy_meta.c
 * @brief PolicyMeta implementation for KnishIO C SDK
 * 
 * Equivalent to JavaScript PolicyMeta class functionality
 */

#include "knishio/policy/engine.h"
#include "knishio/knishio.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief PolicyMeta structure
 * Equivalent to JavaScript PolicyMeta class
 */
typedef struct {
    cJSON* policy;                    /**< Normalized policy object */
} knishio_policy_meta_t;

/* Helper function to find array differences */
static char** array_diff(const char** array1, size_t size1, 
                        const char** array2, size_t size2, size_t* diff_size) {
    if (!array1 || !array2 || !diff_size) {
        *diff_size = 0;
        return NULL;
    }
    
    char** diff_array = malloc(size1 * sizeof(char*));
    if (!diff_array) {
        *diff_size = 0;
        return NULL;
    }
    
    size_t diff_count = 0;
    
    for (size_t i = 0; i < size1; i++) {
        bool found = false;
        for (size_t j = 0; j < size2; j++) {
            if (strcmp(array1[i], array2[j]) == 0) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            diff_array[diff_count] = knishio_strdup(array1[i]);
            if (diff_array[diff_count]) {
                diff_count++;
            }
        }
    }
    
    *diff_size = diff_count;
    return diff_array;
}

/* Helper function to free string array */
static void free_string_array(char** array, size_t size) {
    if (!array) return;
    
    for (size_t i = 0; i < size; i++) {
        if (array[i]) free(array[i]);
    }
    free(array);
}

/**
 * @brief Create new PolicyMeta instance
 * Equivalent to JavaScript: new PolicyMeta(policy, metaKeys)
 */
knishio_policy_meta_t* knishio_policy_meta_create(
    cJSON* policy,
    const char** meta_keys,
    size_t meta_key_count
) {
    knishio_policy_meta_t* policy_meta = calloc(1, sizeof(knishio_policy_meta_t));
    if (!policy_meta) {
        return NULL;
    }
    
    /* Normalize policy */
    policy_meta->policy = knishio_policy_normalize_json(policy);
    if (!policy_meta->policy) {
        policy_meta->policy = cJSON_CreateObject();
    }
    
    /* Fill defaults */
    if (meta_keys && meta_key_count > 0) {
        knishio_policy_fill_defaults(policy_meta->policy, meta_keys, meta_key_count);
    }
    
    return policy_meta;
}

/**
 * @brief Normalize policy JSON structure
 * Equivalent to JavaScript: PolicyMeta.normalizePolicy()
 */
cJSON* knishio_policy_normalize_json(cJSON* policy) {
    if (!policy) {
        return cJSON_CreateObject();
    }
    
    cJSON* policy_meta = cJSON_CreateObject();
    if (!policy_meta) {
        return NULL;
    }
    
    const char* policy_keys[] = { "read", "write" };
    
    for (size_t i = 0; i < 2; i++) {
        const char* policy_key = policy_keys[i];
        cJSON* value = cJSON_GetObjectItem(policy, policy_key);
        
        if (value && !cJSON_IsNull(value)) {
            cJSON* policy_section = cJSON_CreateObject();
            if (policy_section) {
                if (cJSON_IsObject(value)) {
                    cJSON* key_item;
                    cJSON_ArrayForEach(key_item, value) {
                        if (key_item->string) {
                            cJSON_AddItemToObject(policy_section, key_item->string,
                                                cJSON_Duplicate(key_item, true));
                        }
                    }
                }
                cJSON_AddItemToObject(policy_meta, policy_key, policy_section);
            }
        }
    }
    
    return policy_meta;
}

/**
 * @brief Fill default policy values
 * Equivalent to JavaScript: PolicyMeta.fillDefault()
 */
knishio_error_t knishio_policy_fill_defaults(
    cJSON* policy_json,
    const char** meta_keys,
    size_t meta_key_count
) {
    if (!policy_json || !meta_keys) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Extract read and write policy keys */
    const char* policy_types[] = { "read", "write" };

    for (size_t type_idx = 0; type_idx < 2; type_idx++) {
        const char* policy_type = policy_types[type_idx];
        bool is_write_policy = (strcmp(policy_type, "write") == 0);

        /* Get or create policy section */
        cJSON* policy_section = cJSON_GetObjectItem(policy_json, policy_type);
        if (!policy_section) {
            policy_section = cJSON_CreateObject();
            cJSON_AddItemToObject(policy_json, policy_type, policy_section);
        }
        
        /* Get existing policy keys */
        size_t existing_count = 0;
        char** existing_keys = NULL;
        
        cJSON* key_item;
        cJSON_ArrayForEach(key_item, policy_section) {
            if (key_item->string) {
                existing_count++;
            }
        }
        
        if (existing_count > 0) {
            existing_keys = malloc(existing_count * sizeof(char*));
            if (existing_keys) {
                size_t idx = 0;
                cJSON_ArrayForEach(key_item, policy_section) {
                    if (key_item->string) {
                        existing_keys[idx++] = key_item->string;
                    }
                }
            }
        }
        
        /* Find missing keys */
        size_t diff_size;
        char** missing_keys = array_diff(meta_keys, meta_key_count,
                                        (const char**)existing_keys, existing_count,
                                        &diff_size);
        
        /* Add default policies for missing keys */
        for (size_t i = 0; i < diff_size; i++) {
            const char* key = missing_keys[i];
            
            /* Determine default policy */
            bool use_self_policy = (is_write_policy && 
                                   strcmp(key, "characters") != 0 && 
                                   strcmp(key, "pubkey") != 0);
            
            cJSON* default_array = cJSON_CreateArray();
            if (default_array) {
                const char* default_value = use_self_policy ? "self" : "all";
                cJSON_AddItemToArray(default_array, cJSON_CreateString(default_value));
                cJSON_AddItemToObject(policy_section, key, default_array);
            }
        }
        
        /* Cleanup */
        free_string_array(missing_keys, diff_size);
        if (existing_keys) free(existing_keys);
    }
    
    return KNISHIO_SUCCESS;
}

/**
 * @brief Get policy data
 * Equivalent to JavaScript: policyMeta.get()
 */
cJSON* knishio_policy_meta_get(const knishio_policy_meta_t* policy_meta) {
    if (!policy_meta || !policy_meta->policy) {
        return NULL;
    }
    
    return cJSON_Duplicate(policy_meta->policy, true);
}

/**
 * @brief Convert policy to JSON string
 * Equivalent to JavaScript: policyMeta.toJson()
 */
char* knishio_policy_meta_to_json(const knishio_policy_meta_t* policy_meta) {
    if (!policy_meta || !policy_meta->policy) {
        return NULL;
    }
    
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