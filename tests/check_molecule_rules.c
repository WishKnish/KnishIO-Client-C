/**
 * @file check_molecule_rules.c
 * @brief knishio_molecule_check() applies the JS reference rules (CheckMolecule.js) to
 *        correctly signed molecules.
 *
 * Every molecule here carries a valid molecular hash and one-time signature, so a rejection can
 * only come from the rule under test; each case asserts the exact error code. Covered:
 *   - V: non-primary negative value, transfer back to the signing address, a value that is not a
 *     number, a sum that misses zero by 0.005, a 2-atom transfer with a negative end atom, mixed
 *     tokens; a valid transfer is accepted;
 *   - atom order: the atoms are checked in index order, so a valid molecule whose JSON lists its
 *     atoms in reverse verifies, and a reversed malicious one is still rejected by the V rule;
 *   - M: readPolicy/writePolicy entries must name a meta key and list bundle hashes, "all" or
 *     "self";
 *   - T: a "wallet" metaType needs position and bundle meta; every T atom needs token meta;
 *   - R: policy keys are read/write only; rule is a non-empty array of {condition, callback};
 *   - P: USER token and peerHost meta; A: USER token, metaType, metaId and action meta.
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

static int g_failures = 0;

static void check(bool ok, const char *name, const char *detail) {
    printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name, detail ? " — " : "", detail ? detail : "");
    if (!ok) g_failures++;
}

#define POS_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define POS_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define POS_C "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
#define POS_D "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
#define BUNDLE_X "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

static char *g_secret = NULL;
static char *g_other_secret = NULL;

static knishio_wallet_t *wallet(const char *secret, const char *token, const char *position) {
    knishio_wallet_t *w = NULL;
    if (!knishio_wallet_create_with_ots_version(&w, secret, token, position, KNISHIO_OTS_V1)) {
        return NULL;
    }
    return w;
}

/* An atom at the wallet's position and address, with the given fields and meta pairs. */
static knishio_atom_t *atom_of(const knishio_wallet_t *w, knishio_isotope_t isotope, const char *token,
                               const char *value, const char *meta_type, const char *meta_id,
                               const char *const *keys, const char *const *vals, size_t n) {
    knishio_atom_t *a = NULL;
    if (knishio_atom_create(&a, w ? w->position : "", w ? w->address : "", isotope,
                            token ? token : (w ? w->token : NULL), value, NULL) != KNISHIO_SUCCESS) {
        return NULL;
    }
    knishio_free(a->meta_type);
    knishio_free(a->meta_id);
    a->meta_type = meta_type ? knishio_strdup(meta_type) : NULL;
    a->meta_id = meta_id ? knishio_strdup(meta_id) : NULL;
    if (n > 0) {
        a->meta = calloc(n, sizeof(knishio_meta_t *));
        a->meta_count = n;
        for (size_t i = 0; i < n; i++) {
            knishio_meta_create(&a->meta[i], keys[i], vals[i]);
        }
    }
    return a;
}

/* Sign the atoms (atoms[0] signs with g_secret), optionally reverse the atom array afterwards
 * (indices unchanged), and return knishio_molecule_check(m, NULL). */
static knishio_error_t sign_and_check(knishio_atom_t **atoms, size_t n, bool reverse) {
    knishio_molecule_t *m = NULL;
    if (knishio_molecule_create(&m, g_secret, NULL, NULL, NULL, NULL, NULL) != KNISHIO_SUCCESS) {
        return KNISHIO_ERROR_MEMORY;
    }
    for (size_t i = 0; i < n; i++) {
        if (!atoms[i] || knishio_molecule_add_atom(m, atoms[i]) != KNISHIO_SUCCESS) {
            knishio_molecule_free_deep(m);
            return KNISHIO_ERROR_MEMORY;
        }
    }
    knishio_error_t err = knishio_molecule_sign(m, NULL, false, true);
    if (err != KNISHIO_SUCCESS) {
        knishio_molecule_free_deep(m);
        return err;
    }
    if (reverse) {
        for (size_t i = 0; i < n / 2; i++) {
            knishio_atom_t *t = m->atoms[i];
            m->atoms[i] = m->atoms[n - 1 - i];
            m->atoms[n - 1 - i] = t;
        }
    }
    err = knishio_molecule_check(m, NULL);
    knishio_molecule_free_deep(m);
    return err;
}

static void expect(const char *name, knishio_error_t got, knishio_error_t want) {
    char detail[96];
    snprintf(detail, sizeof(detail), "got %d, want %d", (int)got, (int)want);
    check(got == want, name, detail);
}

/* ---- V ---------------------------------------------------------------------------------- */

static knishio_error_t v_case(const char *v0, const char *v1, const char *v2, bool self_transfer,
                              const char *token2, bool reverse) {
    knishio_wallet_t *s = wallet(g_secret, "TEST", POS_A);
    knishio_wallet_t *r = wallet(g_other_secret, "TEST", POS_B);
    knishio_wallet_t *rem = wallet(g_secret, "TEST", POS_C);
    knishio_atom_t *atoms[3];
    size_t n = 0;
    atoms[n++] = atom_of(s, KNISHIO_ISOTOPE_V, NULL, v0, NULL, NULL, NULL, NULL, 0);
    atoms[n++] = atom_of(self_transfer ? s : r, KNISHIO_ISOTOPE_V, token2, v1, "walletBundle", BUNDLE_X,
                         NULL, NULL, 0);
    if (v2) {
        atoms[n++] = atom_of(rem, KNISHIO_ISOTOPE_V, NULL, v2, "walletBundle", BUNDLE_X, NULL, NULL, 0);
    }
    knishio_error_t err = sign_and_check(atoms, n, reverse);
    knishio_wallet_free(s);
    knishio_wallet_free(r);
    knishio_wallet_free(rem);
    return err;
}

static void test_v(void) {
    printf("V isotope\n");
    expect("valid 3-atom transfer is accepted", v_case("-100", "60", "40", false, NULL, false),
           KNISHIO_SUCCESS);
    expect("valid 2-atom transfer is accepted", v_case("-100", "100", NULL, false, NULL, false),
           KNISHIO_SUCCESS);
    expect("negative non-primary V atom is rejected", v_case("-100", "-50", "150", false, NULL, false),
           KNISHIO_ERROR_TRANSFER_MALFORMED);
    expect("transfer back to the signing address is rejected",
           v_case("-100", "60", "40", true, NULL, false), KNISHIO_ERROR_TRANSFER_TO_SELF);
    expect("non-numeric V value is rejected", v_case("-100", "abc", "100", false, NULL, false),
           KNISHIO_ERROR_TRANSFER_MALFORMED);
    expect("V sum off by 0.005 is rejected", v_case("-100", "60", "39.995", false, NULL, false),
           KNISHIO_ERROR_TRANSFER_UNBALANCED);
    expect("2-atom transfer with a negative end atom is rejected",
           v_case("-100", "-100", NULL, false, NULL, false), KNISHIO_ERROR_TRANSFER_MALFORMED);
    expect("mixed V tokens are rejected", v_case("-100", "60", "40", false, "OTHER", false),
           KNISHIO_ERROR_TRANSFER_MISMATCHED);
    expect("valid transfer with its atom array reversed is accepted",
           v_case("-100", "60", "40", false, NULL, true), KNISHIO_SUCCESS);
    expect("negative non-primary V atom with the atom array reversed is rejected",
           v_case("-100", "-50", "150", false, NULL, true), KNISHIO_ERROR_TRANSFER_MALFORMED);
}

/* ---- USER-signed isotopes (M, T, R, P, A) followed by an I atom ------------------------------ */

static knishio_error_t user_case(knishio_isotope_t isotope, const char *token, const char *meta_type,
                                 const char *meta_id, const char *const *keys, const char *const *vals,
                                 size_t n) {
    knishio_wallet_t *u = wallet(g_secret, token, POS_A);
    knishio_wallet_t *next = wallet(g_secret, "USER", POS_D);
    knishio_atom_t *atoms[2];
    atoms[0] = atom_of(u, isotope, NULL, NULL, meta_type, meta_id, keys, vals, n);
    atoms[1] = atom_of(next, KNISHIO_ISOTOPE_I, NULL, NULL, "walletBundle", BUNDLE_X, NULL, NULL, 0);
    knishio_error_t err = sign_and_check(atoms, 2, false);
    knishio_wallet_free(u);
    knishio_wallet_free(next);
    return err;
}

static knishio_error_t m_case(const char *policy) {
    const char *keys[] = { "name", "readPolicy" };
    const char *vals[] = { "x", policy };
    return user_case(KNISHIO_ISOTOPE_M, "USER", "profile", "p1", keys, vals, 2);
}

static void test_m(void) {
    printf("M isotope\n");
    expect("policy listing \"all\" for a meta key is accepted", m_case("{\"name\":[\"all\"]}"),
           KNISHIO_SUCCESS);
    expect("policy listing a bundle hash is accepted", m_case("{\"name\":[\"" BUNDLE_X "\"]}"),
           KNISHIO_SUCCESS);
    expect("policy value that is neither a bundle hash nor all/self is rejected",
           m_case("{\"name\":[\"notahash\"]}"), KNISHIO_ERROR_POLICY_INVALID);
    expect("policy naming a key absent from the meta is rejected", m_case("{\"missing\":[\"all\"]}"),
           KNISHIO_ERROR_POLICY_INVALID);
    expect("policy that is not JSON is rejected", m_case("{name"), KNISHIO_ERROR_POLICY_INVALID);
}

static void test_t(void) {
    printf("T isotope\n");
    {
        const char *keys[] = { "token", "position" };
        const char *vals[] = { "TEST", POS_B };
        expect("T wallet request without bundle meta is rejected",
               user_case(KNISHIO_ISOTOPE_T, "USER", "wallet", "w", keys, vals, 2), KNISHIO_ERROR_META_MISSING);
    }
    {
        const char *keys[] = { "amount" };
        const char *vals[] = { "1" };
        expect("T atom without token meta is rejected",
               user_case(KNISHIO_ISOTOPE_T, "USER", "walletBundle", BUNDLE_X, keys, vals, 1),
               KNISHIO_ERROR_META_MISSING);
    }
    {
        const char *keys[] = { "token", "amount" };
        const char *vals[] = { "TEST", "1" };
        expect("T atom with token meta and no metaType is accepted",
               user_case(KNISHIO_ISOTOPE_T, "USER", NULL, NULL, keys, vals, 2), KNISHIO_SUCCESS);
    }
    {
        const char *keys[] = { "token", "position", "bundle" };
        const char *vals[] = { "TEST", POS_B, BUNDLE_X };
        expect("T wallet request with position, bundle and token meta is accepted",
               user_case(KNISHIO_ISOTOPE_T, "USER", "Wallet", "w", keys, vals, 3), KNISHIO_SUCCESS);
    }
}

static knishio_error_t r_case(const char *key, const char *value) {
    const char *keys[] = { key };
    const char *vals[] = { value };
    return user_case(KNISHIO_ISOTOPE_R, "USER", "profile", "p1", keys, vals, 1);
}

static void test_r(void) {
    printf("R isotope\n");
    expect("valid rule is accepted",
           r_case("rule", "[{\"condition\":[{\"key\":\"k\",\"value\":\"v\",\"comparison\":\"==\"}],"
                          "\"callback\":[{\"action\":\"reject\"}]}]"),
           KNISHIO_SUCCESS);
    expect("rule that is not an array is rejected", r_case("rule", "{\"a\":1}"), KNISHIO_ERROR_META_MISSING);
    expect("empty rule array is rejected", r_case("rule", "[]"), KNISHIO_ERROR_META_MISSING);
    expect("rule without a callback is rejected", r_case("rule", "[{\"condition\":[]}]"),
           KNISHIO_ERROR_META_MISSING);
    expect("rule condition missing value/comparison is rejected",
           r_case("rule", "[{\"condition\":[{\"key\":\"k\"}],\"callback\":[{\"action\":\"a\"}]}]"),
           KNISHIO_ERROR_RULE_INVALID);
    expect("rule callback without an action is rejected",
           r_case("rule", "[{\"condition\":[],\"callback\":[{\"meta\":{}}]}]"), KNISHIO_ERROR_RULE_INVALID);
    expect("R policy with a key other than read/write is rejected", r_case("policy", "{\"foo\":{}}"),
           KNISHIO_ERROR_META_MISSING);
    expect("R policy with read/write keys is accepted", r_case("policy", "{\"read\":{},\"write\":{}}"),
           KNISHIO_SUCCESS);
}

static void test_p_a(void) {
    printf("P and A isotopes\n");
    {
        const char *keys[] = { "peerHost" };
        const char *vals[] = { "https://peer.example" };
        expect("P atom with peerHost is accepted",
               user_case(KNISHIO_ISOTOPE_P, "USER", "walletBundle", BUNDLE_X, keys, vals, 1), KNISHIO_SUCCESS);
        expect("P atom with a non-USER token is rejected",
               user_case(KNISHIO_ISOTOPE_P, "TEST", "walletBundle", BUNDLE_X, keys, vals, 1),
               KNISHIO_ERROR_WRONG_TOKEN_TYPE);
    }
    {
        const char *keys[] = { "other" };
        const char *vals[] = { "x" };
        expect("P atom without peerHost is rejected",
               user_case(KNISHIO_ISOTOPE_P, "USER", "walletBundle", BUNDLE_X, keys, vals, 1),
               KNISHIO_ERROR_META_MISSING);
        expect("A atom without action meta is rejected",
               user_case(KNISHIO_ISOTOPE_A, "USER", "profile", "p1", keys, vals, 1), KNISHIO_ERROR_META_MISSING);
    }
    {
        const char *keys[] = { "action" };
        const char *vals[] = { "add" };
        expect("A atom with metaType, metaId and action is accepted",
               user_case(KNISHIO_ISOTOPE_A, "USER", "profile", "p1", keys, vals, 1), KNISHIO_SUCCESS);
        expect("A atom without metaId is rejected",
               user_case(KNISHIO_ISOTOPE_A, "USER", "profile", NULL, keys, vals, 1), KNISHIO_ERROR_META_MISSING);
    }
}

int main(void) {
    if (!knishio_generate_secret("check-molecule-rules", 2048, &g_secret) ||
        !knishio_generate_secret("check-molecule-rules-other", 2048, &g_other_secret)) {
        printf("FAIL: secret generation\n");
        return 1;
    }
    test_v();
    test_m();
    test_t();
    test_r();
    test_p_a();
    knishio_free(g_secret);
    knishio_free(g_other_secret);
    printf("%s (%d failure%s)\n", g_failures ? "FAIL" : "PASS", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
