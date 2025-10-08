#ifndef KNISHIO_CONFIG_H
#define KNISHIO_CONFIG_H

/**
 * @file config.h
 * @brief KnishIO C Client SDK Configuration Header
 * 
 * Auto-generated configuration file containing build-time settings
 * and feature availability flags for 2025 C17 best practices.
 */

/* Version information */
#define KNISHIO_VERSION_MAJOR 0
#define KNISHIO_VERSION_MINOR 6
#define KNISHIO_VERSION_PATCH 4
#define KNISHIO_VERSION_STRING "0.6.4"

/* Build configuration */
#define KNISHIO_BUILD_TYPE "Release"
#define KNISHIO_SYSTEM_NAME "Darwin"
#define KNISHIO_SYSTEM_PROCESSOR "arm64"

/* Compiler information */
#define KNISHIO_C_COMPILER_ID "AppleClang"
#define KNISHIO_C_COMPILER_VERSION "17.0.0.17000319"
#define KNISHIO_C_STANDARD 17

/* Feature availability flags */
#define HAVE_LIBOQS 1
#define HAVE_LIBWEBSOCKETS 1
#define GMP_FOUND 1
#define CURL_FOUND 1
#define CJSON_FOUND 1

/* Build options */
#define KNISHIO_BUILD_TESTS 1
#define KNISHIO_BUILD_EXAMPLES 1
#define KNISHIO_BUILD_DOCS 0
#define KNISHIO_ENABLE_COVERAGE 0
#define KNISHIO_ENABLE_SANITIZERS 0

/* Crypto library support */
#if HAVE_LIBOQS
#define KNISHIO_HAVE_QUANTUM_CRYPTO 1
#define KNISHIO_HAVE_ML_KEM_768 1
#define KNISHIO_HAVE_XMSS 1
#else
#define KNISHIO_HAVE_QUANTUM_CRYPTO 0
#define KNISHIO_HAVE_ML_KEM_768 0
#define KNISHIO_HAVE_XMSS 0
#endif

/* Mathematical library support */
#if GMP_FOUND
#define KNISHIO_HAVE_GMP 1
#define KNISHIO_HAVE_BIGINT_ARITHMETIC 1
#else
#define KNISHIO_HAVE_GMP 0
#define KNISHIO_HAVE_BIGINT_ARITHMETIC 0
#endif

/* Network library support */
#if CURL_FOUND
#define KNISHIO_HAVE_HTTP_CLIENT 1
#define KNISHIO_HAVE_GRAPHQL_CLIENT 1
#else
#define KNISHIO_HAVE_HTTP_CLIENT 0
#define KNISHIO_HAVE_GRAPHQL_CLIENT 0
#endif

/* JSON library support */
#if CJSON_FOUND
#define KNISHIO_HAVE_JSON_PARSER 1
#define KNISHIO_HAVE_RESPONSE_PARSING 1
#else
#define KNISHIO_HAVE_JSON_PARSER 0
#define KNISHIO_HAVE_RESPONSE_PARSING 0
#endif

/* WebSocket library support */
#if HAVE_LIBWEBSOCKETS
#define KNISHIO_HAVE_WEBSOCKETS 1
#define KNISHIO_HAVE_REALTIME_SUBSCRIPTIONS 1
#define KNISHIO_HAVE_GRAPHQL_SUBSCRIPTIONS 1
#else
#define KNISHIO_HAVE_WEBSOCKETS 0
#define KNISHIO_HAVE_REALTIME_SUBSCRIPTIONS 0
#define KNISHIO_HAVE_GRAPHQL_SUBSCRIPTIONS 0
#endif

/* Security features (2025 C17 best practices) */
#ifdef __STDC_VERSION__
#if __STDC_VERSION__ >= 201710L
#define KNISHIO_HAVE_C17_FEATURES 1
#define KNISHIO_HAVE_STATIC_ASSERTIONS 1
#else
#define KNISHIO_HAVE_C17_FEATURES 0
#define KNISHIO_HAVE_STATIC_ASSERTIONS 0
#endif
#endif

#ifdef _FORTIFY_SOURCE
#define KNISHIO_HAVE_BUFFER_PROTECTION 1
#else
#define KNISHIO_HAVE_BUFFER_PROTECTION 0
#endif

/* Performance features */
#define KNISHIO_PERFORMANCE_TARGET_INIT_MS 50  /* <50ms initialization per Implementation Guide */

/* Platform-specific features */
#ifdef __linux__
#define KNISHIO_PLATFORM_LINUX 1
#else
#define KNISHIO_PLATFORM_LINUX 0
#endif

#ifdef __APPLE__
#define KNISHIO_PLATFORM_MACOS 1
#else
#define KNISHIO_PLATFORM_MACOS 0
#endif

#ifdef _WIN32
#define KNISHIO_PLATFORM_WINDOWS 1
#else
#define KNISHIO_PLATFORM_WINDOWS 0
#endif

/* Memory management features */
#define KNISHIO_DEFAULT_ATOM_CAPACITY 8
#define KNISHIO_MAX_ATOMS_PER_MOLECULE 256
#define KNISHIO_MEMORY_ALIGNMENT 16

/* Cryptographic constants */
#define KNISHIO_SHAKE256_DEFAULT_OUTPUT_BITS 256
#define KNISHIO_MOLECULAR_HASH_LENGTH_BITS 256
#define KNISHIO_MOLECULAR_HASH_LENGTH_CHARS 64

/* OTS signature configuration */
#define KNISHIO_OTS_KEY_CHUNKS 16
#define KNISHIO_OTS_CHUNK_LENGTH 128
#define KNISHIO_OTS_TOTAL_KEY_LENGTH 2048
#define KNISHIO_OTS_MAX_HASH_ROUNDS 16

/* Debugging and development */
#ifdef DEBUG
#define KNISHIO_DEBUG_MODE 1
#else
#define KNISHIO_DEBUG_MODE 0
#endif

#if defined(NDEBUG) || !defined(DEBUG)
#define KNISHIO_RELEASE_MODE 1
#else
#define KNISHIO_RELEASE_MODE 0
#endif

/* Static assertions for critical constants (C17 feature) */
#if KNISHIO_HAVE_C17_FEATURES
_Static_assert(KNISHIO_OTS_KEY_CHUNKS == 16, 
    "OTS key chunks must be 16 as per SDK Implementation Guide");
_Static_assert(KNISHIO_OTS_CHUNK_LENGTH == 128, 
    "OTS chunk length must be 128 characters as per SDK Implementation Guide");
_Static_assert(KNISHIO_OTS_KEY_CHUNKS * KNISHIO_OTS_CHUNK_LENGTH == KNISHIO_OTS_TOTAL_KEY_LENGTH, 
    "OTS total key length must equal chunks * chunk_length");
_Static_assert(KNISHIO_MOLECULAR_HASH_LENGTH_CHARS == 64, 
    "Molecular hash must be 64 hex characters (256 bits)");
#endif

/* Feature compatibility macros */
#define KNISHIO_FEATURE_AVAILABLE(feature) (KNISHIO_HAVE_##feature)

/* SDK Implementation Guide compliance markers */
#define KNISHIO_IMPL_GUIDE_COMPLIANT 1
#define KNISHIO_QUANTUM_RESISTANT 1
#define KNISHIO_POST_BLOCKCHAIN_DLT 1

/* Build timestamp (for debugging and version tracking) */
#define KNISHIO_BUILD_TIMESTAMP "2025-10-05 18:12:51 UTC"

#endif /* KNISHIO_CONFIG_H */
