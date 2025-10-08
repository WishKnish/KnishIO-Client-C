/**
 * @file base64.c
 * @brief Base64 encoding utilities for KnishIO C SDK
 * 
 * Implements base64 and hex encoding/decoding functions matching
 * JavaScript SDK's encoding behavior for OTS fragment compression.
 */

#include "knishio/utils/encoding.h"
#include "knishio/utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Base64 encoding table */
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Base64 decoding table (-1 for invalid characters) */
static const signed char base64_decode_table[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

bool knishio_base64_encode(const unsigned char* data, size_t data_len, char** base64_output) {
    if (!data || !base64_output) {
        return false;
    }

    /* Calculate output length */
    size_t output_len = 4 * ((data_len + 2) / 3);
    
    /* Allocate output buffer (+1 for null terminator) */
    char* output = knishio_malloc(output_len + 1);
    if (!output) {
        return false;
    }

    size_t i = 0, j = 0;
    unsigned char array3[3], array4[4];
    int k = 0;

    while (data_len--) {
        array3[i++] = *(data++);
        if (i == 3) {
            array4[0] = (array3[0] & 0xfc) >> 2;
            array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
            array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
            array4[3] = array3[2] & 0x3f;

            for (i = 0; i < 4; i++) {
                output[j++] = base64_chars[array4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (k = i; k < 3; k++) {
            array3[k] = '\0';
        }

        array4[0] = (array3[0] & 0xfc) >> 2;
        array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
        array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
        array4[3] = array3[2] & 0x3f;

        for (k = 0; k <= i; k++) {
            output[j++] = base64_chars[array4[k]];
        }

        while (i++ < 3) {
            output[j++] = '=';
        }
    }

    output[j] = '\0';
    *base64_output = output;
    return true;
}

bool knishio_base64_decode(const char* base64_str, unsigned char** data_output, size_t* data_len) {
    if (!base64_str || !data_output || !data_len) {
        return false;
    }

    size_t input_len = strlen(base64_str);
    if (input_len % 4 != 0) {
        return false;
    }

    /* Calculate output length */
    size_t output_len = input_len / 4 * 3;
    if (base64_str[input_len - 1] == '=') output_len--;
    if (base64_str[input_len - 2] == '=') output_len--;

    /* Allocate output buffer */
    unsigned char* output = knishio_malloc(output_len);
    if (!output) {
        return false;
    }

    size_t i = 0, j = 0;
    int k = 0;
    unsigned char array3[3], array4[4];

    while (input_len-- && base64_str[i] != '=' && base64_str[i] != '\0') {
        signed char decode_val = base64_decode_table[(unsigned char)base64_str[i]];
        if (decode_val == -1) {
            knishio_free(output);
            return false;
        }
        array4[k++] = decode_val;
        i++;

        if (k == 4) {
            array3[0] = (array4[0] << 2) + ((array4[1] & 0x30) >> 4);
            array3[1] = ((array4[1] & 0xf) << 4) + ((array4[2] & 0x3c) >> 2);
            array3[2] = ((array4[2] & 0x3) << 6) + array4[3];

            for (k = 0; k < 3; k++) {
                if (j < output_len) {
                    output[j++] = array3[k];
                }
            }
            k = 0;
        }
    }

    if (k) {
        for (int l = k; l < 4; l++) {
            array4[l] = 0;
        }

        array3[0] = (array4[0] << 2) + ((array4[1] & 0x30) >> 4);
        array3[1] = ((array4[1] & 0xf) << 4) + ((array4[2] & 0x3c) >> 2);
        array3[2] = ((array4[2] & 0x3) << 6) + array4[3];

        for (int l = 0; l < k - 1; l++) {
            if (j < output_len) {
                output[j++] = array3[l];
            }
        }
    }

    *data_output = output;
    *data_len = j;
    return true;
}

bool knishio_hex_to_base64(const char* hex_str, char** base64_output) {
    if (!hex_str || !base64_output) {
        return false;
    }

    size_t hex_len = strlen(hex_str);
    if (hex_len % 2 != 0) {
        return false;
    }

    /* Convert hex to binary */
    size_t bin_len = hex_len / 2;
    unsigned char* bin_data = knishio_malloc(bin_len);
    if (!bin_data) {
        return false;
    }

    for (size_t i = 0; i < bin_len; i++) {
        char hex_byte[3] = { hex_str[i * 2], hex_str[i * 2 + 1], '\0' };
        char* endptr;
        long value = strtol(hex_byte, &endptr, 16);
        if (*endptr != '\0' || value < 0 || value > 255) {
            knishio_free(bin_data);
            return false;
        }
        bin_data[i] = (unsigned char)value;
    }

    /* Encode binary to base64 */
    bool result = knishio_base64_encode(bin_data, bin_len, base64_output);
    knishio_free(bin_data);
    return result;
}

bool knishio_base64_to_hex(const char* base64_str, char** hex_output) {
    if (!base64_str || !hex_output) {
        return false;
    }

    /* Decode base64 to binary */
    unsigned char* bin_data = NULL;
    size_t bin_len = 0;
    if (!knishio_base64_decode(base64_str, &bin_data, &bin_len)) {
        return false;
    }

    /* Convert binary to hex */
    size_t hex_len = bin_len * 2;
    char* hex_str = knishio_malloc(hex_len + 1);
    if (!hex_str) {
        knishio_free(bin_data);
        return false;
    }

    for (size_t i = 0; i < bin_len; i++) {
        sprintf(hex_str + (i * 2), "%02x", bin_data[i]);
    }
    hex_str[hex_len] = '\0';

    knishio_free(bin_data);
    *hex_output = hex_str;
    return true;
}
