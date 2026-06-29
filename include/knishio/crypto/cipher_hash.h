#ifndef KNISHIO_CRYPTO_CIPHER_HASH_H
#define KNISHIO_CRYPTO_CIPHER_HASH_H

/**
 * @file cipher_hash.h
 * @brief PQ-transport (Phase E): the canonical ML-KEM768 "CipherHash" encrypted-transport helpers.
 *
 * The client wraps a GraphQL request body in a CipherHash envelope encrypted to the validator's
 * ML-KEM public key, and decrypts the validator's response back to the inner GraphQL JSON. The
 * envelope, the hashShare keying, and the request(string)/response(object) plaintext shapes match
 * the Rust validator + all other SDKs.
 */

#include <stddef.h>
#include <stdint.h>
#include "../error/context.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The canonical ML-KEM CipherHash transport query (matches the validator + all SDKs). */
#define KNISHIO_CIPHER_HASH_QUERY "query ( $Hash: String! ) { CipherHash ( Hash: $Hash ) { hash } }"

/**
 * @brief Canonical cross-SDK hashShare for a public key: standard base64 of SHAKE256(pubkey, 8 bytes).
 *        Byte-matches the validator's hash_share + the JS/Kotlin/PHP/TS/Python/C++ hashShare.
 * @param pubkey_b64 The recipient ML-KEM public key (base64 string).
 * @param out Output: the hashShare (malloc'd null-terminated string; caller frees).
 * @return KNISHIO_SUCCESS on success.
 */
knishio_error_t knishio_cipher_hash_share(const char* pubkey_b64, char** out);

/**
 * @brief Build the canonical single-recipient CipherHash request envelope (a stringified JSON map
 *        `{ "<hashShare(server_pubkey_b64)>": {cipherText, encryptedMessage} }`) for `body`
 *        encrypted to `server_pubkey_b64`. `body` is encrypted as a JSON string value (the validator
 *        recovers it as a string and parses the inner request).
 * @param body The GraphQL request body string (`{"query":...,"variables":...}`).
 * @param server_pubkey_b64 The validator's ML-KEM public key (base64).
 * @param envelope_out Output: the stringified envelope (malloc'd; caller frees).
 * @return KNISHIO_SUCCESS on success.
 */
knishio_error_t knishio_cipher_hash_encrypt(const char* body, const char* server_pubkey_b64,
                                            char** envelope_out);

/**
 * @brief Decrypt the CipherHash response map (stringified JSON) addressed to THIS wallet
 *        (`hashShare(my_pubkey_b64)`) using `my_privkey` (raw 2400-byte ML-KEM private key) → the
 *        RAW inner GraphQL response JSON (NOT JSON-decoded; the validator encrypts the response
 *        OBJECT directly).
 * @param map_json The stringified response map.
 * @param my_pubkey_b64 This wallet's ML-KEM public key (base64) — for the hashShare lookup.
 * @param my_privkey This wallet's raw ML-KEM private key.
 * @param my_privkey_len Length of `my_privkey` (must be 2400).
 * @param plaintext_out Output: the raw inner response JSON (malloc'd null-terminated; caller frees).
 * @return KNISHIO_SUCCESS on success; an error if no matching entry / decrypt fails.
 */
knishio_error_t knishio_cipher_hash_decrypt(const char* map_json, const char* my_pubkey_b64,
                                            const uint8_t* my_privkey, size_t my_privkey_len,
                                            char** plaintext_out);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CRYPTO_CIPHER_HASH_H */
