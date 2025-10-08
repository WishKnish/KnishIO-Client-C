/**
 * @file graphql_operations_example.c
 * @brief Comprehensive example of GraphQL operations usage
 * 
 * Demonstrates all GraphQL operations available in the KnishIO C SDK:
 * - Query operations for balance, wallet, token, and atom data
 * - Mutation operations for transfers, token creation, and metadata
 * - Subscription operations for real-time updates
 * - High-level transaction patterns
 * - Authentication and session management
 * 
 * This example shows JavaScript SDK equivalent functionality in C.
 */

#include "knishio/operations/graphql_operations.h"
#include "knishio/client.h"
#include "knishio/wallet.h"
#include "knishio/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Configuration */
#define GRAPHQL_ENDPOINT "http://localhost:8080/graphql"
#define WEBSOCKET_ENDPOINT "ws://localhost:8080/graphql"
#define CELL_SLUG "main"
#define USER_SECRET "my_secure_secret_key_12345"

/* Global variables for the example */
static knishio_client_t* client = NULL;
static knishio_graphql_operations_t* ops = NULL;

/* Error handling helper */
static void handle_error(knishio_error_t error, const char* operation) {
    if (error != KNISHIO_SUCCESS) {
        printf("Error in %s: %d\n", operation, error);
        exit(1);
    }
}

/* Callback functions for subscriptions */
static void wallet_update_callback(const char* data, const char* error, void* user_data) {
    if (error) {
        printf("Wallet subscription error: %s\n", error);
    } else {
        printf("📊 Wallet update received:\n%s\n", data);
    }
}

static void molecule_update_callback(const char* data, const char* error, void* user_data) {
    if (error) {
        printf("Molecule subscription error: %s\n", error);
    } else {
        printf("🔬 New molecule proposal:\n%s\n", data);
    }
}

/* Example 1: Basic Query Operations */
static void example_query_operations(void) {
    printf("\n🔍 Example 1: Query Operations\n");
    printf("==============================\n");
    
    // Get current wallet
    knishio_wallet_t* wallet = knishio_client_get_wallet(client);
    const char* wallet_address = knishio_wallet_get_address(wallet);
    
    printf("Wallet address: %s\n", wallet_address);
    
    // Query balance - equivalent to JavaScript: client.queryBalance({ address })
    printf("\n1.1 Querying balance...\n");
    knishio_response_balance_t* balance_response = NULL;
    knishio_error_t result = knishio_operations_query_balance_simple(
        ops, wallet_address, "KNISH", &balance_response);
    handle_error(result, "query balance");
    
    if (balance_response) {
        const char* balance = knishio_response_balance_get_amount(balance_response);
        const char* token = knishio_response_balance_get_token(balance_response);
        printf("Balance: %s %s\n", balance ? balance : "0", token ? token : "KNISH");
        knishio_response_balance_free(balance_response);
    }
    
    // Query wallet list - equivalent to JavaScript: client.queryWalletList({ tokenSlug })
    printf("\n1.2 Querying wallet list...\n");
    knishio_query_wallet_list_params_t wallet_params = {0};
    wallet_params.token_slug = "KNISH";
    
    knishio_response_wallet_list_t* wallet_response = NULL;
    result = knishio_query_wallet_list(
        knishio_operations_get_graphql_client(ops), &wallet_params, &wallet_response);
    handle_error(result, "query wallet list");
    
    if (wallet_response) {
        printf("Wallet list query completed successfully\n");
        knishio_response_wallet_list_free(wallet_response);
    }
    
    // Query token information - equivalent to JavaScript: client.queryToken({ slug })
    printf("\n1.3 Querying token information...\n");
    knishio_query_token_params_t token_params = {0};
    token_params.slug = "KNISH";
    token_params.limit = 10;
    
    knishio_response_token_t* token_response = NULL;
    result = knishio_query_token(
        knishio_operations_get_graphql_client(ops), &token_params, &token_response);
    handle_error(result, "query token");
    
    if (token_response) {
        printf("Token information query completed successfully\n");
        knishio_response_token_free(token_response);
    }
    
    // Query atoms - equivalent to JavaScript: client.queryAtom({ isotopes, limit })
    printf("\n1.4 Querying recent atoms...\n");
    const char* isotopes[] = {"V", "M"};
    knishio_query_atom_params_t atom_params = {0};
    atom_params.isotopes = isotopes;
    atom_params.isotope_count = 2;
    atom_params.limit = 5;
    atom_params.latest = true;
    
    knishio_response_atom_t* atom_response = NULL;
    result = knishio_query_atom(
        knishio_operations_get_graphql_client(ops), &atom_params, &atom_response);
    handle_error(result, "query atoms");
    
    if (atom_response) {
        printf("Recent atoms query completed successfully\n");
        knishio_response_atom_free(atom_response);
    }
}

/* Example 2: Authentication */
static void example_authentication(void) {
    printf("\n🔐 Example 2: Authentication\n");
    printf("============================\n");
    
    // Authenticate with profile - equivalent to JavaScript: client.requestAuthorization()
    printf("2.1 Authenticating with profile...\n");
    knishio_response_request_authorization_t* auth_response = NULL;
    knishio_error_t result = knishio_operations_authenticate_profile(
        ops, USER_SECRET, CELL_SLUG, &auth_response);
    handle_error(result, "profile authentication");
    
    if (auth_response) {
        bool success = knishio_response_request_authorization_is_success(auth_response);
        if (success) {
            const char* auth_token = knishio_response_request_authorization_get_token(auth_response);
            printf("✓ Authentication successful!\n");
            printf("Auth token: %.20s...\n", auth_token ? auth_token : "none");
            
            // Set auth token for subsequent operations
            if (auth_token) {
                knishio_graphql_operations_set_auth_token(ops, auth_token);
            }
        } else {
            printf("✗ Authentication failed\n");
        }
        knishio_response_request_authorization_free(auth_response);
    }
    
    // Guest authentication - equivalent to JavaScript: client.requestAuthorizationGuest()
    printf("\n2.2 Authenticating as guest...\n");
    knishio_response_request_authorization_guest_t* guest_response = NULL;
    result = knishio_operations_authenticate_guest(ops, CELL_SLUG, &guest_response);
    handle_error(result, "guest authentication");
    
    if (guest_response) {
        bool success = knishio_response_request_authorization_guest_is_success(guest_response);
        printf("%s Guest authentication %s\n", success ? "✓" : "✗", success ? "successful" : "failed");
        knishio_response_request_authorization_guest_free(guest_response);
    }
}

/* Example 3: Token Operations */
static void example_token_operations(void) {
    printf("\n🪙 Example 3: Token Operations\n");
    printf("==============================\n");
    
    knishio_wallet_t* wallet = knishio_client_get_wallet(client);
    
    // Create a new token - equivalent to JavaScript: client.createToken()
    printf("3.1 Creating a new token...\n");
    knishio_response_create_token_t* create_response = NULL;
    knishio_error_t result = knishio_operations_create_token_simple(
        ops, wallet, "EXAMPLE", "1000000", 
        "{\"name\":\"Example Token\",\"symbol\":\"EXAMPLE\",\"description\":\"A test token\"}", 
        &create_response);
    
    if (result == KNISHIO_SUCCESS && create_response) {
        const char* molecular_hash = knishio_response_create_token_get_hash(create_response);
        printf("✓ Token created successfully!\n");
        printf("Molecular hash: %s\n", molecular_hash ? molecular_hash : "unknown");
        knishio_response_create_token_free(create_response);
    } else {
        printf("✗ Token creation failed or skipped\n");
    }
    
    // Request tokens from faucet - equivalent to JavaScript: client.requestTokens()
    printf("\n3.2 Requesting tokens from faucet...\n");
    knishio_response_request_tokens_t* request_response = NULL;
    result = knishio_operations_request_tokens_simple(
        ops, wallet, "KNISH", "100", &request_response);
    
    if (result == KNISHIO_SUCCESS && request_response) {
        const char* molecular_hash = knishio_response_request_tokens_get_hash(request_response);
        printf("✓ Token request submitted!\n");
        printf("Molecular hash: %s\n", molecular_hash ? molecular_hash : "unknown");
        knishio_response_request_tokens_free(request_response);
    } else {
        printf("✗ Token request failed or skipped\n");
    }
    
    // Check balance after request
    printf("\n3.3 Checking balance after token request...\n");
    char* balance_str = NULL;
    result = knishio_operations_get_balance_string(
        ops, knishio_wallet_get_address(wallet), "KNISH", &balance_str);
    
    if (result == KNISHIO_SUCCESS && balance_str) {
        printf("Current KNISH balance: %s\n", balance_str);
        free(balance_str);
    }
}

/* Example 4: Transfer Operations */
static void example_transfer_operations(void) {
    printf("\n💸 Example 4: Transfer Operations\n");
    printf("==================================\n");
    
    knishio_wallet_t* source_wallet = knishio_client_get_wallet(client);
    
    // Simple transfer - equivalent to JavaScript: client.transferTokens()
    printf("4.1 Simple token transfer...\n");
    knishio_response_transfer_tokens_t* transfer_response = NULL;
    knishio_error_t result = knishio_operations_transfer_tokens_simple(
        ops, source_wallet, 
        "Kk4xB5c2fE7hJ8iK9lM2nO3pQ4rS5tU6vW7xY8zA1B", // Recipient address
        "KNISH", "10", &transfer_response);
    
    if (result == KNISHIO_SUCCESS && transfer_response) {
        const char* molecular_hash = knishio_response_transfer_tokens_get_hash(transfer_response);
        printf("✓ Transfer submitted successfully!\n");
        printf("Molecular hash: %s\n", molecular_hash ? molecular_hash : "unknown");
        knishio_response_transfer_tokens_free(transfer_response);
    } else {
        printf("✗ Transfer failed or skipped\n");
    }
    
    // Multi-party transfer
    printf("\n4.2 Multi-party token transfer...\n");
    knishio_transfer_recipient_t recipients[] = {
        {"Kk4xB5c2fE7hJ8iK9lM2nO3pQ4rS5tU6vW7xY8zA1B", "25", "Payment to Alice"},
        {"Kk4xB5c2fE7hJ8iK9lM2nO3pQ4rS5tU6vW7xY8zA2C", "15", "Payment to Bob"},
        {"Kk4xB5c2fE7hJ8iK9lM2nO3pQ4rS5tU6vW7xY8zA3D", "10", "Payment to Carol"}
    };
    
    knishio_response_transfer_tokens_t* multi_response = NULL;
    result = knishio_operations_transfer_tokens_multi(
        ops, source_wallet, recipients, 3, "KNISH", &multi_response);
    
    if (result == KNISHIO_SUCCESS && multi_response) {
        printf("✓ Multi-party transfer completed!\n");
        printf("Transferred 25+15+10=50 KNISH to 3 recipients\n");
        knishio_response_transfer_tokens_free(multi_response);
    } else {
        printf("✗ Multi-party transfer failed or skipped\n");
    }
    
    // Check if balance is sufficient for another operation
    printf("\n4.3 Checking balance sufficiency...\n");
    bool sufficient = false;
    result = knishio_operations_check_sufficient_balance(
        ops, knishio_wallet_get_address(source_wallet), "KNISH", "100", &sufficient);
    
    if (result == KNISHIO_SUCCESS) {
        printf("Sufficient balance for 100 KNISH: %s\n", sufficient ? "Yes" : "No");
    }
}

/* Example 5: Complex Transaction Patterns */
static void example_complex_transactions(void) {
    printf("\n🔄 Example 5: Complex Transaction Patterns\n");
    printf("===========================================\n");
    
    // Create additional wallets for complex operations
    knishio_wallet_t* party1_wallet = NULL;
    knishio_wallet_t* party2_wallet = NULL;
    
    knishio_error_t result = knishio_client_create_wallet(client, "KNISH", &party1_wallet);
    if (result != KNISHIO_SUCCESS) {
        printf("Could not create party1 wallet for complex transactions\n");
        return;
    }
    
    result = knishio_client_create_wallet(client, "EXAMPLE", &party2_wallet);
    if (result != KNISHIO_SUCCESS) {
        printf("Could not create party2 wallet for complex transactions\n");
        knishio_wallet_free(party1_wallet);
        return;
    }
    
    // Token swap - atomic exchange between parties
    printf("5.1 Atomic token swap...\n");
    knishio_response_transfer_tokens_t* swap_response = NULL;
    result = knishio_operations_swap_tokens(
        ops, party1_wallet, party2_wallet,
        "KNISH", "100",    // Party 1 offers 100 KNISH
        "EXAMPLE", "50",   // Party 2 offers 50 EXAMPLE
        &swap_response);
    
    if (result == KNISHIO_SUCCESS && swap_response) {
        printf("✓ Atomic swap completed!\n");
        printf("Exchanged 100 KNISH ↔ 50 EXAMPLE\n");
        knishio_response_transfer_tokens_free(swap_response);
    } else {
        printf("✗ Atomic swap failed or skipped\n");
    }
    
    // Create metadata
    printf("\n5.2 Creating metadata...\n");
    knishio_response_create_meta_t* meta_response = NULL;
    result = knishio_operations_create_meta_simple(
        ops, party1_wallet, "profile", "user_alice", 
        "{\"name\":\"Alice\",\"email\":\"alice@example.com\",\"verified\":true}",
        &meta_response);
    
    if (result == KNISHIO_SUCCESS && meta_response) {
        printf("✓ Metadata created successfully!\n");
        knishio_response_create_meta_free(meta_response);
    } else {
        printf("✗ Metadata creation failed or skipped\n");
    }
    
    // Clean up wallets
    knishio_wallet_free(party1_wallet);
    knishio_wallet_free(party2_wallet);
}

/* Example 6: Subscription Operations */
static void example_subscription_operations(void) {
    printf("\n📡 Example 6: Subscription Operations\n");
    printf("=====================================\n");
    
    // Configure WebSocket for subscriptions
    knishio_websocket_config_t ws_config = {0};
    ws_config.ws_url = WEBSOCKET_ENDPOINT;
    ws_config.protocol = "graphql-ws";
    ws_config.timeout_ms = 5000;
    ws_config.heartbeat_interval_ms = 30000;
    ws_config.auto_reconnect = true;
    ws_config.max_reconnect_attempts = 3;
    
    knishio_error_t result = knishio_graphql_client_set_websocket_config(
        knishio_operations_get_graphql_client(ops), &ws_config);
    handle_error(result, "WebSocket configuration");
    
    // Subscribe to wallet balance changes
    printf("6.1 Subscribing to wallet balance changes...\n");
    knishio_subscription_handle_t* wallet_handle = NULL;
    result = knishio_operations_subscribe_wallet_balance(
        ops, knishio_client_get_bundle_hash(client), 
        wallet_update_callback, NULL, &wallet_handle);
    
    if (result == KNISHIO_SUCCESS && wallet_handle) {
        printf("✓ Wallet balance subscription active\n");
        printf("Monitoring wallet for balance changes...\n");
        
        // Keep subscription active for demo
        sleep(5);
        
        // Unsubscribe
        knishio_subscription_unsubscribe(wallet_handle);
        printf("Wallet subscription ended\n");
    } else {
        printf("✗ Wallet subscription failed\n");
    }
    
    // Subscribe to molecule proposals
    printf("\n6.2 Subscribing to molecule proposals...\n");
    knishio_subscription_handle_t* molecule_handle = NULL;
    result = knishio_operations_subscribe_molecules(
        ops, CELL_SLUG, molecule_update_callback, NULL, &molecule_handle);
    
    if (result == KNISHIO_SUCCESS && molecule_handle) {
        printf("✓ Molecule subscription active\n");
        printf("Monitoring cell for new molecule proposals...\n");
        
        // Keep subscription active for demo
        sleep(5);
        
        // Unsubscribe
        knishio_subscription_unsubscribe(molecule_handle);
        printf("Molecule subscription ended\n");
    } else {
        printf("✗ Molecule subscription failed\n");
    }
}

/* Example 7: Session Management */
static void example_session_management(void) {
    printf("\n🔐 Example 7: Session Management\n");
    printf("================================\n");
    
    knishio_wallet_t* wallet = knishio_client_get_wallet(client);
    
    // Start active session
    printf("7.1 Starting active session...\n");
    knishio_response_active_session_t* session_response = NULL;
    knishio_error_t result = knishio_operations_start_session(
        ops, wallet, "user_session", "session_123",
        "192.168.1.1", "Mozilla/5.0 (compatible; KnishIO-C-SDK)",
        &session_response);
    
    if (result == KNISHIO_SUCCESS && session_response) {
        printf("✓ Active session started successfully!\n");
        knishio_response_active_session_free(session_response);
    } else {
        printf("✗ Session start failed or skipped\n");
    }
    
    // Query active session
    printf("\n7.2 Querying active session...\n");
    knishio_query_active_session_params_t session_params = {0};
    session_params.bundle_hash = knishio_client_get_bundle_hash(client);
    session_params.meta_type = "user_session";
    session_params.meta_id = "session_123";
    
    knishio_response_active_session_t* query_response = NULL;
    result = knishio_query_active_session(
        knishio_operations_get_graphql_client(ops), &session_params, &query_response);
    
    if (result == KNISHIO_SUCCESS && query_response) {
        printf("✓ Active session query completed\n");
        knishio_response_active_session_free(query_response);
    } else {
        printf("✗ Active session query failed\n");
    }
}

/* Example 8: Raw GraphQL Operations */
static void example_raw_graphql(void) {
    printf("\n⚙️  Example 8: Raw GraphQL Operations\n");
    printf("====================================\n");
    
    // Execute raw GraphQL query
    printf("8.1 Executing raw GraphQL query...\n");
    const char* raw_query = 
        "query {\n"
        "  __schema {\n"
        "    types {\n"
        "      name\n"
        "    }\n"
        "  }\n"
        "}";
    
    char* response = NULL;
    knishio_error_t result = knishio_operations_execute_raw_query(
        ops, raw_query, "{}", &response);
    
    if (result == KNISHIO_SUCCESS && response) {
        printf("✓ Raw query executed successfully\n");
        printf("Response (first 200 chars): %.200s...\n", response);
        free(response);
    } else {
        printf("✗ Raw query failed\n");
    }
    
    // Execute raw GraphQL mutation
    printf("\n8.2 Executing raw GraphQL mutation...\n");
    const char* raw_mutation = 
        "mutation {\n"
        "  __typename\n"
        "}";
    
    result = knishio_operations_execute_raw_mutation(
        ops, raw_mutation, "{}", &response);
    
    if (result == KNISHIO_SUCCESS && response) {
        printf("✓ Raw mutation executed successfully\n");
        printf("Response: %s\n", response);
        free(response);
    } else {
        printf("✗ Raw mutation failed\n");
    }
}

/* Main example function */
int main(int argc, char* argv[]) {
    printf("🚀 KnishIO C SDK GraphQL Operations Example\n");
    printf("============================================\n");
    
    // Initialize client
    printf("Initializing KnishIO client...\n");
    knishio_error_t result = knishio_client_create(&client, USER_SECRET);
    handle_error(result, "client creation");
    
    // Initialize GraphQL operations
    printf("Initializing GraphQL operations...\n");
    result = knishio_graphql_operations_create(&ops, client, GRAPHQL_ENDPOINT, CELL_SLUG);
    handle_error(result, "GraphQL operations creation");
    
    // Enable debug mode
    knishio_graphql_operations_set_debug(ops, true);
    
    printf("✓ Initialization complete!\n");
    printf("Client ready with wallet: %s\n", 
           knishio_wallet_get_address(knishio_client_get_wallet(client)));
    
    // Run all examples
    example_query_operations();
    example_authentication();
    example_token_operations();
    example_transfer_operations();
    example_complex_transactions();
    example_subscription_operations();
    example_session_management();
    example_raw_graphql();
    
    // Cleanup
    printf("\n🧹 Cleaning up...\n");
    if (ops) {
        knishio_graphql_operations_free(ops);
    }
    if (client) {
        knishio_client_free(client);
    }
    
    printf("✓ Example completed successfully!\n");
    printf("\nThis example demonstrated:\n");
    printf("- Query operations (balance, wallet, token, atom)\n");
    printf("- Mutation operations (transfer, create, request)\n");
    printf("- Authentication (profile and guest)\n");
    printf("- Complex transactions (multi-party, atomic swaps)\n");
    printf("- Real-time subscriptions (WebSocket-based)\n");
    printf("- Session management\n");
    printf("- Raw GraphQL operations\n");
    printf("\nAll operations are equivalent to JavaScript SDK functionality!\n");
    
    return 0;
}