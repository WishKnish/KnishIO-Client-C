#ifndef KNISHIO_CRYPTO_CIPHER_HASH_H
#define KNISHIO_CRYPTO_CIPHER_HASH_H

/**
 * @file cipher_hash.h
 * @brief PQ-transport (Phase E): the canonical ML-KEM "CipherHash" encrypted-transport helpers.
 *
 * The client wraps a GraphQL request body in a CipherHash envelope encrypted to the validator's
 * ML-KEM public key, and decrypts the validator's response back to the inner GraphQL JSON. The
 * envelope, the hashShare keying, and the request(string)/response(object) plaintext shapes match
 * the Rust validator + all other SDKs.
 *
 * Decryption is dual-identity: a wallet decapsulates a ciphertext at EITHER parameter set by
 * deriving the matching keypair from its own (parameter-set-independent) 64-byte seed. Reading a
 * record addressed to our own ML-KEM-768 identity downgrades nothing — its confidentiality was
 * fixed at 768 by the sender. Encapsulation stays strict: knishio_cipher_hash_encrypt() only ever
 * encrypts to the recipient key it is handed, and a wallet still advertises exactly one public key.
 */

#include <stddef.h>
#include <stdint.h>
#include "../error/context.h"

/* Opaque here: cipher_hash only ever reads a wallet's identity material. */
typedef struct knishio_wallet knishio_wallet_t;

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
 * @brief Decrypt ONE CipherHash envelope (`cipherText` + `encryptedMessage`) with `wallet`'s own
 *        ML-KEM identity, at EITHER parameter set. A ciphertext whose decoded length matches the
 *        wallet's configured set uses the stored private key; one matching the OTHER set derives
 *        that identity on demand from the wallet's seed, decapsulates, and releases the derived
 *        private key immediately. Any other length returns KNISHIO_ERROR_INVALID_ARGS.
 * @param wallet This wallet (supplies the ML-KEM identity).
 * @param cipher_text_b64 base64 ML-KEM ciphertext (1568 bytes for 1024, 1088 for 768).
 * @param encrypted_message_b64 base64 AES-256-GCM output [IV‖ct‖tag].
 * @param plaintext_out Output: the raw decrypted text (malloc'd null-terminated; caller frees).
 * @return KNISHIO_SUCCESS on success.
 */
knishio_error_t knishio_cipher_hash_decrypt_envelope(const knishio_wallet_t* wallet,
                                                     const char* cipher_text_b64,
                                                     const char* encrypted_message_b64,
                                                     char** plaintext_out);

/**
 * @brief Decrypt the CipherHash response map (stringified JSON) addressed to THIS wallet → the
 *        RAW inner GraphQL response JSON (NOT JSON-decoded; the validator encrypts the response
 *        OBJECT directly).
 *
 *        The lookup tries `hashShare(configured pubkey)` first, then `hashShare(the wallet's other
 *        parameter set's pubkey)`, so an envelope addressed by a pre-bump ML-KEM-768 sender is
 *        still found by a wallet configured at ML-KEM-1024.
 * @param map_json The stringified response map.
 * @param wallet This wallet (supplies the hashShare keys and the ML-KEM identity).
 * @param plaintext_out Output: the raw inner response JSON (malloc'd null-terminated; caller frees).
 * @return KNISHIO_SUCCESS on success; an error if no matching entry / decrypt fails.
 */
knishio_error_t knishio_cipher_hash_decrypt(const char* map_json,
                                            const knishio_wallet_t* wallet,
                                            char** plaintext_out);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_CRYPTO_CIPHER_HASH_H */
