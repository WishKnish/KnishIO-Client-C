#ifndef KNISHIO_FINGERPRINT_H
#define KNISHIO_FINGERPRINT_H

/**
 * @file fingerprint.h
 * @brief Device fingerprinting for KnishIO C SDK
 * 
 * Provides C equivalent of @thumbmarkjs/thumbmarkjs functionality
 * for generating deterministic device fingerprints for guest authentication.
 * Ensures consistent fingerprints across program runs on same system.
 */

#include <stddef.h>
#include <stdbool.h>
#include "knishio/error/context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct knishio_wallet knishio_wallet_t;

/**
 * @brief Device fingerprint data structure
 * Contains system information used for fingerprint generation
 */
typedef struct {
    char* hostname;                 /**< System hostname */
    char* cpu_model;                /**< CPU model string */
    char* os_name;                  /**< Operating system name */
    char* os_version;               /**< Operating system version */
    char* platform;                 /**< System platform */
    char* architecture;             /**< System architecture */
    size_t memory_total;            /**< Total system memory (bytes) */
    size_t cpu_cores;               /**< Number of CPU cores */
    char* timezone;                 /**< System timezone */
    char* locale;                   /**< System locale */
    char* mac_address;              /**< Primary network MAC address */
    char* disk_serial;              /**< Primary disk serial number */
} knishio_fingerprint_data_t;

/**
 * @brief Get device fingerprint
 * Equivalent to JavaScript: getFingerprint() from @thumbmarkjs/thumbmarkjs
 * Generates deterministic fingerprint based on system hardware/software
 * 
 * @param fingerprint Output fingerprint string (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_fingerprint(char** fingerprint);

/**
 * @brief Get fingerprint data
 * Equivalent to JavaScript: getFingerprintData() from @thumbmarkjs/thumbmarkjs
 * Collects raw system information used for fingerprint generation
 * 
 * @param data Output fingerprint data structure (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_fingerprint_data(knishio_fingerprint_data_t** data);

/**
 * @brief Generate fingerprint from data
 * Creates deterministic hash from fingerprint data
 * Uses same algorithm as @thumbmarkjs/thumbmarkjs
 * 
 * @param data Fingerprint data
 * @param fingerprint Output fingerprint string (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_generate_fingerprint_from_data(
    const knishio_fingerprint_data_t* data,
    char** fingerprint
);

/**
 * @brief Create guest wallet from fingerprint
 * Equivalent to JavaScript SDK guest wallet creation process
 * Uses fingerprint as deterministic seed for wallet generation
 * 
 * @param fingerprint Device fingerprint string
 * @param wallet Output wallet (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_create_guest_wallet_from_fingerprint(
    const char* fingerprint,
    knishio_wallet_t** wallet
);

/**
 * @brief Clean up fingerprint data
 * Frees all allocated memory in fingerprint data structure
 * 
 * @param data Fingerprint data to clean up
 */
void knishio_fingerprint_data_cleanup(knishio_fingerprint_data_t* data);

/* Platform-specific system information functions */

/**
 * @brief Get system hostname
 * @param hostname Output hostname (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_hostname(char** hostname);

/**
 * @brief Get CPU model information
 * @param cpu_model Output CPU model (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_cpu_model(char** cpu_model);

/**
 * @brief Get operating system information
 * @param os_name Output OS name (allocated, must be freed)
 * @param os_version Output OS version (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_os_info(char** os_name, char** os_version);

/**
 * @brief Get system memory information
 * @param total_memory Output total memory in bytes
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_memory_info(size_t* total_memory);

/**
 * @brief Get CPU core count
 * @param core_count Output number of CPU cores
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_cpu_cores(size_t* core_count);

/**
 * @brief Get system timezone
 * @param timezone Output timezone string (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_timezone(char** timezone);

/**
 * @brief Get primary network MAC address
 * @param mac_address Output MAC address (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_mac_address(char** mac_address);

/**
 * @brief Get primary disk serial number
 * @param disk_serial Output disk serial (allocated, must be freed)
 * @return KNISHIO_SUCCESS on success, error code on failure
 */
knishio_error_t knishio_get_disk_serial(char** disk_serial);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_FINGERPRINT_H */