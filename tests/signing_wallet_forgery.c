/**
 * @file signing_wallet_forgery.c
 * @brief knishio_molecule_check() must not honour a `signingWallet` atom meta.
 *
 * tests/fixtures/signing-wallet-forgery.json is the cross-SDK fixture, built with the published
 * @wishknish/knishio-client-js 1.2.1 and copied byte-for-byte into every SDK. `genuine` is an M
 * (then I) molecule signed normally by wallet B. `forged` is the same build with atoms[0]'s
 * walletAddress set to victim A, B's one-time signature, and a `signingWallet` meta naming B.
 * JS 1.2.1 check() accepts both, because its verifier compares the recovered signer with the meta's
 * address. The C verifier compares only with atoms[0].walletAddress, so `forged` must fail as a
 * signature mismatch. This test pins that; it does not change behaviour.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "knishio/knishio.h"
#include "knishio/atom.h"
#include "knishio/molecule.h"
#include "knishio/meta.h"
#include "knishio/json/serializers.h"

#ifndef KNISHIO_SIGNING_WALLET_FIXTURE_PATH
#define KNISHIO_SIGNING_WALLET_FIXTURE_PATH "tests/fixtures/signing-wallet-forgery.json"
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

/* Parses fixture[`key`] with knishio_molecule_from_json(); *out is NULL when parsing failed. */
static knishio_error_t load(const cJSON *fixture, const char *key, knishio_molecule_t **out) {
    *out = NULL;
    char *text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(fixture, key));
    if (!text) return KNISHIO_ERROR_MEMORY;
    const knishio_error_t err = knishio_molecule_from_json(text, out);
    cJSON_free(text);
    return err;
}

static bool has_meta_key(const knishio_atom_t *atom, const char *key) {
    for (size_t i = 0; atom && i < atom->meta_count; i++) {
        if (atom->meta[i] && atom->meta[i]->key && strcmp(atom->meta[i]->key, key) == 0) return true;
    }
    return false;
}

int main(void) {
    const char *path = KNISHIO_SIGNING_WALLET_FIXTURE_PATH;
    char *text = slurp(path);
    if (!text) {
        fprintf(stderr, "signing_wallet_forgery: cannot read the fixture at %s\n", path);
        return 2;
    }
    cJSON *fixture = cJSON_Parse(text);
    free(text);
    const char *victim = fixture ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(fixture, "victimAddress")) : NULL;
    const char *attacker = fixture ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(fixture, "attackerAddress")) : NULL;
    if (!victim || !attacker || !cJSON_GetObjectItemCaseSensitive(fixture, "genuine")
        || !cJSON_GetObjectItemCaseSensitive(fixture, "forged")) {
        /* The fixture is the whole point of this binary: skipping would publish a pass having
         * asserted nothing. */
        fprintf(stderr, "signing_wallet_forgery: %s lacks victimAddress, attackerAddress, genuine or forged\n", path);
        if (fixture) cJSON_Delete(fixture);
        return 2;
    }

    printf("signingWallet forgery fixture (SDK %s)\n", KNISHIO_VERSION_STRING);
    char detail[256];

    knishio_molecule_t *genuine = NULL;
    knishio_error_t parsed = load(fixture, "genuine", &genuine);
    knishio_error_t verdict = genuine ? knishio_molecule_check(genuine, NULL) : parsed;
    snprintf(detail, sizeof(detail), "from_json=%d check=%d (%s)", (int)parsed, (int)verdict,
             knishio_error_to_string(verdict));
    check(parsed == KNISHIO_SUCCESS && verdict == KNISHIO_SUCCESS,
          "genuine: B's normally signed molecule loads and knishio_molecule_check accepts it", detail);

    knishio_molecule_t *forged = NULL;
    parsed = load(fixture, "forged", &forged);
    const knishio_atom_t *a0 = (forged && forged->atom_count > 0) ? forged->atoms[0] : NULL;

    const bool claims_victim = a0 && a0->wallet_address && strcmp(a0->wallet_address, victim) == 0;
    snprintf(detail, sizeof(detail), "atoms[0].walletAddress=%s", (a0 && a0->wallet_address) ? a0->wallet_address : "(null)");
    check(parsed == KNISHIO_SUCCESS && claims_victim,
          "forged: loads, and atoms[0].walletAddress is the fixture's victimAddress", detail);

    /* The attack vector reached the verifier: had the parser dropped the meta, the rejection
     * below would say nothing about honouring it. */
    check(has_meta_key(a0, "signingWallet"), "forged: atoms[0] carries the signingWallet meta", NULL);

    verdict = forged ? knishio_molecule_check(forged, NULL) : parsed;
    snprintf(detail, sizeof(detail), "check=%d (%s)", (int)verdict, knishio_error_to_string(verdict));
    check(verdict == KNISHIO_ERROR_SIGNATURE_MISMATCH,
          "forged: B's signature under victim A's address is rejected as KNISHIO_ERROR_SIGNATURE_MISMATCH", detail);

    knishio_molecule_free_deep(forged);
    knishio_molecule_free_deep(genuine);
    cJSON_Delete(fixture);
    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
