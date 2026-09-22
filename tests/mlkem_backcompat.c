/**
 * @file mlkem_backcompat.c
 * @brief A build whose default ML-KEM parameter set is 1024 must still READ records a pre-bump
 *        (ML-KEM-768-only) peer produced, and must still restore a pre-bump session as 768.
 *
 * Every assertion is driven by the committed cross-SDK vectors:
 *   - vectors.mlkem768.decrypt              — a frozen 768 CipherHash envelope (pre-migration)
 *   - vectors.legacyMlkem768AuthMolecule    — a frozen signed U+I auth molecule whose U-atom
 *                                             walletPubkey meta is an ML-KEM-768 key
 *
 * The frozen-molecule leg deserializes the molecule with knishio_molecule_from_json() and runs the
 * full knishio_molecule_check() on it — molecular hash, WOTS+ one-time signature and isotope
 * checks — so it proves a 1024 build accepts the pre-bump auth molecule, not just its hash.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "knishio/knishio.h"
#include "knishio/wallet.h"
#include "knishio/molecule.h"
#include "knishio/atom.h"
#include "knishio/meta.h"
#include "knishio/auth_token.h"
#include "knishio/crypto/cipher_hash.h"
#include "knishio/crypto/mlkem.h"
#include "knishio/utils/encoding.h"

#ifndef KNISHIO_TEST_VECTORS_PATH
#define KNISHIO_TEST_VECTORS_PATH "tests/fixtures/cross-platform-test-vectors.json"
#endif

static int g_failures = 0;

static void check(bool ok, const char *name, const char *detail) {
    printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail ? " — " : "", detail ? detail : "");
    if (!ok) g_failures++;
}

static char *slurp(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = NULL;
    if (sz > 0) {
        buf = malloc((size_t)sz + 1);
        if (buf) {
            size_t rd = fread(buf, 1, (size_t)sz, f);
            buf[rd] = '\0';
        }
    }
    fclose(f);
    return buf;
}

/** Raw byte length of a base64 string (0 on decode failure). */
static size_t b64_bytes(const char *b64) {
    unsigned char *raw = NULL;
    size_t len = 0;
    if (!b64 || !knishio_base64_decode(b64, &raw, &len)) return 0;
    free(raw);
    return len;
}

/** The frozen envelopes carry a JSON-encoded plaintext (JS JSON.stringify'd it before AES-GCM). */
static char *json_unwrap(const char *text) {
    if (!text) return NULL;
    cJSON *parsed = cJSON_Parse(text);
    char *out = NULL;
    if (parsed && cJSON_IsString(parsed)) {
        out = strdup(cJSON_GetStringValue(parsed));
    }
    if (parsed) cJSON_Delete(parsed);
    return out;
}

/** Wrap one envelope in a single-recipient CipherHash map keyed by `share`. */
static char *build_map(const char *share, const char *cipher_text, const char *encrypted_message) {
    cJSON *map = cJSON_CreateObject();
    cJSON *inner = cJSON_CreateObject();
    cJSON_AddStringToObject(inner, "cipherText", cipher_text);
    cJSON_AddStringToObject(inner, "encryptedMessage", encrypted_message);
    cJSON_AddItemToObject(map, share, inner);
    char *out = cJSON_PrintUnformatted(map);
    cJSON_Delete(map);
    return out;
}

/* ------------------------------------------------------------------ */
/* Step 12: a 1024-default wallet reads pre-bump 768 records          */
/* ------------------------------------------------------------------ */
static void test_dual_identity_inbound(const cJSON *vectors) {
    printf("\nStep 12 — ML-KEM-1024 default reads pre-bump ML-KEM-768 records\n");

    const cJSON *dec = cJSON_GetObjectItem(cJSON_GetObjectItem((cJSON *)vectors, "mlkem768"), "decrypt");
    const char *secret = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)dec, "secret"));
    const char *token = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)dec, "token"));
    const char *position = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)dec, "position"));
    const char *cipher_text = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)dec, "cipherText"));
    const char *enc_msg = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)dec, "encryptedMessage"));
    const char *expected = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)dec, "expectedPlaintext"));
    if (!secret || !token || !position || !cipher_text || !enc_msg || !expected) {
        check(false, "vectors.mlkem768.decrypt fields present", "missing field(s)");
        return;
    }

    /* The wallet under test takes the CONSTRUCTOR DEFAULT — no explicit step-back anywhere. */
    knishio_wallet_t *wallet = NULL;
    if (knishio_wallet_create_simple(&wallet, secret, token, position) != KNISHIO_SUCCESS) {
        check(false, "default wallet creation", "knishio_wallet_create_simple failed");
        return;
    }
    /* The same KnishIO wallet's ML-KEM-768 identity, for the hashShare a pre-bump sender used. */
    knishio_wallet_t *wallet768 = NULL;
    if (knishio_wallet_create_simple(&wallet768, secret, token, position) != KNISHIO_SUCCESS
        || !knishio_wallet_set_mlkem_param(wallet768, KNISHIO_MLKEM_768)) {
        check(false, "ML-KEM-768 sibling wallet creation", "creation/step-back failed");
        knishio_wallet_free(wallet);
        return;
    }

    char detail[256];

    /* (b) the advertised key is still the configured set's — dual-identity decryption must not
     *     move what the wallet advertises (it lands in signed molecule meta and at auth). */
    size_t pubkey_bytes = b64_bytes(wallet->pubkey);
    snprintf(detail, sizeof(detail), "param=%d, pubkey %zu raw bytes / %zu base64 chars",
             (int)knishio_wallet_get_mlkem_param(wallet), pubkey_bytes, strlen(wallet->pubkey));
    check(knishio_wallet_get_mlkem_param(wallet) == KNISHIO_MLKEM_1024
              && pubkey_bytes == KNISHIO_PUBKEY_LENGTH_1024,
          "12(b) default wallet still advertises ML-KEM-1024 (1568 bytes)", detail);
    check(b64_bytes(wallet768->pubkey) == KNISHIO_PUBKEY_LENGTH_768,
          "sibling 768 identity derives a 1184-byte public key", NULL);

    /* (a) the frozen 768 envelope decrypts directly — no second wallet, no explicit step-back. */
    char *plaintext = NULL;
    knishio_error_t err = knishio_cipher_hash_decrypt_envelope(wallet, cipher_text, enc_msg, &plaintext);
    char *unwrapped = json_unwrap(plaintext);
    snprintf(detail, sizeof(detail), "err=%d, ciphertext %zu raw bytes, plaintext=%s",
             (int)err, b64_bytes(cipher_text), unwrapped ? unwrapped : "(null)");
    check(err == KNISHIO_SUCCESS && unwrapped && strcmp(unwrapped, expected) == 0,
          "12(a) default-1024 wallet decrypts the frozen ML-KEM-768 envelope", detail);
    free(plaintext);
    free(unwrapped);

    /* (d) a ciphertext matching NEITHER parameter set still fails on the existing observable. */
    uint8_t junk[64];
    memset(junk, 0xab, sizeof(junk));
    char *junk_b64 = NULL;
    knishio_base64_encode(junk, sizeof(junk), &junk_b64);
    plaintext = NULL;
    err = knishio_cipher_hash_decrypt_envelope(wallet, junk_b64, enc_msg, &plaintext);
    snprintf(detail, sizeof(detail), "err=%d (KNISHIO_ERROR_INVALID_ARGS=%d), plaintext=%s",
             (int)err, (int)KNISHIO_ERROR_INVALID_ARGS, plaintext ? "set" : "(null)");
    check(err == KNISHIO_ERROR_INVALID_ARGS && plaintext == NULL,
          "12(d) a 64-byte ciphertext still fails with KNISHIO_ERROR_INVALID_ARGS", detail);
    free(plaintext);
    free(junk_b64);

    /* (e) the map-addressed path finds an envelope keyed by hashShare(our 768 pubkey). */
    char *share768 = NULL;
    char *share_configured = NULL;
    knishio_cipher_hash_share(wallet768->pubkey, &share768);
    knishio_cipher_hash_share(wallet->pubkey, &share_configured);
    char *map_json = build_map(share768, cipher_text, enc_msg);
    plaintext = NULL;
    err = knishio_cipher_hash_decrypt(map_json, wallet, &plaintext);
    unwrapped = json_unwrap(plaintext);
    snprintf(detail, sizeof(detail), "err=%d, map key=%s, configured share=%s, plaintext=%s",
             (int)err, share768 ? share768 : "?", share_configured ? share_configured : "?",
             unwrapped ? unwrapped : "(null)");
    check(err == KNISHIO_SUCCESS && unwrapped && strcmp(unwrapped, expected) == 0,
          "12(e) map addressed to the 768 hashShare is found by the 1024 wallet", detail);
    free(plaintext);
    free(unwrapped);
    free(map_json);

    /* The configured-set path is unchanged: a 1024 envelope addressed to our 1024 share still
     * round-trips through the untouched encrypt side. */
    char *envelope = NULL;
    const char *body = "{\"query\":\"{ ping }\"}";
    err = knishio_cipher_hash_encrypt(body, wallet->pubkey, &envelope);
    plaintext = NULL;
    if (err == KNISHIO_SUCCESS) {
        err = knishio_cipher_hash_decrypt(envelope, wallet, &plaintext);
    }
    unwrapped = json_unwrap(plaintext);
    snprintf(detail, sizeof(detail), "err=%d, plaintext=%s", (int)err, unwrapped ? unwrapped : "(null)");
    check(err == KNISHIO_SUCCESS && unwrapped && strcmp(unwrapped, body) == 0,
          "configured ML-KEM-1024 envelope still round-trips unchanged", detail);
    free(plaintext);
    free(unwrapped);
    free(envelope);

    /* 12(c) analogue: outbound is unchanged. The C SDK's only encapsulation entry point takes the
     * RECIPIENT's key (it holds no configured set of its own) and rejects any length that is not a
     * FIPS 203 ML-KEM public key — a permissive INBOUND path must not have relaxed that. */
    envelope = NULL;
    err = knishio_cipher_hash_encrypt(body, junk_b64 ? junk_b64 : "AAAA", &envelope);
    snprintf(detail, sizeof(detail), "err=%d (KNISHIO_ERROR_INVALID_ARGS=%d)",
             (int)err, (int)KNISHIO_ERROR_INVALID_ARGS);
    check(err == KNISHIO_ERROR_INVALID_ARGS && envelope == NULL,
          "12(c) encapsulation still rejects a non-ML-KEM recipient key", detail);
    free(envelope);

    free(share768);
    free(share_configured);
    knishio_wallet_free(wallet768);
    knishio_wallet_free(wallet);
}

/* ------------------------------------------------------------------ */
/* Step 11: the session snapshot carries the parameter set            */
/* ------------------------------------------------------------------ */
static void test_session_snapshot_parameter_set(const cJSON *vectors) {
    printf("\nStep 11 — session snapshot preserves the ML-KEM parameter set\n");

    const cJSON *dec = cJSON_GetObjectItem(cJSON_GetObjectItem((cJSON *)vectors, "mlkem768"), "decrypt");
    const char *secret = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)dec, "secret"));
    const char *position = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)dec, "position"));
    if (!secret || !position) {
        check(false, "vectors.mlkem768.decrypt fields present", "missing field(s)");
        return;
    }

    char detail[512];

    /* An AUTH wallet explicitly stepped back to ML-KEM-768. */
    knishio_wallet_t *wallet = NULL;
    if (knishio_wallet_create_simple(&wallet, secret, "AUTH", position) != KNISHIO_SUCCESS
        || !knishio_wallet_set_mlkem_param(wallet, KNISHIO_MLKEM_768)) {
        check(false, "768 AUTH wallet creation", "creation/step-back failed");
        return;
    }
    char *original_pubkey = strdup(wallet->pubkey);

    knishio_auth_token_config_t config = {
        .token = "test-jwt", .expires_at = 4000000000LL, .encrypt = true, .pubkey = original_pubkey
    };
    knishio_auth_token_t *auth = NULL;
    if (knishio_auth_token_create_with_wallet(&auth, &config, wallet) != KNISHIO_SUCCESS) {
        check(false, "auth token creation", "create_with_wallet failed");
        free(original_pubkey);
        knishio_wallet_free(wallet);
        return;
    }

    /* Round trip: snapshot an explicitly-768 session and restore it. */
    char *snapshot = NULL;
    knishio_error_t err = knishio_auth_token_get_snapshot(auth, &snapshot);
    const char *param_field = snapshot ? strstr(snapshot, "\"mlKemParameterSet\"") : NULL;
    snprintf(detail, sizeof(detail), "err=%d, %.48s", (int)err, param_field ? param_field : "(absent)");
    check(err == KNISHIO_SUCCESS && param_field != NULL,
          "snapshot records wallet.mlKemParameterSet", detail);

    knishio_auth_token_t *restored = NULL;
    err = snapshot ? knishio_auth_token_restore(&restored, snapshot, secret) : KNISHIO_ERROR_INVALID_ARGS;
    const char *restored_pubkey = (err == KNISHIO_SUCCESS && restored && restored->wallet)
                                      ? restored->wallet->pubkey : NULL;
    snprintf(detail, sizeof(detail), "err=%d, restored param=%d, pubkey %zu raw bytes", (int)err,
             (err == KNISHIO_SUCCESS && restored) ? (int)knishio_wallet_get_mlkem_param(restored->wallet) : 0,
             b64_bytes(restored_pubkey));
    check(err == KNISHIO_SUCCESS && restored_pubkey && strcmp(restored_pubkey, original_pubkey) == 0,
          "restoring an explicitly-768 snapshot preserves the wallet's public key", detail);
    if (restored) {
        knishio_wallet_free(restored->wallet);
        knishio_auth_token_cleanup(restored);
    }
    free(snapshot);

    /* A LEGACY snapshot literal: the same shape a pre-bump build persisted — no parameter-set
     * field at all, and a `pubkey` that decodes to 1184 bytes. It must restore as 768, NOT as the
     * (now 1024) constructor default. */
    char legacy[4096];
    snprintf(legacy, sizeof(legacy),
             "{\"token\":\"legacy-jwt\",\"expiresAt\":4000000000,\"pubkey\":\"%s\",\"encrypt\":true,"
             "\"wallet\":{\"position\":\"%s\",\"characters\":\"%s\"}}",
             original_pubkey, position, wallet->characters ? wallet->characters : "BASE64");
    restored = NULL;
    err = knishio_auth_token_restore(&restored, legacy, secret);
    restored_pubkey = (err == KNISHIO_SUCCESS && restored && restored->wallet)
                          ? restored->wallet->pubkey : NULL;
    size_t restored_bytes = b64_bytes(restored_pubkey);
    snprintf(detail, sizeof(detail), "err=%d, restored param=%d, pubkey %zu raw bytes (768=%d, 1024=%d)",
             (int)err,
             (err == KNISHIO_SUCCESS && restored) ? (int)knishio_wallet_get_mlkem_param(restored->wallet) : 0,
             restored_bytes, KNISHIO_PUBKEY_LENGTH_768, KNISHIO_PUBKEY_LENGTH_1024);
    check(err == KNISHIO_SUCCESS && restored_pubkey
              && strcmp(restored_pubkey, original_pubkey) == 0
              && restored_bytes == KNISHIO_PUBKEY_LENGTH_768
              && restored_bytes != KNISHIO_PUBKEY_LENGTH_1024,
          "a pre-bump snapshot without a parameter set restores as ML-KEM-768", detail);
    if (restored) {
        knishio_wallet_free(restored->wallet);
        knishio_auth_token_cleanup(restored);
    }

    free(original_pubkey);
    knishio_auth_token_cleanup(auth);
    knishio_wallet_free(wallet);
}

/* ------------------------------------------------------------------ */
/* Step 14.3: the frozen pre-bump 768 auth molecule still verifies    */
/* ------------------------------------------------------------------ */
static void test_frozen_768_molecule(const cJSON *vectors) {
    printf("\nStep 14.3 — frozen pre-bump ML-KEM-768 auth molecule validates from a 1024 build\n");

    const cJSON *v = cJSON_GetObjectItem((cJSON *)vectors, "legacyMlkem768AuthMolecule");
    if (!v) {
        check(false, "vectors.legacyMlkem768AuthMolecule present", "missing block");
        return;
    }
    const char *expected_hash = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)v, "expectedMolecularHash"));
    const cJSON *expected_bytes = cJSON_GetObjectItem((cJSON *)v, "expectedWalletPubkeyBytes");
    const cJSON *molecule_json = cJSON_GetObjectItem((cJSON *)v, "molecule");
    if (!expected_hash || !cJSON_IsNumber(expected_bytes) || !molecule_json) {
        check(false, "legacyMlkem768AuthMolecule fields present", "missing field(s)");
        return;
    }

    char detail[256];

    /* The record really is a 768 record: the U-atom's advertised key decodes to 1184 bytes. This
     * fails loudly if the fixture is ever regenerated at ML-KEM-1024. */
    size_t pubkey_bytes = 0;
    size_t pubkey_b64_chars = 0;
    const cJSON *mol_atoms = cJSON_GetObjectItem((cJSON *)molecule_json, "atoms");
    const cJSON *atom = NULL;
    cJSON_ArrayForEach(atom, mol_atoms) {
        const cJSON *meta = cJSON_GetObjectItem((cJSON *)atom, "meta");
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, meta) {
            const char *key = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)item, "key"));
            const char *val = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)item, "value"));
            if (key && val && strcmp(key, "walletPubkey") == 0) {
                pubkey_bytes = b64_bytes(val);
                pubkey_b64_chars = strlen(val);
            }
        }
    }
    snprintf(detail, sizeof(detail), "walletPubkey %zu raw bytes / %zu base64 chars, expected %d",
             pubkey_bytes, pubkey_b64_chars, (int)cJSON_GetNumberValue(expected_bytes));
    check(pubkey_bytes == (size_t)cJSON_GetNumberValue(expected_bytes),
          "U-atom walletPubkey meta is an ML-KEM-768 key", detail);

    /* Deserialize the frozen molecule and run the full verifier on it. */
    char *molecule_text = cJSON_PrintUnformatted(molecule_json);
    knishio_molecule_t *molecule = NULL;
    const knishio_error_t err = molecule_text ? knishio_molecule_from_json(molecule_text, &molecule)
                                              : KNISHIO_ERROR_MEMORY;
    cJSON_free(molecule_text);
    const knishio_error_t verdict = (err == KNISHIO_SUCCESS) ? knishio_molecule_check(molecule, NULL) : err;
    const bool frozen = molecule && molecule->molecular_hash
                        && strcmp(molecule->molecular_hash, expected_hash) == 0;
    snprintf(detail, sizeof(detail), "err=%d (%s), molecularHash %s expectedMolecularHash",
             (int)verdict, knishio_error_to_string(verdict), frozen ? "==" : "!=");
    check(verdict == KNISHIO_SUCCESS && frozen,
          "frozen 768 auth molecule passes knishio_molecule_check (hash + OTS + isotopes)", detail);
    knishio_molecule_free_deep(molecule);
}

int main(void) {
    const char *env_path = getenv("KNISHIO_CROSS_PLATFORM_VECTORS");
    const char *path = env_path ? env_path : KNISHIO_TEST_VECTORS_PATH;
    char *text = slurp(path);
    if (!text) {
        fprintf(stderr, "mlkem_backcompat: cannot read cross-SDK vectors at %s\n", path);
        return 2;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    cJSON *vectors = root ? cJSON_GetObjectItem(root, "vectors") : NULL;
    if (!vectors) {
        fprintf(stderr, "mlkem_backcompat: %s has no `vectors` object\n", path);
        if (root) cJSON_Delete(root);
        return 2;
    }

    printf("ML-KEM backwards-compatibility suite (SDK %s, default parameter set %d)\n",
           KNISHIO_VERSION_STRING, (int)knishio_wallet_get_default_mlkem_param());

    test_dual_identity_inbound(vectors);
    test_session_snapshot_parameter_set(vectors);
    test_frozen_768_molecule(vectors);

    cJSON_Delete(root);
    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
