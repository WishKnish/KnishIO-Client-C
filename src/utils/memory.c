#include "knishio/utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Platform-specific includes for secure memory operations */
#ifdef _WIN32
#include <windows.h>
#elif defined(__GLIBC__)
#include <sys/mman.h>
#elif defined(__APPLE__)
#include <sys/mman.h>
#endif

/* Random number generation includes */
#ifdef _WIN32
#include <wincrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

/* Default alignment */
#define DEFAULT_ALIGNMENT 8

/* Debug tracking structures */
#ifdef KNISHIO_DEBUG
typedef struct alloc_record {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    struct alloc_record *next;
} alloc_record_t;

static alloc_record_t *g_allocations = NULL;
static size_t g_allocation_count = 0;
static size_t g_total_bytes = 0;
#endif

/* Utility functions */
static size_t align_size(size_t size, size_t alignment) {
    if (alignment == 0) alignment = DEFAULT_ALIGNMENT;
    return (size + alignment - 1) & ~(alignment - 1);
}

/* Reference counting implementation */

knishio_ref_t* knishio_ref_create(void *data, knishio_destructor_t destructor) {
    if (data == NULL) {
        return NULL;
    }
    
    knishio_ref_t *ref = knishio_malloc(sizeof(knishio_ref_t));
    if (ref == NULL) {
        return NULL;
    }
    
    ref->data = data;
    ref->ref_count = 1;
    ref->destructor = destructor;
    ref->is_valid = true;
    
    return ref;
}

size_t knishio_ref_retain(knishio_ref_t *ref) {
    if (ref == NULL || !ref->is_valid) {
        return 0;
    }
    
    ref->ref_count++;
    return ref->ref_count;
}

size_t knishio_ref_release(knishio_ref_t *ref) {
    if (ref == NULL || !ref->is_valid) {
        return 0;
    }
    
    ref->ref_count--;
    
    if (ref->ref_count == 0) {
        ref->is_valid = false;
        
        if (ref->destructor != NULL) {
            ref->destructor(ref->data);
        }
        
        knishio_free(ref);
        return 0;
    }
    
    return ref->ref_count;
}

size_t knishio_ref_count(const knishio_ref_t *ref) {
    if (ref == NULL || !ref->is_valid) {
        return 0;
    }
    return ref->ref_count;
}

void* knishio_ref_data(const knishio_ref_t *ref) {
    if (ref == NULL || !ref->is_valid) {
        return NULL;
    }
    return ref->data;
}

bool knishio_ref_is_valid(const knishio_ref_t *ref) {
    return ref != NULL && ref->is_valid;
}

/* Memory pool implementation */

knishio_pool_t* knishio_pool_create(size_t initial_size, size_t alignment) {
    if (initial_size == 0) {
        return NULL;
    }
    
    knishio_pool_t *pool = knishio_malloc(sizeof(knishio_pool_t));
    if (pool == NULL) {
        return NULL;
    }
    
    pool->memory = knishio_malloc(initial_size);
    if (pool->memory == NULL) {
        knishio_free(pool);
        return NULL;
    }
    
    pool->size = initial_size;
    pool->used = 0;
    pool->alignment = alignment ? alignment : DEFAULT_ALIGNMENT;
    pool->next = NULL;
    
    return pool;
}

void* knishio_pool_alloc(knishio_pool_t *pool, size_t size) {
    if (pool == NULL || size == 0) {
        return NULL;
    }
    
    size_t aligned_size = align_size(size, pool->alignment);
    
    if (pool->used + aligned_size <= pool->size) {
        void *ptr = pool->memory + pool->used;
        pool->used += aligned_size;
        return ptr;
    }
    
    /* Try next pool in chain */
    if (pool->next != NULL) {
        return knishio_pool_alloc(pool->next, size);
    }
    
    /* Create new pool if needed */
    size_t new_pool_size = pool->size > aligned_size ? pool->size : aligned_size * 2;
    pool->next = knishio_pool_create(new_pool_size, pool->alignment);
    if (pool->next != NULL) {
        return knishio_pool_alloc(pool->next, size);
    }
    
    return NULL;
}

void* knishio_pool_calloc(knishio_pool_t *pool, size_t size) {
    void *ptr = knishio_pool_alloc(pool, size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void knishio_pool_reset(knishio_pool_t *pool) {
    if (pool == NULL) {
        return;
    }
    
    pool->used = 0;
    
    /* Reset all pools in chain */
    if (pool->next != NULL) {
        knishio_pool_reset(pool->next);
    }
}

void knishio_pool_stats(const knishio_pool_t *pool, 
                       size_t *total_size, 
                       size_t *used_size, 
                       size_t *free_size) {
    if (pool == NULL) {
        if (total_size) *total_size = 0;
        if (used_size) *used_size = 0;
        if (free_size) *free_size = 0;
        return;
    }
    
    size_t total = pool->size;
    size_t used = pool->used;
    
    /* Add stats from chained pools */
    if (pool->next != NULL) {
        size_t chain_total, chain_used, chain_free;
        knishio_pool_stats(pool->next, &chain_total, &chain_used, &chain_free);
        total += chain_total;
        used += chain_used;
    }
    
    if (total_size) *total_size = total;
    if (used_size) *used_size = used;
    if (free_size) *free_size = total - used;
}

void knishio_pool_destroy(knishio_pool_t *pool) {
    if (pool == NULL) {
        return;
    }
    
    /* Destroy chained pools first */
    if (pool->next != NULL) {
        knishio_pool_destroy(pool->next);
    }
    
    knishio_free(pool->memory);
    knishio_free(pool);
}

/* Safe memory operations */
#ifdef KNISHIO_DEBUG
static void track_allocation(void *ptr, size_t size) {
    if (ptr == NULL) return;
    
    alloc_record_t *record = malloc(sizeof(alloc_record_t));
    if (record == NULL) return;
    
    record->ptr = ptr;
    record->size = size;
    record->file = __FILE__;
    record->line = __LINE__;
    record->next = g_allocations;
    
    g_allocations = record;
    g_allocation_count++;
    g_total_bytes += size;
}

static void untrack_allocation(void *ptr) {
    if (ptr == NULL) return;
    
    alloc_record_t *current = g_allocations;
    alloc_record_t *previous = NULL;
    
    while (current != NULL) {
        if (current->ptr == ptr) {
            if (previous != NULL) {
                previous->next = current->next;
            } else {
                g_allocations = current->next;
            }
            
            g_allocation_count--;
            g_total_bytes -= current->size;
            
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}
#endif

void* knishio_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
        
#ifdef KNISHIO_DEBUG
        track_allocation(ptr, size);
#endif
    }
    
    return ptr;
}

void* knishio_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    
    /* Check for multiplication overflow */
    if (nmemb > SIZE_MAX / size) {
        return NULL;
    }
    
    size_t total_size = nmemb * size;
    void *ptr = malloc(total_size);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
        
#ifdef KNISHIO_DEBUG
        track_allocation(ptr, total_size);
#endif
    }
    
    return ptr;
}

void* knishio_realloc(void *ptr, size_t size) {
    if (size == 0) {
        knishio_free(ptr);
        return NULL;
    }
    
    if (ptr == NULL) {
        return knishio_malloc(size);
    }
    
#ifdef KNISHIO_DEBUG
    untrack_allocation(ptr);
#endif
    
    void *new_ptr = realloc(ptr, size);
    
#ifdef KNISHIO_DEBUG
    if (new_ptr != NULL) {
        track_allocation(new_ptr, size);
    }
#endif
    
    return new_ptr;
}

void knishio_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
#ifdef KNISHIO_DEBUG
    untrack_allocation(ptr);
#endif
    
    free(ptr);
}

char* knishio_strdup(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t len = strlen(str);
    char *copy = knishio_malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, str, len + 1);
    }
    
    return copy;
}

char* knishio_strndup(const char *str, size_t max_len) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t len = strnlen(str, max_len);
    char *copy = knishio_malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    
    return copy;
}

void knishio_secure_zero(void *ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return;
    }
    
#ifdef _WIN32
    SecureZeroMemory(ptr, size);
#elif defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 25
    explicit_bzero(ptr, size);
#else
    /* Fallback: use volatile to prevent optimization */
    volatile unsigned char *p = ptr;
    while (size--) {
        *p++ = 0;
    }
#endif
}

/* knishio_secure_free moved to security.c to avoid duplicate symbols */

bool knishio_random_bytes(uint8_t *buffer, size_t size) {
    if (buffer == NULL || size == 0) {
        return false;
    }
    
#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return false;
    }
    
    BOOL success = CryptGenRandom(hCryptProv, (DWORD)size, buffer);
    CryptReleaseContext(hCryptProv, 0);
    
    return success != FALSE;
    
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        /* Fallback to /dev/random if urandom is not available */
        fd = open("/dev/random", O_RDONLY);
        if (fd < 0) {
            return false;
        }
    }
    
    ssize_t bytes_read = 0;
    size_t total_read = 0;
    
    while (total_read < size) {
        bytes_read = read(fd, buffer + total_read, size - total_read);
        if (bytes_read <= 0) {
            close(fd);
            return false;
        }
        total_read += (size_t)bytes_read;
    }
    
    close(fd);
    return true;
#endif
}

/* Debug functions */
#ifdef KNISHIO_DEBUG
void knishio_memory_stats(size_t *allocations, size_t *total_bytes) {
    if (allocations) *allocations = g_allocation_count;
    if (total_bytes) *total_bytes = g_total_bytes;
}

size_t knishio_memory_check_leaks(void) {
    return g_allocation_count;
}
#endif
