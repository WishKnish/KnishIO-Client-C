#ifndef KNISHIO_UTILS_ENCODING_H
#define KNISHIO_UTILS_ENCODING_H

/**
 * @file encoding.h
 * @brief Encoding utilities for KnishIO SDK
 * 
 * Provides base64 and hex encoding/decoding functions for
 * cryptographic operations and OTS fragment compression.
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert hexadecimal string to base64
 * @param hex_str Input hexadecimal string
 * @param base64_output Output base64 string (caller must free)
 * @return true on success, false on failure
 */
bool knishio_hex_to_base64(const char* hex_str, char** base64_output);

/**
 * @brief Convert base64 string to hexadecimal
 * @param base64_str Input base64 string
 * @param hex_output Output hexadecimal string (caller must free)
 * @return true on success, false on failure
 */
bool knishio_base64_to_hex(const char* base64_str, char** hex_output);

/**
 * @brief Encode binary data to base64
 * @param data Input binary data
 * @param data_len Length of input data
 * @param base64_output Output base64 string (caller must free)
 * @return true on success, false on failure
 */
bool knishio_base64_encode(const unsigned char* data, size_t data_len, char** base64_output);

/**
 * @brief Decode base64 string to binary data
 * @param base64_str Input base64 string
 * @param data_output Output binary data (caller must free)
 * @param data_len Output data length
 * @return true on success, false on failure
 */
bool knishio_base64_decode(const char* base64_str, unsigned char** data_output, size_t* data_len);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_UTILS_ENCODING_H */
