/**
 * @file websocket_subscriptions_example.c
 * @brief WebSocket subscriptions example for KnishIO C SDK
 * 
 * Demonstrates real-time GraphQL subscriptions using libwebsockets integration.
 * This example shows how to connect to a KnishIO node and subscribe to real-time
 * updates for wallets, transactions, and other blockchain events.
 */

#include "knishio/knishio.h"
#include "knishio/client.h"
#include "knishio/subscribe/subscriptions.h"
#include "knishio/http.h"
#include "knishio/utils/logging.h"
#include "knishio/error/context.h"
#include "src/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

/* Example configuration */
#define KNISHIO_WS_ENDPOINT "wss://testnet.knishio.io/graphql"
#define KNISHIO_BUNDLE_HASH "example-bundle-hash-for-testing"
#define WALLET_ADDRESS "example-wallet-address"

/* Global state for handling shutdown */
static volatile bool keep_running = true;
static knishio_subscription_handle_t* active_wallet_subscription = NULL;
static knishio_subscription_handle_t* wallet_status_subscription = NULL;

/* Signal handler for graceful shutdown */
static void signal_handler(int sig) {
    (void)sig; /* Suppress unused parameter warning */
    
    printf("\n🛑 Shutting down WebSocket subscriptions gracefully...\n");
    keep_running = false;
}

/* Callback functions for different subscription types */

static void on_active_wallet_data(const char* data, const char* error, void* user_data) {
    (void)user_data; /* Suppress unused parameter warning */
    
    if (error) {
        printf("❌ Active Wallet Error: %s\n", error);
        return;
    }
    
    if (data) {
        time_t now = time(NULL);
        struct tm* local_time = localtime(&now);
        
        printf("[%02d:%02d:%02d] 🏦 Active Wallet Update:\n%s\n\n", 
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec, data);
    }
}

static void on_wallet_status_data(const char* data, const char* error, void* user_data) {
    (void)user_data; /* Suppress unused parameter warning */
    
    if (error) {
        printf("❌ Wallet Status Error: %s\n", error);
        return;
    }
    
    if (data) {
        time_t now = time(NULL);
        struct tm* local_time = localtime(&now);
        
        printf("[%02d:%02d:%02d] 📊 Wallet Status Update:\n%s\n\n", 
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec, data);
    }
}

static void on_active_session_data(const char* data, const char* error, void* user_data) {
    (void)user_data; /* Suppress unused parameter warning */
    
    if (error) {
        printf("❌ Active Session Error: %s\n", error);
        return;
    }
    
    if (data) {
        time_t now = time(NULL);
        struct tm* local_time = localtime(&now);
        
        printf("[%02d:%02d:%02d] 🔄 Session Update:\n%s\n\n", 
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec, data);
    }
}

static void on_create_molecule_data(const char* data, const char* error, void* user_data) {
    (void)user_data; /* Suppress unused parameter warning */
    
    if (error) {
        printf("❌ Molecule Creation Error: %s\n", error);
        return;
    }
    
    if (data) {
        time_t now = time(NULL);
        struct tm* local_time = localtime(&now);
        
        printf("[%02d:%02d:%02d] ⚗️  New Molecule:\n%s\n\n", 
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec, data);
    }
}

/* Print feature availability */
static void print_feature_status(void) {
    printf("🔧 KnishIO C SDK Feature Status:\n");
    printf("├─ SDK Version: %s\n", knishio_version());
    
#if KNISHIO_HAVE_WEBSOCKETS
    printf("├─ WebSocket Support: ✅ ENABLED (libwebsockets)\n");
    printf("├─ Real-time Subscriptions: ✅ ENABLED\n");
    printf("├─ GraphQL Subscriptions: ✅ ENABLED\n");
#else
    printf("├─ WebSocket Support: ❌ DISABLED (stub implementation)\n");
    printf("├─ Real-time Subscriptions: ❌ DISABLED\n");
    printf("├─ GraphQL Subscriptions: ❌ DISABLED\n");
#endif

#if KNISHIO_HAVE_QUANTUM_CRYPTO
    printf("├─ Quantum Crypto: ✅ ENABLED\n");
#else
    printf("├─ Quantum Crypto: ❌ DISABLED\n");
#endif

#if KNISHIO_HAVE_HTTP_CLIENT
    printf("└─ HTTP Client: ✅ ENABLED\n");
#else
    printf("└─ HTTP Client: ❌ DISABLED\n");
#endif

    printf("\n");
}

/* Main example function */
int main(int argc, char* argv[]) {
    printf("🚀 KnishIO WebSocket Subscriptions Example\n");
    printf("==========================================\n\n");
    
    /* Parse command line arguments */
    const char* endpoint = KNISHIO_WS_ENDPOINT;
    const char* bundle_hash = KNISHIO_BUNDLE_HASH;
    const char* wallet_address = WALLET_ADDRESS;
    
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--endpoint=", 11) == 0) {
            endpoint = argv[i] + 11;
        } else if (strncmp(argv[i], "--bundle=", 9) == 0) {
            bundle_hash = argv[i] + 9;
        } else if (strncmp(argv[i], "--wallet=", 9) == 0) {
            wallet_address = argv[i] + 9;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --endpoint=URL    WebSocket endpoint (default: %s)\n", KNISHIO_WS_ENDPOINT);
            printf("  --bundle=HASH     Bundle hash to monitor (default: %s)\n", KNISHIO_BUNDLE_HASH);
            printf("  --wallet=ADDRESS  Wallet address to monitor (default: %s)\n", WALLET_ADDRESS);
            printf("  --help, -h        Show this help message\n");
            return 0;
        }
    }
    
    /* Set up signal handling for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Print feature status */
    print_feature_status();
    
#if !KNISHIO_HAVE_WEBSOCKETS
    printf("⚠️  This example requires libwebsockets support.\n");
    printf("   Please install libwebsockets and rebuild the SDK:\n");
    printf("   • macOS: brew install libwebsockets\n");
    printf("   • Ubuntu/Debian: sudo apt-get install libwebsockets-dev\n");
    printf("   • RHEL/CentOS: sudo yum install libwebsockets-devel\n");
    printf("   Then rebuild: cmake . && make\n\n");
    printf("   Running with stub WebSocket implementation for demonstration...\n\n");
#endif
    
    /* Initialize KnishIO SDK */
    knishio_error_t error = knishio_init();
    if (error != KNISHIO_SUCCESS) {
        printf("❌ Failed to initialize KnishIO SDK: %s\n", knishio_error_message(error));
        return 1;
    }
    
    printf("✅ KnishIO SDK initialized successfully\n");
    
    /* Create GraphQL client */
    knishio_client_config_t client_config = {
        .uri = endpoint,
        .cell_slug = "test-cell"
    };
    
    knishio_client_t* client = NULL;
    error = knishio_client_create(&client, &client_config);
    if (error != KNISHIO_SUCCESS) {
        printf("❌ Failed to create client: %s\n", knishio_error_message(error));
        knishio_cleanup();
        return 1;
    }
    
    printf("✅ GraphQL client created for %s\n", endpoint);
    printf("📡 Setting up real-time subscriptions...\n\n");
    
    /* Set up Active Wallet subscription */
    knishio_subscribe_active_wallet_params_t active_wallet_params = {
        .bundle_hash = bundle_hash
    };
    
    error = knishio_subscribe_active_wallet(
        client,
        &active_wallet_params,
        on_active_wallet_data,
        NULL, /* user_data */
        &active_wallet_subscription
    );
    
    if (error == KNISHIO_SUCCESS) {
        printf("✅ Active Wallet subscription started\n");
        printf("   Monitoring bundle: %s\n", bundle_hash);
    } else {
        printf("⚠️  Active Wallet subscription failed: %s\n", knishio_error_message(error));
    }
    
    /* Set up Wallet Status subscription */
    knishio_subscribe_wallet_status_params_t wallet_status_params = {
        .bundle_hash = bundle_hash,
        .wallet_address = wallet_address
    };
    
    error = knishio_subscribe_wallet_status(
        client,
        &wallet_status_params,
        on_wallet_status_data,
        NULL, /* user_data */
        &wallet_status_subscription
    );
    
    if (error == KNISHIO_SUCCESS) {
        printf("✅ Wallet Status subscription started\n");
        printf("   Monitoring wallet: %s\n", wallet_address);
    } else {
        printf("⚠️  Wallet Status subscription failed: %s\n", knishio_error_message(error));
    }
    
    /* Set up Active Session subscription */
    knishio_subscribe_active_session_params_t session_params = {
        .bundle_hash = bundle_hash,
        .meta_type = "example",
        .meta_id = "websocket-demo"
    };
    
    knishio_subscription_handle_t* session_subscription = NULL;
    error = knishio_subscribe_active_session(
        client,
        &session_params,
        on_active_session_data,
        NULL, /* user_data */
        &session_subscription
    );
    
    if (error == KNISHIO_SUCCESS) {
        printf("✅ Active Session subscription started\n");
    } else {
        printf("⚠️  Active Session subscription failed: %s\n", knishio_error_message(error));
    }
    
    /* Set up Molecule Creation subscription */
    knishio_subscribe_create_molecule_params_t molecule_params = {
        .cell_slug = "test-cell",
        .molecular_hashes = NULL, /* Monitor all molecules */
        .molecular_hashes_count = 0
    };
    
    knishio_subscription_handle_t* molecule_subscription = NULL;
    error = knishio_subscribe_create_molecule(
        client,
        &molecule_params,
        on_create_molecule_data,
        NULL, /* user_data */
        &molecule_subscription
    );
    
    if (error == KNISHIO_SUCCESS) {
        printf("✅ Molecule Creation subscription started\n");
    } else {
        printf("⚠️  Molecule Creation subscription failed: %s\n", knishio_error_message(error));
    }
    
    printf("\n🎧 Listening for real-time updates...\n");
    printf("   Press Ctrl+C to stop\n\n");
    
    /* Main event loop */
    int update_counter = 0;
    while (keep_running) {
        /* Check subscription status periodically */
        if (++update_counter % 30 == 0) { /* Every 30 seconds */
            printf("📈 Subscription Status Report:\n");
            
            if (active_wallet_subscription) {
                bool active = knishio_subscription_is_active(active_wallet_subscription);
                printf("   ├─ Active Wallet: %s\n", active ? "✅ ACTIVE" : "❌ INACTIVE");
                
                if (!active) {
                    const char* error_msg = knishio_subscription_get_error(active_wallet_subscription);
                    if (error_msg) {
                        printf("   │  Error: %s\n", error_msg);
                    }
                }
            }
            
            if (wallet_status_subscription) {
                bool active = knishio_subscription_is_active(wallet_status_subscription);
                printf("   └─ Wallet Status: %s\n", active ? "✅ ACTIVE" : "❌ INACTIVE");
                
                if (!active) {
                    const char* error_msg = knishio_subscription_get_error(wallet_status_subscription);
                    if (error_msg) {
                        printf("      Error: %s\n", error_msg);
                    }
                }
            }
            
            printf("\n");
        }
        
        /* Sleep for 1 second */
        sleep(1);
    }
    
    printf("\n🛑 Cleaning up subscriptions...\n");
    
    /* Clean up subscriptions */
    if (active_wallet_subscription) {
        knishio_subscription_unsubscribe(active_wallet_subscription);
        printf("✅ Active Wallet subscription stopped\n");
    }
    
    if (wallet_status_subscription) {
        knishio_subscription_unsubscribe(wallet_status_subscription);
        printf("✅ Wallet Status subscription stopped\n");
    }
    
    if (session_subscription) {
        knishio_subscription_unsubscribe(session_subscription);
        printf("✅ Active Session subscription stopped\n");
    }
    
    if (molecule_subscription) {
        knishio_subscription_unsubscribe(molecule_subscription);
        printf("✅ Molecule Creation subscription stopped\n");
    }
    
    /* Clean up client and SDK */
    knishio_client_destroy(client);
    knishio_cleanup();
    
    printf("✅ KnishIO SDK cleanup completed\n");
    printf("👋 WebSocket subscriptions example finished\n");
    
    return 0;
}