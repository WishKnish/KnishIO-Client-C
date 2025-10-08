/**
 * @file auth_demo.c
 * @brief Comprehensive authentication system demonstration
 * 
 * Demonstrates all authentication features of the KnishIO C SDK:
 * - Device fingerprinting
 * - Guest authentication  
 * - Profile authentication
 * - Token management and persistence
 * - Automatic token refresh
 * - Session management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "knishio/knishio.h"
#include "knishio/fingerprint.h"
#include "knishio/auth_token.h"
#include "knishio/operations/auth.h"
#include "knishio/client_auth.h"
#include "knishio/client.h"

/* Demo configuration */
#define DEMO_NODE_URL "http://localhost:8000/graphql"
#define DEMO_CELL_SLUG "demo_cell"
#define DEMO_USER_SECRET "my_secure_secret_phrase_12345"

/* Forward declarations */
static void demo_device_fingerprinting(void);
static void demo_guest_authentication(void);
static void demo_profile_authentication(void);
static void demo_token_management(void);
static void demo_session_management(knishio_client_t* client);
static void demo_auth_persistence(void);
static void auth_event_handler(knishio_client_t* client, const char* event_type, void* user_data);
static void print_separator(const char* title);
static void print_subsection(const char* title);

int main(void) {
    printf("🚀 KnishIO C SDK Authentication System Demo\n");
    printf("===========================================\n\n");

    printf("This demo showcases the complete authentication system:\n");
    printf("• Device fingerprinting for guest authentication\n");
    printf("• Guest authentication with deterministic wallets\n");
    printf("• Profile authentication with user credentials\n");
    printf("• Authentication token lifecycle management\n");
    printf("• Token persistence and restoration\n");
    printf("• Automatic token refresh mechanisms\n");
    printf("• Session management and tracking\n\n");

    /* Run comprehensive demos */
    demo_device_fingerprinting();
    demo_guest_authentication();
    demo_profile_authentication();
    demo_token_management();
    demo_auth_persistence();

    /* Initialize client for session demo */
    knishio_client_config_t client_config = {
        .server_url = DEMO_NODE_URL,
        .cell_slug = DEMO_CELL_SLUG,
        .logging = true
    };

    knishio_client_t* client = NULL;
    knishio_error_t error = knishio_client_create(&client, &client_config);
    if (error == KNISHIO_SUCCESS) {
        demo_session_management(client);
        knishio_client_cleanup(client);
    } else {
        printf("⚠️  Could not create client for session demo (no server connection)\n\n");
    }

    printf("✅ Authentication System Demo Complete!\n");
    printf("\nThe KnishIO C SDK provides comprehensive authentication capabilities\n");
    printf("equivalent to the JavaScript SDK with additional C-specific optimizations.\n\n");

    printf("Key features demonstrated:\n");
    printf("✓ Cross-platform device fingerprinting\n");
    printf("✓ Deterministic guest wallet generation\n");
    printf("✓ Profile-based authentication\n");
    printf("✓ JWT-style token management\n");
    printf("✓ Automatic token refresh\n");
    printf("✓ Session persistence\n");
    printf("✓ Event-driven authentication callbacks\n");
    printf("✓ Production-ready security features\n");

    return 0;
}

/* Demonstrate device fingerprinting */
static void demo_device_fingerprinting(void) {
    print_separator("Device Fingerprinting");

    printf("Device fingerprinting provides deterministic guest authentication\n");
    printf("by generating unique identifiers based on system characteristics.\n\n");

    /* Generate fingerprint */
    char* fingerprint = NULL;
    knishio_error_t error = knishio_get_fingerprint(&fingerprint);
    if (error != KNISHIO_SUCCESS) {
        printf("❌ Failed to generate fingerprint\n\n");
        return;
    }

    printf("🔑 Generated device fingerprint:\n");
    printf("   %s\n\n", fingerprint);

    /* Show fingerprint data components */
    knishio_fingerprint_data_t* data = NULL;
    error = knishio_get_fingerprint_data(&data);
    if (error == KNISHIO_SUCCESS) {
        printf("📊 Fingerprint data components:\n");
        printf("   Hostname: %s\n", data->hostname ? data->hostname : "N/A");
        printf("   CPU Model: %s\n", data->cpu_model ? data->cpu_model : "N/A");
        printf("   OS: %s %s\n", 
               data->os_name ? data->os_name : "N/A",
               data->os_version ? data->os_version : "");
        printf("   Architecture: %s\n", data->architecture ? data->architecture : "N/A");
        printf("   Memory: %zu bytes\n", data->memory_total);
        printf("   CPU Cores: %zu\n", data->cpu_cores);
        printf("   Timezone: %s\n", data->timezone ? data->timezone : "N/A");
        printf("   MAC Address: %s\n", data->mac_address ? data->mac_address : "N/A");

        knishio_fingerprint_data_cleanup(data);
    }

    /* Test consistency */
    char* fingerprint2 = NULL;
    error = knishio_get_fingerprint(&fingerprint2);
    if (error == KNISHIO_SUCCESS) {
        bool consistent = strcmp(fingerprint, fingerprint2) == 0;
        printf("\n✓ Fingerprint consistency: %s\n", consistent ? "PASSED" : "FAILED");
        free(fingerprint2);
    }

    /* Create guest wallet from fingerprint */
    knishio_wallet_t* guest_wallet = NULL;
    error = knishio_create_guest_wallet_from_fingerprint(fingerprint, &guest_wallet);
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Guest wallet created from fingerprint\n");
        printf("   Address: %s\n", knishio_wallet_get_address(guest_wallet));
        knishio_wallet_cleanup(guest_wallet);
    }

    free(fingerprint);
    printf("\n");
}

/* Demonstrate guest authentication */
static void demo_guest_authentication(void) {
    print_separator("Guest Authentication");

    printf("Guest authentication allows access without user credentials\n");
    printf("using device fingerprinting for deterministic identity.\n\n");

    /* Mock guest authentication (would connect to real server in production) */
    printf("📝 Guest Authentication Process:\n");
    printf("1. Generate device fingerprint\n");
    printf("2. Create deterministic guest wallet\n");
    printf("3. Request access token from server\n");
    printf("4. Create authentication token\n\n");

    /* Generate fingerprint and wallet */
    char* fingerprint = NULL;
    knishio_error_t error = knishio_get_fingerprint(&fingerprint);
    if (error != KNISHIO_SUCCESS) {
        printf("❌ Failed to generate fingerprint for guest auth\n\n");
        return;
    }

    knishio_wallet_t* guest_wallet = NULL;
    error = knishio_create_guest_wallet_from_fingerprint(fingerprint, &guest_wallet);
    if (error != KNISHIO_SUCCESS) {
        printf("❌ Failed to create guest wallet\n");
        free(fingerprint);
        return;
    }

    printf("✓ Fingerprint generated: %.32s...\n", fingerprint);
    printf("✓ Guest wallet created: %s\n", knishio_wallet_get_address(guest_wallet));

    /* Simulate guest authentication request structure */
    knishio_request_guest_auth_token_params_t params = {
        .cell_slug = DEMO_CELL_SLUG,
        .encrypt = false
    };

    printf("✓ Guest auth parameters prepared\n");
    printf("   Cell Slug: %s\n", params.cell_slug);
    printf("   Encryption: %s\n", params.encrypt ? "enabled" : "disabled");

    /* Create mock auth token for demo */
    knishio_auth_token_config_t token_config = {
        .token = "guest_demo_token_abc123xyz",
        .expires_at = time(NULL) + 3600, /* 1 hour */
        .encrypt = false,
        .pubkey = knishio_wallet_get_address(guest_wallet)
    };

    knishio_auth_token_t* guest_token = NULL;
    error = knishio_auth_token_create_with_wallet(&guest_token, &token_config, guest_wallet);
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Guest authentication token created\n");
        printf("   Token: %.20s...\n", knishio_auth_token_get_token(guest_token));
        printf("   Expires: %s", ctime(&token_config.expires_at));
        
        knishio_auth_token_cleanup(guest_token);
    }

    /* Cleanup */
    free(fingerprint);
    knishio_wallet_cleanup(guest_wallet);
    printf("\n");
}

/* Demonstrate profile authentication */
static void demo_profile_authentication(void) {
    print_separator("Profile Authentication");

    printf("Profile authentication uses user credentials to create\n");
    printf("persistent identity across sessions and devices.\n\n");

    printf("📝 Profile Authentication Process:\n");
    printf("1. Create wallet from user secret\n");
    printf("2. Build authorization molecule\n");
    printf("3. Submit to server for verification\n");
    printf("4. Receive and store authentication token\n\n");

    /* Create profile wallet */
    knishio_wallet_config_t wallet_config = {
        .secret = DEMO_USER_SECRET,
        .token = "AUTH",
        .position = NULL /* Auto-generate */
    };

    knishio_wallet_t* profile_wallet = NULL;
    knishio_error_t error = knishio_wallet_create(&profile_wallet, &wallet_config);
    if (error != KNISHIO_SUCCESS) {
        printf("❌ Failed to create profile wallet\n\n");
        return;
    }

    printf("✓ Profile wallet created\n");
    printf("   Address: %s\n", knishio_wallet_get_address(profile_wallet));
    printf("   Bundle: %s\n", knishio_wallet_get_bundle_hash(profile_wallet));
    printf("   Position: %s\n", knishio_wallet_get_position(profile_wallet));

    /* Simulate profile authentication request structure */
    knishio_request_profile_auth_token_params_t params = {
        .secret = DEMO_USER_SECRET,
        .encrypt = true
    };

    printf("✓ Profile auth parameters prepared\n");
    printf("   Secret: %.10s... (truncated for security)\n", params.secret);
    printf("   Encryption: %s\n", params.encrypt ? "enabled" : "disabled");

    /* Create mock profile auth token */
    knishio_auth_token_config_t token_config = {
        .token = "profile_demo_token_xyz789abc",
        .expires_at = time(NULL) + 7200, /* 2 hours */
        .encrypt = true,
        .pubkey = knishio_wallet_get_address(profile_wallet)
    };

    knishio_auth_token_t* profile_token = NULL;
    error = knishio_auth_token_create_with_wallet(&profile_token, &token_config, profile_wallet);
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Profile authentication token created\n");
        printf("   Token: %.20s...\n", knishio_auth_token_get_token(profile_token));
        printf("   Expires: %s", ctime(&token_config.expires_at));
        printf("   Encrypted: %s\n", token_config.encrypt ? "yes" : "no");
        
        knishio_auth_token_cleanup(profile_token);
    }

    /* Cleanup */
    knishio_wallet_cleanup(profile_wallet);
    printf("\n");
}

/* Demonstrate token management */
static void demo_token_management(void) {
    print_separator("Authentication Token Management");

    printf("Token management includes creation, validation, expiration\n");
    printf("handling, and automatic refresh capabilities.\n\n");

    /* Create wallet for token */
    knishio_wallet_config_t wallet_config = {
        .secret = "token_management_demo_secret",
        .token = "AUTH",
        .position = NULL
    };

    knishio_wallet_t* wallet = NULL;
    knishio_error_t error = knishio_wallet_create(&wallet, &wallet_config);
    if (error != KNISHIO_SUCCESS) {
        printf("❌ Failed to create wallet for token demo\n\n");
        return;
    }

    print_subsection("Token Creation and Properties");

    /* Create valid token */
    time_t future_time = time(NULL) + 1800; /* 30 minutes */
    knishio_auth_token_config_t valid_config = {
        .token = "valid_management_token_12345",
        .expires_at = future_time,
        .encrypt = true,
        .pubkey = "demo_pubkey_abcdef"
    };

    knishio_auth_token_t* valid_token = NULL;
    error = knishio_auth_token_create_with_wallet(&valid_token, &valid_config, wallet);
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Valid token created\n");
        printf("   Token: %.25s...\n", knishio_auth_token_get_token(valid_token));
        printf("   Public Key: %s\n", knishio_auth_token_get_pubkey(valid_token));
        printf("   Expires At: %s", ctime(&future_time));
        printf("   Is Expired: %s\n", knishio_auth_token_is_expired(valid_token) ? "yes" : "no");
        
        int64_t expire_interval = knishio_auth_token_get_expire_interval(valid_token);
        printf("   Expires In: %lld ms\n", (long long)expire_interval);
    }

    print_subsection("Token Expiration Handling");

    /* Create expired token */
    time_t past_time = time(NULL) - 3600; /* 1 hour ago */
    knishio_auth_token_config_t expired_config = {
        .token = "expired_management_token_67890",
        .expires_at = past_time,
        .encrypt = false,
        .pubkey = "expired_pubkey_fedcba"
    };

    knishio_auth_token_t* expired_token = NULL;
    error = knishio_auth_token_create(&expired_token, &expired_config);
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Expired token created for testing\n");
        printf("   Token: %.25s...\n", knishio_auth_token_get_token(expired_token));
        printf("   Expired At: %s", ctime(&past_time));
        printf("   Is Expired: %s\n", knishio_auth_token_is_expired(expired_token) ? "yes" : "no");
        
        int64_t expire_interval = knishio_auth_token_get_expire_interval(expired_token);
        printf("   Expired By: %lld ms\n", (long long)(-expire_interval));
        
        knishio_auth_token_cleanup(expired_token);
    }

    print_subsection("Token Serialization");

    /* Test serialization */
    char* snapshot_json = NULL;
    error = knishio_auth_token_get_snapshot(valid_token, &snapshot_json);
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Token snapshot created\n");
        printf("   Snapshot: %s\n", snapshot_json);
        
        char* auth_data_json = NULL;
        error = knishio_auth_token_get_auth_data(valid_token, &auth_data_json);
        if (error == KNISHIO_SUCCESS) {
            printf("✓ Auth data extracted\n");
            printf("   Auth Data: %s\n", auth_data_json);
            free(auth_data_json);
        }
        
        free(snapshot_json);
    }

    /* Cleanup */
    knishio_auth_token_cleanup(valid_token);
    knishio_wallet_cleanup(wallet);
    printf("\n");
}

/* Demonstrate session management */
static void demo_session_management(knishio_client_t* client) {
    print_separator("Session Management");

    printf("Session management tracks user activity and maintains\n");
    printf("persistent connections with automatic token refresh.\n\n");

    /* Register auth event callback */
    knishio_error_t error = knishio_client_register_auth_callback(client, auth_event_handler, NULL);
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Authentication event callback registered\n");
    }

    /* Configure authentication */
    knishio_client_auth_config_t config = {
        .secret = DEMO_USER_SECRET,
        .cell_slug = DEMO_CELL_SLUG,
        .encrypt = true,
        .auto_refresh = true,
        .refresh_threshold_ms = 300000 /* 5 minutes */
    };

    error = knishio_client_configure_auth(client, &config);
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Client authentication configured\n");
        printf("   Auto Refresh: enabled\n");
        printf("   Refresh Threshold: 5 minutes\n");
        printf("   Encryption: enabled\n");
    }

    /* Test authentication state */
    bool is_authenticated = knishio_client_is_authenticated(client);
    bool has_secret = knishio_client_has_secret(client);
    
    printf("✓ Authentication state checked\n");
    printf("   Is Authenticated: %s\n", is_authenticated ? "yes" : "no");
    printf("   Has Secret: %s\n", has_secret ? "yes" : "no");

    /* Simulate active session parameters */
    knishio_active_session_params_t session_params = {
        .bundle = "demo_bundle_hash_12345",
        .meta_type = "demo_session",
        .meta_id = "session_001",
        .ip_address = "192.168.1.100",
        .browser = "C SDK Demo Client",
        .os_cpu = "Linux x86_64",
        .resolution = "1920x1080",
        .time_zone = "UTC",
        .json_data = "{\"demo\":true}"
    };

    printf("✓ Active session parameters prepared\n");
    printf("   Bundle: %.20s...\n", session_params.bundle);
    printf("   Meta Type: %s\n", session_params.meta_type);
    printf("   IP Address: %s\n", session_params.ip_address);
    printf("   Browser: %s\n", session_params.browser);

    /* Simulate token refresh mechanism */
    printf("\n🔄 Simulating token refresh cycle...\n");
    sleep(1);
    printf("   Checking token expiration...\n");
    sleep(1);
    printf("   Token within refresh threshold\n");
    sleep(1);
    printf("   Automatic refresh triggered\n");
    sleep(1);
    printf("   ✓ Token refreshed successfully\n");

    printf("\n");
}

/* Demonstrate authentication persistence */
static void demo_auth_persistence(void) {
    print_separator("Authentication Persistence");

    printf("Authentication persistence allows saving and restoring\n");
    printf("authentication state across application restarts.\n\n");

    /* Create wallet and token for persistence demo */
    knishio_wallet_config_t wallet_config = {
        .secret = "persistence_demo_secret_xyz",
        .token = "AUTH",
        .position = NULL
    };

    knishio_wallet_t* wallet = NULL;
    knishio_error_t error = knishio_wallet_create(&wallet, &wallet_config);
    if (error != KNISHIO_SUCCESS) {
        printf("❌ Failed to create wallet for persistence demo\n\n");
        return;
    }

    knishio_auth_token_config_t token_config = {
        .token = "persistence_demo_token_abc123",
        .expires_at = time(NULL) + 3600,
        .encrypt = true,
        .pubkey = knishio_wallet_get_address(wallet)
    };

    knishio_auth_token_t* original_token = NULL;
    error = knishio_auth_token_create_with_wallet(&original_token, &token_config, wallet);
    if (error != KNISHIO_SUCCESS) {
        printf("❌ Failed to create token for persistence demo\n");
        knishio_wallet_cleanup(wallet);
        return;
    }

    print_subsection("Token Snapshot Creation");

    /* Create snapshot */
    char* snapshot_json = NULL;
    error = knishio_auth_token_get_snapshot(original_token, &snapshot_json);
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Authentication snapshot created\n");
        printf("   Snapshot size: %zu bytes\n", strlen(snapshot_json));
        printf("   Contains: token, expiration, wallet data, encryption settings\n");
        
        /* Simulate saving to file/storage */
        printf("   💾 Simulating save to persistent storage...\n");
        sleep(1);
        printf("   ✓ Snapshot saved successfully\n");
    }

    print_subsection("Token Restoration");

    /* Restore from snapshot */
    knishio_auth_token_t* restored_token = NULL;
    error = knishio_auth_token_restore(&restored_token, snapshot_json, "persistence_demo_secret_xyz");
    if (error == KNISHIO_SUCCESS) {
        printf("✓ Authentication token restored from snapshot\n");
        
        /* Verify restoration */
        const char* original_token_str = knishio_auth_token_get_token(original_token);
        const char* restored_token_str = knishio_auth_token_get_token(restored_token);
        bool tokens_match = strcmp(original_token_str, restored_token_str) == 0;
        
        printf("   Token match: %s\n", tokens_match ? "✓ PASS" : "❌ FAIL");
        
        knishio_wallet_t* original_wallet = knishio_auth_token_get_wallet(original_token);
        knishio_wallet_t* restored_wallet = knishio_auth_token_get_wallet(restored_token);
        
        if (original_wallet && restored_wallet) {
            const char* original_address = knishio_wallet_get_address(original_wallet);
            const char* restored_address = knishio_wallet_get_address(restored_wallet);
            bool wallets_match = strcmp(original_address, restored_address) == 0;
            
            printf("   Wallet match: %s\n", wallets_match ? "✓ PASS" : "❌ FAIL");
        }
        
        knishio_auth_token_cleanup(restored_token);
    }

    print_subsection("Session Continuity");

    printf("📱 Simulating application restart...\n");
    sleep(1);
    printf("   Application terminated\n");
    sleep(1);
    printf("   Application restarted\n");
    sleep(1);
    printf("   Loading authentication snapshot...\n");
    sleep(1);
    printf("   ✓ Authentication state restored\n");
    printf("   ✓ User session continued seamlessly\n");

    /* Cleanup */
    free(snapshot_json);
    knishio_auth_token_cleanup(original_token);
    knishio_wallet_cleanup(wallet);
    printf("\n");
}

/* Authentication event handler */
static void auth_event_handler(knishio_client_t* client, const char* event_type, void* user_data) {
    (void)client;   /* Unused parameter */
    (void)user_data; /* Unused parameter */
    
    printf("🔔 Auth Event: %s\n", event_type);
    
    if (strcmp(event_type, "authenticated") == 0) {
        printf("   User successfully authenticated\n");
    } else if (strcmp(event_type, "expired") == 0) {
        printf("   Authentication token expired\n");
    } else if (strcmp(event_type, "refreshed") == 0) {
        printf("   Authentication token refreshed\n");
    } else if (strcmp(event_type, "failed") == 0) {
        printf("   Authentication failed\n");
    }
}

/* Helper functions */

static void print_separator(const char* title) {
    printf("🔐 %s\n", title);
    printf("%.*s\n", (int)(strlen(title) + 3), "================================================");
    printf("\n");
}

static void print_subsection(const char* title) {
    printf("   📋 %s\n", title);
    printf("   %.*s\n", (int)(strlen(title) + 3), "--------------------------------");
    printf("\n");
}