/**
 * @file atom.c
 * @brief Atom implementation for KnishIO C SDK
 * 
 * Implements JavaScript-compatible atomic operations with C17 best practices.
 * Atoms are smallest transactional units performing single, monodirectional actions.
 */

#include "knishio/knishio.h"
#include "knishio/atom.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"
#include "knishio/json/serializers.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

/* Internal helper function declarations */
static knishio_error_t copy_string_field(char** dest, const char* src);
static void free_string_field(char** field);
static knishio_error_t resize_meta_array(knishio_atom_t* atom);
static int compare_atoms_by_index(const void* a, const void* b);

/* Isotope string mapping */
static const char* isotope_strings[] = {
    NULL,    /* KNISHIO_ISOTOPE_UNKNOWN */
    "V",     /* KNISHIO_ISOTOPE_V */
    "C",     /* KNISHIO_ISOTOPE_C */
    "M",     /* KNISHIO_ISOTOPE_M */
    "U",     /* KNISHIO_ISOTOPE_U */
    "I",     /* KNISHIO_ISOTOPE_I */
    "R",     /* KNISHIO_ISOTOPE_R */
    "T",     /* KNISHIO_ISOTOPE_T */
    "L",     /* KNISHIO_ISOTOPE_L */
    "S"      /* KNISHIO_ISOTOPE_S */
};

/* Atom lifecycle functions */

knishio_error_t knishio_atom_create(
    knishio_atom_t** atom,
    const char* position,
    const char* wallet_address,
    knishio_isotope_t isotope,
    const char* token,
    const char* value,
    const char* batch_id
) {
    return knishio_atom_create_with_meta(
        atom, position, wallet_address, isotope, token, value, batch_id,
        NULL, NULL, NULL, 0
    );
}

knishio_error_t knishio_atom_create_with_meta(
    knishio_atom_t** atom,
    const char* position,
    const char* wallet_address,
    knishio_isotope_t isotope,
    const char* token,
    const char* value,
    const char* batch_id,
    const char* meta_type,
    const char* meta_id,
    knishio_meta_t** meta,
    size_t meta_count
) {
    if (!atom) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Validate required fields */
    if (!position || !wallet_address || !token) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Validate position and address lengths. Empty ("") is allowed: a shadow/burn-target
     * atom (e.g. the all-zeros burn bundle) has no position/address — keyed by bundle +
     * metaType/metaId instead. The molecular hash absorbs "" as a no-op, so this is canonical
     * (matches the other SDKs). A non-empty value must still be exactly 64 hex chars. */
    if ((strlen(position) != 0 && strlen(position) != KNISHIO_POSITION_LENGTH) ||
        (strlen(wallet_address) != 0 && strlen(wallet_address) != KNISHIO_ADDRESS_LENGTH)) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate atom structure */
    knishio_atom_t* at = knishio_malloc(sizeof(knishio_atom_t));
    if (!at) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Initialize all fields to safe defaults */
    memset(at, 0, sizeof(knishio_atom_t));
    
    /* Set creation timestamp - use environment variable for deterministic testing */
    const char* fixed_timestamp = getenv("KNISHIO_FIXED_TIMESTAMP");
    if (fixed_timestamp) {
        at->created_at = (time_t)atoll(fixed_timestamp);
    } else {
        at->created_at = time(NULL);
    }
    at->isotope = isotope;
    at->index = -1;  /* Invalid index until set by molecule */
    
    /* Copy required string fields */
    knishio_error_t error = KNISHIO_SUCCESS;
    
    if ((error = copy_string_field(&at->position, position)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    
    if ((error = copy_string_field(&at->wallet_address, wallet_address)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    
    if ((error = copy_string_field(&at->token, token)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Copy optional string fields */
    if (value && (error = copy_string_field(&at->value, value)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    
    if (batch_id && (error = copy_string_field(&at->batch_id, batch_id)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    
    if (meta_type && (error = copy_string_field(&at->meta_type, meta_type)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }
    
    if (meta_id && (error = copy_string_field(&at->meta_id, meta_id)) != KNISHIO_SUCCESS) {
        goto cleanup;
    }

    /* Initialize metadata array if provided */
    if (meta_count > 0 && meta) {
        at->meta = knishio_malloc(sizeof(knishio_meta_t*) * meta_count);
        if (!at->meta) {
            error = KNISHIO_ERROR_MEMORY;
            goto cleanup;
        }
        
        /* Copy metadata pointers (no ownership transfer) */
        memcpy(at->meta, meta, sizeof(knishio_meta_t*) * meta_count);
        at->meta_count = meta_count;
    }

    *atom = at;
    return KNISHIO_SUCCESS;

cleanup:
    knishio_atom_free(at);
    return error;
}

void knishio_atom_free(knishio_atom_t* atom) {
    if (!atom) {
        return;
    }

    /* Free string fields */
    free_string_field(&atom->position);
    free_string_field(&atom->wallet_address);
    free_string_field(&atom->token);
    free_string_field(&atom->value);
    free_string_field(&atom->batch_id);
    free_string_field(&atom->meta_type);
    free_string_field(&atom->meta_id);
    free_string_field(&atom->ots_fragment);
    free_string_field(&atom->version);

    /* Free metadata array (metadata objects themselves are not owned) */
    if (atom->meta) {
        knishio_free(atom->meta);
    }

    /* Free the atom structure itself */
    knishio_free(atom);
}

/* Atom property getters */

const char* knishio_atom_get_position(const knishio_atom_t* atom) {
    return atom ? atom->position : NULL;
}

const char* knishio_atom_get_wallet_address(const knishio_atom_t* atom) {
    return atom ? atom->wallet_address : NULL;
}

knishio_isotope_t knishio_atom_get_isotope(const knishio_atom_t* atom) {
    return atom ? atom->isotope : KNISHIO_ISOTOPE_UNKNOWN;
}

const char* knishio_atom_get_isotope_string(const knishio_atom_t* atom) {
    if (!atom) {
        return NULL;
    }
    return knishio_isotope_to_string(atom->isotope);
}

const char* knishio_atom_get_token(const knishio_atom_t* atom) {
    return atom ? atom->token : NULL;
}

const char* knishio_atom_get_value(const knishio_atom_t* atom) {
    return atom ? atom->value : NULL;
}

const char* knishio_atom_get_batch_id(const knishio_atom_t* atom) {
    return atom ? atom->batch_id : NULL;
}

int knishio_atom_get_index(const knishio_atom_t* atom) {
    return atom ? atom->index : -1;
}

/* Atom setters */

knishio_error_t knishio_atom_set_index(knishio_atom_t* atom, int index) {
    if (!atom) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    if (index < 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    atom->index = index;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_atom_set_version(knishio_atom_t* atom, const char* version) {
    if (!atom) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    free_string_field(&atom->version);
    
    if (version) {
        return copy_string_field(&atom->version, version);
    }
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_atom_set_ots_fragment(knishio_atom_t* atom, const char* ots_fragment) {
    if (!atom) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    free_string_field(&atom->ots_fragment);
    
    if (ots_fragment) {
        return copy_string_field(&atom->ots_fragment, ots_fragment);
    }
    
    return KNISHIO_SUCCESS;
}

const char* knishio_atom_get_ots_fragment(const knishio_atom_t* atom) {
    return atom ? atom->ots_fragment : NULL;
}

/* Metadata management */

knishio_error_t knishio_atom_add_meta(knishio_atom_t* atom, knishio_meta_t* meta) {
    if (!atom || !meta) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Resize metadata array if needed */
    if (atom->meta_count == 0 || !atom->meta) {
        atom->meta = knishio_malloc(sizeof(knishio_meta_t*));
        if (!atom->meta) {
            return KNISHIO_ERROR_MEMORY;
        }
    } else {
        knishio_error_t error = resize_meta_array(atom);
        if (error != KNISHIO_SUCCESS) {
            return error;
        }
    }

    /* Add metadata pointer */
    atom->meta[atom->meta_count] = meta;
    atom->meta_count++;

    return KNISHIO_SUCCESS;
}

size_t knishio_atom_get_meta_count(const knishio_atom_t* atom) {
    return atom ? atom->meta_count : 0;
}

knishio_meta_t* knishio_atom_get_meta(const knishio_atom_t* atom, size_t index) {
    if (!atom || index >= atom->meta_count) {
        return NULL;
    }
    
    return atom->meta[index];
}

/* Validation */

knishio_error_t knishio_atom_validate(knishio_atom_t* atom) {
    if (!atom) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Check required fields */
    if (!atom->position || !atom->wallet_address || !atom->token) {
        return KNISHIO_ERROR_ATOMS_MISSING;
    }

    /* Validate position and address formats */
    if (strlen(atom->position) != KNISHIO_POSITION_LENGTH ||
        strlen(atom->wallet_address) != KNISHIO_ADDRESS_LENGTH) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Validate isotope-specific rules */
    if (!knishio_atom_is_valid_for_isotope(atom, atom->isotope)) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    atom->is_validated = true;
    return KNISHIO_SUCCESS;
}

bool knishio_atom_is_valid_for_isotope(const knishio_atom_t* atom, knishio_isotope_t isotope) {
    if (!atom) {
        return false;
    }

    switch (isotope) {
        case KNISHIO_ISOTOPE_V:
            /* Value isotopes must have a value field */
            return atom->value != NULL;
            
        case KNISHIO_ISOTOPE_C:
            /* Create isotopes don't require value */
            return true;
            
        case KNISHIO_ISOTOPE_M:
            /* Meta isotopes should have meta type */
            return atom->meta_type != NULL;
            
        case KNISHIO_ISOTOPE_R:
            /* Remainder isotopes can be minimal */
            return true;
            
        case KNISHIO_ISOTOPE_U:
        case KNISHIO_ISOTOPE_I:
        case KNISHIO_ISOTOPE_T:
        case KNISHIO_ISOTOPE_L:
        case KNISHIO_ISOTOPE_S:
            /* Other isotopes have basic validation */
            return true;
            
        default:
            return false;
    }
}

/* Serialization */

knishio_error_t knishio_atom_to_json(
    const knishio_atom_t* atom,
    char** json_output
) {
    if (!atom || !json_output) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* KISS approach: Use string concatenation for proper atom serialization.
     * Size the buffer to the content: the otsFragment (~1100 chars) and meta values
     * (a base64 ML-KEM pubkey is ~1580 chars) overflow a fixed 2048 and produce
     * malformed JSON — sum the variable-length parts. */
    size_t buf_size = 2048;
    if (atom->ots_fragment) buf_size += strlen(atom->ots_fragment);
    if (atom->meta) {
        for (size_t mi = 0; mi < atom->meta_count; mi++) {
            if (atom->meta[mi]) {
                if (atom->meta[mi]->key) buf_size += strlen(atom->meta[mi]->key);
                if (atom->meta[mi]->value) buf_size += strlen(atom->meta[mi]->value);
                buf_size += 16;
            }
        }
    }
    char* buffer = knishio_malloc(buf_size);
    if (!buffer) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Start JSON object */
    strcpy(buffer, "{");
    
    /* Add position (required) */
    if (atom->position) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"position\":\"%s\",", atom->position);
    }
    
    /* Add wallet address (required) */
    if (atom->wallet_address) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"walletAddress\":\"%s\",", atom->wallet_address);
    }
    
    /* Add isotope (required) */
    const char* isotope_str = knishio_isotope_to_string(atom->isotope);
    if (isotope_str) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"isotope\":\"%s\",", isotope_str);
    }
    
    /* Add token (required) */
    if (atom->token) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"token\":\"%s\",", atom->token);
    }
    
    /* Add value (can be null) */
    if (atom->value && strlen(atom->value) > 0) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"value\":\"%s\",", atom->value);
    } else {
        strcat(buffer, "\"value\":null,");
    }
    
    /* Add batch ID if present */
    if (atom->batch_id && strlen(atom->batch_id) > 0) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"batchId\":\"%s\",", atom->batch_id);
    } else {
        strcat(buffer, "\"batchId\":null,");
    }
    
    /* Add meta type if present */
    if (atom->meta_type && strlen(atom->meta_type) > 0) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"metaType\":\"%s\",", atom->meta_type);
    } else {
        strcat(buffer, "\"metaType\":null,");
    }
    
    /* Add meta ID if present */
    if (atom->meta_id && strlen(atom->meta_id) > 0) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            "\"metaId\":\"%s\",", atom->meta_id);
    } else {
        strcat(buffer, "\"metaId\":null,");
    }
    
    /* Add metadata array if present */
    if (atom->meta_count > 0 && atom->meta) {
        strcat(buffer, "\"meta\":[");
        
        for (size_t i = 0; i < atom->meta_count; i++) {
            if (i > 0) {
                strcat(buffer, ",");
            }
            
            knishio_meta_t* meta = atom->meta[i];
            if (meta && meta->key && meta->value) {
                snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
                    "{\"key\":\"%s\",\"value\":\"%s\"}",
                    meta->key, meta->value);
            }
        }
        
        strcat(buffer, "],");
    } else {
        strcat(buffer, "\"meta\":[],");
    }
    
    /* Add creation timestamp (JS SDK compatible - milliseconds) */
    snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
        "\"createdAt\":\"%ld\",", (long)atom->created_at * 1000);
    
    /* Add index */
    snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
        "\"index\":%d", atom->index);
    
    /* Add version if present */
    if (atom->version) {
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            ",\"version\":\"%s\"", atom->version);
    }
    
    /* Add OTS fragment if present */
    if (atom->ots_fragment) {
        /* DEBUG: Log OTS fragment serialization */
#if KNISHIO_DEBUG_MODE
        fprintf(stderr, "DEBUG knishio_atom_to_json: Serializing OTS fragment: length=%zu, first 32 chars: %.32s\n",
                strlen(atom->ots_fragment), atom->ots_fragment);
#endif
        snprintf(buffer + strlen(buffer), buf_size - strlen(buffer),
            ",\"otsFragment\":\"%s\"", atom->ots_fragment);
    } else {
        /* DEBUG: Log missing OTS fragment */
#if KNISHIO_DEBUG_MODE
        fprintf(stderr, "DEBUG knishio_atom_to_json: No OTS fragment on atom (isotope=%d)\n", atom->isotope);
#endif
    }
    
    /* Close JSON object */
    strcat(buffer, "}");
    
    /* Create final output string */
    size_t final_len = strlen(buffer);
    char* final_json = knishio_malloc(final_len + 1);
    if (!final_json) {
        knishio_free(buffer);
        return KNISHIO_ERROR_MEMORY;
    }
    
    strcpy(final_json, buffer);
    knishio_free(buffer);
    
    *json_output = final_json;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_atom_from_json(
    const char* json_input,
    knishio_atom_t** atom
) {
    if (!json_input || !atom) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Use existing JSON deserialization implementation */
    return knishio_atom_from_json_string(json_input, atom);
}

/* Utility functions */

const char* knishio_isotope_to_string(knishio_isotope_t isotope) {
    if (isotope >= 0 && isotope < (sizeof(isotope_strings) / sizeof(isotope_strings[0]))) {
        return isotope_strings[isotope];
    }
    return NULL;
}

knishio_isotope_t knishio_isotope_from_string(const char* isotope_str) {
    if (!isotope_str) {
        return KNISHIO_ISOTOPE_UNKNOWN;
    }

    for (size_t i = 1; i < (sizeof(isotope_strings) / sizeof(isotope_strings[0])); i++) {
        if (isotope_strings[i] && strcmp(isotope_str, isotope_strings[i]) == 0) {
            return (knishio_isotope_t)i;
        }
    }
    
    return KNISHIO_ISOTOPE_UNKNOWN;
}

knishio_error_t knishio_atom_get_hashable_properties(
    char*** properties,
    size_t* property_count
) {
    if (!properties || !property_count) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Hashable properties from JS SDK */
    const char* hashable_props[] = {
        "position",
        "walletAddress", 
        "isotope",
        "token",
        "value",
        "batchId",
        "metaType",
        "metaId",
        "meta",
        "createdAt"
    };
    
    size_t count = sizeof(hashable_props) / sizeof(hashable_props[0]);
    
    /* Allocate array of string pointers */
    char** props = knishio_malloc(sizeof(char*) * count);
    if (!props) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Copy property name strings */
    for (size_t i = 0; i < count; i++) {
        knishio_error_t error = copy_string_field(&props[i], hashable_props[i]);
        if (error != KNISHIO_SUCCESS) {
            /* Cleanup on error */
            for (size_t j = 0; j < i; j++) {
                knishio_free(props[j]);
            }
            knishio_free(props);
            return error;
        }
    }
    
    *properties = props;
    *property_count = count;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_atom_sort_by_index(
    knishio_atom_t** atoms,
    size_t atom_count
) {
    if (!atoms || atom_count == 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    qsort(atoms, atom_count, sizeof(knishio_atom_t*), compare_atoms_by_index);
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

static knishio_error_t resize_meta_array(knishio_atom_t* atom) {
    if (!atom) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    size_t new_count = atom->meta_count + 1;
    knishio_meta_t** new_meta = knishio_realloc(
        atom->meta, 
        sizeof(knishio_meta_t*) * new_count
    );
    if (!new_meta) {
        return KNISHIO_ERROR_MEMORY;
    }

    atom->meta = new_meta;
    return KNISHIO_SUCCESS;
}

static int compare_atoms_by_index(const void* a, const void* b) {
    const knishio_atom_t* atom_a = *(const knishio_atom_t**)a;
    const knishio_atom_t* atom_b = *(const knishio_atom_t**)b;
    
    if (!atom_a || !atom_b) {
        return 0;
    }
    
    if (atom_a->index < atom_b->index) {
        return -1;
    } else if (atom_a->index > atom_b->index) {
        return 1;
    } else {
        return 0;
    }
}
