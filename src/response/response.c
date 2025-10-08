/**
 * @file response.c
 * @brief Base Response implementation for KnishIO SDK
 * 
 * Provides the foundational response handling system that matches the
 * JavaScript SDK's Response class architecture, including GraphQL
 * response parsing, error handling, and memory management.
 */

#include "knishio/response/response.h"
#include "knishio/json/parser.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Internal error constants */
static const char* const ERROR_KEY_EXCEPTION = "exception";
static const char* const ERROR_KEY_ERRORS = "errors";
static const char* const UNAUTHENTICATED_ERROR = "Unauthenticated";
static const char* const DATA_KEY_DEFAULT = "data";

/**
 * @brief Create base response
 */
knishio_response_t* knishio_response_create(knishio_query_t *query,
                                            knishio_json_t *json,
                                            const char *data_key) {
    if (!json) {
        return NULL;
    }

    knishio_response_t *response = knishio_calloc(1, sizeof(knishio_response_t));
    if (!response) {
        return NULL;
    }

    response->query = query;
    response->origin_response = knishio_json_clone(json);
    response->response = knishio_json_clone(json);
    response->payload = NULL;
    response->data_key = data_key ? knishio_strdup(data_key) : NULL;
    response->error_key = knishio_strdup(ERROR_KEY_EXCEPTION);
    response->errors = NULL;
    response->error_count = 0;
    response->is_authenticated = true;
    response->error_message = NULL;

    if (!response->origin_response || !response->response || !response->error_key) {
        knishio_response_free(response);
        return NULL;
    }

    // Initialize the response
    if (!knishio_response_init(response)) {
        knishio_response_free(response);
        return NULL;
    }

    return response;
}

/**
 * @brief Free response
 */
void knishio_response_free(knishio_response_t *response) {
    if (!response) {
        return;
    }

    // Free JSON objects
    if (response->origin_response) {
        knishio_json_free(response->origin_response);
    }
    if (response->response) {
        knishio_json_free(response->response);
    }
    if (response->payload) {
        knishio_json_free(response->payload);
    }

    // Free strings
    knishio_free((char*)response->data_key);
    knishio_free((char*)response->error_key);
    knishio_free(response->error_message);

    // Free errors
    if (response->errors) {
        for (size_t i = 0; i < response->error_count; i++) {
            knishio_response_error_free(&response->errors[i]);
        }
        knishio_free(response->errors);
    }

    knishio_free(response);
}

/**
 * @brief Initialize response
 */
bool knishio_response_init(knishio_response_t *response) {
    if (!response || !response->response) {
        return false;
    }

    // Check for errors in response
    knishio_json_t *error_json = knishio_json_object_get(response->response, response->error_key);
    if (error_json) {
        const char *error_str = knishio_json_get_string(error_json);
        if (error_str) {
            // Check for authentication error
            if (strstr(error_str, UNAUTHENTICATED_ERROR)) {
                response->is_authenticated = false;
            }
            
            response->error_message = knishio_strdup(error_str);
            return false;
        }
    }

    // Check for GraphQL errors
    knishio_json_t *errors_array = knishio_json_object_get(response->response, ERROR_KEY_ERRORS);
    if (errors_array && knishio_json_get_type(errors_array) == KNISHIO_JSON_ARRAY) {
        size_t error_count = knishio_json_array_size(errors_array);
        if (error_count > 0) {
            response->errors = knishio_calloc(error_count, sizeof(knishio_response_error_t));
            if (!response->errors) {
                return false;
            }

            response->error_count = 0;
            for (size_t i = 0; i < error_count; i++) {
                knishio_json_t *error_obj = knishio_json_array_get(errors_array, i);
                if (!error_obj) continue;

                knishio_response_error_t *error = &response->errors[response->error_count];
                
                const char *message = knishio_json_get_string_path(error_obj, "message");
                if (message) {
                    error->message = knishio_strdup(message);
                }

                const char *code = knishio_json_get_string_path(error_obj, "code");
                if (code) {
                    error->code = knishio_strdup(code);
                }

                const char *path = knishio_json_get_string_path(error_obj, "path");
                if (path) {
                    error->path = knishio_strdup(path);
                }

                error->extensions = knishio_json_object_get(error_obj, "extensions");
                if (error->extensions) {
                    error->extensions = knishio_json_clone(error->extensions);
                }

                // Check for authentication error
                if (message && strstr(message, UNAUTHENTICATED_ERROR)) {
                    response->is_authenticated = false;
                }

                response->error_count++;
            }
        }
    }

    return true;
}

/**
 * @brief Get response data using data_key
 */
knishio_json_t* knishio_response_data(knishio_response_t *response) {
    if (!response || !response->response) {
        return NULL;
    }

    if (!response->data_key) {
        return response->response;
    }

    return knishio_json_get_path(response->response, response->data_key);
}

/**
 * @brief Get raw response JSON
 */
knishio_json_t* knishio_response_raw(knishio_response_t *response) {
    return response ? response->response : NULL;
}

/**
 * @brief Get response payload (default implementation returns NULL)
 */
knishio_json_t* knishio_response_payload(knishio_response_t *response) {
    (void)response; // Suppress unused parameter warning
    return NULL;
}

/**
 * @brief Get original query
 */
knishio_query_t* knishio_response_query(knishio_response_t *response) {
    return response ? response->query : NULL;
}

/**
 * @brief Get response status
 */
const char* knishio_response_status(knishio_response_t *response) {
    (void)response; // Default implementation returns NULL
    return NULL;
}

/**
 * @brief Check if response has errors
 */
bool knishio_response_has_errors(knishio_response_t *response) {
    if (!response) {
        return true;
    }
    
    return response->error_count > 0 || response->error_message != NULL;
}

/**
 * @brief Get response errors
 */
knishio_response_error_t* knishio_response_get_errors(knishio_response_t *response,
                                                      size_t *count) {
    if (!response || !count) {
        if (count) *count = 0;
        return NULL;
    }

    *count = response->error_count;
    return response->errors;
}

/**
 * @brief Get error message
 */
const char* knishio_response_error_message(knishio_response_t *response) {
    if (!response) {
        return NULL;
    }

    if (response->error_message) {
        return response->error_message;
    }

    if (response->error_count > 0 && response->errors[0].message) {
        return response->errors[0].message;
    }

    return NULL;
}

/**
 * @brief Check if error is authentication related
 */
bool knishio_response_is_unauthenticated(knishio_response_t *response) {
    return response ? !response->is_authenticated : false;
}

/**
 * @brief Parse GraphQL response format
 */
bool knishio_response_parse_graphql(knishio_response_t *response) {
    if (!response || !response->response) {
        return false;
    }

    // Check if it's a GraphQL response (has data or errors field)
    knishio_json_t *data = knishio_json_object_get(response->response, DATA_KEY_DEFAULT);
    knishio_json_t *errors = knishio_json_object_get(response->response, ERROR_KEY_ERRORS);
    
    return (data != NULL || errors != NULL);
}

/**
 * @brief Extract GraphQL data section
 */
knishio_json_t* knishio_response_graphql_data(knishio_response_t *response) {
    if (!response || !response->response) {
        return NULL;
    }

    return knishio_json_object_get(response->response, DATA_KEY_DEFAULT);
}

/**
 * @brief Extract GraphQL errors section
 */
knishio_json_t* knishio_response_graphql_errors(knishio_response_t *response) {
    if (!response || !response->response) {
        return NULL;
    }

    return knishio_json_object_get(response->response, ERROR_KEY_ERRORS);
}

/**
 * @brief Get string value from response data path
 */
const char* knishio_response_get_string(knishio_response_t *response, const char *path) {
    if (!response || !path) {
        return NULL;
    }

    knishio_json_t *data = knishio_response_data(response);
    if (!data) {
        return NULL;
    }

    return knishio_json_get_string_path(data, path);
}

/**
 * @brief Get number value from response data path
 */
bool knishio_response_get_number(knishio_response_t *response, const char *path, double *value) {
    if (!response || !path || !value) {
        return false;
    }

    knishio_json_t *data = knishio_response_data(response);
    if (!data) {
        return false;
    }

    return knishio_json_get_number_path(data, path, value);
}

/**
 * @brief Get boolean value from response data path
 */
bool knishio_response_get_bool(knishio_response_t *response, const char *path, bool *value) {
    if (!response || !path || !value) {
        return false;
    }

    knishio_json_t *data = knishio_response_data(response);
    if (!data) {
        return false;
    }

    return knishio_json_get_bool_path(data, path, value);
}

/**
 * @brief Get array from response data path
 */
knishio_json_t* knishio_response_get_array(knishio_response_t *response, const char *path) {
    if (!response || !path) {
        return NULL;
    }

    knishio_json_t *data = knishio_response_data(response);
    if (!data) {
        return NULL;
    }

    knishio_json_t *result = knishio_json_get_path(data, path);
    return (result && knishio_json_get_type(result) == KNISHIO_JSON_ARRAY) ? result : NULL;
}

/**
 * @brief Get object from response data path
 */
knishio_json_t* knishio_response_get_object(knishio_response_t *response, const char *path) {
    if (!response || !path) {
        return NULL;
    }

    knishio_json_t *data = knishio_response_data(response);
    if (!data) {
        return NULL;
    }

    knishio_json_t *result = knishio_json_get_path(data, path);
    return (result && knishio_json_get_type(result) == KNISHIO_JSON_OBJECT) ? result : NULL;
}

/**
 * @brief Validate response structure
 */
bool knishio_response_validate(knishio_response_t *response) {
    if (!response || !response->response) {
        return false;
    }

    // Basic validation - response should have no errors or valid data
    return !knishio_response_has_errors(response) || knishio_response_data(response) != NULL;
}

/**
 * @brief Clone response
 */
knishio_response_t* knishio_response_clone(knishio_response_t *response) {
    if (!response) {
        return NULL;
    }

    return knishio_response_create(response->query, 
                                   response->origin_response, 
                                   response->data_key);
}

/**
 * @brief Create response error
 */
knishio_response_error_t* knishio_response_error_create(const char *message,
                                                        const char *code,
                                                        const char *path) {
    knishio_response_error_t *error = knishio_calloc(1, sizeof(knishio_response_error_t));
    if (!error) {
        return NULL;
    }

    if (message) {
        error->message = knishio_strdup(message);
        if (!error->message) {
            knishio_response_error_free(error);
            return NULL;
        }
    }

    if (code) {
        error->code = knishio_strdup(code);
    }

    if (path) {
        error->path = knishio_strdup(path);
    }

    return error;
}

/**
 * @brief Free response error
 */
void knishio_response_error_free(knishio_response_error_t *error) {
    if (!error) {
        return;
    }

    knishio_free(error->message);
    knishio_free(error->code);
    knishio_free(error->path);
    
    if (error->extensions) {
        knishio_json_free(error->extensions);
    }
    
    // Note: We don't free the error itself here since it might be part of an array
    // The caller is responsible for freeing the error structure
}