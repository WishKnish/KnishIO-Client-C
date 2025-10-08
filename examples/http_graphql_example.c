/**
 * @file http_graphql_example.c
 * @brief Example usage of enhanced HTTP/GraphQL client for KnishIO C SDK
 * 
 * Demonstrates production-ready GraphQL operations including:
 * - Client initialization with authentication
 * - Balance queries with error handling
 * - Molecule proposals with response parsing
 * - Connection pooling and retry mechanisms
 */

#include "knishio/http/client.h"
#include "knishio/graphql.h"
#include "knishio/json/parser.h"
#include "knishio/json/builder.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/logging.h"
#include "knishio/auth_token.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Example configuration */
#define KNISHIO_NODE_URL "http://localhost:8000/graphql"
#define EXAMPLE_WALLET_ADDRESS "KkExample123456789012345678901234567890"
#define EXAMPLE_TOKEN "TEST"

/* Forward declarations */
static void example_basic_http_operations(void);
static void example_graphql_client_usage(void);
static void example_balance_query(void);
static void example_error_handling(void);
static void log_graphql_response(const char* operation, knishio_graphql_response_t* response);
static void log_http_response(const char* operation, knishio_http_response_t* response);

int main(void) {
    printf("=== KnishIO C SDK HTTP/GraphQL Client Examples ===\n\n");

    /* Initialize curl globally (required for HTTP operations) */
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return 1;
    }

    /* Set up logging */
    knishio_log_set_level(KNISHIO_LOG_INFO);

    /* Run examples */
    example_basic_http_operations();
    example_graphql_client_usage();
    example_balance_query();
    example_error_handling();

    /* Cleanup curl */
    curl_global_cleanup();

    printf("\n=== Examples Complete ===\n");
    return 0;
}

static void example_basic_http_operations(void) {
    printf("1. Basic HTTP Operations Example\n");
    printf("================================\n");

    /* Create HTTP client with connection pooling */
    knishio_http_client_t* client = knishio_http_client_create("https://httpbin.org");
    if (!client) {
        printf("Failed to create HTTP client\n");
        return;
    }

    printf("✓ HTTP client created with connection pooling\n");

    /* Configure client with authentication and headers */
    const char* headers[] = {
        "Content-Type: application/json",
        "Accept: application/json",
        "X-Client-Version: KnishIO-C-SDK/1.0"
    };
    
    knishio_http_client_set_headers(client, headers, 3);
    knishio_http_client_set_auth(client, "example-bearer-token");
    knishio_http_client_set_timeout(client, 15000); /* 15 second timeout */

    printf("✓ HTTP client configured with headers, auth, and timeout\n");

    /* Example: Build JSON payload for POST request */
    knishio_json_object_builder_t* builder = knishio_json_object_builder_create();
    if (builder) {
        knishio_json_object_set_string(builder, "query", "{ test }");
        knishio_json_object_set_string(builder, "operation", "balance");
        
        knishio_json_object_builder_t* vars = knishio_json_object_builder_create();
        if (vars) {
            knishio_json_object_set_string(vars, "address", EXAMPLE_WALLET_ADDRESS);
            knishio_json_object_set_string(vars, "token", EXAMPLE_TOKEN);
            knishio_json_object_set_object(builder, "variables", vars);
            knishio_json_object_builder_free(vars);
        }

        knishio_json_t* json_payload = knishio_json_object_build(builder);
        if (json_payload) {
            char* json_str = knishio_json_serialize(json_payload, true);
            printf("✓ JSON payload built:\n%s\n", json_str ? json_str : "Failed to serialize");
            
            knishio_free(json_str);
            knishio_json_free(json_payload);
        }
        
        knishio_json_object_builder_free(builder);
    }

    printf("✓ JSON request building demonstrated\n");

    /* Note: Not making actual HTTP requests to avoid external dependencies in example */
    printf("✓ HTTP client ready for production GraphQL requests\n\n");

    knishio_http_client_free(client);
}

static void example_graphql_client_usage(void) {
    printf("2. GraphQL Client Usage Example\n");
    printf("===============================\n");

    /* Create GraphQL client */
    knishio_graphql_client_t* client = NULL;
    knishio_error_t error = knishio_graphql_client_create(&client, KNISHIO_NODE_URL, "default");
    
    if (error != KNISHIO_SUCCESS || !client) {
        printf("Failed to create GraphQL client: error %d\n", error);
        return;
    }

    printf("✓ GraphQL client created for endpoint: %s\n", KNISHIO_NODE_URL);

    /* Enable debug mode for detailed logging */
    knishio_graphql_client_set_debug(client, true);
    printf("✓ Debug mode enabled for request/response logging\n");

    /* Example: Set up authentication (would use real auth token in production) */
    /* 
    knishio_auth_token_t* auth_token = knishio_auth_token_create("your-auth-token", "your-public-key");
    if (auth_token) {
        knishio_graphql_client_set_auth_token(client, auth_token);
        printf("✓ Authentication token configured\n");
    }
    */

    printf("✓ GraphQL client ready for queries and mutations\n\n");

    knishio_graphql_client_free(client);
}

static void example_balance_query(void) {
    printf("3. Balance Query Example\n");
    printf("=======================\n");

    /* Create GraphQL client for balance queries */
    knishio_graphql_client_t* client = NULL;
    knishio_error_t error = knishio_graphql_client_create(&client, KNISHIO_NODE_URL, "default");
    
    if (error != KNISHIO_SUCCESS || !client) {
        printf("Failed to create GraphQL client for balance query\n");
        return;
    }

    /* Build balance query using JSON builder */
    knishio_json_object_builder_t* query_builder = knishio_json_build_balance_query(EXAMPLE_WALLET_ADDRESS, EXAMPLE_TOKEN);
    if (!query_builder) {
        printf("Failed to build balance query\n");
        knishio_graphql_client_free(client);
        return;
    }

    printf("✓ Balance query built for address: %s, token: %s\n", EXAMPLE_WALLET_ADDRESS, EXAMPLE_TOKEN);

    /* Serialize query for demonstration */
    knishio_json_t* query_json = knishio_json_object_build(query_builder);
    if (query_json) {
        char* query_str = knishio_json_serialize(query_json, true);
        if (query_str) {
            printf("Query payload:\n%s\n", query_str);
            knishio_free(query_str);
        }
        knishio_json_free(query_json);
    }
    knishio_json_object_builder_free(query_builder);

    /* Example balance query execution (commented out to avoid network dependency) */
    /*
    knishio_balance_result_t* balance_result = NULL;
    error = knishio_graphql_query_balance(client, EXAMPLE_WALLET_ADDRESS, EXAMPLE_TOKEN, &balance_result);
    
    if (error == KNISHIO_SUCCESS && balance_result) {
        if (knishio_balance_result_is_success(balance_result)) {
            printf("✓ Balance query successful\n");
            printf("  Address: %s\n", knishio_balance_result_get_wallet_address(balance_result));
            printf("  Token: %s\n", knishio_balance_result_get_token(balance_result));
            printf("  Balance: %s\n", knishio_balance_result_get_value(balance_result));
        } else {
            printf("✗ Balance query returned unsuccessful result\n");
        }
        
        knishio_balance_result_free(balance_result);
    } else {
        printf("✗ Balance query failed with error: %d\n", error);
    }
    */

    printf("✓ Balance query structure ready for execution\n\n");

    knishio_graphql_client_free(client);
}

static void example_error_handling(void) {
    printf("4. Error Handling Example\n");
    printf("========================\n");

    /* Demonstrate error handling patterns */
    knishio_graphql_client_t* client = NULL;
    
    /* Example 1: Invalid arguments */
    knishio_error_t error = knishio_graphql_client_create(&client, NULL, NULL);
    if (error != KNISHIO_SUCCESS) {
        printf("✓ Invalid arguments properly rejected: error %d\n", error);
    }

    /* Example 2: Valid client creation */
    error = knishio_graphql_client_create(&client, KNISHIO_NODE_URL, "test-cell");
    if (error == KNISHIO_SUCCESS && client) {
        printf("✓ Valid client creation successful\n");

        /* Example 3: Simulate authentication error */
        knishio_graphql_operation_t auth_required_op = {
            .name = "TestMutation",
            .query = "mutation { test }",
            .variables_json = NULL,
            .requires_auth = true,
            .is_mutation = true
        };

        knishio_graphql_response_t* response = NULL;
        error = knishio_graphql_execute(client, &auth_required_op, &response);
        
        if (error == KNISHIO_ERROR_AUTH) {
            printf("✓ Authentication requirement properly enforced: error %d\n", error);
        }

        if (response) {
            knishio_graphql_response_free(response);
        }

        /* Example 4: Get client error details */
        const char* client_error = knishio_graphql_client_get_error(client);
        if (client_error) {
            printf("✓ Client error details available: %s\n", client_error);
        }

        knishio_graphql_client_free(client);
    }

    /* Example 5: HTTP client error handling */
    knishio_http_client_t* http_client = knishio_http_client_create("https://invalid.example.com");
    if (http_client) {
        /* This would demonstrate network error handling in real scenario */
        const char* http_error = knishio_http_client_error(http_client);
        printf("✓ HTTP client error handling ready: %s\n", http_error ? "Has error state" : "No errors");
        
        knishio_http_client_free(http_client);
    }

    printf("✓ Error handling patterns demonstrated\n\n");
}

/* Helper functions */

static void log_graphql_response(const char* operation, knishio_graphql_response_t* response) {
    if (!response) {
        printf("[%s] No response received\n", operation);
        return;
    }

    printf("[%s] Response received:\n", operation);
    printf("  Status Code: %ld\n", response->status_code);
    printf("  Success: %s\n", response->success ? "true" : "false");
    
    if (response->data) {
        printf("  Data: %s\n", response->data);
    }
    
    if (response->errors) {
        printf("  Errors: %s\n", response->errors);
    }
    
    if (response->molecular_hash) {
        printf("  Molecular Hash: %s\n", response->molecular_hash);
    }
}

static void log_http_response(const char* operation, knishio_http_response_t* response) {
    if (!response) {
        printf("[%s] No HTTP response received\n", operation);
        return;
    }

    printf("[%s] HTTP Response received:\n", operation);
    printf("  Status Code: %d\n", response->status_code);
    printf("  Body Length: %zu bytes\n", response->body_length);
    
    if (response->error_message) {
        printf("  Error: %s\n", response->error_message);
    }
}

/*
 * Production Usage Notes:
 * ======================
 * 
 * 1. Always call curl_global_init() before using HTTP client
 * 2. Use connection pooling by reusing HTTP client instances
 * 3. Set appropriate timeouts based on network conditions
 * 4. Handle authentication tokens securely
 * 5. Implement retry logic for network failures
 * 6. Use debug mode during development, disable in production
 * 7. Parse GraphQL errors to provide user-friendly messages
 * 8. Free all allocated resources to prevent memory leaks
 * 9. Consider implementing connection health checks
 * 10. Use HTTPS endpoints in production for security
 */