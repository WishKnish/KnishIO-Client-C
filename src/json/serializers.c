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
#include "knishio/molecule.h"
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
    /* The output is NULL on every failure, argument errors included. */
    if (atom) *atom = NULL;
    if (!json || !atom || json->type != KNISHIO_JSON_OBJECT) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* One atom parser: knishio_atom_from_json(), the inverse of knishio_atom_to_json(). */
    char *text = knishio_json_serialize(json, false);
    if (!text) {
        return KNISHIO_ERROR_MEMORY;
    }
    knishio_error_t err = knishio_atom_from_json(text, atom);
    cJSON_free(text);
    return err;
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
    if (molecule) *molecule = NULL;
    if (!json || !molecule || json->type != KNISHIO_JSON_OBJECT) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    char *text = knishio_json_serialize(json, false);
    if (!text) {
        return KNISHIO_ERROR_MEMORY;
    }
    knishio_error_t err = knishio_molecule_from_json(text, molecule);
    cJSON_free(text);
    return err;
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
    /* The outputs are empty on every failure, argument errors included. */
    if (atoms) *atoms = NULL;
    if (count) *count = 0;
    if (!json || !atoms || !count || json->type != KNISHIO_JSON_ARRAY) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    
    size_t array_size = knishio_json_array_size(json);
    if (array_size == 0) {
        return KNISHIO_SUCCESS;
    }
    
    knishio_atom_t **atom_array = knishio_calloc(array_size, sizeof(knishio_atom_t*));
    if (!atom_array) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* All or nothing, as knishio_molecule_from_json() treats a molecule: dropping an atom that
     * does not parse would hand back a shorter array that still looks complete. */
    for (size_t i = 0; i < array_size; i++) {
        knishio_json_t *atom_json = knishio_json_array_get(json, i);
        const knishio_error_t err = atom_json ? knishio_atom_from_json_obj(atom_json, &atom_array[i])
                                              : KNISHIO_ERROR_INVALID_JSON;
        knishio_json_free(atom_json);
        if (err != KNISHIO_SUCCESS) {
            for (size_t j = 0; j < i; j++) {
                knishio_atom_free_deep(atom_array[j]);
            }
            knishio_free(atom_array);
            return err;
        }
    }

    *atoms = atom_array;
    *count = array_size;
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
    return knishio_atom_from_json(json_string, atom);
}
