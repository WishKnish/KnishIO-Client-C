/**
 * @file served_molecules.c
 * @brief knishio_verify_from_server_data() verifies a molecule exactly as a validator's GraphQL
 *        API serves it, and the Version4 atom view matches the shared hash vectors.
 *
 * Fixtures (vendored from the monorepo's sdks/shared-test-results):
 *   served-molecules.json      — real served molecules: v1 and v2 (no atom versions), v2ts
 *                                (signed with version "4" atoms, served without versions) and
 *                                v3rust (signed and served with version "3").
 *   version4-hash-vectors.json — client-format atoms, the exact Version4 view string and the
 *                                expected molecular hash for each case.
 * When either file is missing the binary prints SKIP and exits 77.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "knishio/knishio.h"
#include "knishio/atom.h"
#include "knishio/molecule.h"
#include "knishio/utils/memory.h"

#define SKIP_EXIT_CODE 77

#ifndef KNISHIO_SERVED_MOLECULES_PATH
#define KNISHIO_SERVED_MOLECULES_PATH "tests/fixtures/served-molecules.json"
#endif
#ifndef KNISHIO_VERSION4_VECTORS_PATH
#define KNISHIO_VERSION4_VECTORS_PATH "tests/fixtures/version4-hash-vectors.json"
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

/* ------------------------------------------------------------------ */
/* Served molecules                                                   */
/* ------------------------------------------------------------------ */
typedef void (*mutation_fn)(cJSON *molecule);

static void set_versions(cJSON *m, const char *version) {
    cJSON *atom = NULL;
    cJSON_ArrayForEach(atom, cJSON_GetObjectItemCaseSensitive(m, "atoms")) {
        cJSON *item = version ? cJSON_CreateString(version) : cJSON_CreateNull();
        if (cJSON_GetObjectItemCaseSensitive(atom, "version")) {
            cJSON_ReplaceItemInObjectCaseSensitive(atom, "version", item);
        } else {
            cJSON_AddItemToObject(atom, "version", item);
        }
    }
}

static void versions_4(cJSON *m) { set_versions(m, "4"); }
static void versions_null(cJSON *m) { set_versions(m, NULL); }

static void tamper_meta_id(cJSON *m) {
    cJSON *atom0 = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(m, "atoms"), 0);
    cJSON_ReplaceItemInObjectCaseSensitive(atom0, "metaId", cJSON_CreateString("rust-pre-tampered"));
}

static void tamper_meta_id_versions_null(cJSON *m) {
    tamper_meta_id(m);
    versions_null(m);
}

static char *served_variant(const cJSON *fixtures, const char *key, mutation_fn mutate) {
    cJSON *copy = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(fixtures, key), 1);
    if (!copy) return NULL;
    if (mutate) mutate(copy);
    char *json = cJSON_PrintUnformatted(copy);
    cJSON_Delete(copy);
    return json;
}

static knishio_served_verification_t verify_variant(const cJSON *fixtures, const char *key,
                                                    mutation_fn mutate) {
    knishio_served_verification_t out;
    memset(&out, 0, sizeof(out));
    char *json = served_variant(fixtures, key, mutate);
    if (!json || knishio_verify_from_server_data(json, &out) != KNISHIO_SUCCESS) {
        snprintf(out.error, sizeof(out.error), "harness: no result");
    }
    cJSON_free(json);
    return out;
}

static void expect_verified(const cJSON *fixtures, const char *key, mutation_fn mutate,
                            const char *name) {
    const knishio_served_verification_t r = verify_variant(fixtures, key, mutate);
    const char *served_hash = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(fixtures, key), "molecularHash"));
    char detail[400];
    snprintf(detail, sizeof(detail), "verified=%d error='%s' hash=%s", (int)r.verified, r.error,
             r.molecular_hash);
    check(r.verified && r.error[0] == '\0' && served_hash && strcmp(r.molecular_hash, served_hash) == 0,
          name, detail);
}

static void expect_hash_mismatch(const cJSON *fixtures, const char *key, mutation_fn mutate,
                                 const char *name) {
    const knishio_served_verification_t r = verify_variant(fixtures, key, mutate);
    char detail[400];
    snprintf(detail, sizeof(detail), "verified=%d error='%s'", (int)r.verified, r.error);
    check(!r.verified && strcmp(r.error, "Molecular hash mismatch") == 0, name, detail);
}

/* Atom 0's createdAt set to a JSON value; returns that atom's created_at_ms, or -1. */
static long long created_at_ms_for(const cJSON *fixtures, cJSON *value, knishio_error_t *err) {
    cJSON *copy = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(fixtures, "v1"), 1);
    cJSON *atom0 = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(copy, "atoms"), 0);
    cJSON_ReplaceItemInObjectCaseSensitive(atom0, "createdAt", value);
    char *json = cJSON_PrintUnformatted(copy);
    cJSON_Delete(copy);

    knishio_molecule_t *mol = NULL;
    *err = knishio_molecule_from_server_data(json, &mol);
    cJSON_free(json);
    long long ms = -1;
    if (*err == KNISHIO_SUCCESS) {
        ms = mol->atoms[0]->created_at_ms;
        knishio_molecule_free_deep(mol);
    }
    return ms;
}

static void check_created_at(const cJSON *fixtures, cJSON *value, const char *input,
                             long long expected) {
    knishio_error_t err = KNISHIO_SUCCESS;
    const long long ms = created_at_ms_for(fixtures, value, &err);
    char name[160];
    char detail[160];
    snprintf(name, sizeof(name), "createdAt %s normalizes to %lld", input, expected);
    snprintf(detail, sizeof(detail), "err=%d created_at_ms=%lld", (int)err, ms);
    check(err == KNISHIO_SUCCESS && ms == expected, name, detail);
}

static void served_tests(const cJSON *fixtures) {
    printf("Served molecules\n");
    expect_verified(fixtures, "v1", NULL, "v1 verifies");
    expect_verified(fixtures, "v2", NULL, "v2 verifies");
    expect_verified(fixtures, "v2ts", NULL, "v2ts (served without versions) verifies through the \"4\" retry");
    expect_verified(fixtures, "v2ts", versions_4, "v2ts with every atom at version \"4\" verifies directly");
    expect_verified(fixtures, "v3rust", NULL, "v3rust (served with version \"3\") verifies");
    expect_verified(fixtures, "v3rust", versions_null, "v3rust with null versions verifies through the \"3\" retry");
    expect_hash_mismatch(fixtures, "v3rust", tamper_meta_id,
                         "v3rust with a changed metaId fails with a hash mismatch (versions served)");
    expect_hash_mismatch(fixtures, "v3rust", tamper_meta_id_versions_null,
                         "v3rust with a changed metaId fails with a hash mismatch (versions null)");
    expect_hash_mismatch(fixtures, "v1", versions_4,
                         "v1 with every atom at version \"4\" fails with a hash mismatch (no retry)");

    printf("createdAt normalization\n");
    check_created_at(fixtures, cJSON_CreateString("2026-10-14T20:02:37+00:00"),
                     "\"2026-10-14T20:02:37+00:00\"", 1792008157000LL);
    check_created_at(fixtures, cJSON_CreateString("2026-10-14T20:02:37.110Z"),
                     "\"2026-10-14T20:02:37.110Z\"", 1792008157110LL);
    check_created_at(fixtures, cJSON_CreateNumber(1792008157110.0), "1792008157110", 1792008157110LL);
    check_created_at(fixtures, cJSON_CreateString("1792008157110"), "\"1792008157110\"", 1792008157110LL);
    check_created_at(fixtures, cJSON_CreateString("2026-10-14T22:02:37.1109+02:00"),
                     "\"2026-10-14T22:02:37.1109+02:00\"", 1792008157110LL);

    {
        cJSON *copy = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(fixtures, "v1"), 1);
        cJSON *atom0 = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(copy, "atoms"), 0);
        cJSON_ReplaceItemInObjectCaseSensitive(atom0, "createdAt", cJSON_CreateString("not-a-date"));
        char *json = cJSON_PrintUnformatted(copy);
        cJSON_Delete(copy);
        knishio_served_verification_t r;
        const knishio_error_t rc = knishio_verify_from_server_data(json, &r);
        cJSON_free(json);
        char detail[320];
        snprintf(detail, sizeof(detail), "rc=%d verified=%d error='%s'", (int)rc, (int)r.verified, r.error);
        check(rc == KNISHIO_SUCCESS && !r.verified && strcmp(r.error, "invalid createdAt") == 0,
              "createdAt \"not-a-date\" gives verified=false with \"invalid createdAt\"", detail);
    }

    printf("Malformed input\n");
    {
        knishio_served_verification_t r;
        check(knishio_verify_from_server_data(NULL, &r) == KNISHIO_ERROR_INVALID_ARGS
                  && knishio_verify_from_server_data("{}", NULL) == KNISHIO_ERROR_INVALID_ARGS,
              "NULL arguments return KNISHIO_ERROR_INVALID_ARGS", NULL);

        static const char *const malformed[] = { "[]", "{\"molecularHash\":\"abc\"}", "not json",
                                                 "{\"atoms\":[1]}" };
        for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); i++) {
            const knishio_error_t rc = knishio_verify_from_server_data(malformed[i], &r);
            char name[160];
            char detail[320];
            snprintf(name, sizeof(name), "malformed input %s gives verified=false with an error", malformed[i]);
            snprintf(detail, sizeof(detail), "rc=%d verified=%d error='%s'", (int)rc, (int)r.verified, r.error);
            check(rc == KNISHIO_SUCCESS && !r.verified && r.error[0] != '\0', name, detail);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Version4 hash vectors (cases) and legacy hash vectors (legacyCases) */
/* ------------------------------------------------------------------ */
/* A legacy case has unversioned atoms and no viewJson: only its molecular hash is checked. */
static void vector_case(const cJSON *test_case, bool legacy) {
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(test_case, "name"));
    const char *view_expected = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(test_case, "viewJson"));
    const char *hash_expected = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(test_case, "expectedHash"));
    const cJSON *atoms_json = cJSON_GetObjectItemCaseSensitive(test_case, "atoms");
    char label[160];

    knishio_molecule_t *mol = NULL;
    bool built = name && (legacy || view_expected) && hash_expected && cJSON_IsArray(atoms_json)
        && knishio_molecule_create(&mol, NULL, NULL, NULL, NULL, NULL, NULL) == KNISHIO_SUCCESS;
    const cJSON *atom_json = NULL;
    cJSON_ArrayForEach(atom_json, atoms_json) {
        if (!built) break;
        char *text = cJSON_PrintUnformatted(atom_json);
        knishio_atom_t *atom = NULL;
        built = text && knishio_atom_from_json(text, &atom) == KNISHIO_SUCCESS;
        cJSON_free(text);
        if (!built) break;
        const int index = atom->index;
        if (knishio_molecule_add_atom(mol, atom) != KNISHIO_SUCCESS) {
            knishio_atom_free_deep(atom);
            built = false;
            break;
        }
        atom->index = index;
    }

    snprintf(label, sizeof(label), "%s: client-format atoms parse", name ? name : "?");
    check(built, label, NULL);
    if (!built) {
        knishio_molecule_free_deep(mol);
        return;
    }

    if (!legacy) {
        char *view = knishio_atoms_version4_view_json(mol->atoms, mol->atom_count);
        snprintf(label, sizeof(label), "%s: Version4 view equals viewJson", name);
        const bool view_ok = view && strcmp(view, view_expected) == 0;
        check(view_ok, label, NULL);
        if (!view_ok) {
            printf("    expected: %s\n    actual:   %s\n", view_expected, view ? view : "(null)");
        }
        knishio_free(view);
    }

    const knishio_error_t err = knishio_molecule_generate_hash(mol);
    char detail[200];
    snprintf(detail, sizeof(detail), "err=%d hash=%s", (int)err,
             mol->molecular_hash ? mol->molecular_hash : "(null)");
    snprintf(label, sizeof(label), "%s: molecular hash equals expectedHash", name);
    check(err == KNISHIO_SUCCESS && mol->molecular_hash && strcmp(mol->molecular_hash, hash_expected) == 0,
          label, detail);
    knishio_molecule_free_deep(mol);
}

/* Runs every case under `key`; their names must be exactly `expected`, in order. */
static void vector_suite(const cJSON *vectors, const char *key, bool legacy,
                         const char *const *expected, size_t expected_count, const char *label) {
    const cJSON *test_case = NULL;
    size_t cases = 0;
    bool names_match = true;
    cJSON_ArrayForEach(test_case, cJSON_GetObjectItemCaseSensitive(vectors, key)) {
        const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(test_case, "name"));
        if (cases >= expected_count || !name || strcmp(name, expected[cases]) != 0) {
            names_match = false;
        }
        vector_case(test_case, legacy);
        cases++;
    }
    char detail[64];
    snprintf(detail, sizeof(detail), "%zu cases", cases);
    check(names_match && cases == expected_count, label, detail);
}

int main(void) {
    char *served_text = slurp(KNISHIO_SERVED_MOLECULES_PATH);
    char *vectors_text = slurp(KNISHIO_VERSION4_VECTORS_PATH);
    if (!served_text || !vectors_text) {
        printf("SKIP: fixtures not found (%s, %s)\n", KNISHIO_SERVED_MOLECULES_PATH,
               KNISHIO_VERSION4_VECTORS_PATH);
        free(served_text);
        free(vectors_text);
        return SKIP_EXIT_CODE;
    }
    cJSON *served = cJSON_Parse(served_text);
    cJSON *vectors = cJSON_Parse(vectors_text);
    free(served_text);
    free(vectors_text);
    if (!served || !vectors) {
        printf("FAIL: fixtures are not JSON\n");
        cJSON_Delete(served);
        cJSON_Delete(vectors);
        return 1;
    }

    served_tests(served);

    printf("Version4 hash vectors\n");
    static const char *const expected_cases[] = {
        "v4-basic", "v3-basic", "slash-unicode", "control-quote", "line-separators",
        "nulls", "empty-strings", "empty-version", "index-order"
    };
    vector_suite(vectors, "cases", false, expected_cases,
                 sizeof(expected_cases) / sizeof(expected_cases[0]),
                 "the Version4 vector cases are exactly v4-basic, v3-basic, slash-unicode, "
                 "control-quote, line-separators, nulls, empty-strings, empty-version, index-order");

    printf("Legacy hash vectors\n");
    static const char *const expected_legacy[] = { "legacy-empty-strings", "legacy-nulls" };
    vector_suite(vectors, "legacyCases", true, expected_legacy,
                 sizeof(expected_legacy) / sizeof(expected_legacy[0]),
                 "the legacy vector cases are exactly legacy-empty-strings, legacy-nulls");

    cJSON_Delete(served);
    cJSON_Delete(vectors);
    printf("%s (%d failures)\n", g_failures == 0 ? "OK" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
