/**
 * @file molecule_from_json.c
 * @brief knishio_molecule_from_json() + knishio_molecule_check() must verify a molecule another
 *        SDK signed — the capability C's cross-validation needs to be more than a shape check.
 *
 * Until this was implemented, knishio_molecule_from_json() returned
 * KNISHIO_ERROR_NOT_IMPLEMENTED, so the self-test "validated" a peer molecule by checking that
 * molecularHash was a string and atoms was non-empty: a forged signature passed. Every assertion
 * here is driven by the committed cross-SDK vector vectors.wotsSignedMetadataMolecule — the
 * self-test metaCreation molecule signed by KnishIO-Client-JS, whose one-time signature six other
 * SDKs reproduce byte for byte. Each mutation changes exactly one thing, so each assertion can
 * fail for exactly one reason.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "knishio/knishio.h"
#include "knishio/atom.h"
#include "knishio/meta.h"
#include "knishio/molecule.h"

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

/* ------------------------------------------------------------------ */
/* Mutations of a deep copy of the frozen molecule                    */
/* ------------------------------------------------------------------ */
typedef void (*mutation_fn)(cJSON *molecule);

static cJSON *atom_at(const cJSON *molecule, int i) {
    return cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(molecule, "atoms"), i);
}

/* One base64 character of the second fragment: the signature no longer matches the signer. */
static void flip_ots_char(cJSON *m) {
    char *ots = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(atom_at(m, 1), "otsFragment"));
    if (ots && strlen(ots) > 10) ots[10] = (ots[10] == 'A') ? 'B' : 'A';
}

/* A hashed field changes: the recomputed molecular hash no longer matches. */
static void alter_hashed_meta(cJSON *m) {
    cJSON *meta0 = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(atom_at(m, 0), "meta"), 0);
    cJSON_ReplaceItemInObjectCaseSensitive(meta0, "value", cJSON_CreateString("Test Metadata!"));
}

/* The shape JS, TS, Python and PHP emit for an unset meta value. */
static void append_null_meta(cJSON *m) {
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "key", "walletBatchId");
    cJSON_AddNullToObject(entry, "value");
    cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(atom_at(m, 0), "meta"), entry);
}

/* Kotlin omits null-valued keys instead of writing null. */
static void omit_null_keys(cJSON *m) {
    cJSON_DeleteItemFromObjectCaseSensitive(atom_at(m, 0), "value");
    cJSON_DeleteItemFromObjectCaseSensitive(atom_at(m, 0), "batchId");
}

static void sub_second_created_at(cJSON *m) {
    cJSON_ReplaceItemInObjectCaseSensitive(atom_at(m, 0), "createdAt", cJSON_CreateString("1700000000123"));
}

static void drop_index(cJSON *m) {
    cJSON_DeleteItemFromObjectCaseSensitive(atom_at(m, 0), "index");
}

static void unknown_isotope(cJSON *m) {
    cJSON_ReplaceItemInObjectCaseSensitive(atom_at(m, 0), "isotope", cJSON_CreateString("Z"));
}

typedef struct {
    knishio_error_t parsed;   /* knishio_molecule_from_json() */
    knishio_error_t verdict;  /* knishio_molecule_check(), or `parsed` when parsing failed */
    size_t meta0_count;       /* atoms[0]->meta_count when parsed */
    bool null_on_failure;     /* the out-pointer stayed NULL when parsing failed */
} outcome_t;

static outcome_t run(const cJSON *molecule, mutation_fn mutate) {
    outcome_t out = { KNISHIO_ERROR_MEMORY, KNISHIO_ERROR_MEMORY, 0, true };
    cJSON *copy = cJSON_Duplicate(molecule, true);
    if (!copy) return out;
    if (mutate) mutate(copy);
    char *text = cJSON_PrintUnformatted(copy);
    cJSON_Delete(copy);
    if (!text) return out;

    knishio_molecule_t *m = NULL;
    out.parsed = knishio_molecule_from_json(text, &m);
    cJSON_free(text);
    out.null_on_failure = (out.parsed == KNISHIO_SUCCESS) || (m == NULL);
    out.verdict = (out.parsed == KNISHIO_SUCCESS) ? knishio_molecule_check(m, NULL) : out.parsed;
    out.meta0_count = (m && m->atom_count > 0) ? m->atoms[0]->meta_count : 0;
    knishio_molecule_free_deep(m);
    return out;
}

/* ------------------------------------------------------------------ */
/* Cases                                                              */
/* ------------------------------------------------------------------ */
static void test_frozen_molecule(const cJSON *vec) {
    printf("\nThe frozen JS-signed metadata molecule deserializes and verifies\n");
    const cJSON *molecule = cJSON_GetObjectItemCaseSensitive(vec, "molecule");
    const char *expected_hash = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(vec, "expectedMolecularHash"));
    char detail[256];

    char *text = cJSON_PrintUnformatted(molecule);
    knishio_molecule_t *m = NULL;
    knishio_error_t err = text ? knishio_molecule_from_json(text, &m) : KNISHIO_ERROR_MEMORY;
    cJSON_free(text);

    const bool fields = err == KNISHIO_SUCCESS && m && m->atom_count == 2
        && m->atoms[0]->isotope == KNISHIO_ISOTOPE_M && m->atoms[1]->isotope == KNISHIO_ISOTOPE_I
        && m->atoms[0]->meta_count == 2
        && m->atoms[0]->meta_type && strcmp(m->atoms[0]->meta_type, "TestMeta") == 0
        && m->atoms[0]->meta_id && strcmp(m->atoms[0]->meta_id, "TESTMETA123") == 0
        && m->atoms[0]->created_at == (time_t)1700000000
        && m->atoms[1]->index == 1
        && m->molecular_hash && expected_hash && strcmp(m->molecular_hash, expected_hash) == 0;
    snprintf(detail, sizeof(detail), "err=%d, %zu atoms, hash=%s", (int)err, m ? m->atom_count : (size_t)0,
             (m && m->molecular_hash) ? m->molecular_hash : "(null)");
    check(fields, "knishio_molecule_from_json reproduces every hashed field of the frozen molecule", detail);

    const knishio_error_t verdict = m ? knishio_molecule_check(m, NULL) : err;
    snprintf(detail, sizeof(detail), "knishio_molecule_check=%d (%s)", (int)verdict, knishio_error_to_string(verdict));
    check(verdict == KNISHIO_SUCCESS,
          "knishio_molecule_check accepts another SDK's signature (hash, OTS, isotopes)", detail);
    knishio_molecule_free_deep(m);
}

static void test_rejections_and_shapes(const cJSON *vec) {
    printf("\nOne mutation at a time: what must be rejected, and which peer shapes must still verify\n");
    const cJSON *molecule = cJSON_GetObjectItemCaseSensitive(vec, "molecule");
    char detail[160];
    outcome_t o;

    o = run(molecule, flip_ots_char);
    snprintf(detail, sizeof(detail), "parsed=%d verdict=%d", (int)o.parsed, (int)o.verdict);
    check(o.parsed == KNISHIO_SUCCESS && o.verdict == KNISHIO_ERROR_SIGNATURE_MISMATCH,
          "a flipped OTS character is rejected as KNISHIO_ERROR_SIGNATURE_MISMATCH", detail);

    o = run(molecule, alter_hashed_meta);
    snprintf(detail, sizeof(detail), "parsed=%d verdict=%d", (int)o.parsed, (int)o.verdict);
    check(o.parsed == KNISHIO_SUCCESS && o.verdict == KNISHIO_ERROR_MOLECULAR_HASH_MISMATCH,
          "an altered hashed meta value is rejected as KNISHIO_ERROR_MOLECULAR_HASH_MISMATCH", detail);

    o = run(molecule, append_null_meta);
    snprintf(detail, sizeof(detail), "parsed=%d verdict=%d meta_count=%zu", (int)o.parsed, (int)o.verdict, o.meta0_count);
    check(o.parsed == KNISHIO_SUCCESS && o.meta0_count == 2 && o.verdict == KNISHIO_SUCCESS,
          "a {key, value: null} meta entry is dropped, as JS hashing skips it, and the molecule still verifies", detail);

    o = run(molecule, omit_null_keys);
    snprintf(detail, sizeof(detail), "parsed=%d verdict=%d", (int)o.parsed, (int)o.verdict);
    check(o.parsed == KNISHIO_SUCCESS && o.verdict == KNISHIO_SUCCESS,
          "absent value/batchId keys (Kotlin's shape) read as null and the molecule still verifies", detail);

    o = run(molecule, sub_second_created_at);
    snprintf(detail, sizeof(detail), "parsed=%d null_on_failure=%d", (int)o.parsed, (int)o.null_on_failure);
    check(o.parsed == KNISHIO_ERROR_INVALID_JSON && o.null_on_failure,
          "a sub-second atom createdAt is rejected (time_t seconds cannot hold it) instead of changing the hash", detail);

    o = run(molecule, drop_index);
    snprintf(detail, sizeof(detail), "parsed=%d", (int)o.parsed);
    check(o.parsed == KNISHIO_ERROR_INVALID_JSON, "an atom without an index is rejected", detail);

    o = run(molecule, unknown_isotope);
    snprintf(detail, sizeof(detail), "parsed=%d", (int)o.parsed);
    check(o.parsed == KNISHIO_ERROR_INVALID_JSON, "an unknown isotope is rejected", detail);

    knishio_molecule_t *m = NULL;
    const knishio_error_t err = knishio_molecule_from_json("{", &m);
    snprintf(detail, sizeof(detail), "err=%d", (int)err);
    check(err == KNISHIO_ERROR_JSON_PARSE && m == NULL, "text that is not JSON is KNISHIO_ERROR_JSON_PARSE", detail);
}

static void test_atom_from_json(const cJSON *vec) {
    printf("\nknishio_atom_from_json parses every field knishio_atom_to_json emits\n");
    const cJSON *atom0 = atom_at(cJSON_GetObjectItemCaseSensitive(vec, "molecule"), 0);
    char detail[160];

    char *text = cJSON_PrintUnformatted(atom0);
    knishio_atom_t *a = NULL;
    const knishio_error_t err = text ? knishio_atom_from_json(text, &a) : KNISHIO_ERROR_MEMORY;
    cJSON_free(text);

    /* The four fields the previous (placeholder) path dropped. */
    const bool ok = err == KNISHIO_SUCCESS && a
        && a->meta_type && strcmp(a->meta_type, "TestMeta") == 0
        && a->meta_id && strcmp(a->meta_id, "TESTMETA123") == 0
        && a->meta_count == 2
        && a->created_at == (time_t)1700000000;
    snprintf(detail, sizeof(detail), "err=%d meta_count=%zu", (int)err, a ? a->meta_count : (size_t)0);
    check(ok, "metaType, metaId, meta and createdAt all survive", detail);
    knishio_atom_free_deep(a);
}

int main(void) {
    const char *env_path = getenv("KNISHIO_CROSS_PLATFORM_VECTORS");
    const char *path = env_path ? env_path : KNISHIO_TEST_VECTORS_PATH;
    char *text = slurp(path);
    if (!text) {
        fprintf(stderr, "molecule_from_json: cannot read cross-SDK vectors at %s\n", path);
        return 2;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    const cJSON *vectors = root ? cJSON_GetObjectItemCaseSensitive(root, "vectors") : NULL;
    const cJSON *vec = vectors ? cJSON_GetObjectItemCaseSensitive(vectors, "wotsSignedMetadataMolecule") : NULL;
    if (!vec || !cJSON_GetObjectItemCaseSensitive(vec, "molecule")) {
        /* The fixture is the whole point of this binary: skipping would publish a pass having
         * asserted nothing. */
        fprintf(stderr, "molecule_from_json: %s has no vectors.wotsSignedMetadataMolecule.molecule\n", path);
        if (root) cJSON_Delete(root);
        return 2;
    }

    printf("Molecule JSON deserialization + verification suite (SDK %s)\n", KNISHIO_VERSION_STRING);

    test_frozen_molecule(vec);
    test_rejections_and_shapes(vec);
    test_atom_from_json(vec);

    cJSON_Delete(root);
    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
