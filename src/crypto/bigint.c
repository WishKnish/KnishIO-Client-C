#include "knishio/crypto/bigint.h"
#include "knishio/utils/memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

/* Core BigInt operations */

bool knishio_bigint_init(knishio_bigint_t *bigint) {
    if (bigint == NULL) {
        return false;
    }
    
    mpz_init(bigint->value);
    bigint->initialized = true;
    return true;
}

bool knishio_bigint_init_hex(knishio_bigint_t *bigint, const char *hex_str) {
    if (bigint == NULL || hex_str == NULL) {
        return false;
    }
    
    mpz_init(bigint->value);
    
    /* Remove leading "0x" if present */
    const char *actual_hex = hex_str;
    if (strlen(hex_str) >= 2 && hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
        actual_hex = hex_str + 2;
    }
    
    /* Validate hex string */
    for (size_t i = 0; i < strlen(actual_hex); i++) {
        if (!isxdigit((unsigned char)actual_hex[i])) {
            mpz_clear(bigint->value);
            return false;
        }
    }
    
    /* Set value from hex string */
    if (mpz_set_str(bigint->value, actual_hex, 16) != 0) {
        mpz_clear(bigint->value);
        return false;
    }
    
    bigint->initialized = true;
    return true;
}

bool knishio_bigint_init_dec(knishio_bigint_t *bigint, const char *dec_str) {
    if (bigint == NULL || dec_str == NULL) {
        return false;
    }
    
    mpz_init(bigint->value);
    
    /* Validate decimal string */
    for (size_t i = 0; i < strlen(dec_str); i++) {
        if (!isdigit((unsigned char)dec_str[i])) {
            mpz_clear(bigint->value);
            return false;
        }
    }
    
    /* Set value from decimal string */
    if (mpz_set_str(bigint->value, dec_str, 10) != 0) {
        mpz_clear(bigint->value);
        return false;
    }
    
    bigint->initialized = true;
    return true;
}

bool knishio_bigint_init_ui(knishio_bigint_t *bigint, unsigned long value) {
    if (bigint == NULL) {
        return false;
    }
    
    mpz_init_set_ui(bigint->value, value);
    bigint->initialized = true;
    return true;
}

bool knishio_bigint_copy(knishio_bigint_t *dest, const knishio_bigint_t *src) {
    if (dest == NULL || src == NULL || !src->initialized) {
        return false;
    }
    
    mpz_init_set(dest->value, src->value);
    dest->initialized = true;
    return true;
}

void knishio_bigint_cleanup(knishio_bigint_t *bigint) {
    if (bigint != NULL && bigint->initialized) {
        mpz_clear(bigint->value);
        bigint->initialized = false;
    }
}

/* Arithmetic operations */

bool knishio_bigint_add(knishio_bigint_t *result, const knishio_bigint_t *a, const knishio_bigint_t *b) {
    if (result == NULL || a == NULL || b == NULL || 
        !result->initialized || !a->initialized || !b->initialized) {
        return false;
    }
    
    mpz_add(result->value, a->value, b->value);
    return true;
}

bool knishio_bigint_add_ui(knishio_bigint_t *result, const knishio_bigint_t *a, unsigned long b) {
    if (result == NULL || a == NULL || !result->initialized || !a->initialized) {
        return false;
    }
    
    mpz_add_ui(result->value, a->value, b);
    return true;
}

bool knishio_bigint_mul(knishio_bigint_t *result, const knishio_bigint_t *a, const knishio_bigint_t *b) {
    if (result == NULL || a == NULL || b == NULL || 
        !result->initialized || !a->initialized || !b->initialized) {
        return false;
    }
    
    mpz_mul(result->value, a->value, b->value);
    return true;
}

bool knishio_bigint_mul_ui(knishio_bigint_t *result, const knishio_bigint_t *a, unsigned long b) {
    if (result == NULL || a == NULL || !result->initialized || !a->initialized) {
        return false;
    }
    
    mpz_mul_ui(result->value, a->value, b);
    return true;
}

unsigned long knishio_bigint_divmod_ui(knishio_bigint_t *quotient, knishio_bigint_t *remainder, 
                                      const knishio_bigint_t *dividend, unsigned long divisor) {
    if (dividend == NULL || !dividend->initialized || divisor == 0) {
        return ULONG_MAX;
    }
    
    if (quotient != NULL && remainder != NULL) {
        if (!quotient->initialized || !remainder->initialized) {
            return ULONG_MAX;
        }
        return mpz_divmod_ui(quotient->value, remainder->value, dividend->value, divisor);
    } else if (quotient != NULL) {
        if (!quotient->initialized) {
            return ULONG_MAX;
        }
        return mpz_divmod_ui(quotient->value, NULL, dividend->value, divisor);
    } else if (remainder != NULL) {
        if (!remainder->initialized) {
            return ULONG_MAX;
        }
        return mpz_divmod_ui(NULL, remainder->value, dividend->value, divisor);
    } else {
        return mpz_divmod_ui(NULL, NULL, dividend->value, divisor);
    }
}

unsigned long knishio_bigint_mod_ui(const knishio_bigint_t *a, unsigned long b) {
    if (a == NULL || !a->initialized || b == 0) {
        return ULONG_MAX;
    }
    
    return mpz_mod_ui(NULL, a->value, b);
}

/* Comparison operations */

int knishio_bigint_cmp(const knishio_bigint_t *a, const knishio_bigint_t *b) {
    if (a == NULL || b == NULL || !a->initialized || !b->initialized) {
        return 0; /* Consider equal on error */
    }
    
    return mpz_cmp(a->value, b->value);
}

int knishio_bigint_cmp_ui(const knishio_bigint_t *a, unsigned long b) {
    if (a == NULL || !a->initialized) {
        return 0; /* Consider equal on error */
    }
    
    return mpz_cmp_ui(a->value, b);
}

bool knishio_bigint_is_zero(const knishio_bigint_t *a) {
    if (a == NULL || !a->initialized) {
        return true; /* Consider zero on error */
    }
    
    return mpz_cmp_ui(a->value, 0) == 0;
}

/* String conversion operations */

bool knishio_bigint_to_hex(const knishio_bigint_t *bigint, char **hex_str) {
    if (bigint == NULL || !bigint->initialized || hex_str == NULL) {
        return false;
    }
    
    /* Get string representation in base 16 */
    char *gmp_str = mpz_get_str(NULL, 16, bigint->value);
    if (gmp_str == NULL) {
        return false;
    }
    
    /* Allocate our own string */
    size_t len = strlen(gmp_str);
    *hex_str = knishio_malloc(len + 1);
    if (*hex_str == NULL) {
        free(gmp_str); /* GMP allocates with regular malloc */
        return false;
    }
    
    strcpy(*hex_str, gmp_str);
    free(gmp_str); /* Free GMP allocated string */
    
    return true;
}

bool knishio_bigint_to_dec(const knishio_bigint_t *bigint, char **dec_str) {
    if (bigint == NULL || !bigint->initialized || dec_str == NULL) {
        return false;
    }
    
    /* Get string representation in base 10 */
    char *gmp_str = mpz_get_str(NULL, 10, bigint->value);
    if (gmp_str == NULL) {
        return false;
    }
    
    /* Allocate our own string */
    size_t len = strlen(gmp_str);
    *dec_str = knishio_malloc(len + 1);
    if (*dec_str == NULL) {
        free(gmp_str); /* GMP allocates with regular malloc */
        return false;
    }
    
    strcpy(*dec_str, gmp_str);
    free(gmp_str); /* Free GMP allocated string */
    
    return true;
}

bool knishio_bigint_set_hex(knishio_bigint_t *bigint, const char *hex_str) {
    if (bigint == NULL || !bigint->initialized || hex_str == NULL) {
        return false;
    }
    
    /* Remove leading "0x" if present */
    const char *actual_hex = hex_str;
    if (strlen(hex_str) >= 2 && hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
        actual_hex = hex_str + 2;
    }
    
    /* Validate hex string */
    for (size_t i = 0; i < strlen(actual_hex); i++) {
        if (!isxdigit((unsigned char)actual_hex[i])) {
            return false;
        }
    }
    
    /* Set value from hex string */
    return mpz_set_str(bigint->value, actual_hex, 16) == 0;
}

bool knishio_bigint_set_dec(knishio_bigint_t *bigint, const char *dec_str) {
    if (bigint == NULL || !bigint->initialized || dec_str == NULL) {
        return false;
    }
    
    /* Validate decimal string */
    for (size_t i = 0; i < strlen(dec_str); i++) {
        if (!isdigit((unsigned char)dec_str[i])) {
            return false;
        }
    }
    
    /* Set value from decimal string */
    return mpz_set_str(bigint->value, dec_str, 10) == 0;
}

bool knishio_bigint_set_ui(knishio_bigint_t *bigint, unsigned long value) {
    if (bigint == NULL || !bigint->initialized) {
        return false;
    }
    
    mpz_set_ui(bigint->value, value);
    return true;
}

/* High-level wallet operations */

bool knishio_wallet_key_from_hex(const char *secret_hex, const char *position_hex, char **key_hex) {
    if (secret_hex == NULL || position_hex == NULL || key_hex == NULL) {
        return false;
    }
    
    knishio_bigint_t secret, position, result;
    bool success = false;
    
    /* Initialize BigInts */
    if (!knishio_bigint_init_hex(&secret, secret_hex)) {
        return false;
    }
    
    if (!knishio_bigint_init_hex(&position, position_hex)) {
        knishio_bigint_cleanup(&secret);
        return false;
    }
    
    if (!knishio_bigint_init(&result)) {
        knishio_bigint_cleanup(&secret);
        knishio_bigint_cleanup(&position);
        return false;
    }
    
    /* Perform addition: result = secret + position */
    if (knishio_bigint_add(&result, &secret, &position)) {
        /* Convert result to hex string */
        success = knishio_bigint_to_hex(&result, key_hex);
    }
    
    /* Cleanup */
    knishio_bigint_cleanup(&secret);
    knishio_bigint_cleanup(&position);
    knishio_bigint_cleanup(&result);
    
    return success;
}

/* Base conversion for molecular hashing */

bool knishio_bigint_base_convert(const char *input, int from_base, int to_base,
                                const char *from_symbols, const char *to_symbols, 
                                char **output) {
    if (input == NULL || output == NULL || from_symbols == NULL || to_symbols == NULL ||
        from_base < 2 || from_base > 36 || to_base < 2 || to_base > 36) {
        return false;
    }
    
    knishio_bigint_t val, base_from, base_to, remainder;
    bool success = false;
    char *result = NULL;
    size_t result_capacity = 16;
    
    /* Initialize BigInts */
    if (!knishio_bigint_init(&val) || 
        !knishio_bigint_init_ui(&base_from, from_base) ||
        !knishio_bigint_init_ui(&base_to, to_base) ||
        !knishio_bigint_init(&remainder)) {
        goto cleanup;
    }
    
    /* Allocate initial result buffer */
    result = knishio_malloc(result_capacity);
    if (result == NULL) {
        goto cleanup;
    }
    
    /* Convert from source base to BigInt */
    size_t input_len = strlen(input);
    for (size_t i = 0; i < input_len; i++) {
        char *symbol_pos = strchr(from_symbols, input[i]);
        if (symbol_pos == NULL) {
            goto cleanup; /* Invalid symbol */
        }
        
        int symbol_index = (int)(symbol_pos - from_symbols);
        if (symbol_index >= from_base) {
            goto cleanup; /* Symbol not valid for this base */
        }
        
        /* val = val * from_base + symbol_index */
        if (!knishio_bigint_mul(&val, &val, &base_from) ||
            !knishio_bigint_add_ui(&val, &val, symbol_index)) {
            goto cleanup;
        }
    }
    
    /* Convert from BigInt to destination base */
    if (knishio_bigint_is_zero(&val)) {
        /* Handle zero case */
        result[0] = to_symbols[0];
        result[1] = '\0';
    } else {
        /* Build result string from right to left */
        char *temp_result = knishio_malloc(1000); /* Large temporary buffer */
        if (temp_result == NULL) {
            goto cleanup;
        }
        
        size_t temp_len = 0;
        while (!knishio_bigint_is_zero(&val)) {
            unsigned long rem = knishio_bigint_divmod_ui(&val, &remainder, &val, to_base);
            if (rem == ULONG_MAX || rem >= (unsigned long)to_base) {
                knishio_free(temp_result);
                goto cleanup;
            }
            
            temp_result[temp_len++] = to_symbols[rem];
            
            if (temp_len >= 999) { /* Prevent overflow */
                knishio_free(temp_result);
                goto cleanup;
            }
        }
        
        /* Reverse the string */
        if (temp_len >= result_capacity) {
            result_capacity = temp_len + 1;
            char *new_result = knishio_realloc(result, result_capacity);
            if (new_result == NULL) {
                knishio_free(temp_result);
                goto cleanup;
            }
            result = new_result;
        }
        
        for (size_t i = 0; i < temp_len; i++) {
            result[i] = temp_result[temp_len - 1 - i];
        }
        result[temp_len] = '\0';
        
        knishio_free(temp_result);
    }
    
    *output = result;
    success = true;
    result = NULL; /* Don't free on cleanup */
    
cleanup:
    knishio_bigint_cleanup(&val);
    knishio_bigint_cleanup(&base_from);
    knishio_bigint_cleanup(&base_to);
    knishio_bigint_cleanup(&remainder);
    
    if (result != NULL) {
        knishio_free(result);
    }
    
    return success;
}

/* Test and validation functions */

bool knishio_bigint_self_test(void) {
    /* Test basic operations */
    knishio_bigint_t a, b, result;
    char *hex_str = NULL;
    bool test_passed = true;
    
    /* Test 1: Basic initialization and hex conversion */
    if (!knishio_bigint_init_hex(&a, "1234567890abcdef") ||
        !knishio_bigint_to_hex(&a, &hex_str)) {
        test_passed = false;
        goto cleanup_test1;
    }
    
    if (strcmp(hex_str, "1234567890abcdef") != 0) {
        test_passed = false;
    }
    
    knishio_free(hex_str);
    hex_str = NULL;
    
cleanup_test1:
    knishio_bigint_cleanup(&a);
    
    if (!test_passed) {
        return false;
    }
    
    /* Test 2: Addition operation */
    if (!knishio_bigint_init_hex(&a, "1000") ||
        !knishio_bigint_init_hex(&b, "2000") ||
        !knishio_bigint_init(&result)) {
        test_passed = false;
        goto cleanup_test2;
    }
    
    if (!knishio_bigint_add(&result, &a, &b) ||
        !knishio_bigint_to_hex(&result, &hex_str)) {
        test_passed = false;
        goto cleanup_test2;
    }
    
    if (strcmp(hex_str, "3000") != 0) {
        test_passed = false;
    }
    
    knishio_free(hex_str);
    hex_str = NULL;
    
cleanup_test2:
    knishio_bigint_cleanup(&a);
    knishio_bigint_cleanup(&b);
    knishio_bigint_cleanup(&result);
    
    if (!test_passed) {
        return false;
    }
    
    /* Test 3: Large number addition (wallet key derivation simulation) */
    const char *large_secret = "123456789abcdef0fedcba987654321023456789abcdef0fedcba9876543210";
    const char *position = "1000000000000000000000000000000000000000000000000000000000000001";
    const char *expected = "1123456789abcdef0fedcba987654321023456789abcdef0fedcba9876543211";
    
    char *wallet_key = NULL;
    if (!knishio_wallet_key_from_hex(large_secret, position, &wallet_key)) {
        return false;
    }
    
    if (strcmp(wallet_key, expected) != 0) {
        test_passed = false;
    }
    
    knishio_free(wallet_key);
    
    return test_passed;
}

bool knishio_bigint_test_vector(const char *secret_hex, const char *position_hex, const char *expected_hex) {
    if (secret_hex == NULL || position_hex == NULL || expected_hex == NULL) {
        return false;
    }
    
    char *result_hex = NULL;
    if (!knishio_wallet_key_from_hex(secret_hex, position_hex, &result_hex)) {
        return false;
    }
    
    bool matches = (strcmp(result_hex, expected_hex) == 0);
    knishio_free(result_hex);
    
    return matches;
}