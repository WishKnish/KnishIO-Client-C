/**
 * @file client.c  
 * @brief Enhanced HTTP client implementation for KnishIO SDK GraphQL communication
 * 
 * Provides comprehensive HTTP client functionality for GraphQL queries and mutations,
 * fully compatible with the JavaScript SDK's network layer and URQL client capabilities.
 */

#include "knishio/http/client.h"
#include "knishio/json/parser.h" 
#include "knishio/json/builder.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/utils/logging.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>

/* Use the structure defined in knishio/http.h 
 * Additional internal fields stored separately */
typedef struct {
    CURL *curl_handle;                 /**< Reusable curl handle for connection pooling */
    char **default_headers;            /**< Default headers for all requests */
    size_t header_count;               /**< Number of default headers */
    char *last_error;                  /**< Last error message */
} http_client_internal_t;

/* Global storage for internal client data - indexed by client pointer */
static http_client_internal_t* get_internal_data(knishio_http_client_t *client) {
    /* In production, use a proper map. For now, attach as opaque pointer */
    return (http_client_internal_t*)client->auth_pubkey; /* Hijack unused field temporarily */
}

/* Internal response buffer for libcurl callbacks */
typedef struct {
    char *memory;
    size_t size;
    size_t allocated;
} response_buffer_t;

/* Internal header buffer for libcurl callbacks */
typedef struct {
    char **headers;
    size_t count;
    size_t allocated;
} header_buffer_t;

/* Internal function declarations */
static size_t write_response_callback(void *contents, size_t size, size_t nmemb, response_buffer_t *buffer);
static size_t header_callback(char *buffer, size_t size, size_t nitems, header_buffer_t *headers);
static knishio_http_response_t* execute_request(knishio_http_client_t *client, const knishio_http_config_t *config);
static void set_client_error(knishio_http_client_t *client, const char *error);
static void clear_client_error(knishio_http_client_t *client);
static char* build_full_url(const char *base_url, const char *path);
static struct curl_slist* build_header_list(knishio_http_client_t *client, const char **additional_headers, size_t additional_count);
static knishio_http_response_t* create_response(void);
static void free_response_internals(knishio_http_response_t *response);

/* Client lifecycle */

knishio_error_t knishio_http_client_create(
    knishio_http_client_t** client,
    const char* base_url) {
    
    if (!client) return KNISHIO_ERROR_INVALID_ARGS;
    
    *client = knishio_calloc(1, sizeof(knishio_http_client_t));
    if (!*client) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Allocate internal data structure */
    http_client_internal_t *internal = knishio_calloc(1, sizeof(http_client_internal_t));
    if (!internal) {
        knishio_free(*client);
        *client = NULL;
        return KNISHIO_ERROR_MEMORY;
    }

    /* Initialize curl handle for connection reuse */
    internal->curl_handle = curl_easy_init();
    if (!internal->curl_handle) {
        knishio_free(internal);
        knishio_free(*client);
        *client = NULL;
        return KNISHIO_ERROR_HTTP_INIT;
    }

    /* Set base URL if provided */
    if (base_url) {
        (*client)->base_url = knishio_strdup(base_url);
        if (!(*client)->base_url) {
            curl_easy_cleanup(internal->curl_handle);
            knishio_free(internal);
            knishio_free(*client);
            *client = NULL;
            return KNISHIO_ERROR_MEMORY;
        }
    }

    /* Set default values */
    (*client)->timeout_seconds = 30;  /* 30 second timeout */
    (*client)->verify_ssl = true;
    (*client)->user_agent = knishio_strdup("KnishIO-C-SDK/1.0 (libcurl)");
    
    if (!(*client)->user_agent) {
        knishio_free((*client)->base_url);
        curl_easy_cleanup(internal->curl_handle);
        knishio_free(internal);
        knishio_free(*client);
        *client = NULL;
        return KNISHIO_ERROR_MEMORY;
    }

    /* Store internal data in auth_pubkey field temporarily */
    (*client)->auth_pubkey = (char*)internal;

    /* Configure curl handle with default options */
    curl_easy_setopt(internal->curl_handle, CURLOPT_TIMEOUT, (*client)->timeout_seconds);
    curl_easy_setopt(internal->curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(internal->curl_handle, CURLOPT_USERAGENT, (*client)->user_agent);
    curl_easy_setopt(internal->curl_handle, CURLOPT_SSL_VERIFYPEER, (*client)->verify_ssl ? 1L : 0L);
    curl_easy_setopt(internal->curl_handle, CURLOPT_SSL_VERIFYHOST, (*client)->verify_ssl ? 2L : 0L);
    curl_easy_setopt(internal->curl_handle, CURLOPT_NOSIGNAL, 1L);  /* Thread-safe */

    return KNISHIO_SUCCESS;
}

void knishio_http_client_free(knishio_http_client_t *client) {
    if (!client) return;

    /* Get internal data */
    http_client_internal_t *internal = get_internal_data(client);
    
    /* Free curl handle */
    if (internal && internal->curl_handle) {
        curl_easy_cleanup(internal->curl_handle);
    }

    /* Free strings */
    knishio_free(client->base_url);
    knishio_free(client->auth_token);
    knishio_free(client->user_agent);
    
    /* Free internal data */
    if (internal) {
        knishio_free(internal->last_error);
        
        /* Free default headers */
        if (internal->default_headers) {
            for (size_t i = 0; i < internal->header_count; i++) {
                knishio_free(internal->default_headers[i]);
            }
            knishio_free(internal->default_headers);
        }
        
        knishio_free(internal);
    }

    knishio_free(client);
}

bool knishio_http_client_set_headers(knishio_http_client_t *client,
                                     const char **headers, size_t count) {
    if (!client) return false;
    
    http_client_internal_t *internal = get_internal_data(client);
    if (!internal) return false;

    /* Free existing headers */
    if (internal->default_headers) {
        for (size_t i = 0; i < internal->header_count; i++) {
            knishio_free(internal->default_headers[i]);
        }
        knishio_free(internal->default_headers);
    }

    if (count == 0 || !headers) {
        internal->default_headers = NULL;
        internal->header_count = 0;
        return true;
    }

    /* Allocate new headers array */
    internal->default_headers = knishio_calloc(count, sizeof(char*));
    if (!internal->default_headers) {
        internal->header_count = 0;
        return false;
    }

    /* Copy headers */
    internal->header_count = count;
    for (size_t i = 0; i < count; i++) {
        if (headers[i]) {
            internal->default_headers[i] = knishio_strdup(headers[i]);
            if (!internal->default_headers[i]) {
                /* Cleanup on failure */
                for (size_t j = 0; j < i; j++) {
                    knishio_free(internal->default_headers[j]);
                }
                knishio_free(internal->default_headers);
                internal->default_headers = NULL;
                internal->header_count = 0;
                return false;
            }
        }
    }

    return true;
}

bool knishio_http_client_set_auth(knishio_http_client_t *client, const char *token) {
    if (!client) return false;

    knishio_free(client->auth_token);
    client->auth_token = token ? knishio_strdup(token) : NULL;
    
    return token ? (client->auth_token != NULL) : true;
}

knishio_error_t knishio_http_client_set_timeout(
    knishio_http_client_t* client,
    long timeout_seconds) {
    
    if (!client || timeout_seconds < 0) return KNISHIO_ERROR_INVALID_ARGS;
    
    http_client_internal_t *internal = get_internal_data(client);
    if (!internal) return KNISHIO_ERROR_INVALID_STATE;

    client->timeout_seconds = timeout_seconds;
    
    /* Update curl handle */
    curl_easy_setopt(internal->curl_handle, CURLOPT_TIMEOUT, timeout_seconds);
    
    return KNISHIO_SUCCESS;
}

/* Request execution */

knishio_http_response_t* knishio_http_request(knishio_http_client_t *client,
                                              const knishio_http_config_t *config) {
    if (!client || !config) {
        if (client) set_client_error(client, "Invalid arguments");
        return NULL;
    }

    return execute_request(client, config);
}

knishio_http_response_t* knishio_http_get(knishio_http_client_t *client, const char *url) {
    if (!client || !url) {
        if (client) set_client_error(client, "Invalid arguments");
        return NULL;
    }

    knishio_http_config_t config = {
        .url = url,
        .method = KNISHIO_HTTP_GET,
        .body = NULL,
        .body_length = 0,
        .headers = NULL,
        .header_count = 0,
        .timeout_ms = client->timeout_ms,
        .follow_redirects = client->follow_redirects,
        .user_agent = client->user_agent
    };

    return execute_request(client, &config);
}

knishio_http_response_t* knishio_http_post(knishio_http_client_t *client,
                                           const char *url, const char *body) {
    if (!client || !url) {
        if (client) set_client_error(client, "Invalid arguments");
        return NULL;
    }

    const char *content_type_header = "Content-Type: application/octet-stream";
    const char *headers[] = { content_type_header };

    knishio_http_config_t config = {
        .url = url,
        .method = KNISHIO_HTTP_POST,
        .body = body,
        .body_length = body ? strlen(body) : 0,
        .headers = headers,
        .header_count = 1,
        .timeout_ms = client->timeout_ms,
        .follow_redirects = client->follow_redirects,
        .user_agent = client->user_agent
    };

    return execute_request(client, &config);
}

knishio_http_response_t* knishio_http_post_json(knishio_http_client_t *client,
                                                const char *url, knishio_json_t *json) {
    if (!client || !url || !json) {
        if (client) set_client_error(client, "Invalid arguments");
        return NULL;
    }

    /* Serialize JSON */
    char *json_string = knishio_json_serialize(json, false);
    if (!json_string) {
        set_client_error(client, "Failed to serialize JSON");
        return NULL;
    }

    const char *content_type_header = "Content-Type: application/json";
    const char *headers[] = { content_type_header };

    knishio_http_config_t config = {
        .url = url,
        .method = KNISHIO_HTTP_POST,
        .body = json_string,
        .body_length = strlen(json_string),
        .headers = headers,
        .header_count = 1,
        .timeout_ms = client->timeout_ms,
        .follow_redirects = client->follow_redirects,
        .user_agent = client->user_agent
    };

    knishio_http_response_t *response = execute_request(client, &config);
    
    knishio_free(json_string);
    
    return response;
}

/* GraphQL operations */

knishio_http_response_t* knishio_http_graphql_query(knishio_http_client_t *client,
                                                    const char *endpoint,
                                                    const char *query,
                                                    knishio_json_t *variables) {
    if (!client || !endpoint || !query) {
        if (client) set_client_error(client, "Invalid arguments");
        return NULL;
    }

    /* Build GraphQL request payload */
    knishio_json_object_builder_t *builder = knishio_json_object_builder_create();
    if (!builder) {
        set_client_error(client, "Failed to create JSON builder");
        return NULL;
    }

    knishio_json_object_set_string(builder, "query", query);
    
    if (variables) {
        knishio_json_object_set(builder, "variables", variables);
    }

    knishio_json_t *payload = knishio_json_object_build(builder);
    knishio_json_object_builder_free(builder);

    if (!payload) {
        set_client_error(client, "Failed to build GraphQL payload");
        return NULL;
    }

    /* Send as JSON POST */
    knishio_http_response_t *response = knishio_http_post_json(client, endpoint, payload);
    knishio_json_free(payload);

    return response;
}

knishio_http_response_t* knishio_http_graphql_mutation(knishio_http_client_t *client,
                                                       const char *endpoint,
                                                       const char *mutation,
                                                       knishio_json_t *variables) {
    /* Mutations use the same protocol as queries in GraphQL */
    return knishio_http_graphql_query(client, endpoint, mutation, variables);
}

/* Response handling */

knishio_json_t* knishio_http_response_json(knishio_http_response_t *response) {
    if (!response || !response->body) {
        return NULL;
    }

    char *error_msg = NULL;
    knishio_json_t *json = knishio_json_parse(response->body, &error_msg);
    
    if (error_msg) {
        knishio_log(KNISHIO_LOG_DEBUG, "JSON parse error: %s", error_msg);
        knishio_free(error_msg);
    }

    return json;
}

const char* knishio_http_response_header(knishio_http_response_t *response, const char *name) {
    if (!response || !name || !response->headers) {
        return NULL;
    }

    size_t name_len = strlen(name);
    
    for (size_t i = 0; i < response->header_count; i++) {
        if (response->headers[i]) {
            /* Check if header starts with the name followed by colon */
            if (strncasecmp(response->headers[i], name, name_len) == 0 && 
                response->headers[i][name_len] == ':') {
                
                /* Skip colon and any whitespace */
                const char *value = response->headers[i] + name_len + 1;
                while (*value == ' ' || *value == '\t') {
                    value++;
                }
                return value;
            }
        }
    }

    return NULL;
}

void knishio_http_response_free(knishio_http_response_t *response) {
    if (!response) return;

    free_response_internals(response);
    knishio_free(response);
}

/* Utility functions */

char* knishio_http_url_encode(const char *str) {
    if (!str) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    char *encoded = curl_easy_escape(curl, str, strlen(str));
    curl_easy_cleanup(curl);

    if (!encoded) return NULL;

    /* Copy to our memory management */
    char *result = knishio_strdup(encoded);
    curl_free(encoded);

    return result;
}

char* knishio_http_url_decode(const char *str) {
    if (!str) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    int decoded_len;
    char *decoded = curl_easy_unescape(curl, str, strlen(str), &decoded_len);
    curl_easy_cleanup(curl);

    if (!decoded) return NULL;

    /* Copy to our memory management */
    char *result = knishio_strndup(decoded, decoded_len);
    curl_free(decoded);

    return result;
}

char* knishio_http_build_url(const char *base_url,
                             const char **params,
                             const char **values,
                             size_t count) {
    if (!base_url) return NULL;

    if (count == 0 || !params || !values) {
        return knishio_strdup(base_url);
    }

    /* Calculate required buffer size */
    size_t total_len = strlen(base_url) + 2; /* base + '?' + '\0' */
    
    for (size_t i = 0; i < count; i++) {
        if (params[i] && values[i]) {
            char *encoded_key = knishio_http_url_encode(params[i]);
            char *encoded_value = knishio_http_url_encode(values[i]);
            
            if (encoded_key && encoded_value) {
                total_len += strlen(encoded_key) + strlen(encoded_value) + 2; /* key=value& */
            }
            
            knishio_free(encoded_key);
            knishio_free(encoded_value);
        }
    }

    char *result = knishio_malloc(total_len);
    if (!result) return NULL;

    strcpy(result, base_url);
    strcat(result, "?");

    bool first = true;
    for (size_t i = 0; i < count; i++) {
        if (params[i] && values[i]) {
            char *encoded_key = knishio_http_url_encode(params[i]);
            char *encoded_value = knishio_http_url_encode(values[i]);
            
            if (encoded_key && encoded_value) {
                if (!first) {
                    strcat(result, "&");
                }
                strcat(result, encoded_key);
                strcat(result, "=");
                strcat(result, encoded_value);
                first = false;
            }
            
            knishio_free(encoded_key);
            knishio_free(encoded_value);
        }
    }

    return result;
}

/* Error handling */

const char* knishio_http_client_error(knishio_http_client_t *client) {
    return client ? client->last_error : NULL;
}

void knishio_http_client_clear_error(knishio_http_client_t *client) {
    if (client) {
        clear_client_error(client);
    }
}

/* Internal implementation functions */

static size_t write_response_callback(void *contents, size_t size, size_t nmemb, response_buffer_t *buffer) {
    size_t real_size = size * nmemb;
    
    /* Ensure we have enough space */
    if (buffer->size + real_size + 1 > buffer->allocated) {
        size_t new_size = buffer->allocated == 0 ? 4096 : buffer->allocated * 2;
        while (new_size < buffer->size + real_size + 1) {
            new_size *= 2;
        }
        
        char *new_memory = knishio_realloc(buffer->memory, new_size);
        if (!new_memory) {
            return 0; /* Signal error to libcurl */
        }
        
        buffer->memory = new_memory;
        buffer->allocated = new_size;
    }

    memcpy(&(buffer->memory[buffer->size]), contents, real_size);
    buffer->size += real_size;
    buffer->memory[buffer->size] = '\0';

    return real_size;
}

static size_t header_callback(char *buffer, size_t size, size_t nitems, header_buffer_t *headers) {
    size_t real_size = size * nitems;
    
    /* Skip status line and empty lines */
    if (real_size < 2 || buffer[0] == '\r' || buffer[0] == '\n') {
        return real_size;
    }

    /* Ensure we have space for another header */
    if (headers->count >= headers->allocated) {
        size_t new_alloc = headers->allocated == 0 ? 16 : headers->allocated * 2;
        char **new_headers = knishio_realloc(headers->headers, new_alloc * sizeof(char*));
        if (!new_headers) {
            return 0; /* Signal error */
        }
        headers->headers = new_headers;
        headers->allocated = new_alloc;
    }

    /* Copy header, removing trailing CRLF */
    size_t header_len = real_size;
    while (header_len > 0 && (buffer[header_len - 1] == '\r' || buffer[header_len - 1] == '\n')) {
        header_len--;
    }

    headers->headers[headers->count] = knishio_strndup(buffer, header_len);
    if (headers->headers[headers->count]) {
        headers->count++;
    }

    return real_size;
}

static knishio_http_response_t* execute_request(knishio_http_client_t *client, const knishio_http_config_t *config) {
    clear_client_error(client);

    knishio_http_response_t *response = create_response();
    if (!response) {
        set_client_error(client, "Failed to allocate response");
        return NULL;
    }

    /* Prepare response and header buffers */
    response_buffer_t resp_buffer = {0};
    header_buffer_t header_buffer = {0};

    /* Build full URL if needed */
    char *full_url = NULL;
    if (client->base_url && config->url[0] != 'h') {
        full_url = build_full_url(client->base_url, config->url);
        if (!full_url) {
            set_client_error(client, "Failed to build URL");
            knishio_http_response_free(response);
            return NULL;
        }
    }

    const char *request_url = full_url ? full_url : config->url;

    /* Configure curl for this request */
    CURL *curl = client->curl_handle;
    curl_easy_setopt(curl, CURLOPT_URL, request_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_buffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_buffer);

    /* Set HTTP method and body */
    switch (config->method) {
        case KNISHIO_HTTP_GET:
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            break;
        case KNISHIO_HTTP_POST:
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (config->body) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, config->body);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, config->body_length > 0 ? config->body_length : strlen(config->body));
            }
            break;
        case KNISHIO_HTTP_PUT:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            if (config->body) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, config->body);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, config->body_length > 0 ? config->body_length : strlen(config->body));
            }
            break;
        case KNISHIO_HTTP_DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case KNISHIO_HTTP_PATCH:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            if (config->body) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, config->body);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, config->body_length > 0 ? config->body_length : strlen(config->body));
            }
            break;
    }

    /* Build headers */
    struct curl_slist *headers = build_header_list(client, config->headers, config->header_count);
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    /* Override timeout if specified */
    if (config->timeout_ms > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config->timeout_ms);
    }

    /* Execute request */
    CURLcode res = curl_easy_perform(curl);

    /* Get response information */
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    response->status_code = (int)status_code;

    if (res == CURLE_OK) {
        /* Success - copy response data */
        if (resp_buffer.memory) {
            response->body = resp_buffer.memory;
            response->body_length = resp_buffer.size;
        }
        
        if (header_buffer.headers) {
            response->headers = header_buffer.headers;
            response->header_count = header_buffer.count;
        }
    } else {
        /* Error - cleanup and set error message */
        knishio_free(resp_buffer.memory);
        if (header_buffer.headers) {
            for (size_t i = 0; i < header_buffer.count; i++) {
                knishio_free(header_buffer.headers[i]);
            }
            knishio_free(header_buffer.headers);
        }

        char error_buf[256];
        snprintf(error_buf, sizeof(error_buf), "HTTP request failed: %s", curl_easy_strerror(res));
        set_client_error(client, error_buf);
        response->error_message = knishio_strdup(error_buf);
    }

    /* Cleanup */
    if (headers) {
        curl_slist_free_all(headers);
    }
    knishio_free(full_url);
    
    /* Reset curl options for reuse */
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, client->timeout_ms);

    return response;
}

static void set_client_error(knishio_http_client_t *client, const char *error) {
    if (!client || !error) return;
    
    knishio_free(client->last_error);
    client->last_error = knishio_strdup(error);
}

static void clear_client_error(knishio_http_client_t *client) {
    if (!client) return;
    
    knishio_free(client->last_error);
    client->last_error = NULL;
}

static char* build_full_url(const char *base_url, const char *path) {
    if (!base_url || !path) return NULL;

    size_t base_len = strlen(base_url);
    size_t path_len = strlen(path);
    bool need_slash = (base_len > 0 && base_url[base_len - 1] != '/' && path[0] != '/');

    char *full_url = knishio_malloc(base_len + path_len + (need_slash ? 2 : 1));
    if (!full_url) return NULL;

    strcpy(full_url, base_url);
    if (need_slash) {
        strcat(full_url, "/");
    }
    strcat(full_url, path);

    return full_url;
}

static struct curl_slist* build_header_list(knishio_http_client_t *client, const char **additional_headers, size_t additional_count) {
    struct curl_slist *headers = NULL;

    /* Add default headers */
    for (size_t i = 0; i < client->header_count; i++) {
        if (client->default_headers[i]) {
            headers = curl_slist_append(headers, client->default_headers[i]);
        }
    }

    /* Add authentication header if present */
    if (client->auth_token) {
        char auth_header[1024];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", client->auth_token);
        headers = curl_slist_append(headers, auth_header);
    }

    /* Add request-specific headers */
    for (size_t i = 0; i < additional_count; i++) {
        if (additional_headers[i]) {
            headers = curl_slist_append(headers, additional_headers[i]);
        }
    }

    return headers;
}

static knishio_http_response_t* create_response(void) {
    knishio_http_response_t *response = knishio_calloc(1, sizeof(knishio_http_response_t));
    if (!response) return NULL;

    /* Initialize fields to safe defaults */
    response->status_code = 0;
    response->body = NULL;
    response->body_length = 0;
    response->headers = NULL;
    response->header_count = 0;
    response->error_message = NULL;

    return response;
}

static void free_response_internals(knishio_http_response_t *response) {
    if (!response) return;

    knishio_free(response->body);
    knishio_free(response->error_message);

    if (response->headers) {
        for (size_t i = 0; i < response->header_count; i++) {
            knishio_free(response->headers[i]);
        }
        knishio_free(response->headers);
    }
}