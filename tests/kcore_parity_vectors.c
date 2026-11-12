/**
 * @file kcore_parity_vectors.c
 * @brief kcore parity: the shared full-molecule vectors reproduce byte for byte on this build's
 *        backend (KNISHIO_USE_KCORE=ON or OFF).
 *
 * The vectors (tests/fixtures/kcore-parity-vectors.json, vendored from the monorepo's shared
 * cross-SDK master of the same name) are 16 signed molecules: metadata, value
 * transfer, token creation and wallet creation, at OTS v1 and v2, with unique and with colliding
 * createdAt values (whole seconds by construction; knishio_atom_from_json keeps the exact
 * millisecond value either way). For every case:
 *   verify  — knishio_molecule_from_json parses the molecule, the declared hash is the expected
 *             one, knishio_molecule_check_ots accepts the signature, and
 *             knishio_molecule_generate_hash recomputes the expected hash from the atoms. Full
 *             knishio_molecule_check is not the parity surface: a value transfer carries no
 *             funded source wallet here.
 *   re-sign — the molecule is parsed again, every atom's otsFragment is cleared, the molecule is
 *             given the case's secret and a source wallet at (sourceToken, sourcePosition,
 *             otsVersion), and signed with compressed fragments; the molecular hash and every
 *             atom's otsFragment (all written by this signing) must equal the expected values.
 * The ctest entry runs in both CI lanes (gate: pure; gate-kcore: KNISHIO_USE_KCORE=ON). A
 * missing fixture is a failure, not a skip: the fixture is vendored in this repository.
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

#ifdef KNISHIO_HAVE_KCORE
#include <kcore.h>
#endif

#ifndef KNISHIO_KCORE_PARITY_VECTORS_PATH
#define KNISHIO_KCORE_PARITY_VECTORS_PATH "tests/fixtures/kcore-parity-vectors.json"
#endif

#define EXPECTED_CASES 16

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

static bool same(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static const char *str_of(const cJSON *obj, const char *key) {
    return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, key));
}

/* The first offset at which a and b differ (the shorter length when one is a prefix), or -1
 * when either is NULL. */
static long first_difference(const char *a, const char *b) {
    if (!a || !b) return -1;
    long i = 0;
    while (a[i] && a[i] == b[i]) i++;
    return i;
}

static knishio_molecule_t *parse_molecule(const cJSON *vector, knishio_error_t *err) {
    char *text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(vector, "molecule"));
    knishio_molecule_t *m = NULL;
    *err = text ? knishio_molecule_from_json(text, &m) : KNISHIO_ERROR_MEMORY;
    cJSON_free(text);
    return m;
}

/* (a) parse, declared hash, OTS signature, recomputed hash. */
static void test_verify(const cJSON *t, const char *name, const char *expected_hash) {
    char label[160], detail[200];
    knishio_error_t err = KNISHIO_SUCCESS;
    knishio_molecule_t *m = parse_molecule(t, &err);

    snprintf(label, sizeof(label), "%s: verify (declared hash, OTS signature, recomputed hash)", name);
    if (err != KNISHIO_SUCCESS) {
        snprintf(detail, sizeof(detail), "from_json=%d (%s)", (int)err, knishio_error_to_string(err));
        check(false, label, detail);
        knishio_molecule_free_deep(m);
        return;
    }

    const bool declared = same(m->molecular_hash, expected_hash);
    const knishio_error_t ots = knishio_molecule_check_ots(m);
    const knishio_error_t hashed = knishio_molecule_generate_hash(m);
    const bool recomputed = hashed == KNISHIO_SUCCESS && same(m->molecular_hash, expected_hash);
    snprintf(detail, sizeof(detail), "declared=%s check_ots=%d (%s) generate_hash=%d recomputed=%s",
             declared ? "match" : "DIFF", (int)ots, knishio_error_to_string(ots), (int)hashed,
             recomputed ? "match" : (m->molecular_hash ? m->molecular_hash : "(none)"));
    check(declared && ots == KNISHIO_SUCCESS && recomputed, label, detail);
    knishio_molecule_free_deep(m);
}

/* (b) re-sign with the case's secret and source wallet; hash and every fragment byte for byte. */
static void test_resign(const cJSON *t, const char *name, const char *expected_hash, const cJSON *fragments) {
    char label[160], detail[200];
    const cJSON *ots_version = cJSON_GetObjectItemCaseSensitive(t, "otsVersion");
    const char *secret = str_of(t, "secret");
    knishio_error_t err = KNISHIO_ERROR_INVALID_ARGS;
    knishio_molecule_t *m = NULL;
    knishio_wallet_t *source = NULL;

    if (secret && str_of(t, "sourceToken") && str_of(t, "sourcePosition") && cJSON_IsNumber(ots_version)) {
        m = parse_molecule(t, &err);
        if (err == KNISHIO_SUCCESS) {
            m->secret = knishio_strdup(secret);
            err = (m->secret && knishio_wallet_create_with_ots_version(&source, secret, str_of(t, "sourceToken"),
                                                                       str_of(t, "sourcePosition"),
                                                                       (int)cJSON_GetNumberValue(ots_version)))
                ? KNISHIO_SUCCESS : KNISHIO_ERROR_CRYPTO;
        }
        if (err == KNISHIO_SUCCESS) {
            m->source_wallet = source;
            /* Clear the parsed fragments so every compared byte comes from this signing. */
            for (size_t i = 0; err == KNISHIO_SUCCESS && i < m->atom_count; i++) {
                err = knishio_atom_set_ots_fragment(m->atoms[i], NULL);
            }
        }
        if (err == KNISHIO_SUCCESS) {
            err = knishio_molecule_sign(m, NULL, false, true);
        }
    }

#ifdef KNISHIO_HAVE_KCORE
    /* kcore itself, in this process, derives the source wallet's address from its key. */
    if (err == KNISHIO_SUCCESS) {
        char address[65] = {0};
        const size_t key_len = source->private_key ? strlen(source->private_key) : 0;
        const int rc = key_len == 8192 ? kcore_wots_address_v2(source->private_key, address)
                                       : kcore_wots_address(source->private_key, address);
        snprintf(label, sizeof(label), "%s: kcore_wots_address%s equals the SDK wallet address", name,
                 key_len == 8192 ? "_v2" : "");
        check(rc == 0 && same(address, source->address), label, address);
    }
#endif

    snprintf(label, sizeof(label), "%s: re-sign reproduces molecularHash and every otsFragment", name);
    if (err != KNISHIO_SUCCESS) {
        snprintf(detail, sizeof(detail), "sign=%d (%s)", (int)err, knishio_error_to_string(err));
        check(false, label, detail);
    } else if (!same(m->molecular_hash, expected_hash)) {
        snprintf(detail, sizeof(detail), "molecularHash differs at offset %ld: %s",
                 first_difference(m->molecular_hash, expected_hash), m->molecular_hash ? m->molecular_hash : "(none)");
        check(false, label, detail);
    } else if (!cJSON_IsArray(fragments) || (size_t)cJSON_GetArraySize(fragments) != m->atom_count) {
        snprintf(detail, sizeof(detail), "%zu atoms, %d expected fragments", m->atom_count,
                 cJSON_IsArray(fragments) ? cJSON_GetArraySize(fragments) : -1);
        check(false, label, detail);
    } else {
        size_t bad = m->atom_count;
        for (size_t i = 0; bad == m->atom_count && i < m->atom_count; i++) {
            if (!same(m->atoms[i]->ots_fragment, cJSON_GetStringValue(cJSON_GetArrayItem(fragments, (int)i)))) bad = i;
        }
        if (bad == m->atom_count) {
            snprintf(detail, sizeof(detail), "%zu atoms", m->atom_count);
            check(true, label, detail);
        } else {
            snprintf(detail, sizeof(detail), "atoms[%zu].otsFragment differs at offset %ld", bad,
                     first_difference(m->atoms[bad]->ots_fragment,
                                      cJSON_GetStringValue(cJSON_GetArrayItem(fragments, (int)bad))));
            check(false, label, detail);
        }
    }

    if (m) m->source_wallet = NULL;
    knishio_molecule_free_deep(m);
    if (source) knishio_wallet_free(source);
}

int main(void) {
    const char *env_path = getenv("KNISHIO_KCORE_PARITY_VECTORS");
    const char *path = env_path ? env_path : KNISHIO_KCORE_PARITY_VECTORS_PATH;
    char *text = slurp(path);
    if (!text) {
        fprintf(stderr, "kcore_parity_vectors: cannot read %s\n", path);
        return 1;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    const cJSON *tests = root ? cJSON_GetObjectItemCaseSensitive(
                                    cJSON_GetObjectItemCaseSensitive(
                                        cJSON_GetObjectItemCaseSensitive(root, "vectors"), "molecules"),
                                    "tests")
                              : NULL;
    if (!cJSON_IsArray(tests)) {
        fprintf(stderr, "kcore_parity_vectors: %s has no vectors.molecules.tests array\n", path);
        if (root) cJSON_Delete(root);
        return 1;
    }

#ifdef KNISHIO_HAVE_KCORE
    printf("kcore parity vectors (SDK %s, backend: kcore %s)\n", KNISHIO_VERSION_STRING, KCORE_VERSION_STRING);
    {
        char detail[64];
        snprintf(detail, sizeof(detail), "kcore_abi_version()=%d", kcore_abi_version());
        check(kcore_abi_version() == KCORE_ABI_VERSION, "kcore is linked and answers at run time", detail);
    }
#else
    printf("kcore parity vectors (SDK %s, backend: pure, KNISHIO_USE_KCORE=OFF)\n", KNISHIO_VERSION_STRING);
#endif

    int cases = 0;
    const cJSON *t = NULL;
    cJSON_ArrayForEach(t, tests) {
        const char *name = str_of(t, "name") ? str_of(t, "name") : "?";
        const cJSON *expected = cJSON_GetObjectItemCaseSensitive(t, "expected");
        const char *expected_hash = str_of(expected, "molecularHash");
        printf("\n%s\n", name);
        test_verify(t, name, expected_hash);
        test_resign(t, name, expected_hash, cJSON_GetObjectItemCaseSensitive(expected, "otsFragments"));
        cases++;
    }

    char detail[64];
    snprintf(detail, sizeof(detail), "%d cases", cases);
    check(cases == EXPECTED_CASES, "every case ran (16)", detail);

    cJSON_Delete(root);
    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
