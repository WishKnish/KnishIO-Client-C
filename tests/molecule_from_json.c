/**
 * @file molecule_from_json.c
 * @brief knishio_molecule_from_json() + knishio_molecule_check() must verify a molecule another
 *        SDK signed — the capability C's cross-validation needs to be more than a shape check.
 *
 * Until this was implemented, knishio_molecule_from_json() returned
 * KNISHIO_ERROR_NOT_IMPLEMENTED, so the self-test "validated" a peer molecule by checking that
 * molecularHash was a string and atoms was non-empty: a forged signature passed. The parsing
 * assertions are driven by the committed cross-SDK vector vectors.wotsSignedMetadataMolecule —
 * the self-test metaCreation molecule signed by KnishIO-Client-JS, whose one-time signature every
 * other SDK reproduces byte for byte. Each mutation changes exactly one thing, so each assertion
 * can fail for exactly one reason. C's own signature of that molecule is pinned to the same
 * vector, and the V-atom token rule is checked on a signed V transfer built here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "knishio/knishio.h"
#include "knishio/atom.h"
#include "knishio/meta.h"
#include "knishio/molecule.h"
#include "knishio/wallet.h"
#include "knishio/json/parser.h"
#include "knishio/json/serializers.h"
#include "knishio/utils/memory.h"

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

/* A peer molecule with no signature at all: otsFragment is optional in the parser, so this
 * parses, and check_ots() rebuilds an empty OTS from it. Under ASan this case caught
 * knishio_base64_decode() reading before that empty string. */
static void strip_ots(cJSON *m) {
    for (int i = 0; atom_at(m, i) != NULL; i++) {
        cJSON_DeleteItemFromObjectCaseSensitive(atom_at(m, i), "otsFragment");
    }
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

    o = run(molecule, strip_ots);
    snprintf(detail, sizeof(detail), "parsed=%d verdict=%d", (int)o.parsed, (int)o.verdict);
    check(o.parsed == KNISHIO_SUCCESS && o.verdict == KNISHIO_ERROR_SIGNATURE_MISMATCH,
          "an unsigned peer molecule (no otsFragment) is rejected as KNISHIO_ERROR_SIGNATURE_MISMATCH", detail);

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

/* The four fields the previous (placeholder) parsers dropped. */
static bool keeps_dropped_fields(knishio_error_t err, const knishio_atom_t *a) {
    return err == KNISHIO_SUCCESS && a
        && a->meta_type && strcmp(a->meta_type, "TestMeta") == 0
        && a->meta_id && strcmp(a->meta_id, "TESTMETA123") == 0
        && a->meta_count == 2
        && a->created_at == (time_t)1700000000;
}

/* [copy of `atom`] or, when `malformed`, [copy of `atom`, {"isotope": "Z"}], as knishio_json_t. */
static knishio_json_t *atom_array_json(const cJSON *atom, bool malformed) {
    cJSON *array = cJSON_CreateArray();
    if (!array) return NULL;
    cJSON_AddItemToArray(array, cJSON_Duplicate(atom, true));
    if (malformed) {
        cJSON *bad = cJSON_CreateObject();
        cJSON_AddStringToObject(bad, "isotope", "Z");
        cJSON_AddItemToArray(array, bad);
    }
    char *text = cJSON_PrintUnformatted(array);
    cJSON_Delete(array);
    char *err_msg = NULL;
    knishio_json_t *json = text ? knishio_json_parse(text, &err_msg) : NULL;
    cJSON_free(text);
    knishio_free(err_msg);
    return json;
}

static void test_atom_from_json(const cJSON *vec) {
    printf("\nOne atom parser: knishio_atom_from_json, and the json/serializers.h entry points through it\n");
    const cJSON *atom0 = atom_at(cJSON_GetObjectItemCaseSensitive(vec, "molecule"), 0);
    char detail[160];

    char *text = cJSON_PrintUnformatted(atom0);
    knishio_atom_t *a = NULL;
    knishio_error_t err = text ? knishio_atom_from_json(text, &a) : KNISHIO_ERROR_MEMORY;
    snprintf(detail, sizeof(detail), "err=%d meta_count=%zu", (int)err, a ? a->meta_count : (size_t)0);
    check(keeps_dropped_fields(err, a), "metaType, metaId, meta and createdAt all survive", detail);
    knishio_atom_free_deep(a);

    a = NULL;
    err = text ? knishio_atom_from_json_string(text, &a) : KNISHIO_ERROR_MEMORY;
    cJSON_free(text);
    snprintf(detail, sizeof(detail), "err=%d meta_count=%zu", (int)err, a ? a->meta_count : (size_t)0);
    check(keeps_dropped_fields(err, a), "knishio_atom_from_json_string keeps all four as well", detail);
    knishio_atom_free_deep(a);

    knishio_json_t *array = atom_array_json(atom0, false);
    knishio_atom_t **atoms = NULL;
    size_t count = 0;
    err = array ? knishio_json_parse_atom_array(array, &atoms, &count) : KNISHIO_ERROR_MEMORY;
    snprintf(detail, sizeof(detail), "err=%d count=%zu", (int)err, count);
    check(count == 1 && keeps_dropped_fields(err, atoms[0]),
          "knishio_json_parse_atom_array (through knishio_atom_from_json_obj) keeps all four as well", detail);
    for (size_t i = 0; i < count; i++) knishio_atom_free_deep(atoms[i]);
    knishio_free(atoms);
    knishio_json_free(array);

    /* The out-parameters start non-empty, so NULL and 0 afterwards are the function's doing. */
    array = atom_array_json(atom0, true);
    knishio_atom_t *sentinel = NULL;
    atoms = &sentinel;
    count = 99;
    err = array ? knishio_json_parse_atom_array(array, &atoms, &count) : KNISHIO_ERROR_MEMORY;
    snprintf(detail, sizeof(detail), "err=%d count=%zu atoms=%s", (int)err, count, atoms ? "non-NULL" : "NULL");
    check(err == KNISHIO_ERROR_INVALID_JSON && count == 0 && atoms == NULL,
          "knishio_json_parse_atom_array rejects an array containing a malformed atom", detail);
    knishio_json_free(array);

    /* Argument errors reset the outputs too, so a caller that frees on failure never sees a stale
     * pointer. Each argument has the wrong JSON type; the outputs start as a non-NULL address. */
    static char not_an_output;
    knishio_atom_t *atom_out = (knishio_atom_t *)(void *)&not_an_output;
    knishio_molecule_t *molecule_out = (knishio_molecule_t *)(void *)&not_an_output;
    char *err_msg = NULL;
    knishio_json_t *object = knishio_json_parse("{}", &err_msg);
    knishio_free(err_msg);
    array = atom_array_json(atom0, false);
    atoms = &sentinel;
    count = 99;
    const knishio_error_t e_atom = array ? knishio_atom_from_json_obj(array, &atom_out) : KNISHIO_ERROR_MEMORY;
    const knishio_error_t e_mol = array ? knishio_molecule_from_json_obj(array, &molecule_out) : KNISHIO_ERROR_MEMORY;
    const knishio_error_t e_arr = object ? knishio_json_parse_atom_array(object, &atoms, &count) : KNISHIO_ERROR_MEMORY;
    snprintf(detail, sizeof(detail), "atom_obj=%d %s, molecule_obj=%d %s, atom_array=%d %s count=%zu",
             (int)e_atom, atom_out ? "non-NULL" : "NULL", (int)e_mol, molecule_out ? "non-NULL" : "NULL",
             (int)e_arr, atoms ? "non-NULL" : "NULL", count);
    check(e_atom == KNISHIO_ERROR_INVALID_ARGS && atom_out == NULL
              && e_mol == KNISHIO_ERROR_INVALID_ARGS && molecule_out == NULL
              && e_arr == KNISHIO_ERROR_INVALID_ARGS && atoms == NULL && count == 0,
          "a wrong-type argument leaves every json/serializers.h parse output empty", detail);
    knishio_json_free(object);
    knishio_json_free(array);

    /* A NULL input is an argument error as well, and resets the output the same way. */
    knishio_atom_t *atom_null = (knishio_atom_t *)(void *)&not_an_output;
    knishio_atom_t *atom_string_null = (knishio_atom_t *)(void *)&not_an_output;
    knishio_molecule_t *molecule_null = (knishio_molecule_t *)(void *)&not_an_output;
    const knishio_error_t n_atom = knishio_atom_from_json(NULL, &atom_null);
    const knishio_error_t n_string = knishio_atom_from_json_string(NULL, &atom_string_null);
    const knishio_error_t n_mol = knishio_molecule_from_json(NULL, &molecule_null);
    snprintf(detail, sizeof(detail), "atom=%d %s, atom_string=%d %s, molecule=%d %s",
             (int)n_atom, atom_null ? "non-NULL" : "NULL", (int)n_string,
             atom_string_null ? "non-NULL" : "NULL", (int)n_mol, molecule_null ? "non-NULL" : "NULL");
    check(n_atom == KNISHIO_ERROR_INVALID_ARGS && atom_null == NULL
              && n_string == KNISHIO_ERROR_INVALID_ARGS && atom_string_null == NULL
              && n_mol == KNISHIO_ERROR_INVALID_ARGS && molecule_null == NULL,
          "a NULL input leaves the knishio_atom_from_json, _string and knishio_molecule_from_json output NULL",
          detail);
}

/* ------------------------------------------------------------------ */
/* The V-atom token rule                                              */
/* ------------------------------------------------------------------ */

/* A signed three-atom V transfer (-1000 / +1000 / 0) whose third atom carries `third_token`.
 * The hash and OTS are recomputed on every call, the V sum is 0, and atoms[0]'s token is not
 * USER (so ContinuID does not apply): the token rule is the only check that can reject it. */
static knishio_error_t verify_transfer(const char *secret, const char *bundle, knishio_wallet_t *source,
                                       const char *third_token) {
    knishio_molecule_t *m = NULL;
    knishio_error_t err = knishio_molecule_create(&m, secret, bundle, source, NULL, NULL, KNISHIO_VERSION_STRING);
    if (err != KNISHIO_SUCCESS) return err;

    const struct { const char *position, *address, *token, *value; } v[3] = {
        { source->position, source->address, "TEST", "-1000" },
        { "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210",
          "1111111111111111111111111111111111111111111111111111111111111111", "TEST", "1000" },
        { "bbbb000000000000cccc111111111111dddd222222222222eeee333333333333",
          "2222222222222222222222222222222222222222222222222222222222222222", third_token, "0" },
    };
    for (size_t i = 0; i < 3 && err == KNISHIO_SUCCESS; i++) {
        knishio_atom_t *a = NULL;
        err = knishio_atom_create(&a, v[i].position, v[i].address, KNISHIO_ISOTOPE_V, v[i].token, v[i].value, NULL);
        if (err == KNISHIO_SUCCESS && (err = knishio_molecule_add_atom(m, a)) != KNISHIO_SUCCESS) {
            knishio_atom_free_deep(a);
        }
    }
    if (err == KNISHIO_SUCCESS) err = knishio_molecule_generate_hash(m);
    if (err == KNISHIO_SUCCESS) err = knishio_molecule_sign(m, bundle, false, true);
    if (err == KNISHIO_SUCCESS) err = knishio_molecule_check(m, NULL);
    knishio_molecule_free_deep(m);
    return err;
}

static void test_v_token_rule(void) {
    printf("\nToken consistency is scoped to V atoms, and still enforced among them\n");
    char *secret = NULL;
    char *bundle = NULL;
    knishio_wallet_t *source = NULL;
    char detail[160];

    const bool setup = knishio_generate_secret("TESTSEED", 2048, &secret)
        && knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)
        && knishio_wallet_create_simple(&source, secret, "TEST",
               "0123456789abcdeffedcba9876543210fedcba9876543210fedcba9876543210") == KNISHIO_SUCCESS;
    if (!setup) {
        check(false, "V transfer setup (secret, bundle, TEST source wallet)", NULL);
    } else {
        knishio_error_t verdict = verify_transfer(secret, bundle, source, "TEST");
        snprintf(detail, sizeof(detail), "verdict=%d (%s)", (int)verdict, knishio_error_to_string(verdict));
        check(verdict == KNISHIO_SUCCESS, "control: the same signed three-atom V transfer with one token verifies", detail);

        verdict = verify_transfer(secret, bundle, source, "OTHER");
        snprintf(detail, sizeof(detail), "verdict=%d (%s)", (int)verdict, knishio_error_to_string(verdict));
        check(verdict == KNISHIO_ERROR_TRANSFER_MISMATCHED,
              "a V atom whose token differs from atoms[0]'s is rejected as TRANSFER_MISMATCHED", detail);
    }

    if (source) knishio_wallet_free(source);
    free(secret);
    free(bundle);
}

/* ------------------------------------------------------------------ */
/* C's own signature of the frozen molecule                           */
/* ------------------------------------------------------------------ */

static const char *vec_string(const cJSON *vec, const char *key) {
    return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(vec, key));
}

static void test_c_signs_the_frozen_vector(const cJSON *vec) {
    printf("\nC signs the frozen molecule byte-identically, and reads its own output back\n");
    const char *seed = vec_string(vec, "seed");
    const char *token = vec_string(vec, "token");
    const char *source_position = vec_string(vec, "sourcePosition");
    const char *remainder_position = vec_string(vec, "remainderPosition");
    const char *meta_type = vec_string(vec, "metaType");
    const char *meta_id = vec_string(vec, "metaId");
    const char *expected_hash = vec_string(vec, "expectedMolecularHash");
    const cJSON *expected_ots = cJSON_GetObjectItemCaseSensitive(vec, "expectedOtsFragments");
    const char *meta_keys[] = { "name", "description" };
    const char *meta_values[] = { "Test Metadata", "This is a test metadata for SDK testing." };
    char detail[256];

    char *secret = NULL;
    char *bundle = NULL;
    char *json = NULL;
    knishio_wallet_t *source = NULL;
    knishio_wallet_t *remainder = NULL;
    knishio_molecule_t *m = NULL;
    knishio_molecule_t *parsed = NULL;

    /* The metaCreation builder of self-test.c (test_meta_creation), canonical timestamps included. */
    bool built = seed && token && source_position && remainder_position && meta_type && meta_id
        && knishio_generate_secret(seed, 2048, &secret)
        && knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)
        && knishio_wallet_create_simple(&source, secret, token, source_position) == KNISHIO_SUCCESS
        && knishio_wallet_create_simple(&remainder, secret, token, remainder_position) == KNISHIO_SUCCESS
        && knishio_molecule_create(&m, secret, bundle, source, remainder, NULL, KNISHIO_VERSION_STRING) == KNISHIO_SUCCESS
        && knishio_molecule_init_meta(m, meta_type, meta_id, meta_keys, meta_values, 2) == KNISHIO_SUCCESS;
    if (built) {
        for (size_t i = 0; i < m->atom_count; i++) {
            m->atoms[i]->created_at = (time_t)(1700000000L + (long)i);
        }
        built = knishio_molecule_generate_hash(m) == KNISHIO_SUCCESS
            && knishio_molecule_sign(m, bundle, false, true) == KNISHIO_SUCCESS;
    }

    const char *c0 = (built && m->atom_count > 0) ? m->atoms[0]->ots_fragment : NULL;
    const char *c1 = (built && m->atom_count > 1) ? m->atoms[1]->ots_fragment : NULL;
    const char *v0 = cJSON_GetStringValue(cJSON_GetArrayItem(expected_ots, 0));
    const char *v1 = cJSON_GetStringValue(cJSON_GetArrayItem(expected_ots, 1));
    const bool identical = built && m->atom_count == 2 && cJSON_GetArraySize(expected_ots) == 2
        && c0 && c1 && v0 && v1 && strcmp(c0, v0) == 0 && strcmp(c1, v1) == 0
        && m->molecular_hash && expected_hash && strcmp(m->molecular_hash, expected_hash) == 0;
    snprintf(detail, sizeof(detail), "C [%zu,%zu] %.16s %.16s; vector [%zu,%zu] %.16s %.16s",
             c0 ? strlen(c0) : 0, c1 ? strlen(c1) : 0, c0 ? c0 : "(null)", c1 ? c1 : "(null)",
             v0 ? strlen(v0) : 0, v1 ? strlen(v1) : 0, v0 ? v0 : "(null)", v1 ? v1 : "(null)");
    check(identical, "C signs the metaCreation molecule byte-identically to the cross-SDK vector (684/684)", detail);

    knishio_error_t parse = KNISHIO_ERROR_MEMORY;
    knishio_error_t verdict = KNISHIO_ERROR_MEMORY;
    if (built && knishio_molecule_to_json(m, &json) == KNISHIO_SUCCESS && json) {
        parse = knishio_molecule_from_json(json, &parsed);
        verdict = (parse == KNISHIO_SUCCESS) ? knishio_molecule_check(parsed, NULL) : parse;
    }
    const bool same_hash = parsed && parsed->molecular_hash && m->molecular_hash
        && strcmp(parsed->molecular_hash, m->molecular_hash) == 0;
    snprintf(detail, sizeof(detail), "from_json=%d check=%d (%s) same hash=%d", (int)parse, (int)verdict,
             knishio_error_to_string(verdict), (int)same_hash);
    check(parse == KNISHIO_SUCCESS && verdict == KNISHIO_SUCCESS && same_hash,
          "C's own knishio_molecule_to_json output round-trips through knishio_molecule_from_json and verifies", detail);

    knishio_molecule_free_deep(parsed);
    knishio_molecule_free_deep(m);
    if (source) knishio_wallet_free(source);
    if (remainder) knishio_wallet_free(remainder);
    free(json);
    free(secret);
    free(bundle);
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
    test_v_token_rule();
    test_c_signs_the_frozen_vector(vec);

    cJSON_Delete(root);
    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
