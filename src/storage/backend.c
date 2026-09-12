#include "knishio/storage/backend.h"
#include "knishio/utils/memory.h"
#include "storage_internal.h"
#include <cjson/cJSON.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
    char *key;
    char *value;
} kv_pair_t;

typedef struct {
    kv_pair_t *pairs;
    size_t count;
    size_t capacity;
    pthread_mutex_t lock;
} memory_backend_state_t;

typedef struct {
    char *file_path;
    kv_pair_t *pairs;
    size_t count;
    size_t capacity;
    pthread_mutex_t lock;
} file_backend_state_t;

/* --- Memory Backend Implementation --- */

static knishio_error_t mem_get_item(knishio_storage_backend_t *backend, const char *key, char **value_out) {
    if (!backend || !key || !value_out) return KNISHIO_ERROR_NULL_POINTER;
    memory_backend_state_t *state = (memory_backend_state_t *)backend->user_data;
    if (!state) return KNISHIO_ERROR_INVALID_STATE;

    *value_out = NULL;
    pthread_mutex_lock(&state->lock);
    for (size_t i = 0; i < state->count; i++) {
        if (strcmp(state->pairs[i].key, key) == 0) {
            *value_out = strdup(state->pairs[i].value);
            break;
        }
    }
    pthread_mutex_unlock(&state->lock);
    return KNISHIO_SUCCESS;
}

static knishio_error_t mem_set_item(knishio_storage_backend_t *backend, const char *key, const char *value) {
    if (!backend || !key || !value) return KNISHIO_ERROR_NULL_POINTER;
    memory_backend_state_t *state = (memory_backend_state_t *)backend->user_data;
    if (!state) return KNISHIO_ERROR_INVALID_STATE;

    pthread_mutex_lock(&state->lock);
    for (size_t i = 0; i < state->count; i++) {
        if (strcmp(state->pairs[i].key, key) == 0) {
            free(state->pairs[i].value);
            state->pairs[i].value = strdup(value);
            pthread_mutex_unlock(&state->lock);
            return KNISHIO_SUCCESS;
        }
    }

    if (state->count >= state->capacity) {
        size_t new_cap = state->capacity ? state->capacity * 2 : 16;
        kv_pair_t *new_pairs = realloc(state->pairs, new_cap * sizeof(kv_pair_t));
        if (!new_pairs) {
            pthread_mutex_unlock(&state->lock);
            return KNISHIO_ERROR_MEMORY;
        }
        state->pairs = new_pairs;
        state->capacity = new_cap;
    }

    state->pairs[state->count].key = strdup(key);
    state->pairs[state->count].value = strdup(value);
    state->count++;
    pthread_mutex_unlock(&state->lock);
    return KNISHIO_SUCCESS;
}

static knishio_error_t mem_remove_item(knishio_storage_backend_t *backend, const char *key, bool *existed_out) {
    if (!backend || !key) return KNISHIO_ERROR_NULL_POINTER;
    memory_backend_state_t *state = (memory_backend_state_t *)backend->user_data;
    if (!state) return KNISHIO_ERROR_INVALID_STATE;

    bool found = false;
    pthread_mutex_lock(&state->lock);
    for (size_t i = 0; i < state->count; i++) {
        if (strcmp(state->pairs[i].key, key) == 0) {
            found = true;
            free(state->pairs[i].key);
            free(state->pairs[i].value);
            for (size_t j = i; j + 1 < state->count; j++) {
                state->pairs[j] = state->pairs[j + 1];
            }
            state->count--;
            break;
        }
    }
    pthread_mutex_unlock(&state->lock);

    if (existed_out) *existed_out = found;
    return KNISHIO_SUCCESS;
}

static knishio_error_t mem_keys(knishio_storage_backend_t *backend, char ***keys_out, size_t *count_out) {
    if (!backend || !keys_out || !count_out) return KNISHIO_ERROR_NULL_POINTER;
    memory_backend_state_t *state = (memory_backend_state_t *)backend->user_data;
    if (!state) return KNISHIO_ERROR_INVALID_STATE;

    pthread_mutex_lock(&state->lock);
    size_t count = state->count;
    char **keys = NULL;
    if (count > 0) {
        keys = calloc(count, sizeof(char *));
        if (!keys) {
            pthread_mutex_unlock(&state->lock);
            return KNISHIO_ERROR_MEMORY;
        }
        for (size_t i = 0; i < count; i++) {
            keys[i] = strdup(state->pairs[i].key);
        }
    }
    pthread_mutex_unlock(&state->lock);

    *keys_out = keys;
    *count_out = count;
    return KNISHIO_SUCCESS;
}

static void mem_free_backend(knishio_storage_backend_t *backend) {
    if (!backend) return;
    memory_backend_state_t *state = (memory_backend_state_t *)backend->user_data;
    if (state) {
        pthread_mutex_lock(&state->lock);
        for (size_t i = 0; i < state->count; i++) {
            free(state->pairs[i].key);
            free(state->pairs[i].value);
        }
        free(state->pairs);
        pthread_mutex_unlock(&state->lock);
        pthread_mutex_destroy(&state->lock);
        free(state);
    }
    free(backend);
}

knishio_error_t knishio_memory_storage_backend_create(knishio_storage_backend_t **backend_out) {
    if (!backend_out) return KNISHIO_ERROR_NULL_POINTER;

    knishio_storage_backend_t *backend = calloc(1, sizeof(knishio_storage_backend_t));
    if (!backend) return KNISHIO_ERROR_MEMORY;

    memory_backend_state_t *state = calloc(1, sizeof(memory_backend_state_t));
    if (!state) {
        free(backend);
        return KNISHIO_ERROR_MEMORY;
    }

    pthread_mutex_init(&state->lock, NULL);
    backend->user_data = state;
    backend->get_item = mem_get_item;
    backend->set_item = mem_set_item;
    backend->remove_item = mem_remove_item;
    backend->keys = mem_keys;
    backend->free_backend = mem_free_backend;

    *backend_out = backend;
    return KNISHIO_SUCCESS;
}

/* --- File Backend Implementation --- */

static void ensure_parent_dir(const char *path) {
    char *copy = strdup(path);
    if (!copy) return;
    char *last_slash = strrchr(copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (strlen(copy) > 0) {
            mkdir(copy, 0700);
        }
    }
    free(copy);
}

static knishio_error_t persist_file_backend_locked(file_backend_state_t *state) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return KNISHIO_ERROR_MEMORY;

    for (size_t i = 0; i < state->count; i++) {
        cJSON_AddStringToObject(root, state->pairs[i].key, state->pairs[i].value);
    }

    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json) return KNISHIO_ERROR_MEMORY;

    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", state->file_path, (int)getpid());

    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        free(json);
        return KNISHIO_ERROR_INVALID_STATE;
    }

    size_t json_len = strlen(json);
    ssize_t written = write(fd, json, json_len);
    free(json);

    if (written < 0 || (size_t)written != json_len) {
        close(fd);
        unlink(tmp_path);
        return KNISHIO_ERROR_INVALID_STATE;
    }

    fsync(fd);
    close(fd);

    if (rename(tmp_path, state->file_path) != 0) {
        unlink(tmp_path);
        return KNISHIO_ERROR_INVALID_STATE;
    }

    return KNISHIO_SUCCESS;
}

static knishio_error_t file_get_item(knishio_storage_backend_t *backend, const char *key, char **value_out) {
    if (!backend || !key || !value_out) return KNISHIO_ERROR_NULL_POINTER;
    file_backend_state_t *state = (file_backend_state_t *)backend->user_data;
    if (!state) return KNISHIO_ERROR_INVALID_STATE;

    *value_out = NULL;
    pthread_mutex_lock(&state->lock);
    for (size_t i = 0; i < state->count; i++) {
        if (strcmp(state->pairs[i].key, key) == 0) {
            *value_out = strdup(state->pairs[i].value);
            break;
        }
    }
    pthread_mutex_unlock(&state->lock);
    return KNISHIO_SUCCESS;
}

static knishio_error_t file_set_item(knishio_storage_backend_t *backend, const char *key, const char *value) {
    if (!backend || !key || !value) return KNISHIO_ERROR_NULL_POINTER;
    file_backend_state_t *state = (file_backend_state_t *)backend->user_data;
    if (!state) return KNISHIO_ERROR_INVALID_STATE;

    pthread_mutex_lock(&state->lock);
    bool updated = false;
    for (size_t i = 0; i < state->count; i++) {
        if (strcmp(state->pairs[i].key, key) == 0) {
            free(state->pairs[i].value);
            state->pairs[i].value = strdup(value);
            updated = true;
            break;
        }
    }

    if (!updated) {
        if (state->count >= state->capacity) {
            size_t new_cap = state->capacity ? state->capacity * 2 : 16;
            kv_pair_t *new_pairs = realloc(state->pairs, new_cap * sizeof(kv_pair_t));
            if (!new_pairs) {
                pthread_mutex_unlock(&state->lock);
                return KNISHIO_ERROR_MEMORY;
            }
            state->pairs = new_pairs;
            state->capacity = new_cap;
        }
        state->pairs[state->count].key = strdup(key);
        state->pairs[state->count].value = strdup(value);
        state->count++;
    }

    knishio_error_t err = persist_file_backend_locked(state);
    pthread_mutex_unlock(&state->lock);
    return err;
}

static knishio_error_t file_remove_item(knishio_storage_backend_t *backend, const char *key, bool *existed_out) {
    if (!backend || !key) return KNISHIO_ERROR_NULL_POINTER;
    file_backend_state_t *state = (file_backend_state_t *)backend->user_data;
    if (!state) return KNISHIO_ERROR_INVALID_STATE;

    bool found = false;
    pthread_mutex_lock(&state->lock);
    for (size_t i = 0; i < state->count; i++) {
        if (strcmp(state->pairs[i].key, key) == 0) {
            found = true;
            free(state->pairs[i].key);
            free(state->pairs[i].value);
            for (size_t j = i; j + 1 < state->count; j++) {
                state->pairs[j] = state->pairs[j + 1];
            }
            state->count--;
            break;
        }
    }

    knishio_error_t err = KNISHIO_SUCCESS;
    if (found) {
        err = persist_file_backend_locked(state);
    }
    pthread_mutex_unlock(&state->lock);

    if (existed_out) *existed_out = found;
    return err;
}

static knishio_error_t file_keys(knishio_storage_backend_t *backend, char ***keys_out, size_t *count_out) {
    if (!backend || !keys_out || !count_out) return KNISHIO_ERROR_NULL_POINTER;
    file_backend_state_t *state = (file_backend_state_t *)backend->user_data;
    if (!state) return KNISHIO_ERROR_INVALID_STATE;

    pthread_mutex_lock(&state->lock);
    size_t count = state->count;
    char **keys = NULL;
    if (count > 0) {
        keys = calloc(count, sizeof(char *));
        if (!keys) {
            pthread_mutex_unlock(&state->lock);
            return KNISHIO_ERROR_MEMORY;
        }
        for (size_t i = 0; i < count; i++) {
            keys[i] = strdup(state->pairs[i].key);
        }
    }
    pthread_mutex_unlock(&state->lock);

    *keys_out = keys;
    *count_out = count;
    return KNISHIO_SUCCESS;
}

static void file_free_backend(knishio_storage_backend_t *backend) {
    if (!backend) return;
    file_backend_state_t *state = (file_backend_state_t *)backend->user_data;
    if (state) {
        pthread_mutex_lock(&state->lock);
        for (size_t i = 0; i < state->count; i++) {
            free(state->pairs[i].key);
            free(state->pairs[i].value);
        }
        free(state->pairs);
        free(state->file_path);
        pthread_mutex_unlock(&state->lock);
        pthread_mutex_destroy(&state->lock);
        free(state);
    }
    free(backend);
}

knishio_error_t knishio_file_storage_backend_create(const char *file_path, knishio_storage_backend_t **backend_out) {
    if (!file_path || !backend_out) return KNISHIO_ERROR_NULL_POINTER;

    knishio_storage_backend_t *backend = calloc(1, sizeof(knishio_storage_backend_t));
    if (!backend) return KNISHIO_ERROR_MEMORY;

    file_backend_state_t *state = calloc(1, sizeof(file_backend_state_t));
    if (!state) {
        free(backend);
        return KNISHIO_ERROR_MEMORY;
    }

    pthread_mutex_init(&state->lock, NULL);
    state->file_path = strdup(file_path);
    ensure_parent_dir(file_path);

    /* Read existing file if present */
    FILE *f = fopen(file_path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            char *buf = malloc((size_t)sz + 1);
            if (buf) {
                size_t rd = fread(buf, 1, (size_t)sz, f);
                buf[rd] = '\0';
                cJSON *root = cJSON_Parse(buf);
                if (root && cJSON_IsObject(root)) {
                    cJSON *child = root->child;
                    while (child) {
                        if (cJSON_IsString(child) && child->string) {
                            if (state->count >= state->capacity) {
                                size_t new_cap = state->capacity ? state->capacity * 2 : 16;
                                kv_pair_t *new_pairs = realloc(state->pairs, new_cap * sizeof(kv_pair_t));
                                if (new_pairs) {
                                    state->pairs = new_pairs;
                                    state->capacity = new_cap;
                                }
                            }
                            if (state->count < state->capacity) {
                                state->pairs[state->count].key = strdup(child->string);
                                state->pairs[state->count].value = strdup(cJSON_GetStringValue(child));
                                state->count++;
                            }
                        }
                        child = child->next;
                    }
                    cJSON_Delete(root);
                }
                free(buf);
            }
        }
        fclose(f);
    }

    backend->user_data = state;
    backend->get_item = file_get_item;
    backend->set_item = file_set_item;
    backend->remove_item = file_remove_item;
    backend->keys = file_keys;
    backend->free_backend = file_free_backend;

    *backend_out = backend;
    return KNISHIO_SUCCESS;
}

void knishio_storage_backend_free(knishio_storage_backend_t *backend) {
    if (!backend) return;
    if (backend->free_backend) {
        backend->free_backend(backend);
    } else {
        free(backend);
    }
}

knishio_error_t knishio_storage_backend_delete_secret(
    knishio_storage_backend_t *backend,
    const char *bundle_hash,
    bool *existed_out
) {
    if (!backend || !bundle_hash) {
        return KNISHIO_ERROR_NULL_POINTER;
    }
    if (!backend->remove_item) {
        return KNISHIO_ERROR_NOT_IMPLEMENTED;
    }

    char *sec_key = knishio_storage_build_key(KNISHIO_SECRET_STORAGE_PREFIX, bundle_hash);
    if (!sec_key) return KNISHIO_ERROR_MEMORY;

    char *rec_key = knishio_storage_build_key(KNISHIO_RECOVERY_KEY_PREFIX, bundle_hash);
    if (!rec_key) {
        free(sec_key);
        return KNISHIO_ERROR_MEMORY;
    }
    bool sec_existed = false;
    bool rec_existed = false;
    knishio_error_t err1 = backend->remove_item(backend, sec_key, &sec_existed);
    knishio_error_t err2 = backend->remove_item(backend, rec_key, &rec_existed);

    free(sec_key);
    free(rec_key);

    if (existed_out) {
        *existed_out = (sec_existed || rec_existed);
    }

    if (err1 != KNISHIO_SUCCESS) return err1;
    if (err2 != KNISHIO_SUCCESS) return err2;
    return KNISHIO_SUCCESS;
}
