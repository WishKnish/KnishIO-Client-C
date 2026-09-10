/**
 * @file cipher_hash.c
 * @brief PQ-transport (Phase E): canonical ML-KEM CipherHash transport helpers.
 *
 * Built from the low-level primitives (encapsulate/decapsulate + AES-256-GCM + base64), mirroring
 * the self-test's canonical envelope: cipherText = base64(KEM ciphertext), encryptedMessage =
 * base64(AES-GCM output [IV‖ct‖tag]). Request = encrypt the body as a JSON string value; response =
 * raw-decrypt the validator-encrypted object. All allocations are owned + freed here.
 *
 * Inbound is dual-identity (either parameter set, derived on demand); outbound is strict.
 */

#include "knishio/crypto/cipher_hash.h"
#include "knishio/crypto/mlkem.h"
#include "knishio/crypto/aes_gcm.h"
#include "knishio/crypto/shake256.h"
#include "knishio/utils/encoding.h"
#include "knishio/utils/memory.h"
#include "knishio/wallet.h"
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

#define KNISHIO_MLKEM1024_PUBKEY_BYTES     1568
#define KNISHIO_MLKEM1024_CIPHERTEXT_BYTES 1568
#define KNISHIO_MLKEM1024_PRIVKEY_BYTES    3168

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
        || (pubkey_raw_len != KNISHIO_MLKEM1024_PUBKEY_BYTES && pubkey_raw_len != KNISHIO_MLKEM768_PUBKEY_BYTES)) {
        err = KNISHIO_ERROR_INVALID_ARGS;
        goto done;
    }

    /* 2. JSON-wrap the body string (a JSON string literal — the validator recovers a String). */
    msg_json = cJSON_CreateString(body);
    if (!msg_json) { err = KNISHIO_ERROR_MEMORY; goto done; }
    json_msg = cJSON_PrintUnformatted(msg_json);
    if (!json_msg) { err = KNISHIO_ERROR_MEMORY; goto done; }

    /* 3. Encapsulate (KEM) + AES-256-GCM encrypt with the shared secret. */
    knishio_mlkem_ciphertext_t kem_ct;
    knishio_mlkem_shared_secret_t shared_secret;
    err = knishio_mlkem_encapsulate(pubkey_raw, pubkey_raw_len, &kem_ct, &shared_secret);
    if (err != KNISHIO_SUCCESS) { goto done; }
    err = knishio_aes_gcm_encrypt((const uint8_t*)json_msg, strlen(json_msg),
                                  shared_secret.shared_secret, &aes_out, &aes_len);
    if (err != KNISHIO_SUCCESS) { goto done; }

    /* 4. Base64 both halves of the canonical envelope. */
    if (!knishio_base64_encode(kem_ct.ciphertext, kem_ct.ciphertext_len, &cipher_text_b64)
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

/** The sibling parameter set: the one this wallet is NOT configured at. */
static knishio_mlkem_param_t other_mlkem_param(knishio_mlkem_param_t param) {
    return (param == KNISHIO_MLKEM_768) ? KNISHIO_MLKEM_1024 : KNISHIO_MLKEM_768;
}

static size_t ciphertext_bytes_for(knishio_mlkem_param_t param) {
    return (param == KNISHIO_MLKEM_768) ? KNISHIO_MLKEM768_CIPHERTEXT_BYTES
                                        : KNISHIO_MLKEM1024_CIPHERTEXT_BYTES;
}

/**
 * @brief Derive `wallet`'s ML-KEM keypair at an ARBITRARY parameter set, without mutating the
 *        wallet. The 64-byte (d‖z) seed — generateSecret(wallet key, 128) — takes no
 *        parameter-set input, so every KnishIO wallet can deterministically derive both its
 *        ML-KEM-768 and its ML-KEM-1024 identity from material it already holds. This is the
 *        derivation knishio_wallet_initialize_mlkem() performs for the CONFIGURED set.
 *
 *        The caller owns `keypair` and MUST knishio_secure_zero() it once done: the derived
 *        private key is deliberately never cached on the wallet.
 */
static knishio_error_t derive_mlkem_keypair(const knishio_wallet_t* wallet,
                                            knishio_mlkem_param_t param,
                                            knishio_mlkem_keypair_t* keypair) {
    if (!wallet || !wallet->private_key || !keypair) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }

    char* seed_hex = NULL;
    if (!knishio_generate_secret(wallet->private_key, 128, &seed_hex)) {
        return KNISHIO_ERROR_CRYPTO;
    }
    if (!seed_hex || strlen(seed_hex) != 128) {
        if (seed_hex) free(seed_hex);
        return KNISHIO_ERROR_CRYPTO;
    }

    uint8_t seed[64];
    for (int i = 0; i < 64; i++) {
        char pair[3] = { seed_hex[i * 2], seed_hex[i * 2 + 1], '\0' };
        seed[i] = (uint8_t)strtol(pair, NULL, 16);
    }
    knishio_secure_zero(seed_hex, strlen(seed_hex));
    free(seed_hex);

    knishio_error_t err = knishio_mlkem_keypair_from_seed(keypair, seed, sizeof(seed), param);
    knishio_secure_zero(seed, sizeof(seed));
    return err;
}

knishio_error_t knishio_cipher_hash_decrypt_envelope(const knishio_wallet_t* wallet,
                                                     const char* cipher_text_b64,
                                                     const char* encrypted_message_b64,
                                                     char** plaintext_out) {
    if (!wallet || !cipher_text_b64 || !encrypted_message_b64 || !plaintext_out) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    *plaintext_out = NULL;

    knishio_error_t err = KNISHIO_SUCCESS;
    unsigned char* kem_raw = NULL;
    size_t kem_raw_len = 0;
    unsigned char* enc_raw = NULL;
    size_t enc_raw_len = 0;
    uint8_t* pt = NULL;
    size_t pt_len = 0;
    knishio_mlkem_keypair_t derived;
    bool derived_valid = false;

    if (!knishio_base64_decode(cipher_text_b64, &kem_raw, &kem_raw_len)) {
        err = KNISHIO_ERROR_INVALID_ARGS;
        goto done;
    }
    if (!knishio_base64_decode(encrypted_message_b64, &enc_raw, &enc_raw_len)) {
        err = KNISHIO_ERROR_INVALID_ARGS;
        goto done;
    }

    const knishio_mlkem_param_t configured = knishio_wallet_get_mlkem_param(wallet);
    const knishio_mlkem_param_t sibling = other_mlkem_param(configured);

    /* Dispatch on the DECODED CIPHERTEXT length, not on the wallet's configuration. */
    const uint8_t* privkey = NULL;
    size_t privkey_len = 0;
    if (kem_raw_len == ciphertext_bytes_for(configured)) {
        if (!wallet->privkey_bytes || wallet->privkey_bytes_len == 0) {
            err = KNISHIO_ERROR_INVALID_STATE;
            goto done;
        }
        privkey = wallet->privkey_bytes;
        privkey_len = wallet->privkey_bytes_len;
    } else if (kem_raw_len == ciphertext_bytes_for(sibling)) {
        /* A record addressed to our OTHER identity (e.g. a pre-bump ML-KEM-768 sender). Derive
         * that identity on demand and release it immediately — never cached on the wallet. */
        err = derive_mlkem_keypair(wallet, sibling, &derived);
        if (err != KNISHIO_SUCCESS) { goto done; }
        derived_valid = true;
        privkey = derived.private_key;
        privkey_len = derived.private_key_len;
    } else {
        err = KNISHIO_ERROR_INVALID_ARGS;
        goto done;
    }

    knishio_mlkem_ciphertext_t kem_ct;
    memcpy(kem_ct.ciphertext, kem_raw, kem_raw_len);
    kem_ct.ciphertext_len = kem_raw_len;  /* explicit length: never sizeof(kem_ct.ciphertext) */
    knishio_mlkem_shared_secret_t shared_secret;
    err = knishio_mlkem_decapsulate(privkey, privkey_len, &kem_ct, &shared_secret);
    if (err != KNISHIO_SUCCESS) { goto done; }
    err = knishio_aes_gcm_decrypt(enc_raw, enc_raw_len, shared_secret.shared_secret, &pt, &pt_len);
    knishio_secure_zero(shared_secret.shared_secret, sizeof(shared_secret.shared_secret));
    if (err != KNISHIO_SUCCESS) { goto done; }

    *plaintext_out = malloc(pt_len + 1);
    if (!*plaintext_out) { err = KNISHIO_ERROR_MEMORY; goto done; }
    memcpy(*plaintext_out, pt, pt_len);
    (*plaintext_out)[pt_len] = '\0';

done:
    if (derived_valid) knishio_secure_zero(&derived, sizeof(derived));
    if (kem_raw) free(kem_raw);
    if (enc_raw) free(enc_raw);
    if (pt) free(pt);
    return err;
}

knishio_error_t knishio_cipher_hash_decrypt(const char* map_json,
                                            const knishio_wallet_t* wallet,
                                            char** plaintext_out) {
    if (!map_json || !wallet || !wallet->pubkey || !plaintext_out) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    *plaintext_out = NULL;

    knishio_error_t err = KNISHIO_SUCCESS;
    char* share = NULL;
    char* sibling_share = NULL;
    char* sibling_pubkey_b64 = NULL;
    cJSON* map = NULL;
    knishio_mlkem_keypair_t derived;
    bool derived_valid = false;

    map = cJSON_Parse(map_json);
    if (!map) { err = KNISHIO_ERROR_INVALID_ARGS; goto done; }

    err = knishio_cipher_hash_share(wallet->pubkey, &share);
    if (err != KNISHIO_SUCCESS) { goto done; }

    cJSON* entry = cJSON_GetObjectItem(map, share);
    if (!entry) {
        /* A pre-bump sender addressed the envelope to our OTHER identity's hash share. Derive that
         * identity's PUBLIC key to compute its share; the private half is derived again (and
         * released) inside decrypt_envelope, so nothing long-lived holds a second private key. */
        if (derive_mlkem_keypair(wallet, other_mlkem_param(knishio_wallet_get_mlkem_param(wallet)),
                                 &derived) == KNISHIO_SUCCESS) {
            derived_valid = true;
            if (knishio_base64_encode(derived.public_key, derived.public_key_len,
                                      &sibling_pubkey_b64)
                && knishio_cipher_hash_share(sibling_pubkey_b64, &sibling_share) == KNISHIO_SUCCESS) {
                entry = cJSON_GetObjectItem(map, sibling_share);
            }
        }
    }
    if (!entry) { err = KNISHIO_ERROR_INVALID_STATE; goto done; }  /* no envelope for this wallet */

    cJSON* ct = cJSON_GetObjectItem(entry, "cipherText");
    cJSON* em = cJSON_GetObjectItem(entry, "encryptedMessage");
    if (!cJSON_IsString(ct) || !cJSON_IsString(em)) { err = KNISHIO_ERROR_INVALID_ARGS; goto done; }

    err = knishio_cipher_hash_decrypt_envelope(wallet, cJSON_GetStringValue(ct),
                                               cJSON_GetStringValue(em), plaintext_out);

done:
    if (derived_valid) knishio_secure_zero(&derived, sizeof(derived));
    if (share) free(share);
    if (sibling_share) free(sibling_share);
    if (sibling_pubkey_b64) free(sibling_pubkey_b64);
    if (map) cJSON_Delete(map);
    return err;
}
