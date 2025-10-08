/**
 * @file fingerprint.c
 * @brief STUB implementation of device fingerprinting for KnishIO C SDK
 * 
 * This is a minimal stub implementation to allow SDK compilation.
 * Returns mock data for all fingerprint operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "knishio/fingerprint.h"
#include "knishio/wallet.h"
#include "knishio/crypto/shake256.h"

/* Get device fingerprint - returns a deterministic mock fingerprint */
knishio_error_t knishio_get_fingerprint(char** fingerprint) {
    if (!fingerprint) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    /* Return a consistent mock fingerprint for testing */
    *fingerprint = malloc(65);
    if (!*fingerprint) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    strcpy(*fingerprint, "mock1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab");
    return KNISHIO_SUCCESS;
}

/* Get fingerprint data - returns mock data */
knishio_error_t knishio_get_fingerprint_data(knishio_fingerprint_data_t** data) {
    if (!data) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    knishio_fingerprint_data_t* fp_data = calloc(1, sizeof(knishio_fingerprint_data_t));
    if (!fp_data) {
        return KNISHIO_ERROR_MEMORY;
    }

    /* Populate with mock data */
    fp_data->hostname = strdup("mock-hostname");
    fp_data->cpu_model = strdup("Mock CPU");
    fp_data->os_name = strdup("MockOS");
    fp_data->os_version = strdup("1.0.0");
    fp_data->platform = strdup("mock");
    fp_data->architecture = strdup("x86_64");
    fp_data->memory_total = 8589934592; /* 8GB */
    fp_data->cpu_cores = 4;
    fp_data->timezone = strdup("UTC");
    fp_data->locale = strdup("en_US.UTF-8");
    fp_data->mac_address = strdup("00:00:00:00:00:00");
    fp_data->disk_serial = strdup("MOCK123456");

    *data = fp_data;
    return KNISHIO_SUCCESS;
}

/* Generate fingerprint from data */
knishio_error_t knishio_generate_fingerprint_from_data(
    const knishio_fingerprint_data_t* data,
    char** fingerprint
) {
    if (!data || !fingerprint) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    /* Return mock fingerprint */
    *fingerprint = malloc(65);
    if (!*fingerprint) {
        return KNISHIO_ERROR_MEMORY;
    }
    
    strcpy(*fingerprint, "mock1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab");
    return KNISHIO_SUCCESS;
}

/* Create guest wallet from fingerprint */
knishio_error_t knishio_create_guest_wallet_from_fingerprint(
    const char* fingerprint,
    knishio_wallet_t** wallet
) {
    if (!fingerprint || !wallet) {
        return KNISHIO_ERROR_NULL_POINTER;
    }

    /* Use fingerprint as secret for deterministic wallet generation */
    /* knishio_wallet_create takes (wallet, seed, token, position) */
    bool success = knishio_wallet_create(wallet, fingerprint, "AUTH", NULL);
    
    return success ? KNISHIO_SUCCESS : KNISHIO_ERROR_WALLET_CREDENTIAL;
}

/* Clean up fingerprint data */
void knishio_fingerprint_data_cleanup(knishio_fingerprint_data_t* data) {
    if (!data) return;

    if (data->hostname) free(data->hostname);
    if (data->cpu_model) free(data->cpu_model);
    if (data->os_name) free(data->os_name);
    if (data->os_version) free(data->os_version);
    if (data->platform) free(data->platform);
    if (data->architecture) free(data->architecture);
    if (data->timezone) free(data->timezone);
    if (data->locale) free(data->locale);
    if (data->mac_address) free(data->mac_address);
    if (data->disk_serial) free(data->disk_serial);

    free(data);
}

/* Platform-specific implementations - all return mock data */

knishio_error_t knishio_get_hostname(char** hostname) {
    if (!hostname) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *hostname = strdup("mock-hostname");
    return *hostname ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
}

knishio_error_t knishio_get_cpu_model(char** cpu_model) {
    if (!cpu_model) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *cpu_model = strdup("Mock CPU");
    return *cpu_model ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
}

knishio_error_t knishio_get_os_info(char** os_name, char** os_version) {
    if (!os_name || !os_version) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *os_name = strdup("MockOS");
    *os_version = strdup("1.0.0");
    return (*os_name && *os_version) ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
}

knishio_error_t knishio_get_memory_info(size_t* total_memory) {
    if (!total_memory) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *total_memory = 8589934592; /* 8GB */
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_get_cpu_cores(size_t* core_count) {
    if (!core_count) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *core_count = 4;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_get_timezone(char** timezone) {
    if (!timezone) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *timezone = strdup("UTC");
    return *timezone ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
}

knishio_error_t knishio_get_mac_address(char** mac_address) {
    if (!mac_address) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *mac_address = strdup("00:00:00:00:00:00");
    return *mac_address ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
}

knishio_error_t knishio_get_disk_serial(char** disk_serial) {
    if (!disk_serial) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    *disk_serial = strdup("MOCK123456");
    return *disk_serial ? KNISHIO_SUCCESS : KNISHIO_ERROR_MEMORY;
}