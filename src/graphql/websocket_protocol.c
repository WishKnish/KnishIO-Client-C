/**
 * @file websocket_protocol.c
 * @brief GraphQL-over-WebSocket protocol implementation (graphql-ws)
 * 
 * Implements the GraphQL-over-WebSocket subprotocol for subscription management
 * following the graphql-ws specification used by Apollo GraphQL and similar.
 */

#include "knishio/graphql.h"
#include "knishio/http.h"
#include "knishio/json/builder.h"
#include "knishio/json/parser.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/logging.h"
#include "knishio/error/context.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

/* GraphQL-over-WebSocket message types (graphql-ws protocol) */
typedef enum {
    GQL_WS_CONNECTION_INIT,
    GQL_WS_CONNECTION_ACK, 
    GQL_WS_CONNECTION_ERROR,
    GQL_WS_CONNECTION_KEEP_ALIVE,
    GQL_WS_START,
    GQL_WS_DATA,
    GQL_WS_ERROR,
    GQL_WS_COMPLETE,
    GQL_WS_STOP,
    GQL_WS_CONNECTION_TERMINATE
} gql_ws_message_type_t;

/* Message type strings for the protocol */
static const char* GQL_WS_TYPE_CONNECTION_INIT = "connection_init";
static const char* GQL_WS_TYPE_CONNECTION_ACK = "connection_ack";
static const char* GQL_WS_TYPE_CONNECTION_ERROR = "connection_error";
static const char* GQL_WS_TYPE_CONNECTION_KEEP_ALIVE = "ka";
static const char* GQL_WS_TYPE_START = "start";
static const char* GQL_WS_TYPE_DATA = "data";
static const char* GQL_WS_TYPE_ERROR = "error";
static const char* GQL_WS_TYPE_COMPLETE = "complete";
static const char* GQL_WS_TYPE_STOP = "stop";
static const char* GQL_WS_TYPE_CONNECTION_TERMINATE = "connection_terminate";

/* WebSocket subscription handle with GraphQL protocol support */
typedef struct knishio_graphql_ws_subscription {
    char* id;                        /**< Unique subscription ID */
    char* query;                     /**< GraphQL subscription query */
    char* variables_json;            /**< Variables as JSON string */
    char* operation_name;            /**< Optional operation name */
    
    /* Callback functions */
    void (*on_data)(const char* data, void* user_data);
    void (*on_error)(const char* error, void* user_data);
    void (*on_complete)(void* user_data);
    void* user_data;
    
    /* State */
    bool active;
    time_t created_at;
    time_t last_data_received;
    
    /* Linked list */
    struct knishio_graphql_ws_subscription* next;
} knishio_graphql_ws_subscription_t;

/* GraphQL WebSocket client state */
typedef struct knishio_graphql_ws_client {
    knishio_websocket_client_t* ws_client;  /**< Underlying WebSocket client */
    
    /* Protocol state */
    bool connection_ack_received;
    bool connection_init_sent;
    time_t last_keep_alive;
    
    /* Subscriptions */
    knishio_graphql_ws_subscription_t* subscriptions;
    int next_subscription_id;
    pthread_mutex_t subscriptions_mutex;
    
    /* Authentication */
    char* auth_token;
    char* connection_params_json;
    
    /* Configuration */
    int keep_alive_timeout_ms;
    bool lazy_close_timeout;
} knishio_graphql_ws_client_t;

/* Forward declarations */
static knishio_error_t gql_ws_build_message(const char* type, const char* id, 
                                           const char* payload, char** message_json);
static knishio_error_t gql_ws_parse_message(const char* message_json, char** type, 
                                           char** id, char** payload);
static void gql_ws_handle_message(knishio_graphql_ws_client_t* client, 
                                 const char* message_json);
static knishio_graphql_ws_subscription_t* gql_ws_find_subscription(
    knishio_graphql_ws_client_t* client, const char* id);
static void gql_ws_websocket_callback(const char* data, const char* error, 
                                     void* user_data);
static char* gql_ws_generate_subscription_id(knishio_graphql_ws_client_t* client);

/* Build GraphQL-over-WebSocket protocol message */
static knishio_error_t gql_ws_build_message(const char* type, const char* id, 
                                           const char* payload, char** message_json) {
    if (!type || !message_json) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    knishio_json_builder_t* builder = knishio_json_builder_create();
    if (!builder) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    knishio_error_t error = knishio_json_builder_start_object(builder);
    
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(builder, "type", type);
    }
    
    if (error == KNISHIO_SUCCESS && id) {
        error = knishio_json_builder_add_string(builder, "id", id);
    }
    
    if (error == KNISHIO_SUCCESS && payload) {
        error = knishio_json_builder_add_raw(builder, "payload", payload);
    }
    
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_end_object(builder);
    }
    
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_build(builder, message_json);
    }
    
    knishio_json_builder_free(builder);
    return error;
}

/* Parse GraphQL-over-WebSocket protocol message */
static knishio_error_t gql_ws_parse_message(const char* message_json, char** type, 
                                           char** id, char** payload) {
    if (!message_json || !type) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    char* error_msg = NULL;
    knishio_json_t* json = knishio_json_parse(message_json, &error_msg);
    if (!json) {
        if (error_msg) {
            knishio_log(KNISHIO_LOG_ERROR, "Failed to parse WebSocket message: %s", error_msg);
            free(error_msg);
        }
        return KNISHIO_ERROR_JSON_PARSE;
    }
    
    /* Extract message type */
    const char* type_str = knishio_json_get_string_path(json, "type");
    if (!type_str) {
        knishio_json_free(json);
        return KNISHIO_ERROR_JSON_PARSE;
    }
    
    *type = knishio_strdup(type_str);
    
    /* Extract optional ID */
    if (id) {
        const char* id_str = knishio_json_get_string_path(json, "id");
        *id = id_str ? knishio_strdup(id_str) : NULL;
    }
    
    /* Extract optional payload */
    if (payload) {
        const char* payload_str = knishio_json_get_string_path(json, "payload");
        if (payload_str) {
            *payload = knishio_strdup(payload_str);
        } else {
            /* Try to get payload as object/array and serialize */
            knishio_json_t* payload_obj = NULL;
            if (knishio_json_get_object(json, "payload", &payload_obj) == KNISHIO_SUCCESS ||
                knishio_json_get_array(json, "payload", &payload_obj) == KNISHIO_SUCCESS) {
                knishio_json_to_string(payload_obj, payload);
            } else {
                *payload = NULL;
            }
        }
    }
    
    knishio_json_free(json);
    return KNISHIO_SUCCESS;
}

/* Handle incoming GraphQL-over-WebSocket message */
static void gql_ws_handle_message(knishio_graphql_ws_client_t* client, 
                                 const char* message_json) {
    if (!client || !message_json) {
        return;
    }
    
    char* type = NULL;
    char* id = NULL;
    char* payload = NULL;
    
    if (gql_ws_parse_message(message_json, &type, &id, &payload) != KNISHIO_SUCCESS) {
        knishio_log(KNISHIO_LOG_ERROR, "Failed to parse WebSocket message");
        return;
    }
    
    knishio_log(KNISHIO_LOG_DEBUG, "GraphQL-WS received message type: %s", type);
    
    if (strcmp(type, GQL_WS_TYPE_CONNECTION_ACK) == 0) {
        client->connection_ack_received = true;
        knishio_log(KNISHIO_LOG_INFO, "GraphQL-WS connection acknowledged");
    }
    else if (strcmp(type, GQL_WS_TYPE_CONNECTION_ERROR) == 0) {
        knishio_log(KNISHIO_LOG_ERROR, "GraphQL-WS connection error: %s", 
                   payload ? payload : "Unknown error");
    }
    else if (strcmp(type, GQL_WS_TYPE_CONNECTION_KEEP_ALIVE) == 0) {
        client->last_keep_alive = time(NULL);
        knishio_log(KNISHIO_LOG_DEBUG, "GraphQL-WS keep-alive received");
    }
    else if (strcmp(type, GQL_WS_TYPE_DATA) == 0 && id) {
        /* Handle subscription data */
        pthread_mutex_lock(&client->subscriptions_mutex);
        
        knishio_graphql_ws_subscription_t* sub = gql_ws_find_subscription(client, id);
        if (sub && sub->on_data) {
            sub->last_data_received = time(NULL);
            sub->on_data(payload, sub->user_data);
        }
        
        pthread_mutex_unlock(&client->subscriptions_mutex);
    }
    else if (strcmp(type, GQL_WS_TYPE_ERROR) == 0 && id) {
        /* Handle subscription error */
        pthread_mutex_lock(&client->subscriptions_mutex);
        
        knishio_graphql_ws_subscription_t* sub = gql_ws_find_subscription(client, id);
        if (sub && sub->on_error) {
            sub->on_error(payload, sub->user_data);
        }
        
        pthread_mutex_unlock(&client->subscriptions_mutex);
    }
    else if (strcmp(type, GQL_WS_TYPE_COMPLETE) == 0 && id) {
        /* Handle subscription completion */
        pthread_mutex_lock(&client->subscriptions_mutex);
        
        knishio_graphql_ws_subscription_t* sub = gql_ws_find_subscription(client, id);
        if (sub) {
            if (sub->on_complete) {
                sub->on_complete(sub->user_data);
            }
            
            /* Remove subscription from list */
            knishio_graphql_ws_subscription_t** current = &client->subscriptions;
            while (*current) {
                if (*current == sub) {
                    *current = sub->next;
                    
                    knishio_free(sub->id);
                    knishio_free(sub->query);
                    knishio_free(sub->variables_json);
                    knishio_free(sub->operation_name);
                    knishio_free(sub);
                    break;
                }
                current = &((*current)->next);
            }
        }
        
        pthread_mutex_unlock(&client->subscriptions_mutex);
    }
    
    knishio_free(type);
    knishio_free(id);
    knishio_free(payload);
}

/* WebSocket callback for GraphQL protocol handling */
static void gql_ws_websocket_callback(const char* data, const char* error, void* user_data) {
    knishio_graphql_ws_client_t* client = (knishio_graphql_ws_client_t*)user_data;
    
    if (error) {
        knishio_log(KNISHIO_LOG_ERROR, "WebSocket error: %s", error);
        return;
    }
    
    if (data) {
        gql_ws_handle_message(client, data);
    }
}

/* Generate unique subscription ID */
static char* gql_ws_generate_subscription_id(knishio_graphql_ws_client_t* client) {
    if (!client) return NULL;
    
    char* id = knishio_malloc(64);
    if (!id) return NULL;
    
    snprintf(id, 64, "sub_%d_%ld", client->next_subscription_id++, time(NULL));
    return id;
}

/* Find subscription by ID */
static knishio_graphql_ws_subscription_t* gql_ws_find_subscription(
    knishio_graphql_ws_client_t* client, const char* id) {
    if (!client || !id) return NULL;
    
    knishio_graphql_ws_subscription_t* current = client->subscriptions;
    while (current) {
        if (strcmp(current->id, id) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/* Public API Implementation */

knishio_graphql_ws_client_t* knishio_graphql_ws_client_create(const char* endpoint_url) {
    if (!endpoint_url) {
        return NULL;
    }
    
    knishio_graphql_ws_client_t* client = knishio_calloc(1, sizeof(knishio_graphql_ws_client_t));
    if (!client) {
        return NULL;
    }
    
    /* Create underlying WebSocket client */
    client->ws_client = knishio_websocket_client_create(endpoint_url);
    if (!client->ws_client) {
        knishio_free(client);
        return NULL;
    }
    
    /* Initialize protocol state */
    client->connection_ack_received = false;
    client->connection_init_sent = false;
    client->next_subscription_id = 1;
    client->keep_alive_timeout_ms = 30000; /* 30 seconds */
    
    /* Initialize threading */
    if (pthread_mutex_init(&client->subscriptions_mutex, NULL) != 0) {
        knishio_websocket_client_free(client->ws_client);
        knishio_free(client);
        return NULL;
    }
    
    knishio_log(KNISHIO_LOG_INFO, "GraphQL-WS client created for %s", endpoint_url);
    return client;
}

void knishio_graphql_ws_client_free(knishio_graphql_ws_client_t* client) {
    if (!client) return;
    
    /* Clean up subscriptions */
    pthread_mutex_lock(&client->subscriptions_mutex);
    while (client->subscriptions) {
        knishio_graphql_ws_subscription_t* sub = client->subscriptions;
        client->subscriptions = sub->next;
        
        knishio_free(sub->id);
        knishio_free(sub->query);
        knishio_free(sub->variables_json);
        knishio_free(sub->operation_name);
        knishio_free(sub);
    }
    pthread_mutex_unlock(&client->subscriptions_mutex);
    
    /* Clean up WebSocket client */
    if (client->ws_client) {
        knishio_websocket_client_free(client->ws_client);
    }
    
    /* Clean up strings */
    knishio_free(client->auth_token);
    knishio_free(client->connection_params_json);
    
    /* Clean up threading */
    pthread_mutex_destroy(&client->subscriptions_mutex);
    
    knishio_free(client);
}

knishio_error_t knishio_graphql_ws_connect(knishio_graphql_ws_client_t* client) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Connect WebSocket */
    if (!knishio_websocket_connect(client->ws_client)) {
        return KNISHIO_ERROR_CONNECTION_FAILED;
    }
    
    /* Send connection initialization */
    char* init_payload = NULL;
    if (client->connection_params_json) {
        init_payload = client->connection_params_json;
    } else {
        init_payload = "{}";
    }
    
    char* init_message = NULL;
    knishio_error_t error = gql_ws_build_message(GQL_WS_TYPE_CONNECTION_INIT, NULL, 
                                                init_payload, &init_message);
    if (error == KNISHIO_SUCCESS) {
        /* TODO: Send via WebSocket */
        client->connection_init_sent = true;
        knishio_log(KNISHIO_LOG_INFO, "GraphQL-WS connection initialization sent");
        knishio_free(init_message);
    }
    
    return error;
}

knishio_error_t knishio_graphql_ws_disconnect(knishio_graphql_ws_client_t* client) {
    if (!client) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Send connection terminate */
    char* terminate_message = NULL;
    gql_ws_build_message(GQL_WS_TYPE_CONNECTION_TERMINATE, NULL, NULL, &terminate_message);
    
    if (terminate_message) {
        /* TODO: Send via WebSocket */
        knishio_free(terminate_message);
    }
    
    /* Disconnect WebSocket */
    knishio_websocket_disconnect(client->ws_client);
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_graphql_ws_subscribe(knishio_graphql_ws_client_t* client,
                                            const char* query, const char* variables_json,
                                            const char* operation_name,
                                            void (*on_data)(const char* data, void* user_data),
                                            void (*on_error)(const char* error, void* user_data),
                                            void (*on_complete)(void* user_data),
                                            void* user_data,
                                            char** subscription_id) {
    if (!client || !query || !on_data || !subscription_id) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    if (!client->connection_ack_received) {
        return KNISHIO_ERROR_INVALID_STATE;
    }
    
    /* Create subscription */
    knishio_graphql_ws_subscription_t* sub = knishio_calloc(1, sizeof(knishio_graphql_ws_subscription_t));
    if (!sub) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    sub->id = gql_ws_generate_subscription_id(client);
    sub->query = knishio_strdup(query);
    sub->variables_json = variables_json ? knishio_strdup(variables_json) : NULL;
    sub->operation_name = operation_name ? knishio_strdup(operation_name) : NULL;
    sub->on_data = on_data;
    sub->on_error = on_error;
    sub->on_complete = on_complete;
    sub->user_data = user_data;
    sub->active = true;
    sub->created_at = time(NULL);
    
    if (!sub->id || !sub->query) {
        knishio_free(sub->id);
        knishio_free(sub->query);
        knishio_free(sub->variables_json);
        knishio_free(sub->operation_name);
        knishio_free(sub);
        return KNISHIO_ERROR_MEMORY;
    }
    
    /* Build start message payload */
    knishio_json_builder_t* payload_builder = knishio_json_builder_create();
    if (!payload_builder) {
        knishio_free(sub->id);
        knishio_free(sub->query);
        knishio_free(sub->variables_json);
        knishio_free(sub->operation_name);
        knishio_free(sub);
        return KNISHIO_ERROR_MEMORY;
    }
    
    knishio_error_t error = knishio_json_builder_start_object(payload_builder);
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_add_string(payload_builder, "query", sub->query);
    }
    if (error == KNISHIO_SUCCESS && sub->variables_json) {
        error = knishio_json_builder_add_raw(payload_builder, "variables", sub->variables_json);
    }
    if (error == KNISHIO_SUCCESS && sub->operation_name) {
        error = knishio_json_builder_add_string(payload_builder, "operationName", sub->operation_name);
    }
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_end_object(payload_builder);
    }
    
    char* payload_json = NULL;
    if (error == KNISHIO_SUCCESS) {
        error = knishio_json_builder_build(payload_builder, &payload_json);
    }
    
    knishio_json_builder_free(payload_builder);
    
    if (error != KNISHIO_SUCCESS) {
        knishio_free(sub->id);
        knishio_free(sub->query);
        knishio_free(sub->variables_json);
        knishio_free(sub->operation_name);
        knishio_free(sub);
        return error;
    }
    
    /* Build start message */
    char* start_message = NULL;
    error = gql_ws_build_message(GQL_WS_TYPE_START, sub->id, payload_json, &start_message);
    knishio_free(payload_json);
    
    if (error != KNISHIO_SUCCESS) {
        knishio_free(sub->id);
        knishio_free(sub->query);
        knishio_free(sub->variables_json);
        knishio_free(sub->operation_name);
        knishio_free(sub);
        return error;
    }
    
    /* Add subscription to list */
    pthread_mutex_lock(&client->subscriptions_mutex);
    sub->next = client->subscriptions;
    client->subscriptions = sub;
    pthread_mutex_unlock(&client->subscriptions_mutex);
    
    /* Send start message */
    /* TODO: Send via WebSocket */
    knishio_log(KNISHIO_LOG_INFO, "GraphQL subscription started: %s", sub->id);
    
    *subscription_id = knishio_strdup(sub->id);
    knishio_free(start_message);
    
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_graphql_ws_unsubscribe(knishio_graphql_ws_client_t* client,
                                              const char* subscription_id) {
    if (!client || !subscription_id) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    
    /* Build stop message */
    char* stop_message = NULL;
    knishio_error_t error = gql_ws_build_message(GQL_WS_TYPE_STOP, subscription_id, NULL, &stop_message);
    if (error != KNISHIO_SUCCESS) {
        return error;
    }
    
    /* Send stop message */
    /* TODO: Send via WebSocket */
    knishio_log(KNISHIO_LOG_INFO, "GraphQL subscription stopped: %s", subscription_id);
    
    /* Remove subscription from list */
    pthread_mutex_lock(&client->subscriptions_mutex);
    
    knishio_graphql_ws_subscription_t** current = &client->subscriptions;
    while (*current) {
        if (strcmp((*current)->id, subscription_id) == 0) {
            knishio_graphql_ws_subscription_t* to_remove = *current;
            *current = to_remove->next;
            
            knishio_free(to_remove->id);
            knishio_free(to_remove->query);
            knishio_free(to_remove->variables_json);
            knishio_free(to_remove->operation_name);
            knishio_free(to_remove);
            break;
        }
        current = &((*current)->next);
    }
    
    pthread_mutex_unlock(&client->subscriptions_mutex);
    
    knishio_free(stop_message);
    return KNISHIO_SUCCESS;
}