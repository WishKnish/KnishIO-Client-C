/**
 * @file ots_v2_vectors.c
 * @brief One-time signature (OTS) v2: 8192-hex keys (64 chains) next to v1 (2048 hex, 16 chains).
 *
 * Two modes, registered as two ctest entries:
 *   roundtrip  (OtsV2RoundTrip) — always runs: a v2 wallet signs a molecule, the signature
 *              verifies, and a one-character change to it does not.
 *   vectors    (OtsV2Vectors)   — the shared OTS v2 vectors (key derivation, address, normalized
 *              hash, ML-KEM identities, molecule verdicts, re-signing, malformed declared hash).
 *              The file is read from $KNISHIO_OTS_V2_VECTORS; when it is absent (a standalone
 *              checkout of this SDK) the binary prints SKIP and exits 77.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "knishio/knishio.h"
#include "knishio/atom.h"
#include "knishio/molecule.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"

#define SKIP_EXIT_CODE 77

static int g_failures = 0;

#ifndef KNISHIO_OTS_V2_VECTORS_PATH
#define KNISHIO_OTS_V2_VECTORS_PATH "../shared-test-results/ots-v2-vectors.json"
#endif

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

static bool same(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static size_t total_ots_length(const knishio_molecule_t *m) {
    size_t n = 0;
    for (size_t i = 0; i < m->atom_count; i++) {
        if (m->atoms[i]->ots_fragment) n += strlen(m->atoms[i]->ots_fragment);
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Round trip (no vectors needed)                                     */
/* ------------------------------------------------------------------ */

/* A three-atom V transfer (-1000 / +1000 / 0, token TEST so ContinuID does not apply) from
 * `source`, signed with compressed (base64) signatures. */
static knishio_error_t build_signed_transfer(const char *secret, const char *bundle, knishio_wallet_t *source,
                                             knishio_molecule_t **out) {
    knishio_molecule_t *m = NULL;
    knishio_error_t err = knishio_molecule_create(&m, secret, bundle, source, NULL, NULL, KNISHIO_VERSION_STRING);
    if (err != KNISHIO_SUCCESS) return err;

    const struct { const char *position, *address, *value; } v[3] = {
        { source->position, source->address, "-1000" },
        { "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210",
          "1111111111111111111111111111111111111111111111111111111111111111", "1000" },
        { "bbbb000000000000cccc111111111111dddd222222222222eeee333333333333",
          "2222222222222222222222222222222222222222222222222222222222222222", "0" },
    };
    for (size_t i = 0; i < 3 && err == KNISHIO_SUCCESS; i++) {
        knishio_atom_t *a = NULL;
        err = knishio_atom_create(&a, v[i].position, v[i].address, KNISHIO_ISOTOPE_V, "TEST", v[i].value, NULL);
        if (err == KNISHIO_SUCCESS && (err = knishio_molecule_add_atom(m, a)) != KNISHIO_SUCCESS) {
            knishio_atom_free_deep(a);
        }
    }
    if (err == KNISHIO_SUCCESS) err = knishio_molecule_generate_hash(m);
    if (err == KNISHIO_SUCCESS) err = knishio_molecule_sign(m, bundle, false, true);
    if (err != KNISHIO_SUCCESS) {
        knishio_molecule_free_deep(m);
        return err;
    }
    *out = m;
    return KNISHIO_SUCCESS;
}

static int run_roundtrip(void) {
    printf("OTS v2 round trip (SDK %s)\n", KNISHIO_VERSION_STRING);
    const char *position = "0123456789abcdeffedcba9876543210fedcba9876543210fedcba9876543210";
    char detail[200];
    char *secret = NULL;
    char *bundle = NULL;
    knishio_wallet_t *source = NULL;
    knishio_wallet_t *v1 = NULL;
    knishio_wallet_t *refused = NULL;
    knishio_molecule_t *m = NULL;

    const bool setup = knishio_generate_secret("TESTSEED", 2048, &secret)
        && knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)
        && knishio_wallet_create_with_ots_version(&source, secret, "TEST", position, KNISHIO_OTS_V2)
        && knishio_wallet_create_with_continuid(&v1, secret, "TEST", position);
    check(setup, "v1 and v2 TEST wallets from one hex secret", NULL);
    if (!setup) goto done;

    snprintf(detail, sizeof(detail), "v2 key %zu, v1 key %zu, versions %d/%d",
             source->private_key ? strlen(source->private_key) : 0, v1->private_key ? strlen(v1->private_key) : 0,
             source->ots_version, v1->ots_version);
    check(source->ots_version == KNISHIO_OTS_V2 && v1->ots_version == KNISHIO_OTS_V1
              && source->private_key && strlen(source->private_key) == 8192
              && v1->private_key && strlen(v1->private_key) == 2048
              && source->address && v1->address && !same(source->address, v1->address),
          "a v2 wallet holds an 8192-hex key and its own address; the v1 wallet keeps 2048", detail);

    check(!knishio_wallet_create_with_ots_version(&refused, secret, "TEST", position, 3) && refused == NULL,
          "an unsupported OTS version is refused", NULL);

    knishio_error_t err = build_signed_transfer(secret, bundle, source, &m);
    snprintf(detail, sizeof(detail), "sign=%d (%s)", (int)err, knishio_error_to_string(err));
    check(err == KNISHIO_SUCCESS, "a molecule signs with the v2 source wallet", detail);
    if (err != KNISHIO_SUCCESS) goto done;

    snprintf(detail, sizeof(detail), "%zu characters", total_ots_length(m));
    check(total_ots_length(m) == 5464, "the compressed v2 signature is 5464 base64 characters", detail);

    err = knishio_molecule_check(m, NULL);
    snprintf(detail, sizeof(detail), "check=%d (%s)", (int)err, knishio_error_to_string(err));
    check(err == KNISHIO_SUCCESS, "the v2 signature verifies", detail);

    char *ots0 = m->atoms[0]->ots_fragment;
    ots0[10] = (ots0[10] == 'A') ? 'B' : 'A';
    err = knishio_molecule_check(m, NULL);
    snprintf(detail, sizeof(detail), "check=%d (%s)", (int)err, knishio_error_to_string(err));
    check(err == KNISHIO_ERROR_SIGNATURE_MISMATCH,
          "one changed base64 character is rejected as KNISHIO_ERROR_SIGNATURE_MISMATCH", detail);

    m->molecular_hash[1] = 'G';
    err = knishio_molecule_check_ots(m);
    snprintf(detail, sizeof(detail), "check_ots=%d (%s)", (int)err, knishio_error_to_string(err));
    check(err == KNISHIO_ERROR_SIGNATURE_MALFORMED,
          "a declared hash outside [0-9a-g] is rejected as KNISHIO_ERROR_SIGNATURE_MALFORMED", detail);

done:
    knishio_molecule_free_deep(m);
    if (source) knishio_wallet_free(source);
    if (v1) knishio_wallet_free(v1);
    free(secret);
    free(bundle);
    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* Shared vectors                                                     */
/* ------------------------------------------------------------------ */

static const char *str_of(const cJSON *obj, const char *key) {
    return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, key));
}

static const cJSON *family_tests(const cJSON *vectors, const char *family) {
    return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(vectors, family), "tests");
}

static const cJSON *find_by_name(const cJSON *tests, const char *name) {
    const cJSON *t = NULL;
    cJSON_ArrayForEach(t, tests) {
        if (same(str_of(t, "name"), name)) return t;
    }
    return NULL;
}

static knishio_molecule_t *parse_molecule(const cJSON *vector, knishio_error_t *err) {
    char *text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(vector, "molecule"));
    knishio_molecule_t *m = NULL;
    *err = text ? knishio_molecule_from_json(text, &m) : KNISHIO_ERROR_MEMORY;
    cJSON_free(text);
    return m;
}

/* The verdict a vector's "expected" names; false for a name this test does not know. */
static bool expected_code(const char *expected, knishio_error_t *code) {
    if (same(expected, "verified")) *code = KNISHIO_SUCCESS;
    else if (same(expected, "SignatureMismatch")) *code = KNISHIO_ERROR_SIGNATURE_MISMATCH;
    else if (same(expected, "SignatureMalformed")) *code = KNISHIO_ERROR_SIGNATURE_MALFORMED;
    else return false;
    return true;
}

static void test_key_derivation(const cJSON *vectors) {
    printf("\nKey derivation and address\n");
    const cJSON *t = NULL;
    cJSON_ArrayForEach(t, family_tests(vectors, "key_derivation")) {
        const char *name = str_of(t, "name");
        const char *secret = str_of(t, "secret");
        const char *token = str_of(t, "token");
        const char *position = str_of(t, "position");
        char *k1 = NULL, *k2 = NULL, *a1 = NULL, *a2 = NULL;
        knishio_wallet_t *w = NULL;
        char label[160];

        const bool keys = knishio_generate_wallet_key_with_ots_version(secret, token, position, KNISHIO_OTS_V1, &k1)
            && knishio_generate_wallet_key_with_ots_version(secret, token, position, KNISHIO_OTS_V2, &k2);
        snprintf(label, sizeof(label), "%s: v1 and v2 keys", name ? name : "?");
        check(keys && same(k1, str_of(t, "expectedKeyV1")) && same(k2, str_of(t, "expectedKeyV2")), label, NULL);

        const bool addresses = keys && knishio_generate_address(k1, &a1) && knishio_generate_address(k2, &a2);
        snprintf(label, sizeof(label), "%s: v1 and v2 addresses", name ? name : "?");
        check(addresses && same(a1, str_of(t, "expectedAddressV1")) && same(a2, str_of(t, "expectedAddressV2")),
              label, NULL);

        const bool wallet = knishio_wallet_create_with_ots_version(&w, secret, token, position, KNISHIO_OTS_V2);
        snprintf(label, sizeof(label), "%s: v2 wallet address", name ? name : "?");
        check(wallet && w->ots_version == KNISHIO_OTS_V2 && same(w->address, str_of(t, "expectedAddressV2")),
              label, wallet ? w->address : "wallet not created");

        if (w) knishio_wallet_free(w);
        free(k1); free(k2); free(a1); free(a2);
    }

    const cJSON *first = cJSON_GetArrayItem(family_tests(vectors, "key_derivation"), 0);
    char *k3 = NULL;
    check(first && !knishio_generate_wallet_key_with_ots_version(str_of(first, "secret"), str_of(first, "token"),
                                                                 str_of(first, "position"), 3, &k3) && k3 == NULL,
          "an unsupported OTS version is refused", NULL);
}

static void test_normalized_hash(const cJSON *vectors) {
    printf("\nNormalized hash\n");
    const cJSON *t = NULL;
    cJSON_ArrayForEach(t, family_tests(vectors, "signature")) {
        const cJSON *expected = cJSON_GetObjectItemCaseSensitive(t, "expectedNormalized");
        int *enumerated = NULL, *normalized = NULL;
        size_t n = 0;
        bool ok = knishio_molecule_enumerate_hash(str_of(t, "molecularHash"), &enumerated, &n) == KNISHIO_SUCCESS
            && knishio_molecule_normalize_hash(enumerated, n, &normalized) == KNISHIO_SUCCESS
            && cJSON_IsArray(expected) && (size_t)cJSON_GetArraySize(expected) == n;
        for (size_t i = 0; ok && i < n; i++) {
            ok = normalized[i] == (int)cJSON_GetNumberValue(cJSON_GetArrayItem(expected, (int)i));
        }
        char label[160];
        snprintf(label, sizeof(label), "%s: normalize(enumerate(hash))", str_of(t, "name") ? str_of(t, "name") : "?");
        check(ok, label, NULL);
        free(enumerated);
        free(normalized);
    }
}

static void test_mlkem_identity(const cJSON *vectors) {
    printf("\nML-KEM identities of v2 wallets\n");
    const cJSON *t = NULL;
    cJSON_ArrayForEach(t, family_tests(vectors, "mlkem_identity")) {
        const cJSON *k = find_by_name(family_tests(vectors, "key_derivation"), str_of(t, "keyFrom"));
        knishio_wallet_t *w = NULL;
        bool ok = k && knishio_wallet_create_with_ots_version(&w, str_of(k, "secret"), str_of(k, "token"),
                                                              str_of(k, "position"), KNISHIO_OTS_V2);
        ok = ok && knishio_wallet_set_mlkem_param(w, KNISHIO_MLKEM_1024)
            && same(w->pubkey, str_of(t, "expectedMlKem1024Pubkey"));
        ok = ok && knishio_wallet_set_mlkem_param(w, KNISHIO_MLKEM_768)
            && same(w->pubkey, str_of(t, "expectedMlKem768Pubkey"));
        char label[160];
        snprintf(label, sizeof(label), "%s: ML-KEM-1024 and ML-KEM-768 public keys",
                 str_of(t, "name") ? str_of(t, "name") : "?");
        check(ok, label, NULL);
        if (w) knishio_wallet_free(w);
    }
}

static void test_molecule_verdicts(const cJSON *vectors) {
    printf("\nMolecule verdicts (knishio_molecule_check)\n");
    const cJSON *t = NULL;
    cJSON_ArrayForEach(t, family_tests(vectors, "molecules")) {
        knishio_error_t parsed = KNISHIO_SUCCESS;
        knishio_molecule_t *m = parse_molecule(t, &parsed);
        const knishio_error_t verdict = (parsed == KNISHIO_SUCCESS) ? knishio_molecule_check(m, NULL) : parsed;
        const char *expected = str_of(t, "expected");
        char label[160], detail[160];
        snprintf(label, sizeof(label), "%s -> %s", str_of(t, "name") ? str_of(t, "name") : "?",
                 expected ? expected : "?");
        snprintf(detail, sizeof(detail), "parsed=%d verdict=%d (%s)", (int)parsed, (int)verdict,
                 knishio_error_to_string(verdict));
        knishio_error_t want = KNISHIO_SUCCESS;
        check(expected_code(expected, &want) && parsed == KNISHIO_SUCCESS && verdict == want, label, detail);
        knishio_molecule_free_deep(m);
    }
}

static void test_resign(const cJSON *vectors, const char *name) {
    const cJSON *t = find_by_name(family_tests(vectors, "molecules"), name);
    const cJSON *k = find_by_name(family_tests(vectors, "key_derivation"), "hex_secret_user");
    const cJSON *ots_version = t ? cJSON_GetObjectItemCaseSensitive(t, "otsVersion") : NULL;
    knishio_error_t err = KNISHIO_ERROR_INVALID_ARGS;
    knishio_molecule_t *m = NULL;
    knishio_wallet_t *source = NULL;
    char label[160], detail[160];

    if (t && k && cJSON_IsNumber(ots_version)) {
        m = parse_molecule(t, &err);
        if (err == KNISHIO_SUCCESS) {
            m->secret = knishio_strdup(str_of(k, "secret"));
            err = (m->secret && knishio_wallet_create_with_ots_version(&source, str_of(k, "secret"), "USER",
                                                                       str_of(k, "position"),
                                                                       (int)cJSON_GetNumberValue(ots_version)))
                ? KNISHIO_SUCCESS : KNISHIO_ERROR_CRYPTO;
        }
        if (err == KNISHIO_SUCCESS) {
            m->source_wallet = source;
            err = knishio_molecule_sign(m, NULL, false, true);
        }
    }

    const cJSON *atoms = t ? cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(t, "molecule"), "atoms")
                           : NULL;
    bool identical = err == KNISHIO_SUCCESS && cJSON_IsArray(atoms)
        && (size_t)cJSON_GetArraySize(atoms) == m->atom_count;
    for (size_t i = 0; identical && i < m->atom_count; i++) {
        identical = same(m->atoms[i]->ots_fragment, str_of(cJSON_GetArrayItem(atoms, (int)i), "otsFragment"));
    }
    snprintf(label, sizeof(label), "re-signing %s reproduces every otsFragment", name);
    snprintf(detail, sizeof(detail), "sign=%d (%s)", (int)err, knishio_error_to_string(err));
    check(identical, label, detail);

    if (m) m->source_wallet = NULL;
    knishio_molecule_free_deep(m);
    if (source) knishio_wallet_free(source);
}

static void test_malformed_hash(const cJSON *vectors) {
    printf("\nMalformed declared hash (knishio_molecule_check_ots)\n");
    const cJSON *t = NULL;
    cJSON_ArrayForEach(t, family_tests(vectors, "malformed_hash")) {
        knishio_error_t parsed = KNISHIO_SUCCESS;
        knishio_molecule_t *m = parse_molecule(t, &parsed);
        const knishio_error_t verdict = (parsed == KNISHIO_SUCCESS) ? knishio_molecule_check_ots(m) : parsed;
        const char *expected = str_of(t, "expected");
        char label[160], detail[160];
        snprintf(label, sizeof(label), "%s -> %s", str_of(t, "name") ? str_of(t, "name") : "?",
                 expected ? expected : "?");
        snprintf(detail, sizeof(detail), "parsed=%d verdict=%d (%s)", (int)parsed, (int)verdict,
                 knishio_error_to_string(verdict));
        knishio_error_t want = KNISHIO_SUCCESS;
        check(expected_code(expected, &want) && parsed == KNISHIO_SUCCESS && verdict == want, label, detail);
        knishio_molecule_free_deep(m);
    }
}

static int run_vectors(void) {
    const char *env_path = getenv("KNISHIO_OTS_V2_VECTORS");
    const char *path = env_path ? env_path : KNISHIO_OTS_V2_VECTORS_PATH;
    char *text = slurp(path);
    if (!text) {
        printf("SKIP: ots-v2-vectors.json not found (standalone checkout)\n");
        return SKIP_EXIT_CODE;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    const cJSON *vectors = root ? cJSON_GetObjectItemCaseSensitive(root, "vectors") : NULL;
    if (!vectors) {
        fprintf(stderr, "ots_v2_vectors: %s has no \"vectors\" object\n", path);
        if (root) cJSON_Delete(root);
        return 2;
    }

    printf("OTS v2 shared vectors (SDK %s)\n", KNISHIO_VERSION_STRING);
    test_key_derivation(vectors);
    test_normalized_hash(vectors);
    test_mlkem_identity(vectors);
    test_molecule_verdicts(vectors);
    printf("\nRe-signing\n");
    test_resign(vectors, "genuine_v2");
    test_resign(vectors, "genuine_v1_control");
    test_malformed_hash(vectors);

    cJSON_Delete(root);
    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "roundtrip") == 0) return run_roundtrip();
    if (argc == 2 && strcmp(argv[1], "vectors") == 0) return run_vectors();
    fprintf(stderr, "usage: %s roundtrip|vectors\n", argv[0]);
    return 2;
}
