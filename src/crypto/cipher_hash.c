/**
 * @file cipher_hash.c
 * @brief PQ-transport (Phase E): canonical ML-KEM768 CipherHash transport helpers.
 *
 * Built from the low-level primitives (encapsulate/decapsulate + AES-256-GCM + base64), mirroring
 * the self-test's canonical envelope: cipherText = base64(KEM ciphertext, 1088), encryptedMessage =
 * base64(AES-GCM output [IV‖ct‖tag]). Request = encrypt the body as a JSON string value; response =
 * raw-decrypt the validator-encrypted object. All allocations are owned + freed here.
 */

#include "knishio/crypto/cipher_hash.h"
#include "knishio/crypto/mlkem768.h"
#include "knishio/crypto/aes_gcm.h"
#include "knishio/crypto/shake256.h"
#include "knishio/utils/encoding.h"
#include "knishio/error/context.h"

#include <stdlib.h>
#include <string.h>

/* cJSON (same include guard as src/json/parser.c). */
#ifdef __has_include
  #if __has_include(<cjson/cJSON.h>)
    #include <cjson/cJSON.h>
  #elif __has_include(<cJSON.h>)
    #include <cJSON.h>
  #endif
#else
  #include <cjson/cJSON.h>
#endif

#define KNISHIO_MLKEM768_PUBKEY_BYTES     1184
#define KNISHIO_MLKEM768_CIPHERTEXT_BYTES 1088
#define KNISHIO_MLKEM768_PRIVKEY_BYTES    2400

knishio_error_t knishio_cipher_hash_share(const char* pubkey_b64, char** out) {
    if (!pubkey_b64 || !out) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    *out = NULL;
    uint8_t digest[8];
    if (!knishio_shake256_hash_raw((const uint8_t*)pubkey_b64, strlen(pubkey_b64),
                                   digest, sizeof(digest))) {
        return KNISHIO_ERROR_CRYPTO;
    }
    if (!knishio_base64_encode(digest, sizeof(digest), out)) {
        return KNISHIO_ERROR_MEMORY;
    }
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_cipher_hash_encrypt(const char* body, const char* server_pubkey_b64,
                                            char** envelope_out) {
    if (!body || !server_pubkey_b64 || !envelope_out) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    *envelope_out = NULL;

    knishio_error_t err = KNISHIO_SUCCESS;
    unsigned char* pubkey_raw = NULL;
    size_t pubkey_raw_len = 0;
    cJSON* msg_json = NULL;
    char* json_msg = NULL;
    uint8_t* aes_out = NULL;
    size_t aes_len = 0;
    char* cipher_text_b64 = NULL;
    char* enc_msg_b64 = NULL;
    char* share = NULL;
    cJSON* envelope = NULL;
    cJSON* inner = NULL;

    /* 1. Decode + validate the server's ML-KEM public key (exactly 1184 bytes). A wrong length —
     *    e.g. a node predating the PQ-transport build that advertised a non-ML-KEM `key` — returns a
     *    clean KNISHIO_ERROR_INVALID_ARGS rather than failing deep inside encapsulate. This is the C
     *    analogue of the other SDKs' encrypt guards (which throw an actionable error). */
    if (!knishio_base64_decode(server_pubkey_b64, &pubkey_raw, &pubkey_raw_len)
        || pubkey_raw_len != KNISHIO_MLKEM768_PUBKEY_BYTES) {
        err = KNISHIO_ERROR_INVALID_ARGS;
        goto done;
    }

    /* 2. JSON-wrap the body string (a JSON string literal — the validator recovers a String). */
    msg_json = cJSON_CreateString(body);
    if (!msg_json) { err = KNISHIO_ERROR_MEMORY; goto done; }
    json_msg = cJSON_PrintUnformatted(msg_json);
    if (!json_msg) { err = KNISHIO_ERROR_MEMORY; goto done; }

    /* 3. Encapsulate (KEM) + AES-256-GCM encrypt with the shared secret. */
    knishio_mlkem768_ciphertext_t kem_ct;
    knishio_mlkem768_shared_secret_t shared_secret;
    err = knishio_mlkem768_encapsulate(pubkey_raw, &kem_ct, &shared_secret);
    if (err != KNISHIO_SUCCESS) { goto done; }
    err = knishio_aes_gcm_encrypt((const uint8_t*)json_msg, strlen(json_msg),
                                  shared_secret.shared_secret, &aes_out, &aes_len);
    if (err != KNISHIO_SUCCESS) { goto done; }

    /* 4. Base64 both halves of the canonical envelope. */
    if (!knishio_base64_encode(kem_ct.ciphertext, sizeof(kem_ct.ciphertext), &cipher_text_b64)
        || !knishio_base64_encode(aes_out, aes_len, &enc_msg_b64)) {
        err = KNISHIO_ERROR_MEMORY;
        goto done;
    }

    /* 5. hashShare(server pubkey) keys the single-recipient map. */
    err = knishio_cipher_hash_share(server_pubkey_b64, &share);
    if (err != KNISHIO_SUCCESS) { goto done; }

    /* 6. Build { "<share>": {cipherText, encryptedMessage} } and stringify. */
    envelope = cJSON_CreateObject();
    inner = cJSON_CreateObject();
    if (!envelope || !inner) { err = KNISHIO_ERROR_MEMORY; goto done; }
    cJSON_AddStringToObject(inner, "cipherText", cipher_text_b64);
    cJSON_AddStringToObject(inner, "encryptedMessage", enc_msg_b64);
    cJSON_AddItemToObject(envelope, share, inner);
    inner = NULL;  /* ownership moved into `envelope` */

    *envelope_out = cJSON_PrintUnformatted(envelope);
    if (!*envelope_out) { err = KNISHIO_ERROR_MEMORY; }

done:
    if (pubkey_raw) free(pubkey_raw);
    if (msg_json) cJSON_Delete(msg_json);
    if (json_msg) free(json_msg);
    if (aes_out) free(aes_out);
    if (cipher_text_b64) free(cipher_text_b64);
    if (enc_msg_b64) free(enc_msg_b64);
    if (share) free(share);
    if (inner) cJSON_Delete(inner);
    if (envelope) cJSON_Delete(envelope);
    return err;
}

knishio_error_t knishio_cipher_hash_decrypt(const char* map_json, const char* my_pubkey_b64,
                                            const uint8_t* my_privkey, size_t my_privkey_len,
                                            char** plaintext_out) {
    if (!map_json || !my_pubkey_b64 || !my_privkey || !plaintext_out) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    if (my_privkey_len != KNISHIO_MLKEM768_PRIVKEY_BYTES) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    *plaintext_out = NULL;

    knishio_error_t err = KNISHIO_SUCCESS;
    char* share = NULL;
    cJSON* map = NULL;
    unsigned char* kem_raw = NULL;
    size_t kem_raw_len = 0;
    unsigned char* enc_raw = NULL;
    size_t enc_raw_len = 0;
    uint8_t* pt = NULL;
    size_t pt_len = 0;

    err = knishio_cipher_hash_share(my_pubkey_b64, &share);
    if (err != KNISHIO_SUCCESS) { goto done; }

    map = cJSON_Parse(map_json);
    if (!map) { err = KNISHIO_ERROR_INVALID_ARGS; goto done; }

    cJSON* entry = cJSON_GetObjectItem(map, share);
    if (!entry) { err = KNISHIO_ERROR_INVALID_STATE; goto done; }  /* no envelope for this wallet */

    cJSON* ct = cJSON_GetObjectItem(entry, "cipherText");
    cJSON* em = cJSON_GetObjectItem(entry, "encryptedMessage");
    if (!cJSON_IsString(ct) || !cJSON_IsString(em)) { err = KNISHIO_ERROR_INVALID_ARGS; goto done; }

    if (!knishio_base64_decode(cJSON_GetStringValue(ct), &kem_raw, &kem_raw_len)
        || kem_raw_len != KNISHIO_MLKEM768_CIPHERTEXT_BYTES) {
        err = KNISHIO_ERROR_INVALID_ARGS;
        goto done;
    }
    if (!knishio_base64_decode(cJSON_GetStringValue(em), &enc_raw, &enc_raw_len)) {
        err = KNISHIO_ERROR_INVALID_ARGS;
        goto done;
    }

    knishio_mlkem768_ciphertext_t kem_ct;
    memcpy(kem_ct.ciphertext, kem_raw, KNISHIO_MLKEM768_CIPHERTEXT_BYTES);
    knishio_mlkem768_shared_secret_t shared_secret;
    err = knishio_mlkem768_decapsulate(my_privkey, &kem_ct, &shared_secret);
    if (err != KNISHIO_SUCCESS) { goto done; }

    err = knishio_aes_gcm_decrypt(enc_raw, enc_raw_len, shared_secret.shared_secret, &pt, &pt_len);
    if (err != KNISHIO_SUCCESS) { goto done; }

    /* The validator encrypts the response OBJECT → `pt` is the raw inner response JSON. */
    *plaintext_out = malloc(pt_len + 1);
    if (!*plaintext_out) { err = KNISHIO_ERROR_MEMORY; goto done; }
    memcpy(*plaintext_out, pt, pt_len);
    (*plaintext_out)[pt_len] = '\0';

done:
    if (share) free(share);
    if (map) cJSON_Delete(map);
    if (kem_raw) free(kem_raw);
    if (enc_raw) free(enc_raw);
    if (pt) free(pt);
    return err;
}
