/**
 * @file meta.c
 * @brief Meta implementation for KnishIO C SDK
 * 
 * Implements JavaScript-compatible metadata handling with C17 best practices.
 * Meta objects represent key-value pairs conveyed by atoms.
 * Simple KISS design following ultrathink methodology.
 */

#include "knishio/knishio.h"
#include "knishio/meta.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* Internal helper function declarations */
static knishio_error_t copy_string_field(char** dest, const char* src);
static void free_string_field(char** field);
static knishio_error_t resize_meta_array(knishio_meta_array_t* array);

/* Meta lifecycle functions */

knishio_error_t knishio_meta_create(
    knishio_meta_t** meta,
    const char* key,
    const char* value
) {
    if (!meta || !key || !value) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Validate key and value lengths */
    if (strlen(key) > KNISHIO_MAX_META_KEY_LENGTH ||
        strlen(value) > KNISHIO_MAX_META_VALUE_LENGTH) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate meta structure */
    knishio_meta_t* m = knishio_malloc(sizeof(knishio_meta_t));
    if (!m) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Initialize all fields to safe defaults */
    memset(m, 0, sizeof(knishio_meta_t));

    /* Copy key and value strings */
    knishio_error_t error = KNISHIO_SUCCESS;
    
    if ((error = copy_string_field(&m->key, key)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    
    if ((error = copy_string_field(&m->value, value)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    *meta = m;
    return KNISHIO_SUCCESS;

cleanup:
    knishio_meta_free(m);
    return error;
}

void knishio_meta_free(knishio_meta_t* meta) {
    if (!meta) {
        return;
    }

    /* Free string fields */
    free_string_field(&meta->key);
    free_string_field(&meta->value);

    /* Free the meta structure itself */
    knishio_free(meta);
}

/* Meta property functions */

const char* knishio_meta_get_key(const knishio_meta_t* meta) {
    return meta ? meta->key : NULL;
}

const char* knishio_meta_get_value(const knishio_meta_t* meta) {
    return meta ? meta->value : NULL;
}

knishio_error_t knishio_meta_set_value(knishio_meta_t* meta, const char* value) {
    if (!meta || !value) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    if (strlen(value) > KNISHIO_MAX_META_VALUE_LENGTH) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    free_string_field(&meta->value);
    return copy_string_field(&meta->value, value);
}

/* Meta array lifecycle functions */

knishio_error_t knishio_meta_array_create(knishio_meta_array_t** array) {
    if (!array) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate array structure */
    knishio_meta_array_t* arr = knishio_malloc(sizeof(knishio_meta_array_t));
    if (!arr) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Initialize with small initial capacity */
    const size_t initial_capacity = 4;
    arr->items = knishio_malloc(sizeof(knishio_meta_t*) * initial_capacity);
    if (!arr->items) {
        knishio_free(arr);
        return KNISHIO_ERROR_MEMORY;
    }

    arr->count = 0;
    arr->capacity = initial_capacity;

    *array = arr;
    return KNISHIO_SUCCESS;
}

void knishio_meta_array_free(knishio_meta_array_t* array, bool free_items) {
    if (!array) {
        return;
    }

    /* Free items if requested */
    if (free_items && array->items) {
        for (size_t i = 0; i < array->count; i++) {
            knishio_meta_free(array->items[i]);
        }
    }

    /* Free items array */
    if (array->items) {
        knishio_free(array->items);
    }

    /* Free array structure */
    knishio_free(array);
}

knishio_error_t knishio_meta_array_add(knishio_meta_array_t* array, knishio_meta_t* meta) {
    if (!array || !meta) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Resize array if needed */
    if (array->count >= array->capacity) {
        knishio_error_t error = resize_meta_array(array);
        if (error != KNISHIO_SUCCESS) {
            return error;
        }
    }

    /* Add meta to array */
    array->items[array->count] = meta;
    array->count++;

    return KNISHIO_SUCCESS;
}

knishio_meta_t* knishio_meta_array_get(const knishio_meta_array_t* array, size_t index) {
    if (!array || index >= array->count) {
        return NULL;
    }
    
    return array->items[index];
}

size_t knishio_meta_array_size(const knishio_meta_array_t* array) {
    return array ? array->count : 0;
}

knishio_meta_t* knishio_meta_array_find(const knishio_meta_array_t* array, const char* key) {
    if (!array || !key) {
        return NULL;
    }

    for (size_t i = 0; i < array->count; i++) {
        if (array->items[i] && array->items[i]->key &&
            strcmp(array->items[i]->key, key) == 0) {
            return array->items[i];
        }
    }

    return NULL;
}

/* JavaScript SDK compatibility functions */

knishio_error_t knishio_meta_normalize(
    const char* input_json,
    knishio_meta_array_t** output_array
) {
    if (!input_json || !output_array) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Create output array */
    knishio_error_t error = knishio_meta_array_create(output_array);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* TODO: Parse JSON object and convert to meta array */
    /* For now, implement a simple placeholder */
    /* This would use the JSON parser to parse the object and create meta items */
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_meta_aggregate(
    const knishio_meta_array_t* input_array,
    char** output_json
) {
    if (!input_array || !output_json) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* TODO: Convert meta array to JSON object */
    /* For now, implement a simple placeholder */
    
    /* Create empty object as placeholder */
    const char* placeholder_json = "{}";
    size_t json_len = strlen(placeholder_json);
    char* json_copy = knishio_malloc(json_len + 1);
    if (!json_copy) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    strcpy(json_copy, placeholder_json);
    *output_json = json_copy;
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_meta_array_from_json_object(
    const char* json_input,
    knishio_meta_array_t** output_array
) {
    /* This is the same as normalize - converting object to array */
    return knishio_meta_normalize(json_input, output_array);
}

knishio_error_t knishio_meta_array_from_json_array(
    const char* json_input,
    knishio_meta_array_t** output_array
) {
    if (!json_input || !output_array) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Create output array */
    knishio_error_t error = knishio_meta_array_create(output_array);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* TODO: Parse JSON array and create meta items */
    /* This would parse JSON like [{"key": "name", "value": "Alice"}, ...] */
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_meta_array_to_json(
    const knishio_meta_array_t* array,
    char** output_json
) {
    if (!array || !output_json) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Simple implementation using string concatenation (KISS approach) */
    char json_buffer[4096];  /* Reasonable buffer for most cases */
    strcpy(json_buffer, "[");
    
    for (size_t i = 0; i < array->count; i++) {
        if (i > 0) {
            strcat(json_buffer, ",");
        }
        
        knishio_meta_t* meta = array->items[i];
        if (meta && meta->key && meta->value) {
            char item_buffer[512];
            snprintf(item_buffer, sizeof(item_buffer),
                "{\"key\":\"%s\",\"value\":\"%s\"}",
                meta->key, meta->value);
            strcat(json_buffer, item_buffer);
        }
    }
    
    strcat(json_buffer, "]");
    
    size_t json_len = strlen(json_buffer);
    char* json_copy = knishio_malloc(json_len + 1);
    if (!json_copy) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    strcpy(json_copy, json_buffer);
    *output_json = json_copy;
    
    return KNISHIO_SUCCESS;
}

/* Validation functions */

knishio_error_t knishio_meta_validate(const knishio_meta_t* meta) {
    if (!meta) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Check required fields */
    if (!meta->key || !meta->value) {
        return KNISHIO_ERROR_META_MISSING;
    }

    /* Check field lengths */
    if (strlen(meta->key) > KNISHIO_MAX_META_KEY_LENGTH ||
        strlen(meta->value) > KNISHIO_MAX_META_VALUE_LENGTH) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_meta_array_validate(const knishio_meta_array_t* array) {
    if (!array) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Validate all items in array */
    for (size_t i = 0; i < array->count; i++) {
        knishio_error_t error = knishio_meta_validate(array->items[i]);
        if (error != KNISHIO_SUCCESS) {
            return error;
        }
    }

    return KNISHIO_SUCCESS;
}

/* Internal helper functions */

static knishio_error_t copy_string_field(char** dest, const char* src) {
    if (!dest || !src) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    size_t len = strlen(src);
    char* copy = knishio_malloc(len + 1);
    if (!copy) {
        return KNISHIO_ERROR_MEMORY;
    }

    strcpy(copy, src);
    *dest = copy;
    return KNISHIO_SUCCESS;
}

static void free_string_field(char** field) {
    if (field && *field) {
        knishio_free(*field);
        *field = NULL;
    }
}

static knishio_error_t resize_meta_array(knishio_meta_array_t* array) {
    if (!array) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    size_t new_capacity = array->capacity * 2;
    knishio_meta_t** new_items = knishio_realloc(
        array->items, 
        sizeof(knishio_meta_t*) * new_capacity
    );
    if (!new_items) {
        return KNISHIO_ERROR_MEMORY;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    return KNISHIO_SUCCESS;
}
