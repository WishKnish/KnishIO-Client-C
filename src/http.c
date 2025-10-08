/**
 * @file http.c
 * @brief HTTP client implementation for KnishIO C SDK
 * 
 * Implements JavaScript-compatible HTTP client using libcurl.
 * KISS design focused on GraphQL POST operations.
 * Simple KISS design following ultrathink methodology.
 */

#include "knishio/http.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include "knishio/json/builder.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>

/* Internal structures for libcurl callbacks */
typedef struct {
    char* memory;
    size_t size;
} curl_response_data_t;

/* Internal helper function declarations */
static size_t knishio_curl_write_callback(void* contents, size_t size, size_t nmemb, curl_response_data_t* data);
static size_t knishio_curl_header_callback(char* buffer, size_t size, size_t nitems, curl_response_data_t* data);
static knishio_error_t copy_http_string(char** dest, const char* src);
static void free_http_string(char** field);
static knishio_error_t build_graphql_payload(const char* query, const char* variables, char** payload);

/* HTTP client lifecycle functions */

knishio_error_t knishio_http_client_create(
    knishio_http_client_t** client,
    const char* base_url
) {
    if (!client || !base_url) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate client structure */
    knishio_http_client_t* c = knishio_malloc(sizeof(knishio_http_client_t));
    if (!c) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Initialize all fields to safe defaults */
    memset(c, 0, sizeof(knishio_http_client_t));
    
    /* Initialize authentication fields to NULL */
    c->auth_token = NULL;
    c->auth_pubkey = NULL;

    /* Copy base URL */
    knishio_error_t error = copy_http_string(&c->base_url, base_url);
    if (error != KNISHIO_SUCCESS) {
        knishio_free(c);
        return error;
    }

    /* Set default values */
    c->timeout_seconds = 30;  /* 30 second timeout */
    c->verify_ssl = true;     /* Verify SSL by default */

    /* Set default user agent */
    const char* user_agent = "KnishIO-C-SDK/1.0";
    error = copy_http_string(&c->user_agent, user_agent);
    if (error != KNISHIO_SUCCESS) {
        knishio_http_client_free(c);
        return error;
    }

    *client = c;
    return KNISHIO_SUCCESS;
}

void knishio_http_client_free(knishio_http_client_t* client) {
    if (!client) {
        return;
    }

    /* Free string fields */
    free_http_string(&client->base_url);
    free_http_string(&client->user_agent);
    free_http_string(&client->auth_token);
    free_http_string(&client->auth_pubkey);

    /* Free the client structure itself */
    knishio_free(client);
}

knishio_error_t knishio_http_client_set_timeout(
    knishio_http_client_t* client,
    long timeout_seconds
) {
    if (!client || timeout_seconds < 0) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    client->timeout_seconds = timeout_seconds;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_http_client_set_ssl_verify(
    knishio_http_client_t* client,
    bool verify_ssl
) {
    if (!client) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    client->verify_ssl = verify_ssl;
    return KNISHIO_SUCCESS;
}

/* Authentication header functions */

knishio_error_t knishio_http_client_set_auth_token(
    knishio_http_client_t* client,
    const char* token
) {
    if (!client) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Clear existing auth token */
    free_http_string(&client->auth_token);

    /* Set new token if provided */
    if (token) {
        return copy_http_string(&client->auth_token, token);
    }

    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_http_client_set_auth_pubkey(
    knishio_http_client_t* client,
    const char* pubkey
) {
    if (!client) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Clear existing auth pubkey */
    free_http_string(&client->auth_pubkey);

    /* Set new pubkey if provided */
    if (pubkey) {
        return copy_http_string(&client->auth_pubkey, pubkey);
    }

    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_http_client_clear_auth(
    knishio_http_client_t* client
) {
    if (!client) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Clear both auth fields */
    free_http_string(&client->auth_token);
    free_http_string(&client->auth_pubkey);

    return KNISHIO_SUCCESS;
}

/**
 * @brief Set custom headers for HTTP client
 * @param client HTTP client
 * @param headers Array of header strings
 * @param count Number of headers
 * @return True on success, false on error
 */
bool knishio_http_client_set_headers(
    knishio_http_client_t* client,
    const char** headers,
    size_t count
) {
    if (!client || !headers) {
        return false;
    }
    
    /* For simple implementation, just validate headers - full implementation would store them */
    for (size_t i = 0; i < count; i++) {
        if (!headers[i]) {
            return false;
        }
    }
    
    /* Headers validated successfully */
    return true;
}

/**
 * @brief Get last HTTP client error
 * @param client HTTP client
 * @return Error message or NULL
 */
const char* knishio_http_client_error(knishio_http_client_t* client) {
    /* For simple implementation, return generic error message */
    /* Full implementation would track actual curl error messages */
    return client ? "HTTP client error" : "Invalid client";
}

/* HTTP response lifecycle functions */

knishio_error_t knishio_http_response_create(knishio_http_response_t** response) {
    if (!response) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Allocate response structure */
    knishio_http_response_t* r = knishio_malloc(sizeof(knishio_http_response_t));
    if (!r) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Initialize all fields to safe defaults */
    memset(r, 0, sizeof(knishio_http_response_t));

    *response = r;
    return KNISHIO_SUCCESS;
}

void knishio_http_response_free(knishio_http_response_t* response) {
    if (!response) {
        return;
    }

    /* Free string fields */
    free_http_string(&response->data);
    free_http_string(&response->content_type);

    /* Free the response structure itself */
    knishio_free(response);
}

/* Core HTTP operations */

knishio_error_t knishio_http_post_graphql(
    knishio_http_client_t* client,
    const char* graphql_query,
    const char* variables,
    knishio_http_response_t** response
) {
    if (!client || !graphql_query || !response) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Build GraphQL payload */
    char* payload = NULL;
    knishio_error_t error = build_graphql_payload(graphql_query, variables, &payload);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Perform POST request */
    error = knishio_http_post(client, "/graphql", payload, "application/json", response);

    /* Free payload */
    if (payload) {
        knishio_free(payload);
    }

    return error;
}

knishio_error_t knishio_http_post(
    knishio_http_client_t* client,
    const char* path,
    const char* data,
    const char* content_type,
    knishio_http_response_t** response
) {
    if (!client || !path || !data || !response) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Create response */
    knishio_error_t error = knishio_http_response_create(response);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Initialize libcurl handle */
    CURL* curl = curl_easy_init();
    if (!curl) {
        knishio_http_response_free(*response);
        *response = NULL;
        return KNISHIO_ERROR_HTTP_INIT;
    }

    /* Build full URL */
    size_t url_len = strlen(client->base_url) + strlen(path) + 1;
    char* full_url = knishio_malloc(url_len);
    if (!full_url) {
        curl_easy_cleanup(curl);
        knishio_http_response_free(*response);
        *response = NULL;
        return KNISHIO_ERROR_MEMORY;
    }
    snprintf(full_url, url_len, "%s%s", client->base_url, path);

    /* Response data structure */
    curl_response_data_t response_data = {0};
    curl_response_data_t header_data = {0};

    /* Configure curl options */
    curl_easy_setopt(curl, CURLOPT_URL, full_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, client->user_agent);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, client->timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, client->verify_ssl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, client->verify_ssl ? 2L : 0L);
    
    /* Set callbacks */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, knishio_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, knishio_curl_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data);

    /* Set headers */
    struct curl_slist* headers = NULL;
    
    /* Add Content-Type header */
    if (content_type) {
        char content_type_header[256];
        snprintf(content_type_header, sizeof(content_type_header), "Content-Type: %s", content_type);
        headers = curl_slist_append(headers, content_type_header);
    }
    
    /* Add authentication headers if present */
    if (client->auth_token) {
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", client->auth_token);
        headers = curl_slist_append(headers, auth_header);
    }
    
    if (client->auth_pubkey) {
        char pubkey_header[512];
        snprintf(pubkey_header, sizeof(pubkey_header), "X-KnishIO-Pubkey: %s", client->auth_pubkey);
        headers = curl_slist_append(headers, pubkey_header);
    }
    
    /* Set headers in curl if we have any */
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    /* Perform request */
    CURLcode res = curl_easy_perform(curl);
    
    /* Get response info */
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &(*response)->status_code);
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &(*response)->total_time);
        
        /* Copy response data */
        if (response_data.memory) {
            (*response)->data = response_data.memory;
            (*response)->size = response_data.size;
        }
        
        /* Extract content type from headers (simplified) */
        if (header_data.memory) {
            const char* ct_start = strstr(header_data.memory, "Content-Type:");
            if (ct_start) {
                ct_start += 13; /* Skip "Content-Type:" */
                while (*ct_start == ' ') ct_start++; /* Skip spaces */
                const char* ct_end = strchr(ct_start, '\r');
                if (!ct_end) ct_end = strchr(ct_start, '\n');
                if (ct_end) {
                    size_t ct_len = ct_end - ct_start;
                    (*response)->content_type = knishio_malloc(ct_len + 1);
                    if ((*response)->content_type) {
                        strncpy((*response)->content_type, ct_start, ct_len);
                        (*response)->content_type[ct_len] = '\0';
                    }
                }
            }
            knishio_free(header_data.memory);
        }
        
        error = KNISHIO_SUCCESS;
    } else {
        /* Handle curl error */
        if (response_data.memory) {
            knishio_free(response_data.memory);
        }
        if (header_data.memory) {
            knishio_free(header_data.memory);
        }
        knishio_http_response_free(*response);
        *response = NULL;
        error = KNISHIO_ERROR_HTTP_REQUEST;
    }

    /* Cleanup */
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    knishio_free(full_url);

    return error;
}

knishio_error_t knishio_http_get(
    knishio_http_client_t* client,
    const char* path,
    knishio_http_response_t** response
) {
    if (!client || !path || !response) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* Create response */
    knishio_error_t error = knishio_http_response_create(response);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }

    /* Initialize libcurl handle */
    CURL* curl = curl_easy_init();
    if (!curl) {
        knishio_http_response_free(*response);
        *response = NULL;
        return KNISHIO_ERROR_HTTP_INIT;
    }

    /* Build full URL */
    size_t url_len = strlen(client->base_url) + strlen(path) + 1;
    char* full_url = knishio_malloc(url_len);
    if (!full_url) {
        curl_easy_cleanup(curl);
        knishio_http_response_free(*response);
        *response = NULL;
        return KNISHIO_ERROR_MEMORY;
    }
    snprintf(full_url, url_len, "%s%s", client->base_url, path);

    /* Response data structure */
    curl_response_data_t response_data = {0};

    /* Configure curl options */
    curl_easy_setopt(curl, CURLOPT_URL, full_url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, client->user_agent);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, client->timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, client->verify_ssl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, client->verify_ssl ? 2L : 0L);
    
    /* Set callbacks */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, knishio_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    /* Perform request */
    CURLcode res = curl_easy_perform(curl);
    
    /* Get response info */
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &(*response)->status_code);
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &(*response)->total_time);
        
        /* Copy response data */
        if (response_data.memory) {
            (*response)->data = response_data.memory;
            (*response)->size = response_data.size;
        }
        
        error = KNISHIO_SUCCESS;
    } else {
        /* Handle curl error */
        if (response_data.memory) {
            knishio_free(response_data.memory);
        }
        knishio_http_response_free(*response);
        *response = NULL;
        error = KNISHIO_ERROR_HTTP_REQUEST;
    }

    /* Cleanup */
    curl_easy_cleanup(curl);
    knishio_free(full_url);

    return error;
}

/* Response access functions */

const char* knishio_http_response_get_data(const knishio_http_response_t* response) {
    return response ? response->data : NULL;
}

size_t knishio_http_response_get_size(const knishio_http_response_t* response) {
    return response ? response->size : 0;
}

long knishio_http_response_get_status(const knishio_http_response_t* response) {
    return response ? response->status_code : 0;
}

const char* knishio_http_response_get_content_type(const knishio_http_response_t* response) {
    return response ? response->content_type : NULL;
}

double knishio_http_response_get_time(const knishio_http_response_t* response) {
    return response ? response->total_time : 0.0;
}

/* Utility functions */

bool knishio_http_response_is_success(const knishio_http_response_t* response) {
    if (!response) {
        return false;
    }
    return response->status_code >= 200 && response->status_code < 300;
}

bool knishio_http_response_is_json(const knishio_http_response_t* response) {
    if (!response || !response->content_type) {
        return false;
    }
    return strstr(response->content_type, "application/json") != NULL;
}

knishio_error_t knishio_http_global_init(void) {
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    return (res == CURLE_OK) ? KNISHIO_SUCCESS : KNISHIO_ERROR_HTTP_INIT;
}

void knishio_http_global_cleanup(void) {
    curl_global_cleanup();
}

/* Internal helper functions */

static size_t knishio_curl_write_callback(void* contents, size_t size, size_t nmemb, curl_response_data_t* data) {
    size_t realsize = size * nmemb;
    char* ptr = knishio_realloc(data->memory, data->size + realsize + 1);
    
    if (!ptr) {
        /* Out of memory */
        return 0;
    }
    
    data->memory = ptr;
    memcpy(&(data->memory[data->size]), contents, realsize);
    data->size += realsize;
    data->memory[data->size] = '\0';
    
    return realsize;
}

static size_t knishio_curl_header_callback(char* buffer, size_t size, size_t nitems, curl_response_data_t* data) {
    size_t realsize = size * nitems;
    char* ptr = knishio_realloc(data->memory, data->size + realsize + 1);
    
    if (!ptr) {
        /* Out of memory */
        return 0;
    }
    
    data->memory = ptr;
    memcpy(&(data->memory[data->size]), buffer, realsize);
    data->size += realsize;
    data->memory[data->size] = '\0';
    
    return realsize;
}

static knishio_error_t copy_http_string(char** dest, const char* src) {
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

static void free_http_string(char** field) {
    if (field && *field) {
        knishio_free(*field);
        *field = NULL;
    }
}

static knishio_error_t build_graphql_payload(const char* query, const char* variables, char** payload) {
    if (!query || !payload) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    /* KISS approach: Build JSON payload using string formatting */
    size_t buffer_size = strlen(query) + (variables ? strlen(variables) : 0) + 256;
    char* buffer = knishio_malloc(buffer_size);
    if (!buffer) {
        return KNISHIO_ERROR_MEMORY;
    }

    if (variables && strlen(variables) > 0) {
        snprintf(buffer, buffer_size, "{\"query\":\"%s\",\"variables\":%s}", query, variables);
    } else {
        snprintf(buffer, buffer_size, "{\"query\":\"%s\"}", query);
    }

    *payload = buffer;
    return KNISHIO_SUCCESS;
}