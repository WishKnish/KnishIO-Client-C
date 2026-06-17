/**
 * @file parser.c
 * @brief JSON parsing implementation for KnishIO SDK
 */

#include "knishio/json/parser.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Include cJSON properly */
#ifdef __has_include
  #if __has_include(<cjson/cJSON.h>)
    #include <cjson/cJSON.h>
  #elif __has_include(<cJSON.h>)
    #include <cJSON.h>
  #endif
#else
  #include <cjson/cJSON.h>
#endif

/* Internal structures */
struct knishio_json_array {
    cJSON *cjson;
    bool owned;
};

struct knishio_json_object {
    cJSON *cjson;
    bool owned;
};

/* Helper to wrap cJSON in our structure */
static knishio_json_t* wrap_cjson(cJSON *cjson, bool owned) {
    if (!cjson) return NULL;
    
    knishio_json_t *json = knishio_calloc(1, sizeof(knishio_json_t));
    if (!json) {
        if (owned) cJSON_Delete(cjson);
        return NULL;
    }
    
    switch (cjson->type) {
        case cJSON_NULL:
            json->type = KNISHIO_JSON_NULL;
            break;
            
        case cJSON_False:
        case cJSON_True:
            json->type = KNISHIO_JSON_BOOL;
            json->data.bool_value = cJSON_IsTrue(cjson);
            break;
            
        case cJSON_Number:
            json->type = KNISHIO_JSON_NUMBER;
            json->data.number_value = cjson->valuedouble;
            break;
            
        case cJSON_String:
            json->type = KNISHIO_JSON_STRING;
            /* Reference the cJSON node's string (NOT a copy): it lives in the parent cJSON tree and
             * is freed when the owning OBJECT/ARRAY root is cJSON_Delete'd (knishio_json_free). This
             * keeps the pointer returned by knishio_json_get_string[_path] valid until the ROOT json
             * is freed — the borrowed-until-root-free contract every caller assumes (they strdup
             * before freeing the root). Fixes the get_string_path use-after-free, which freed the
             * wrapper (and thus the owned copy) before the borrowed pointer was read. */
            json->data.string_value = cjson->valuestring;
            break;
            
        case cJSON_Array:
            json->type = KNISHIO_JSON_ARRAY;
            json->data.array_value = knishio_calloc(1, sizeof(struct knishio_json_array));
            if (json->data.array_value) {
                json->data.array_value->cjson = cjson;
                json->data.array_value->owned = owned;
            }
            break;
            
        case cJSON_Object:
            json->type = KNISHIO_JSON_OBJECT;
            json->data.object_value = knishio_calloc(1, sizeof(struct knishio_json_object));
            if (json->data.object_value) {
                json->data.object_value->cjson = cjson;
                json->data.object_value->owned = owned;
            }
            break;
            
        default:
            knishio_free(json);
            if (owned) cJSON_Delete(cjson);
            return NULL;
    }
    
    return json;
}

/* Parse JSON string */
knishio_json_t* knishio_json_parse(const char *json_string, char **error_msg) {
    if (!json_string) {
        if (error_msg) *error_msg = knishio_strdup("NULL input string");
        return NULL;
    }
    
    const char *error_ptr = NULL;
    cJSON *cjson = cJSON_ParseWithOpts(json_string, &error_ptr, 0);
    
    if (!cjson) {
        if (error_msg && error_ptr) {
            char buffer[256];
            int position = (int)(error_ptr - json_string);
            snprintf(buffer, sizeof(buffer), "JSON parse error at position %d", position);
            *error_msg = knishio_strdup(buffer);
        }
        return NULL;
    }
    
    return wrap_cjson(cjson, true);
}

/* Parse JSON with length */
knishio_json_t* knishio_json_parse_n(const char *json_string, size_t length, char **error_msg) {
    if (!json_string) {
        if (error_msg) *error_msg = knishio_strdup("NULL input string");
        return NULL;
    }
    
    char *temp = knishio_malloc(length + 1);
    if (!temp) {
        if (error_msg) *error_msg = knishio_strdup("Memory allocation failed");
        return NULL;
    }
    
    memcpy(temp, json_string, length);
    temp[length] = '\0';
    
    knishio_json_t *result = knishio_json_parse(temp, error_msg);
    knishio_free(temp);
    
    return result;
}

/* Free JSON object */
void knishio_json_free(knishio_json_t *json) {
    if (!json) return;
    
    switch (json->type) {
        case KNISHIO_JSON_STRING:
            /* string_value references the cJSON tree's valuestring (see wrap_cjson) — it is NOT
             * owned by this wrapper, so do not free it here. It is released with the owning root's
             * cJSON tree (the OBJECT/ARRAY case below cJSON_Delete's that tree when owned). */
            break;
            
        case KNISHIO_JSON_ARRAY:
            if (json->data.array_value) {
                if (json->data.array_value->owned) {
                    cJSON_Delete(json->data.array_value->cjson);
                }
                knishio_free(json->data.array_value);
            }
            break;
            
        case KNISHIO_JSON_OBJECT:
            if (json->data.object_value) {
                if (json->data.object_value->owned) {
                    cJSON_Delete(json->data.object_value->cjson);
                }
                knishio_free(json->data.object_value);
            }
            break;
            
        default:
            break;
    }
    
    knishio_free(json);
}

/* Get JSON value type */
knishio_json_type_t knishio_json_get_type(const knishio_json_t *json) {
    return json ? json->type : KNISHIO_JSON_NULL;
}

/* Check if JSON is null */
bool knishio_json_is_null(const knishio_json_t *json) {
    return !json || json->type == KNISHIO_JSON_NULL;
}

/* Get boolean value */
bool knishio_json_get_bool(const knishio_json_t *json, bool *value) {
    if (!json || !value || json->type != KNISHIO_JSON_BOOL) {
        return false;
    }
    *value = json->data.bool_value;
    return true;
}

/* Get number value */
bool knishio_json_get_number(const knishio_json_t *json, double *value) {
    if (!json || !value || json->type != KNISHIO_JSON_NUMBER) {
        return false;
    }
    *value = json->data.number_value;
    return true;
}

/* Get integer value */
bool knishio_json_get_int(const knishio_json_t *json, int64_t *value) {
    if (!json || !value || json->type != KNISHIO_JSON_NUMBER) {
        return false;
    }
    *value = (int64_t)json->data.number_value;
    return true;
}

/* Get string value */
const char* knishio_json_get_string(const knishio_json_t *json) {
    if (!json || json->type != KNISHIO_JSON_STRING) {
        return NULL;
    }
    return json->data.string_value;
}

/* Get string value with length */
const char* knishio_json_get_string_n(const knishio_json_t *json, size_t *length) {
    if (!json || json->type != KNISHIO_JSON_STRING) {
        if (length) *length = 0;
        return NULL;
    }
    if (length) {
        *length = json->data.string_value ? strlen(json->data.string_value) : 0;
    }
    return json->data.string_value;
}

/* Get array size */
size_t knishio_json_array_size(const knishio_json_t *json) {
    if (!json || json->type != KNISHIO_JSON_ARRAY || !json->data.array_value) {
        return 0;
    }
    return cJSON_GetArraySize(json->data.array_value->cjson);
}

/* Get array item */
knishio_json_t* knishio_json_array_get(const knishio_json_t *json, size_t index) {
    if (!json || json->type != KNISHIO_JSON_ARRAY || !json->data.array_value) {
        return NULL;
    }
    
    cJSON *item = cJSON_GetArrayItem(json->data.array_value->cjson, (int)index);
    if (!item) return NULL;
    
    return wrap_cjson(item, false);
}

/* Get object property */
knishio_json_t* knishio_json_object_get(const knishio_json_t *json, const char *key) {
    if (!json || !key || json->type != KNISHIO_JSON_OBJECT || !json->data.object_value) {
        return NULL;
    }
    
    cJSON *item = cJSON_GetObjectItem(json->data.object_value->cjson, key);
    if (!item) return NULL;
    
    return wrap_cjson(item, false);
}

/* Check if object has property */
bool knishio_json_object_has(const knishio_json_t *json, const char *key) {
    if (!json || !key || json->type != KNISHIO_JSON_OBJECT || !json->data.object_value) {
        return false;
    }
    
    return cJSON_HasObjectItem(json->data.object_value->cjson, key);
}

/* Serialize JSON to string */
char* knishio_json_serialize(const knishio_json_t *json, bool pretty) {
    if (!json) return NULL;
    
    cJSON *cjson = NULL;
    
    switch (json->type) {
        case KNISHIO_JSON_NULL:
            cjson = cJSON_CreateNull();
            break;
            
        case KNISHIO_JSON_BOOL:
            cjson = cJSON_CreateBool(json->data.bool_value);
            break;
            
        case KNISHIO_JSON_NUMBER:
            cjson = cJSON_CreateNumber(json->data.number_value);
            break;
            
        case KNISHIO_JSON_STRING:
            cjson = cJSON_CreateString(json->data.string_value);
            break;
            
        case KNISHIO_JSON_ARRAY:
            if (json->data.array_value && json->data.array_value->cjson) {
                char *str = pretty ? 
                    cJSON_Print(json->data.array_value->cjson) :
                    cJSON_PrintUnformatted(json->data.array_value->cjson);
                return str;
            }
            break;
            
        case KNISHIO_JSON_OBJECT:
            if (json->data.object_value && json->data.object_value->cjson) {
                char *str = pretty ? 
                    cJSON_Print(json->data.object_value->cjson) :
                    cJSON_PrintUnformatted(json->data.object_value->cjson);
                return str;
            }
            break;
    }
    
    if (cjson) {
        char *str = pretty ? cJSON_Print(cjson) : cJSON_PrintUnformatted(cjson);
        cJSON_Delete(cjson);
        return str;
    }
    
    return NULL;
}

/* Validate JSON string */
bool knishio_json_validate(const char *json_string, char **error_msg) {
    knishio_json_t *json = knishio_json_parse(json_string, error_msg);
    if (json) {
        knishio_json_free(json);
        return true;
    }
    return false;
}

/* Missing functions for complete parser functionality */

/* Get object size */
size_t knishio_json_object_size(const knishio_json_t *json) {
    if (!json || json->type != KNISHIO_JSON_OBJECT || !json->data.object_value) {
        return 0;
    }
    return cJSON_GetArraySize(json->data.object_value->cjson);
}

/* Get object property keys */
bool knishio_json_object_keys(const knishio_json_t *json, char ***keys, size_t *count) {
    if (!json || !keys || !count || json->type != KNISHIO_JSON_OBJECT || !json->data.object_value) {
        if (count) *count = 0;
        return false;
    }
    
    cJSON *obj = json->data.object_value->cjson;
    size_t key_count = cJSON_GetArraySize(obj);
    
    if (key_count == 0) {
        *keys = NULL;
        *count = 0;
        return true;
    }
    
    char **key_array = knishio_calloc(key_count, sizeof(char*));
    if (!key_array) {
        *count = 0;
        return false;
    }
    
    cJSON *item = NULL;
    size_t i = 0;
    cJSON_ArrayForEach(item, obj) {
        if (item->string && i < key_count) {
            key_array[i] = knishio_strdup(item->string);
            if (!key_array[i]) {
                /* Cleanup on failure */
                for (size_t j = 0; j < i; j++) {
                    knishio_free(key_array[j]);
                }
                knishio_free(key_array);
                *count = 0;
                return false;
            }
            i++;
        }
    }
    
    *keys = key_array;
    *count = key_count;
    return true;
}

/* Array iterator */
bool knishio_json_array_foreach(const knishio_json_t *json,
                                bool (*callback)(size_t index, knishio_json_t *item, void *user_data),
                                void *user_data) {
    if (!json || !callback || json->type != KNISHIO_JSON_ARRAY || !json->data.array_value) {
        return false;
    }
    
    size_t array_size = knishio_json_array_size(json);
    for (size_t i = 0; i < array_size; i++) {
        knishio_json_t *item = knishio_json_array_get(json, i);
        if (!item) continue;
        
        if (!callback(i, item, user_data)) {
            knishio_json_free(item);
            return false;
        }
        knishio_json_free(item);
    }
    
    return true;
}

/* Object iterator */
bool knishio_json_object_foreach(const knishio_json_t *json,
                                 bool (*callback)(const char *key, knishio_json_t *value, void *user_data),
                                 void *user_data) {
    if (!json || !callback || json->type != KNISHIO_JSON_OBJECT || !json->data.object_value) {
        return false;
    }
    
    cJSON *obj = json->data.object_value->cjson;
    cJSON *item = NULL;
    
    cJSON_ArrayForEach(item, obj) {
        if (!item->string) continue;
        
        knishio_json_t *value = wrap_cjson(item, false);
        if (!value) continue;
        
        if (!callback(item->string, value, user_data)) {
            knishio_json_free(value);
            return false;
        }
        knishio_json_free(value);
    }
    
    return true;
}

/* Path navigation */
knishio_json_t* knishio_json_get_path(const knishio_json_t *json, const char *path) {
    if (!json || !path) return NULL;
    
    char *path_copy = knishio_strdup(path);
    if (!path_copy) return NULL;
    
    knishio_json_t *current = (knishio_json_t*)json;
    char *token = strtok(path_copy, ".");
    
    while (token && current) {
        if (current->type == KNISHIO_JSON_OBJECT) {
            knishio_json_t *next = knishio_json_object_get(current, token);
            if (current != json) knishio_json_free(current);  /* Don't free original */
            current = next;
        } else if (current->type == KNISHIO_JSON_ARRAY) {
            char *endptr;
            long index = strtol(token, &endptr, 10);
            if (*endptr != '\0' || index < 0) {
                if (current != json) knishio_json_free(current);
                current = NULL;
                break;
            }
            knishio_json_t *next = knishio_json_array_get(current, (size_t)index);
            if (current != json) knishio_json_free(current);
            current = next;
        } else {
            if (current != json) knishio_json_free(current);
            current = NULL;
            break;
        }
        token = strtok(NULL, ".");
    }
    
    knishio_free(path_copy);
    return current;
}

/* Path utility functions */
const char* knishio_json_get_string_path(const knishio_json_t *json, const char *path) {
    knishio_json_t *value = knishio_json_get_path(json, path);
    if (!value) return NULL;
    
    const char *result = knishio_json_get_string(value);
    knishio_json_free(value);
    return result;
}

bool knishio_json_get_number_path(const knishio_json_t *json, const char *path, double *value) {
    knishio_json_t *json_value = knishio_json_get_path(json, path);
    if (!json_value) return false;
    
    bool result = knishio_json_get_number(json_value, value);
    knishio_json_free(json_value);
    return result;
}

bool knishio_json_get_bool_path(const knishio_json_t *json, const char *path, bool *value) {
    knishio_json_t *json_value = knishio_json_get_path(json, path);
    if (!json_value) return false;
    
    bool result = knishio_json_get_bool(json_value, value);
    knishio_json_free(json_value);
    return result;
}

/* Extended serialization */
char* knishio_json_serialize_ex(const knishio_json_t *json, int indent, bool ensure_ascii) {
    if (!json) return NULL;
    
    /* For now, use standard serialization - cJSON doesn't directly support ensure_ascii */
    return knishio_json_serialize(json, indent > 0);
}

/* Deep clone */
knishio_json_t* knishio_json_clone(const knishio_json_t *json) {
    if (!json) return NULL;
    
    char *json_str = knishio_json_serialize(json, false);
    if (!json_str) return NULL;
    
    knishio_json_t *clone = knishio_json_parse(json_str, NULL);
    cJSON_free(json_str);
    
    return clone;
}

/* Equality comparison */
bool knishio_json_equal(const knishio_json_t *a, const knishio_json_t *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->type != b->type) return false;
    
    switch (a->type) {
        case KNISHIO_JSON_NULL:
            return true;
            
        case KNISHIO_JSON_BOOL:
            return a->data.bool_value == b->data.bool_value;
            
        case KNISHIO_JSON_NUMBER:
            return a->data.number_value == b->data.number_value;
            
        case KNISHIO_JSON_STRING:
            if (!a->data.string_value && !b->data.string_value) return true;
            if (!a->data.string_value || !b->data.string_value) return false;
            return strcmp(a->data.string_value, b->data.string_value) == 0;
            
        case KNISHIO_JSON_ARRAY: {
            size_t size_a = knishio_json_array_size(a);
            size_t size_b = knishio_json_array_size(b);
            if (size_a != size_b) return false;
            
            for (size_t i = 0; i < size_a; i++) {
                knishio_json_t *item_a = knishio_json_array_get(a, i);
                knishio_json_t *item_b = knishio_json_array_get(b, i);
                bool equal = knishio_json_equal(item_a, item_b);
                knishio_json_free(item_a);
                knishio_json_free(item_b);
                if (!equal) return false;
            }
            return true;
        }
        
        case KNISHIO_JSON_OBJECT: {
            size_t size_a = knishio_json_object_size(a);
            size_t size_b = knishio_json_object_size(b);
            if (size_a != size_b) return false;
            
            char **keys_a;
            size_t count_a;
            if (!knishio_json_object_keys(a, &keys_a, &count_a)) return false;
            
            bool equal = true;
            for (size_t i = 0; i < count_a && equal; i++) {
                if (!knishio_json_object_has(b, keys_a[i])) {
                    equal = false;
                    break;
                }
                
                knishio_json_t *value_a = knishio_json_object_get(a, keys_a[i]);
                knishio_json_t *value_b = knishio_json_object_get(b, keys_a[i]);
                equal = knishio_json_equal(value_a, value_b);
                knishio_json_free(value_a);
                knishio_json_free(value_b);
            }
            
            /* Cleanup keys */
            for (size_t i = 0; i < count_a; i++) {
                knishio_free(keys_a[i]);
            }
            knishio_free(keys_a);
            
            return equal;
        }
    }
    
    return false;
}

/* Helper functions for compatibility */

#include "knishio/error/context.h"
#include <string.h>
#include <stdio.h>

knishio_error_t knishio_json_get_object(const knishio_json_t *json, const char *key, knishio_json_t **value) {
    if (!json || !key || !value) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    *value = knishio_json_object_get(json, key);
    return (*value) ? KNISHIO_SUCCESS : KNISHIO_ERROR_NULL_POINTER;
}

knishio_error_t knishio_json_get_array(const knishio_json_t *json, const char *key, knishio_json_t **array) {
    if (!json || !key || !array) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    knishio_json_t *value = knishio_json_object_get(json, key);
    if (!value) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (knishio_json_get_type(value) != KNISHIO_JSON_ARRAY) {
        knishio_json_free(value);
        return KNISHIO_ERROR_INVALID_STATE;
    }
    
    *array = value;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_json_to_string(const knishio_json_t *json, char **output) {
    if (!json || !output) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Use cJSON library function if available */
    #ifdef HAVE_CJSON
    cJSON *cjson = knishio_json_to_cjson(json);
    if (!cjson) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    *output = cJSON_PrintUnformatted(cjson);
    cJSON_Delete(cjson);
    
    return (*output) ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
    #else
    /* Simple fallback implementation */
    /* This is a simplified version - production would need full implementation */
    size_t buffer_size = 4096;
    *output = knishio_calloc(buffer_size, 1);
    if (!*output) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Basic serialization */
    switch (knishio_json_get_type(json)) {
        case KNISHIO_JSON_NULL:
            strcpy(*output, "null");
            break;
        case KNISHIO_JSON_BOOL:
            {
                bool value;
                knishio_json_get_bool(json, &value);
                strcpy(*output, value ? "true" : "false");
            }
            break;
        case KNISHIO_JSON_NUMBER:
            {
                double value;
                knishio_json_get_number(json, &value);
                snprintf(*output, buffer_size, "%g", value);
            }
            break;
        case KNISHIO_JSON_STRING:
            {
                const char *str = knishio_json_get_string(json);
                snprintf(*output, buffer_size, "\"%s\"", str ? str : "");
            }
            break;
        default:
            /* For arrays and objects, return a placeholder */
            strcpy(*output, "{}");
            break;
    }
    
    return KNISHIO_SUCCESS;
    #endif
}
