#include "knishio/storage/types.h"
#include "knishio/utils/memory.h"
#include "storage_internal.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void knishio_storage_options_init(knishio_storage_options_t *options) {
    if (!options) return;
    options->label = NULL;
    options->passphrase = NULL;
    options->recovery_passphrase = NULL;
    options->allow_unrecoverable = false;
}

void knishio_secret_metadata_init(knishio_secret_metadata_t *meta) {
    if (!meta) return;
    meta->bundle_hash = NULL;
    meta->label = NULL;
    meta->created_at = 0;
    meta->hardware_backed = false;
    meta->provider_type = NULL;
}

void knishio_secret_metadata_cleanup(knishio_secret_metadata_t *meta) {
    if (!meta) return;
    if (meta->bundle_hash) {
        free(meta->bundle_hash);
        meta->bundle_hash = NULL;
    }
    if (meta->label) {
        free(meta->label);
        meta->label = NULL;
    }
    if (meta->provider_type) {
        free(meta->provider_type);
        meta->provider_type = NULL;
    }
    meta->created_at = 0;
    meta->hardware_backed = false;
}

void knishio_encrypted_payload_init(knishio_encrypted_payload_t *payload) {
    if (!payload) return;
    payload->version = 1;
    payload->ciphertext = NULL;
    payload->iv = NULL;
    payload->salt = NULL;
    payload->algorithm = NULL;
    payload->iterations = KNISHIO_STORAGE_DEFAULT_ITERATIONS;
    knishio_secret_metadata_init(&payload->metadata);
}

void knishio_encrypted_payload_cleanup(knishio_encrypted_payload_t *payload) {
    if (!payload) return;
    if (payload->ciphertext) {
        free(payload->ciphertext);
        payload->ciphertext = NULL;
    }
    if (payload->iv) {
        free(payload->iv);
        payload->iv = NULL;
    }
    if (payload->salt) {
        free(payload->salt);
        payload->salt = NULL;
    }
    if (payload->algorithm) {
        free(payload->algorithm);
        payload->algorithm = NULL;
    }
    payload->version = 1;
    payload->iterations = 0;
    knishio_secret_metadata_cleanup(&payload->metadata);
}

char* knishio_encrypted_payload_to_json(const knishio_encrypted_payload_t *payload) {
    if (!payload) return NULL;

    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddNumberToObject(root, "version", (double)(payload->version ? payload->version : 1));
    cJSON_AddStringToObject(root, "ciphertext", payload->ciphertext ? payload->ciphertext : "");
    cJSON_AddStringToObject(root, "iv", payload->iv ? payload->iv : "");
    cJSON_AddStringToObject(root, "salt", payload->salt ? payload->salt : "");
    cJSON_AddStringToObject(root, "algorithm", payload->algorithm ? payload->algorithm : "AES-GCM");
    cJSON_AddNumberToObject(root, "iterations", (double)(payload->iterations ? payload->iterations : KNISHIO_STORAGE_DEFAULT_ITERATIONS));

    cJSON *meta = cJSON_CreateObject();
    if (!meta) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON_AddStringToObject(meta, "bundleHash", payload->metadata.bundle_hash ? payload->metadata.bundle_hash : "");
    if (payload->metadata.label != NULL) {
        cJSON_AddStringToObject(meta, "label", payload->metadata.label);
    }
    cJSON_AddNumberToObject(meta, "createdAt", (double)payload->metadata.created_at);
    cJSON_AddBoolToObject(meta, "hardwareBacked", payload->metadata.hardware_backed);
    cJSON_AddStringToObject(meta, "providerType", payload->metadata.provider_type ? payload->metadata.provider_type : "aes-gcm");

    cJSON_AddItemToObject(root, "metadata", meta);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

knishio_error_t knishio_encrypted_payload_from_json(const char *json_str, knishio_encrypted_payload_t *payload) {
    if (!json_str || !payload) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        return KNISHIO_ERROR_INVALID_JSON;
    }

    knishio_encrypted_payload_init(payload);

    cJSON *version_item = cJSON_GetObjectItem(root, "version");
    if (version_item && cJSON_IsNumber(version_item)) {
        payload->version = (uint32_t)version_item->valuedouble;
    } else {
        payload->version = 1;
    }

    cJSON *ct_item = cJSON_GetObjectItem(root, "ciphertext");
    if (!ct_item || !cJSON_IsString(ct_item)) {
        cJSON_Delete(root);
        knishio_encrypted_payload_cleanup(payload);
        return KNISHIO_ERROR_INVALID_JSON;
    }
    payload->ciphertext = strdup(cJSON_GetStringValue(ct_item));

    cJSON *iv_item = cJSON_GetObjectItem(root, "iv");
    if (!iv_item || !cJSON_IsString(iv_item)) {
        cJSON_Delete(root);
        knishio_encrypted_payload_cleanup(payload);
        return KNISHIO_ERROR_INVALID_JSON;
    }
    payload->iv = strdup(cJSON_GetStringValue(iv_item));

    cJSON *salt_item = cJSON_GetObjectItem(root, "salt");
    if (!salt_item || !cJSON_IsString(salt_item)) {
        cJSON_Delete(root);
        knishio_encrypted_payload_cleanup(payload);
        return KNISHIO_ERROR_INVALID_JSON;
    }
    payload->salt = strdup(cJSON_GetStringValue(salt_item));

    cJSON *algo_item = cJSON_GetObjectItem(root, "algorithm");
    if (algo_item && cJSON_IsString(algo_item)) {
        payload->algorithm = strdup(cJSON_GetStringValue(algo_item));
    } else {
        payload->algorithm = strdup("AES-GCM");
    }

    cJSON *iter_item = cJSON_GetObjectItem(root, "iterations");
    if (iter_item && cJSON_IsNumber(iter_item)) {
        payload->iterations = (uint32_t)iter_item->valuedouble;
    } else {
        payload->iterations = KNISHIO_STORAGE_DEFAULT_ITERATIONS;
    }

    cJSON *meta_obj = cJSON_GetObjectItem(root, "metadata");
    if (!meta_obj || !cJSON_IsObject(meta_obj)) {
        cJSON_Delete(root);
        knishio_encrypted_payload_cleanup(payload);
        return KNISHIO_ERROR_INVALID_JSON;
    }

    /* bundleHash (or alias bundle_hash) */
    cJSON *bh_item = cJSON_GetObjectItem(meta_obj, "bundleHash");
    if (!bh_item) {
        bh_item = cJSON_GetObjectItem(meta_obj, "bundle_hash");
    }
    if (bh_item && cJSON_IsString(bh_item)) {
        payload->metadata.bundle_hash = strdup(cJSON_GetStringValue(bh_item));
    }

    /* label (optional, omit when absent) */
    cJSON *lbl_item = cJSON_GetObjectItem(meta_obj, "label");
    if (lbl_item && cJSON_IsString(lbl_item)) {
        payload->metadata.label = strdup(cJSON_GetStringValue(lbl_item));
    } else {
        payload->metadata.label = NULL;
    }

    /* createdAt (or alias created_at) */
    cJSON *ca_item = cJSON_GetObjectItem(meta_obj, "createdAt");
    if (!ca_item) {
        ca_item = cJSON_GetObjectItem(meta_obj, "created_at");
    }
    if (ca_item && cJSON_IsNumber(ca_item)) {
        payload->metadata.created_at = (int64_t)ca_item->valuedouble;
    }

    /* hardwareBacked (or alias hardware_backed) */
    cJSON *hb_item = cJSON_GetObjectItem(meta_obj, "hardwareBacked");
    if (!hb_item) {
        hb_item = cJSON_GetObjectItem(meta_obj, "hardware_backed");
    }
    if (hb_item) {
        payload->metadata.hardware_backed = cJSON_IsTrue(hb_item);
    }

    /* providerType (or alias provider_type) */
    cJSON *pt_item = cJSON_GetObjectItem(meta_obj, "providerType");
    if (!pt_item) {
        pt_item = cJSON_GetObjectItem(meta_obj, "provider_type");
    }
    if (pt_item && cJSON_IsString(pt_item)) {
        payload->metadata.provider_type = strdup(cJSON_GetStringValue(pt_item));
    }

    cJSON_Delete(root);
    return KNISHIO_SUCCESS;
}

char *knishio_storage_build_key(const char *prefix, const char *bundle_hash) {
    if (!prefix || !bundle_hash) {
        return NULL;
    }
    size_t prefix_len = strlen(prefix);
    size_t hash_len = strlen(bundle_hash);
    size_t total_len = prefix_len + hash_len + 1;
    char *key = (char *)malloc(total_len);
    if (!key) {
        return NULL;
    }
    int written = snprintf(key, total_len, "%s%s", prefix, bundle_hash);
    if (written < 0 || (size_t)written >= total_len) {
        free(key);
        return NULL;
    }
    return key;
}
