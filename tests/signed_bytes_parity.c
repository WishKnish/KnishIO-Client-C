/**
 * @file signed_bytes_parity.c
 * @brief Molecules the C builders make hash to the same molecular hash as the JS SDK's.
 *
 * Every digest below was printed by the JS SDK (KnishIO-Client-JS dist build, KNISHIO_KCORE=off)
 * and by this SDK from the same pinned inputs, and the two outputs were character-identical
 * (paired probe, 2026-11-12). Inputs: secret = generateSecret("fleet-audit-2026-11"), wallets at
 * positions "aa"x32 / "bb"x32 / "cc"x32 (OTS v1), createdAt = "1700000000000" written on every
 * atom, hashed with the molecular hash (Atom.hashAtoms in JS), not a signed molecule.
 *
 * Cases:
 *   auth_u          initAuthorization as requestAuthToken fills it: U meta [encrypt, walletPubkey]
 *   meta_v4         initMeta on a version "4" molecule: every atom carries version "4"
 *   burn_batched    burnToken with a batched source and remainder (batchId on both, not the burn)
 *   transfer_large  initValue of 2^53-1: the values are "-9007199254740991", "9007199254740991",
 *                   "0" (canonical-patent-vectors atom_value_format.large_i64)
 *   identifier      initIdentifierCreation: C meta [code, hash = SHAKE256(contact.trim())]
 *   policy          addPolicyAtom + addContinuIdAtom: R meta [policy]
 *   rule            createRule: R meta [rule, policy] with the JS PolicyMeta defaults
 * Then P (peering) and A (append request) molecules signed by the JS SDK
 * (tests/fixtures/peering-append-molecules.json) parse and pass knishio_molecule_check().
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "knishio/knishio.h"
#include "knishio/atom.h"
#include "knishio/meta.h"
#include "knishio/molecule.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"

#ifndef KNISHIO_PEERING_APPEND_FIXTURE_PATH
#define KNISHIO_PEERING_APPEND_FIXTURE_PATH "tests/fixtures/peering-append-molecules.json"
#endif

#define POS_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define POS_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define POS_C "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
#define BATCH "babababababababababababababababababababababababababababababababa"

static int g_failures = 0;
static char *g_secret = NULL;

static void check(bool ok, const char *name, const char *detail) {
    printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name, detail ? " — " : "", detail ? detail : "");
    if (!ok) g_failures++;
}

static knishio_wallet_t *wallet(const char *token, const char *position) {
    knishio_wallet_t *w = NULL;
    return knishio_wallet_create_with_ots_version(&w, g_secret, token, position, KNISHIO_OTS_V1) ? w : NULL;
}

typedef struct {
    knishio_molecule_t *m;
    knishio_wallet_t *w[3];
} built_t;

static built_t begin(const char *src_token, const char *rem_token, const char *version) {
    built_t b = { 0 };
    b.w[0] = wallet(src_token, POS_A);
    b.w[1] = wallet(rem_token, POS_C);
    if (b.w[0] && b.w[1]) {
        knishio_molecule_create(&b.m, g_secret, NULL, b.w[0], b.w[1], NULL, version);
    }
    return b;
}

static void expect_digest(const char *name, built_t *b, knishio_error_t built, const char *want) {
    char detail[200];
    bool ok = b->m && built == KNISHIO_SUCCESS;
    if (ok) {
        for (size_t i = 0; i < b->m->atom_count; i++) b->m->atoms[i]->created_at_ms = 1700000000000LL;
        ok = knishio_molecule_generate_hash(b->m) == KNISHIO_SUCCESS && strcmp(b->m->molecular_hash, want) == 0;
        snprintf(detail, sizeof(detail), "got %s", b->m->molecular_hash ? b->m->molecular_hash : "(none)");
    } else {
        snprintf(detail, sizeof(detail), "build failed (%d)", (int)built);
    }
    check(ok, name, detail);
    knishio_molecule_free_deep(b->m);
    for (size_t i = 0; i < 3; i++) knishio_wallet_free(b->w[i]);
}

static void test_digests(void) {
    printf("Molecular hashes equal the JS SDK's\n");
    {
        built_t b = begin("AUTH", "USER", NULL);
        knishio_error_t e = b.m ? knishio_molecule_init_authorization(b.m, false) : KNISHIO_ERROR_MEMORY;
        expect_digest("auth_u", &b, e, "0308f41249ef5c77fb3ag9417170026840c4ac34gc62d4268df581aa1b3b8a64");
    }
    {
        built_t b = begin("USER", "USER", "4");
        const char *k[] = { "name" }, *v[] = { "x" };
        knishio_error_t e = b.m ? knishio_molecule_init_meta(b.m, "profile", "p1", k, v, 1) : KNISHIO_ERROR_MEMORY;
        bool versioned = e == KNISHIO_SUCCESS;
        for (size_t i = 0; versioned && i < b.m->atom_count; i++) {
            versioned = b.m->atoms[i]->version && strcmp(b.m->atoms[i]->version, "4") == 0;
        }
        check(versioned, "meta_v4 atoms carry version \"4\"", NULL);
        expect_digest("meta_v4", &b, e, "00328d9810bfce306014cdd708c578g8e07d959940b825dc9fdaeb5gd9375c4a");
    }
    {
        built_t b = begin("TEST", "TEST", NULL);
        knishio_error_t e = KNISHIO_ERROR_MEMORY;
        if (b.m) {
            b.w[0]->balance = 1000;
            b.w[0]->batch_id = knishio_strdup(BATCH);
            b.w[1]->batch_id = knishio_strdup(BATCH);
            e = knishio_molecule_init_burn(b.m, 300);
        }
        expect_digest("burn_batched", &b, e, "045ee247086d5d2gg43f1ea00732g243g4ad3cdd670d7gc339f915eg6b0e0bf4");
    }
    {
        built_t b = begin("TEST", "TEST", NULL);
        knishio_error_t e = KNISHIO_ERROR_MEMORY;
        b.w[2] = wallet("TEST", POS_B);
        if (b.m && b.w[2]) {
            b.w[0]->balance = 9007199254740991.0;
            e = knishio_molecule_init_value(b.m, b.w[2], 9007199254740991LL);
        }
        bool values = e == KNISHIO_SUCCESS && b.m->atom_count == 3
            && strcmp(b.m->atoms[0]->value, "-9007199254740991") == 0
            && strcmp(b.m->atoms[1]->value, "9007199254740991") == 0
            && strcmp(b.m->atoms[2]->value, "0") == 0;
        check(values, "transfer of 2^53-1 writes -9007199254740991 / 9007199254740991 / 0", NULL);
        expect_digest("transfer_large", &b, e, "029bcca23006d7cg31db77b15caef18616ffdfbgbfa76700304gf8efe5b2ccaf");
    }
}

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    if (buf) buf[n] = '\0';
    fclose(f);
    return buf;
}

#include <cjson/cJSON.h>

static void test_peering_append(void) {
    printf("P and A molecules signed by the JS SDK parse and verify\n");
    char *text = slurp(KNISHIO_PEERING_APPEND_FIXTURE_PATH);
    cJSON *root = text ? cJSON_Parse(text) : NULL;
    free(text);
    check(root != NULL, "fixture loads", KNISHIO_PEERING_APPEND_FIXTURE_PATH);
    static const char *const names[] = { "peering", "append" };
    static const knishio_isotope_t isotopes[] = { KNISHIO_ISOTOPE_P, KNISHIO_ISOTOPE_A };
    for (size_t i = 0; root && i < 2; i++) {
        char *json = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(root, names[i]));
        knishio_molecule_t *m = NULL;
        knishio_error_t parsed = json ? knishio_molecule_from_json(json, &m) : KNISHIO_ERROR_MEMORY;
        cJSON_free(json);
        char detail[96];
        snprintf(detail, sizeof(detail), "parse %d", (int)parsed);
        check(parsed == KNISHIO_SUCCESS && m->atoms[0]->isotope == isotopes[i], names[i], detail);
        if (parsed == KNISHIO_SUCCESS) {
            knishio_error_t verdict = knishio_molecule_check(m, NULL);
            snprintf(detail, sizeof(detail), "check %d", (int)verdict);
            check(verdict == KNISHIO_SUCCESS, names[i], detail);
        }
        knishio_molecule_free_deep(m);
    }
    cJSON_Delete(root);
}

int main(void) {
    if (!knishio_generate_secret("fleet-audit-2026-11", 2048, &g_secret)) {
        printf("FAIL: secret\n");
        return 1;
    }
    test_digests();
    test_peering_append();
    knishio_free(g_secret);
    printf("%s (%d failure%s)\n", g_failures ? "FAIL" : "PASS", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
