#include "knishio/utils/string.h"
#include "knishio/utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

/* Default initial capacity */
#define DEFAULT_CAPACITY 32

/* Minimum capacity growth */
#define MIN_GROWTH 16

/* Helper functions */
static bool ensure_capacity(knishio_string_t *str, size_t needed_capacity);
static size_t calculate_new_capacity(size_t current, size_t needed);
static bool is_whitespace(char c);

/* String creation and destruction */

knishio_string_t* knishio_string_create(void) {
    knishio_string_t *str = knishio_malloc(sizeof(knishio_string_t));
    if (str == NULL) {
        return NULL;
    }
    
    str->data = knishio_malloc(DEFAULT_CAPACITY);
    if (str->data == NULL) {
        knishio_free(str);
        return NULL;
    }
    
    str->data[0] = '\0';
    str->length = 0;
    str->capacity = DEFAULT_CAPACITY;
    str->is_static = false;
    
    return str;
}

knishio_string_t* knishio_string_create_with_cstr(const char *initial) {
    if (initial == NULL) {
        return knishio_string_create();
    }
    
    size_t len = strlen(initial);
    return knishio_string_create_with_length(initial, len);
}

knishio_string_t* knishio_string_create_with_length(const char *initial, size_t length) {
    if (initial == NULL) {
        return knishio_string_create();
    }
    
    knishio_string_t *str = knishio_malloc(sizeof(knishio_string_t));
    if (str == NULL) {
        return NULL;
    }
    
    size_t capacity = length + 1;
    if (capacity < DEFAULT_CAPACITY) {
        capacity = DEFAULT_CAPACITY;
    }
    
    str->data = knishio_malloc(capacity);
    if (str->data == NULL) {
        knishio_free(str);
        return NULL;
    }
    
    memcpy(str->data, initial, length);
    str->data[length] = '\0';
    str->length = length;
    str->capacity = capacity;
    str->is_static = false;
    
    return str;
}

knishio_string_t* knishio_string_create_with_capacity(size_t capacity) {
    if (capacity < DEFAULT_CAPACITY) {
        capacity = DEFAULT_CAPACITY;
    }
    
    knishio_string_t *str = knishio_malloc(sizeof(knishio_string_t));
    if (str == NULL) {
        return NULL;
    }
    
    str->data = knishio_malloc(capacity);
    if (str->data == NULL) {
        knishio_free(str);
        return NULL;
    }
    
    str->data[0] = '\0';
    str->length = 0;
    str->capacity = capacity;
    str->is_static = false;
    
    return str;
}

knishio_string_t* knishio_string_create_static(const char *static_str) {
    if (static_str == NULL) {
        return NULL;
    }
    
    knishio_string_t *str = knishio_malloc(sizeof(knishio_string_t));
    if (str == NULL) {
        return NULL;
    }
    
    str->data = (char*)static_str;
    str->length = strlen(static_str);
    str->capacity = str->length + 1;
    str->is_static = true;
    
    return str;
}

knishio_string_t* knishio_string_clone(const knishio_string_t *str) {
    if (str == NULL) {
        return NULL;
    }
    
    return knishio_string_create_with_length(str->data, str->length);
}

void knishio_string_destroy(knishio_string_t *str) {
    if (str == NULL) {
        return;
    }
    
    if (!str->is_static && str->data != NULL) {
        knishio_free(str->data);
    }
    
    knishio_free(str);
}

/* String properties */

size_t knishio_string_length(const knishio_string_t *str) {
    return (str != NULL) ? str->length : 0;
}

size_t knishio_string_capacity(const knishio_string_t *str) {
    return (str != NULL) ? str->capacity : 0;
}

bool knishio_string_is_empty(const knishio_string_t *str) {
    return (str == NULL) || (str->length == 0);
}

const char* knishio_string_cstr(const knishio_string_t *str) {
    return (str != NULL && str->data != NULL) ? str->data : "";
}

/* String modification */

bool knishio_string_set(knishio_string_t *str, const char *content) {
    if (str == NULL) {
        return false;
    }
    
    if (content == NULL) {
        knishio_string_clear(str);
        return true;
    }
    
    size_t len = strlen(content);
    return knishio_string_set_length(str, content, len);
}

bool knishio_string_set_length(knishio_string_t *str, const char *content, size_t length) {
    if (str == NULL || str->is_static) {
        return false;
    }
    
    if (content == NULL) {
        knishio_string_clear(str);
        return true;
    }
    
    if (!ensure_capacity(str, length + 1)) {
        return false;
    }
    
    memcpy(str->data, content, length);
    str->data[length] = '\0';
    str->length = length;
    
    return true;
}

bool knishio_string_append(knishio_string_t *str, const char *content) {
    if (content == NULL) {
        return true;
    }
    
    size_t len = strlen(content);
    return knishio_string_append_length(str, content, len);
}

bool knishio_string_append_length(knishio_string_t *str, const char *content, size_t length) {
    if (str == NULL || str->is_static || content == NULL || length == 0) {
        return false;
    }
    
    size_t new_length = str->length + length;
    if (!ensure_capacity(str, new_length + 1)) {
        return false;
    }
    
    memcpy(str->data + str->length, content, length);
    str->data[new_length] = '\0';
    str->length = new_length;
    
    return true;
}

bool knishio_string_append_char(knishio_string_t *str, char ch) {
    return knishio_string_append_length(str, &ch, 1);
}

bool knishio_string_append_string(knishio_string_t *str, const knishio_string_t *other) {
    if (other == NULL) {
        return true;
    }
    
    return knishio_string_append_length(str, other->data, other->length);
}

bool knishio_string_prepend(knishio_string_t *str, const char *content) {
    if (str == NULL || str->is_static || content == NULL) {
        return false;
    }
    
    size_t content_len = strlen(content);
    if (content_len == 0) {
        return true;
    }
    
    size_t new_length = str->length + content_len;
    if (!ensure_capacity(str, new_length + 1)) {
        return false;
    }
    
    /* Move existing content */
    memmove(str->data + content_len, str->data, str->length);
    
    /* Copy new content at beginning */
    memcpy(str->data, content, content_len);
    
    str->data[new_length] = '\0';
    str->length = new_length;
    
    return true;
}

bool knishio_string_insert(knishio_string_t *str, size_t pos, const char *content) {
    if (str == NULL || str->is_static || content == NULL) {
        return false;
    }
    
    if (pos > str->length) {
        pos = str->length;
    }
    
    size_t content_len = strlen(content);
    if (content_len == 0) {
        return true;
    }
    
    size_t new_length = str->length + content_len;
    if (!ensure_capacity(str, new_length + 1)) {
        return false;
    }
    
    /* Move content after insertion point */
    if (pos < str->length) {
        memmove(str->data + pos + content_len, str->data + pos, str->length - pos);
    }
    
    /* Insert new content */
    memcpy(str->data + pos, content, content_len);
    
    str->data[new_length] = '\0';
    str->length = new_length;
    
    return true;
}

void knishio_string_clear(knishio_string_t *str) {
    if (str == NULL || str->is_static) {
        return;
    }
    
    if (str->data != NULL) {
        str->data[0] = '\0';
    }
    str->length = 0;
}

bool knishio_string_reserve(knishio_string_t *str, size_t capacity) {
    if (str == NULL || str->is_static) {
        return false;
    }
    
    return ensure_capacity(str, capacity);
}

bool knishio_string_shrink_to_fit(knishio_string_t *str) {
    if (str == NULL || str->is_static || str->data == NULL) {
        return false;
    }
    
    size_t needed = str->length + 1;
    if (needed >= str->capacity) {
        return true; /* Already at minimum size */
    }
    
    char *new_data = knishio_realloc(str->data, needed);
    if (new_data == NULL) {
        return false;
    }
    
    str->data = new_data;
    str->capacity = needed;
    
    return true;
}

/* String manipulation */

knishio_string_t* knishio_string_substr(const knishio_string_t *str, size_t start, size_t length) {
    if (str == NULL || start >= str->length) {
        return knishio_string_create();
    }
    
    size_t available = str->length - start;
    if (length > available) {
        length = available;
    }
    
    return knishio_string_create_with_length(str->data + start, length);
}

size_t knishio_string_find(const knishio_string_t *str, const char *needle, size_t start) {
    if (str == NULL || needle == NULL || start >= str->length) {
        return SIZE_MAX;
    }
    
    const char *found = strstr(str->data + start, needle);
    if (found == NULL) {
        return SIZE_MAX;
    }
    
    return found - str->data;
}

size_t knishio_string_rfind(const knishio_string_t *str, const char *needle) {
    if (str == NULL || needle == NULL || str->length == 0) {
        return SIZE_MAX;
    }
    
    size_t needle_len = strlen(needle);
    if (needle_len > str->length) {
        return SIZE_MAX;
    }
    
    /* Search backwards */
    for (size_t i = str->length - needle_len; i != SIZE_MAX; i--) {
        if (memcmp(str->data + i, needle, needle_len) == 0) {
            return i;
        }
    }
    
    return SIZE_MAX;
}

size_t knishio_string_replace(knishio_string_t *str, const char *search, const char *replace) {
    if (str == NULL || str->is_static || search == NULL || replace == NULL) {
        return 0;
    }
    
    size_t search_len = strlen(search);
    size_t replace_len = strlen(replace);
    size_t count = 0;
    
    if (search_len == 0) {
        return 0;
    }
    
    /* Count occurrences first */
    const char *pos = str->data;
    while ((pos = strstr(pos, search)) != NULL) {
        count++;
        pos += search_len;
    }
    
    if (count == 0) {
        return 0;
    }
    
    /* Calculate new size */
    size_t new_length = str->length + count * (replace_len - search_len);
    
    if (replace_len > search_len) {
        /* Need to grow string */
        if (!ensure_capacity(str, new_length + 1)) {
            return 0;
        }
    }
    
    /* Perform replacements from end to beginning to avoid overlap issues */
    if (replace_len != search_len) {
        /* Create new string for complex replacement */
        knishio_string_t *new_str = knishio_string_create_with_capacity(new_length + 1);
        if (new_str == NULL) {
            return 0;
        }
        
        const char *current = str->data;
        while (true) {
            const char *found = strstr(current, search);
            if (found == NULL) {
                knishio_string_append(new_str, current);
                break;
            }
            
            /* Append text before match */
            knishio_string_append_length(new_str, current, found - current);
            
            /* Append replacement */
            knishio_string_append(new_str, replace);
            
            current = found + search_len;
        }
        
        /* Swap string contents */
        char *old_data = str->data;
        str->data = new_str->data;
        str->length = new_str->length;
        str->capacity = new_str->capacity;
        
        new_str->data = old_data;
        knishio_string_destroy(new_str);
    } else {
        /* Simple in-place replacement */
        char *pos = str->data;
        while ((pos = strstr(pos, search)) != NULL) {
            /* Equal-length in-place replacement (this is the `replace_len == search_len`
             * branch; the != branch builds a new string). The memcpy overwrites within the
             * already-NUL-terminated str->data, so the terminator is preserved — clang-tidy's
             * not-null-terminated-result check is a false positive here. (clang-tidy-22 flags
             * it; local LLVM 22.1.4 doesn't.) Same-line NOLINT so the block comment above
             * doesn't shift the directive off the memcpy line. */
            memcpy(pos, replace, replace_len);  // NOLINT(bugprone-not-null-terminated-result)
            pos += replace_len;
        }
    }
    
    return count;
}

void knishio_string_trim(knishio_string_t *str) {
    knishio_string_ltrim(str);
    knishio_string_rtrim(str);
}

void knishio_string_ltrim(knishio_string_t *str) {
    if (str == NULL || str->is_static || str->length == 0) {
        return;
    }
    
    size_t start = 0;
    while (start < str->length && is_whitespace(str->data[start])) {
        start++;
    }
    
    if (start > 0) {
        size_t new_length = str->length - start;
        memmove(str->data, str->data + start, new_length);
        str->data[new_length] = '\0';
        str->length = new_length;
    }
}

void knishio_string_rtrim(knishio_string_t *str) {
    if (str == NULL || str->is_static || str->length == 0) {
        return;
    }
    
    size_t end = str->length;
    while (end > 0 && is_whitespace(str->data[end - 1])) {
        end--;
    }
    
    if (end < str->length) {
        str->data[end] = '\0';
        str->length = end;
    }
}

void knishio_string_to_lower(knishio_string_t *str) {
    if (str == NULL || str->is_static) {
        return;
    }
    
    for (size_t i = 0; i < str->length; i++) {
        str->data[i] = tolower((unsigned char)str->data[i]);
    }
}

void knishio_string_to_upper(knishio_string_t *str) {
    if (str == NULL || str->is_static) {
        return;
    }
    
    for (size_t i = 0; i < str->length; i++) {
        str->data[i] = toupper((unsigned char)str->data[i]);
    }
}

/* String comparison */

int knishio_string_compare(const knishio_string_t *str1, const knishio_string_t *str2) {
    if (str1 == NULL && str2 == NULL) return 0;
    if (str1 == NULL) return -1;
    if (str2 == NULL) return 1;
    
    return strcmp(str1->data, str2->data);
}

int knishio_string_compare_cstr(const knishio_string_t *str, const char *cstr) {
    if (str == NULL && cstr == NULL) return 0;
    if (str == NULL) return -1;
    if (cstr == NULL) return 1;
    
    return strcmp(str->data, cstr);
}

bool knishio_string_equals(const knishio_string_t *str1, const knishio_string_t *str2) {
    return knishio_string_compare(str1, str2) == 0;
}

bool knishio_string_equals_cstr(const knishio_string_t *str, const char *cstr) {
    return knishio_string_compare_cstr(str, cstr) == 0;
}

bool knishio_string_starts_with(const knishio_string_t *str, const char *prefix) {
    if (str == NULL || prefix == NULL) {
        return false;
    }
    
    size_t prefix_len = strlen(prefix);
    if (prefix_len > str->length) {
        return false;
    }
    
    return memcmp(str->data, prefix, prefix_len) == 0;
}

bool knishio_string_ends_with(const knishio_string_t *str, const char *suffix) {
    if (str == NULL || suffix == NULL) {
        return false;
    }
    
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str->length) {
        return false;
    }
    
    return memcmp(str->data + str->length - suffix_len, suffix, suffix_len) == 0;
}

/* String formatting */

knishio_string_t* knishio_string_sprintf(const char *format, ...) {
    if (format == NULL) {
        return NULL;
    }
    
    va_list args;
    va_start(args, format);
    
    /* Calculate required length */
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (len < 0) {
        va_end(args);
        return NULL;
    }
    
    knishio_string_t *str = knishio_string_create_with_capacity(len + 1);
    if (str == NULL) {
        va_end(args);
        return NULL;
    }
    
    vsnprintf(str->data, len + 1, format, args);
    str->length = len;
    
    va_end(args);
    return str;
}

bool knishio_string_append_sprintf(knishio_string_t *str, const char *format, ...) {
    if (str == NULL || str->is_static || format == NULL) {
        return false;
    }
    
    va_list args;
    va_start(args, format);
    
    /* Calculate required length */
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (len < 0) {
        va_end(args);
        return false;
    }
    
    size_t new_length = str->length + len;
    if (!ensure_capacity(str, new_length + 1)) {
        va_end(args);
        return false;
    }
    
    vsnprintf(str->data + str->length, len + 1, format, args);
    str->length = new_length;
    
    va_end(args);
    return true;
}

/* Utility functions */

bool knishio_string_is_hex(const knishio_string_t *str) {
    if (str == NULL || str->length == 0) {
        return false;
    }
    
    for (size_t i = 0; i < str->length; i++) {
        char c = str->data[i];
        if (!((c >= '0' && c <= '9') || 
              (c >= 'a' && c <= 'f') || 
              (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    
    return true;
}

bool knishio_string_is_numeric(const knishio_string_t *str) {
    if (str == NULL || str->length == 0) {
        return false;
    }
    
    size_t start = 0;
    if (str->data[0] == '-' || str->data[0] == '+') {
        start = 1;
        if (str->length == 1) return false;
    }
    
    bool has_decimal = false;
    for (size_t i = start; i < str->length; i++) {
        char c = str->data[i];
        if (c >= '0' && c <= '9') {
            continue;
        } else if (c == '.' && !has_decimal) {
            has_decimal = true;
        } else {
            return false;
        }
    }
    
    return true;
}

void knishio_string_array_free(knishio_string_t **strings, size_t count) {
    if (strings == NULL) {
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        knishio_string_destroy(strings[i]);
    }
    
    knishio_free(strings);
}

/* Helper functions */

static bool ensure_capacity(knishio_string_t *str, size_t needed_capacity) {
    if (str == NULL || str->is_static) {
        return false;
    }
    
    if (needed_capacity <= str->capacity) {
        return true;
    }
    
    size_t new_capacity = calculate_new_capacity(str->capacity, needed_capacity);
    char *new_data = knishio_realloc(str->data, new_capacity);
    if (new_data == NULL) {
        return false;
    }
    
    str->data = new_data;
    str->capacity = new_capacity;
    
    return true;
}

static size_t calculate_new_capacity(size_t current, size_t needed) {
    size_t new_capacity = current;
    
    while (new_capacity < needed) {
        if (new_capacity < MIN_GROWTH) {
            new_capacity += MIN_GROWTH;
        } else {
            new_capacity *= 2;
        }
    }
    
    return new_capacity;
}

static bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
