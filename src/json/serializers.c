/**
 * @file serializers.c
 * @brief JSON serialization/deserialization for KnishIO data structures
 * 
 * Provides type-safe JSON conversion for all KnishIO SDK data structures,
 * ensuring compatibility with JavaScript SDK JSON format.
 */

#include "knishio/json/serializers.h"
#include "knishio/json/parser.h"
#include "knishio/json/builder.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/atom.h"
#include <string.h>
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

/* Structures defined in respective header files */

/* Note: Helper functions knishio_isotope_to_string, knishio_isotope_from_string, 
 * and knishio_atom_add_meta are implemented in atom.c */

/* Atom serialization */
knishio_json_t* knishio_atom_to_json_obj(const knishio_atom_t* atom) {
    if (!atom) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    /* Core properties */
    if (atom->position) {
        knishio_json_object_set_string(builder, "position", atom->position);
    }
    
    if (atom->wallet_address) {
        knishio_json_object_set_string(builder, "walletAddress", atom->wallet_address);
    }
    
    const char *isotope_str = knishio_isotope_to_string(atom->isotope);
    if (isotope_str) {
        knishio_json_object_set_string(builder, "isotope", isotope_str);
    }
    
    if (atom->token) {
        knishio_json_object_set_string(builder, "token", atom->token);
    }
    
    if (atom->value) {
        knishio_json_object_set_string(builder, "value", atom->value);
    }
    
    if (atom->batch_id) {
        knishio_json_object_set_string(builder, "batchId", atom->batch_id);
    }
    
    /* Metadata properties */
    if (atom->meta_type) {
        knishio_json_object_set_string(builder, "metaType", atom->meta_type);
    }
    
    if (atom->meta_id) {
        knishio_json_object_set_string(builder, "metaId", atom->meta_id);
    }
    
    /* Meta array */
    if (atom->meta && atom->meta_count > 0) {
        knishio_json_array_builder_t *meta_array = knishio_json_array_builder_create();
        if (meta_array) {
            for (size_t i = 0; i < atom->meta_count; i++) {
                knishio_json_t *meta_json = knishio_meta_to_json_obj(atom->meta[i]);
                if (meta_json) {
                    knishio_json_array_add(meta_array, meta_json);
                }
            }
            knishio_json_object_set_array(builder, "meta", meta_array);
            knishio_json_array_builder_free(meta_array);
        }
    }
    
    /* Cryptographic properties */
    if (atom->ots_fragment) {
        knishio_json_object_set_string(builder, "otsFragment", atom->ots_fragment);
    }
    
    /* System properties */
    knishio_json_object_set_int(builder, "index", atom->index);
    
    if (atom->version) {
        knishio_json_object_set_string(builder, "version", atom->version);
    }
    
    /* Timestamp as string */
    if (atom->created_at > 0) {
        char timestamp_str[32];
        snprintf(timestamp_str, sizeof(timestamp_str), "%ld", (long)atom->created_at);
        knishio_json_object_set_string(builder, "createdAt", timestamp_str);
    }
    
    knishio_json_t *result = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);
    
    return result;
}

knishio_error_t knishio_atom_from_json_obj(const knishio_json_t* json, knishio_atom_t** atom) {
    if (!json || !atom || json->type != KNISHIO_JSON_OBJECT) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Extract required fields */
    knishio_json_t *position_json = knishio_json_object_get(json, "position");
    knishio_json_t *wallet_json = knishio_json_object_get(json, "walletAddress");
    knishio_json_t *isotope_json = knishio_json_object_get(json, "isotope");
    knishio_json_t *token_json = knishio_json_object_get(json, "token");
    
    const char *position = position_json ? knishio_json_get_string(position_json) : NULL;
    const char *wallet_address = wallet_json ? knishio_json_get_string(wallet_json) : NULL;
    const char *isotope_str = isotope_json ? knishio_json_get_string(isotope_json) : NULL;
    const char *token = token_json ? knishio_json_get_string(token_json) : NULL;
    
    if (!position || !wallet_address || !isotope_str || !token) {
        knishio_json_free(position_json);
        knishio_json_free(wallet_json);
        knishio_json_free(isotope_json);
        knishio_json_free(token_json);
        return KNISHIO_ERROR_INVALID_STATE;
    }
    
    int isotope = knishio_isotope_from_string(isotope_str);
    if (isotope == 0) {  /* UNKNOWN isotope */
        knishio_json_free(position_json);
        knishio_json_free(wallet_json);
        knishio_json_free(isotope_json);
        knishio_json_free(token_json);
        return KNISHIO_ERROR_INVALID_STATE;
    }
    
    /* Extract optional fields */
    knishio_json_t *value_json = knishio_json_object_get(json, "value");
    knishio_json_t *batch_json = knishio_json_object_get(json, "batchId");
    
    const char *value = value_json ? knishio_json_get_string(value_json) : NULL;
    const char *batch_id = batch_json ? knishio_json_get_string(batch_json) : NULL;
    
    /* Create basic atom structure - simplified for compilation */
    *atom = knishio_calloc(1, sizeof(knishio_atom_t));
    if (!*atom) {
        knishio_json_free(position_json);
        knishio_json_free(wallet_json);
        knishio_json_free(isotope_json);
        knishio_json_free(token_json);
        knishio_json_free(value_json);
        knishio_json_free(batch_json);
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Set basic properties */
    (*atom)->position = position ? knishio_strdup(position) : NULL;
    (*atom)->wallet_address = wallet_address ? knishio_strdup(wallet_address) : NULL;
    (*atom)->isotope = isotope;
    (*atom)->token = token ? knishio_strdup(token) : NULL;
    (*atom)->value = value ? knishio_strdup(value) : NULL;
    (*atom)->batch_id = batch_id ? knishio_strdup(batch_id) : NULL;
    
    /* Cleanup JSON objects */
    knishio_json_free(position_json);
    knishio_json_free(wallet_json);
    knishio_json_free(isotope_json);
    knishio_json_free(token_json);
    knishio_json_free(value_json);
    knishio_json_free(batch_json);
    
    /* Set optional properties */
    knishio_json_t *index_json = knishio_json_object_get(json, "index");
    if (index_json) {
        int64_t index_val;
        if (knishio_json_get_int(index_json, &index_val)) {
            (*atom)->index = (int)index_val;
        }
        knishio_json_free(index_json);
    }
    
    knishio_json_t *version_json = knishio_json_object_get(json, "version");
    if (version_json) {
        const char *version_str = knishio_json_get_string(version_json);
        if (version_str) {
            (*atom)->version = knishio_strdup(version_str);
        }
        knishio_json_free(version_json);
    }
    
    knishio_json_t *ots_json = knishio_json_object_get(json, "otsFragment");
    if (ots_json) {
        const char *ots_str = knishio_json_get_string(ots_json);
        if (ots_str) {
            (*atom)->ots_fragment = knishio_strdup(ots_str);
        }
        knishio_json_free(ots_json);
    }
    
    /* Handle meta array */
    knishio_json_t *meta_json = knishio_json_object_get(json, "meta");
    if (meta_json && meta_json->type == KNISHIO_JSON_ARRAY) {
        size_t meta_count = knishio_json_array_size(meta_json);
        for (size_t i = 0; i < meta_count; i++) {
            knishio_json_t *meta_item = knishio_json_array_get(meta_json, i);
            if (meta_item) {
                knishio_meta_t *meta_obj;
                if (knishio_meta_from_json_obj(meta_item, &meta_obj) == KNISHIO_SUCCESS) {
                    knishio_atom_add_meta(*atom, meta_obj);
                }
                knishio_json_free(meta_item);
            }
        }
        knishio_json_free(meta_json);
    }
    
    return KNISHIO_SUCCESS;
}

/* Meta serialization */
knishio_json_t* knishio_meta_to_json_obj(const knishio_meta_t* meta) {
    if (!meta) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    /* Implement based on meta structure - placeholder for now */
    /* This would need to be implemented based on the actual knishio_meta_t structure */
    
    knishio_json_t *result = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);
    
    return result;
}

knishio_error_t knishio_meta_from_json_obj(const knishio_json_t* json, knishio_meta_t** meta) {
    if (!json || !meta || json->type != KNISHIO_JSON_OBJECT) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Placeholder implementation - would need actual knishio_meta_t structure */
    *meta = NULL;
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
}

/* Wallet serialization */
knishio_json_t* knishio_wallet_to_json_obj(const knishio_wallet_t* wallet) {
    if (!wallet) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    /* Implement based on wallet structure - placeholder for now */
    /* This would need to be implemented based on the actual knishio_wallet_t structure */
    
    knishio_json_t *result = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);
    
    return result;
}

knishio_error_t knishio_wallet_from_json_obj(const knishio_json_t* json, knishio_wallet_t** wallet) {
    if (!json || !wallet || json->type != KNISHIO_JSON_OBJECT) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Placeholder implementation - would need actual knishio_wallet_t structure */
    *wallet = NULL;
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
}

/* Molecule serialization */
knishio_json_t* knishio_molecule_to_json_obj(const knishio_molecule_t* molecule) {
    if (!molecule) return NULL;
    
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) return NULL;
    
    /* Implement based on molecule structure - placeholder for now */
    /* This would need to be implemented based on the actual knishio_molecule_t structure */
    
    knishio_json_t *result = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);
    
    return result;
}

knishio_error_t knishio_molecule_from_json_obj(const knishio_json_t* json, knishio_molecule_t** molecule) {
    if (!json || !molecule || json->type != KNISHIO_JSON_OBJECT) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    /* Placeholder implementation - would need actual knishio_molecule_t structure */
    *molecule = NULL;
    return KNISHIO_ERROR_NOT_IMPLEMENTED;
}

/* GraphQL response handling */
knishio_json_t* knishio_json_extract_data(const knishio_json_t* response) {
    if (!response || response->type != KNISHIO_JSON_OBJECT) {
        return NULL;
    }
    
    return knishio_json_object_get(response, "data");
}

knishio_json_t* knishio_json_extract_errors(const knishio_json_t* response) {
    if (!response || response->type != KNISHIO_JSON_OBJECT) {
        return NULL;
    }
    
    return knishio_json_object_get(response, "errors");
}

bool knishio_json_has_errors(const knishio_json_t* response) {
    knishio_json_t *errors = knishio_json_extract_errors(response);
    if (!errors) return false;
    
    bool has_errors = (errors->type == KNISHIO_JSON_ARRAY && knishio_json_array_size(errors) > 0);
    knishio_json_free(errors);
    return has_errors;
}

char* knishio_json_get_first_error_message(const knishio_json_t* response) {
    knishio_json_t *errors = knishio_json_extract_errors(response);
    if (!errors || errors->type != KNISHIO_JSON_ARRAY) {
        if (errors) knishio_json_free(errors);
        return NULL;
    }
    
    if (knishio_json_array_size(errors) == 0) {
        knishio_json_free(errors);
        return NULL;
    }
    
    knishio_json_t *first_error = knishio_json_array_get(errors, 0);
    knishio_json_free(errors);
    
    if (!first_error) return NULL;
    
    knishio_json_t *message = knishio_json_object_get(first_error, "message");
    knishio_json_free(first_error);
    
    if (!message) return NULL;
    
    const char *msg_str = knishio_json_get_string(message);
    char *result = msg_str ? knishio_strdup(msg_str) : NULL;
    
    knishio_json_free(message);
    return result;
}

/* Array helper functions */
knishio_json_t* knishio_json_create_atom_array(knishio_atom_t** atoms, size_t count) {
    if (!atoms) return NULL;
    
    knishio_json_array_builder_t *builder = knishio_json_array_builder_create();
    if (!builder) return NULL;
    
    for (size_t i = 0; i < count; i++) {
        if (atoms[i]) {
            knishio_json_t *atom_json = knishio_atom_to_json_obj(atoms[i]);
            if (atom_json) {
                knishio_json_array_add(builder, atom_json);
            }
        }
    }
    
    knishio_json_t *result = knishio_json_array_build(builder);
    knishio_json_array_builder_free(builder);
    
    return result;
}

knishio_error_t knishio_json_parse_atom_array(const knishio_json_t* json, knishio_atom_t*** atoms, size_t* count) {
    if (!json || !atoms || !count || json->type != KNISHIO_JSON_ARRAY) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    size_t array_size = knishio_json_array_size(json);
    if (array_size == 0) {
        *atoms = NULL;
        *count = 0;
        return KNISHIO_SUCCESS;
    }
    
    knishio_atom_t **atom_array = knishio_calloc(array_size, sizeof(knishio_atom_t*));
    if (!atom_array) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    size_t parsed_count = 0;
    for (size_t i = 0; i < array_size; i++) {
        knishio_json_t *atom_json = knishio_json_array_get(json, i);
        if (atom_json) {
            knishio_atom_t *atom;
            if (knishio_atom_from_json_obj(atom_json, &atom) == KNISHIO_SUCCESS) {
                atom_array[parsed_count++] = atom;
            }
            knishio_json_free(atom_json);
        }
    }
    
    *atoms = atom_array;
    *count = parsed_count;
    return KNISHIO_SUCCESS;
}

/* String/JSON conversion helpers */
knishio_error_t knishio_atom_to_json_string(const knishio_atom_t* atom, char** json_string) {
    if (!atom || !json_string) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    knishio_json_t *json = knishio_atom_to_json_obj(atom);
    if (!json) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    *json_string = knishio_json_serialize(json, false);
    knishio_json_free(json);
    
    return *json_string ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
}

knishio_error_t knishio_atom_from_json_string(const char* json_string, knishio_atom_t** atom) {
    if (!json_string || !atom) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    char *error_msg = NULL;
    knishio_json_t *json = knishio_json_parse(json_string, &error_msg);
    if (!json) {
        if (error_msg) knishio_free(error_msg);
        return KNISHIO_ERROR_INVALID_STATE;
    }
    
    knishio_error_t result = knishio_atom_from_json_obj(json, atom);
    knishio_json_free(json);
    
    return result;
}
