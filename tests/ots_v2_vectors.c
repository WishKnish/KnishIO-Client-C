/**
 * @file ots_v2_vectors.c
 * @brief One-time signature (OTS) v2: 8192-hex keys (64 chains) next to v1 (2048 hex, 16 chains).
 *
 * Three modes, registered as three ctest entries:
 *   roundtrip  (OtsV2RoundTrip) — always runs: a v2 wallet signs a molecule, the signature
 *              verifies, and a one-character change to it does not.
 *   client     (OtsV2Client)    — always runs: the client OTS version option, version detection
 *              for the client's own ledger wallets (against a loopback GraphQL stub), and the
 *              AuthToken snapshot field.
 *   vectors    (OtsV2Vectors)   — the shared OTS v2 vectors (key derivation, address, normalized
 *              hash, ML-KEM identities, molecule verdicts, re-signing, malformed declared hash).
 *              The file is read from $KNISHIO_OTS_V2_VECTORS; when it is absent (a standalone
 *              checkout of this SDK) the binary prints SKIP and exits 77.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cjson/cJSON.h>

#include "knishio/knishio.h"
#include "knishio/atom.h"
#include "knishio/auth_token.h"
#include "knishio/client.h"
#include "knishio/client_ops.h"
#include "knishio/error/context.h"
#include "knishio/meta.h"
#include "knishio/molecule.h"
#include "knishio/operations/auth.h"
#include "knishio/operations/transfer.h"
#include "knishio/operations/wallet.h"
#include "knishio/wallet.h"
#include "knishio/utils/memory.h"
#include "operations/operations_internal.h"

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
/* Client-level OTS version (loopback GraphQL stub, no network)        */
/* ------------------------------------------------------------------ */

#define STUB_MAX_REQUESTS 8

typedef struct {
    int listen_fd;
    unsigned short port;
    pthread_t thread;
    pthread_mutex_t lock;
    bool stop;
    const char *continuid_reply;      /* reply to QueryContinuId */
    const char *balance_reply;        /* reply to QueryBalance (unscripted when NULL) */
    char *proposals[STUB_MAX_REQUESTS]; /* recorded ProposeMolecule bodies */
    size_t proposal_count;
} stub_t;

static const char *const STUB_ACCEPTED =
    "{\"data\":{\"ProposeMolecule\":{\"molecularHash\":\"h\",\"status\":\"accepted\","
    "\"reason\":null,\"payload\":\"{\\\"token\\\":\\\"jwt-accepted\\\"}\",\"createdAt\":\"0\"}}}";
static const char *const STUB_GENESIS = "{\"data\":{\"ContinuId\":null}}";

static bool stub_send_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n <= 0) return false;
        buf += n;
        len -= (size_t)n;
    }
    return true;
}

/* Reads one HTTP request (headers + Content-Length body); returns the body or NULL. */
static char *stub_read_request(int fd) {
    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    char *header_end = NULL;
    while (!header_end) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *grown = realloc(buf, cap);
            if (!grown) { free(buf); return NULL; }
            buf = grown;
        }
        ssize_t n = recv(fd, buf + len, cap - len - 1, 0);
        if (n <= 0) { free(buf); return NULL; }
        len += (size_t)n;
        buf[len] = '\0';
        header_end = strstr(buf, "\r\n\r\n");
    }
    size_t header_len = (size_t)(header_end - buf) + 4;
    size_t content_length = 0;
    bool expect_continue = false;
    for (char *line = buf; line < header_end;) {
        char *eol = strstr(line, "\r\n");
        if (!eol) break;
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            content_length = (size_t)strtoul(line + 15, NULL, 10);
        } else if (strncasecmp(line, "Expect:", 7) == 0 && strstr(line, "100-continue")) {
            expect_continue = true;
        }
        line = eol + 2;
    }
    if (expect_continue && len == header_len) {
        const char *cont = "HTTP/1.1 100 Continue\r\n\r\n";
        if (!stub_send_all(fd, cont, strlen(cont))) { free(buf); return NULL; }
    }
    while (len < header_len + content_length) {
        if (len + 1 >= cap) {
            cap = header_len + content_length + 1;
            char *grown = realloc(buf, cap);
            if (!grown) { free(buf); return NULL; }
            buf = grown;
        }
        ssize_t n = recv(fd, buf + len, cap - len - 1, 0);
        if (n <= 0) { free(buf); return NULL; }
        len += (size_t)n;
        buf[len] = '\0';
    }
    char *body = malloc(content_length + 1);
    if (body) {
        memcpy(body, buf + header_len, content_length);
        body[content_length] = '\0';
    }
    free(buf);
    return body;
}

static void *stub_serve(void *arg) {
    stub_t *s = arg;
    for (;;) {
        int fd = accept(s->listen_fd, NULL, NULL);
        pthread_mutex_lock(&s->lock);
        bool stop = s->stop;
        pthread_mutex_unlock(&s->lock);
        if (fd < 0 || stop) {
            if (fd >= 0) close(fd);
            break;
        }
        char *body = stub_read_request(fd);
        const char *reply = "{\"errors\":[{\"message\":\"unscripted request\"}]}";
        pthread_mutex_lock(&s->lock);
        if (body && strstr(body, "QueryContinuId")) {
            reply = s->continuid_reply;
        } else if (body && strstr(body, "QueryBalance") && s->balance_reply) {
            reply = s->balance_reply;
        } else if (body && strstr(body, "QueryWalletList")) {
            reply = "{\"data\":{\"Wallet\":[]}}";
        } else if (body && strstr(body, "ProposeMolecule")) {
            reply = STUB_ACCEPTED;
            if (s->proposal_count < STUB_MAX_REQUESTS) {
                s->proposals[s->proposal_count++] = body;
                body = NULL;
            }
        }
        pthread_mutex_unlock(&s->lock);
        free(body);
        char head[160];
        int head_len = snprintf(head, sizeof(head),
                                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                                "Content-Length: %zu\r\nConnection: close\r\n\r\n", strlen(reply));
        if (stub_send_all(fd, head, (size_t)head_len)) {
            stub_send_all(fd, reply, strlen(reply));
        }
        close(fd);
    }
    return NULL;
}

static bool stub_start_with_balance(stub_t *s, const char *continuid_reply, const char *balance_reply) {
    memset(s, 0, sizeof(*s));
    s->continuid_reply = continuid_reply;
    s->balance_reply = balance_reply;
    pthread_mutex_init(&s->lock, NULL);
    s->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->listen_fd < 0) return false;
    int one = 1;
    setsockopt(s->listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addr_len = sizeof(addr);
    if (bind(s->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0
        || listen(s->listen_fd, 8) != 0
        || getsockname(s->listen_fd, (struct sockaddr *)&addr, &addr_len) != 0) {
        close(s->listen_fd);
        return false;
    }
    s->port = ntohs(addr.sin_port);
    return pthread_create(&s->thread, NULL, stub_serve, s) == 0;
}

static bool stub_start(stub_t *s, const char *continuid_reply) {
    return stub_start_with_balance(s, continuid_reply, NULL);
}

static void stub_stop(stub_t *s) {
    pthread_mutex_lock(&s->lock);
    s->stop = true;
    pthread_mutex_unlock(&s->lock);
    int fd = socket(AF_INET, SOCK_STREAM, 0);  /* wake the blocking accept() */
    if (fd >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(s->port);
        connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        close(fd);
    }
    pthread_join(s->thread, NULL);
    close(s->listen_fd);
    for (size_t i = 0; i < s->proposal_count; i++) free(s->proposals[i]);
    pthread_mutex_destroy(&s->lock);
}

/* {"data":{"ContinuId":{...}}} for a USER wallet at position with address. */
static char *continuid_reply(const char *position, const char *address) {
    cJSON *root = cJSON_CreateObject();
    cJSON *cid = cJSON_AddObjectToObject(cJSON_AddObjectToObject(root, "data"), "ContinuId");
    cJSON_AddStringToObject(cid, "position", position);
    cJSON_AddStringToObject(cid, "address", address);
    cJSON_AddStringToObject(cid, "tokenSlug", "USER");
    cJSON_AddNullToObject(cid, "pubkey");
    cJSON_AddNullToObject(cid, "characters");
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return text;
}

/* {"data":{"Balance":{...}}} for one of the bundle's token wallets. */
static char *balance_reply(const char *bundle, const char *token, const char *position, const char *address,
                           const char *amount) {
    cJSON *root = cJSON_CreateObject();
    cJSON *bal = cJSON_AddObjectToObject(cJSON_AddObjectToObject(root, "data"), "Balance");
    cJSON_AddStringToObject(bal, "address", address);
    cJSON_AddStringToObject(bal, "bundleHash", bundle);
    cJSON_AddStringToObject(bal, "tokenSlug", token);
    cJSON_AddStringToObject(bal, "position", position);
    cJSON_AddStringToObject(bal, "amount", amount);
    cJSON_AddNullToObject(bal, "batchId");
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return text;
}

/* The i-th recorded proposal, parsed. */
static knishio_molecule_t *proposed_molecule(const stub_t *s, size_t i) {
    if (i >= s->proposal_count) return NULL;
    cJSON *root = cJSON_Parse(s->proposals[i]);
    cJSON *mol = root ? cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "variables"),
                                                         "molecule") : NULL;
    char *text = mol ? cJSON_PrintUnformatted(mol) : NULL;
    knishio_molecule_t *m = NULL;
    if (text && knishio_molecule_from_json(text, &m) != KNISHIO_SUCCESS) m = NULL;
    cJSON_free(text);
    if (root) cJSON_Delete(root);
    return m;
}

/* The first atom of `isotope` in `m`, or NULL. */
static const knishio_atom_t *isotope_atom(const knishio_molecule_t *m, knishio_isotope_t isotope) {
    for (size_t i = 0; m && i < m->atom_count; i++) {
        if (m->atoms[i]->isotope == isotope) return m->atoms[i];
    }
    return NULL;
}

/* The last message the SDK sent to the global error handler. */
static char g_last_error[512];

static void capture_error(const knishio_error_context_t *ctx, void *user_data) {
    (void)user_data;
    const char *message = knishio_error_context_get_message(ctx);
    snprintf(g_last_error, sizeof(g_last_error), "%s", message ? message : "");
}

/* A restored token and the wallet restore built for it (the token does not own its wallet). */
static void free_token(knishio_auth_token_t *token) {
    if (!token) return;
    knishio_wallet_t *wallet = knishio_auth_token_get_wallet(token);
    knishio_auth_token_cleanup(token);
    if (wallet) knishio_wallet_free(wallet);
}

static knishio_client_t *client_at(const stub_t *s, int ots_version, const char *secret) {
    char uri[64];
    snprintf(uri, sizeof(uri), "http://127.0.0.1:%u/graphql", s ? (unsigned)s->port : 9U);
    knishio_client_config_t config = {0};
    config.uri = uri;
    config.cell_slug = "ots-v2";
    config.ots_version = ots_version;
    knishio_client_t *client = NULL;
    if (knishio_client_create(&client, &config) != KNISHIO_SUCCESS) return NULL;
    if (secret && knishio_client_set_secret(client, secret) != KNISHIO_SUCCESS) {
        knishio_client_destroy(client);
        return NULL;
    }
    return client;
}

/* True when `address` is the address of the key `secret` derives at (token, position, version). */
static bool address_at(const char *secret, const char *token, const char *position, int version,
                       const char *address) {
    knishio_wallet_t *w = NULL;
    bool ok = address && knishio_wallet_create_with_ots_version(&w, secret, token, position, version)
        && same(w->address, address);
    if (w) knishio_wallet_free(w);
    return ok;
}

static int run_client(void) {
    printf("Client-level OTS version (SDK %s)\n", KNISHIO_VERSION_STRING);
    knishio_set_error_handler(capture_error, NULL);
    char secret[2049], position[65], unrelated[65], expected[256], detail[256];
    memset(secret, 'a', 2048); secret[2048] = '\0';
    memset(position, 'b', 64); position[64] = '\0';
    memset(unrelated, 'c', 64); unrelated[64] = '\0';
    knishio_wallet_t *w1 = NULL, *w2 = NULL;
    if (!knishio_wallet_create_with_ots_version(&w1, secret, "USER", position, KNISHIO_OTS_V1)
        || !knishio_wallet_create_with_ots_version(&w2, secret, "USER", position, KNISHIO_OTS_V2)) {
        check(false, "v1 and v2 USER wallets from one hex secret", NULL);
        return 1;
    }

    /* (a) detection, both preferred orders */
    for (int preferred = 1; preferred <= 2; preferred++) {
        snprintf(detail, sizeof(detail), "preferred %d", preferred);
        check(knishio_detect_ots_version(secret, "USER", position, w1->address, preferred) == 1,
              "detect: the v1 address is version 1", detail);
        check(knishio_detect_ots_version(secret, "USER", position, w2->address, preferred) == 2,
              "detect: the v2 address is version 2", detail);
        check(knishio_detect_ots_version(secret, "USER", position, unrelated, preferred) == 0,
              "detect: an unrelated address is neither version (0)", detail);
    }

    /* (b)/(c) knishio_wallet_set_ots_version */
    knishio_wallet_t keyless = {0};
    keyless.token = "USER";
    keyless.position = position;
    keyless.address = w2->address;
    check(knishio_wallet_set_ots_version(&keyless, KNISHIO_OTS_V2) && keyless.ots_version == KNISHIO_OTS_V2,
          "set_ots_version sets a keyless wallet's version", NULL);
    g_last_error[0] = '\0';
    check(!knishio_wallet_set_ots_version(w1, KNISHIO_OTS_V2) && w1->ots_version == KNISHIO_OTS_V1,
          "set_ots_version refuses to change a keyed wallet", NULL);
    check(same(g_last_error, "KnishIO: cannot change the OTS version of a keyed wallet."),
          "  with the keyed-wallet message", g_last_error);
    check(knishio_wallet_set_ots_version(w1, KNISHIO_OTS_V1), "set_ots_version keeps a keyed wallet's own version",
          NULL);

    /* Client option */
    knishio_client_t *client = client_at(NULL, 0, NULL);
    check(client && knishio_client_get_ots_version(client) == 1, "a zero-initialised config is OTS version 1", NULL);
    g_last_error[0] = '\0';
    check(client && knishio_client_set_ots_version(client, 3) == KNISHIO_ERROR_INVALID_ARGS
              && knishio_client_get_ots_version(client) == 1
              && same(g_last_error, "KnishIO: unsupported OTS version 3; expected 1 or 2."),
          "set_ots_version(3) is refused with the unsupported-version message", g_last_error);
    check(client && knishio_client_set_ots_version(client, 2) == KNISHIO_SUCCESS
              && knishio_client_get_ots_version(client) == 2, "set_ots_version(2) is kept", NULL);
    if (client) knishio_client_destroy(client);
    client = client_at(NULL, 3, NULL);
    check(client == NULL, "a config with ots_version 3 is refused", NULL);
    if (client) knishio_client_destroy(client);

    /* (e)/(f) binding an own ledger wallet */
    client = client_at(NULL, 1, secret);
    int bound = 0;
    knishio_error_t err = client
        ? knishio_client_bind_own_wallet_ots_version(client, secret, "USER", position, w2->address, &bound)
        : KNISHIO_ERROR_INVALID_STATE;
    check(err == KNISHIO_SUCCESS && bound == 2, "a v1 client binds a v2-derived ledger wallet at version 2", NULL);
    err = client ? knishio_client_bind_own_wallet_ots_version(client, secret, "USER", position, NULL, &bound)
                 : KNISHIO_ERROR_INVALID_STATE;
    check(err == KNISHIO_SUCCESS && bound == 1, "a ledger wallet without an address binds at the client version",
          NULL);
    g_last_error[0] = '\0';
    err = client ? knishio_client_bind_own_wallet_ots_version(client, secret, "USER", position, unrelated, &bound)
                 : KNISHIO_ERROR_INVALID_STATE;
    snprintf(expected, sizeof(expected),
             "KnishIO: wallet %s is not derived from this secret at position %s under OTS version 1 or 2.",
             unrelated, position);
    check(err == KNISHIO_ERROR_WALLET_MISMATCH && same(g_last_error, expected),
          "a wallet neither version derives is refused (KNISHIO_ERROR_WALLET_MISMATCH)", g_last_error);
    if (client) knishio_client_destroy(client);

    /* (d) keys created from the secret by a v2 client */
    client = client_at(NULL, 2, secret);
    knishio_wallet_t *source = NULL;
    err = client ? knishio_client_get_source_wallet(client, "USER", &source) : KNISHIO_ERROR_INVALID_STATE;
    check(err == KNISHIO_SUCCESS && source && source->ots_version == 2 && strlen(source->private_key) == 8192,
          "get_source_wallet at a v2 client holds a v2 key", NULL);
    if (source) knishio_wallet_free(source);
    source = NULL;
    if (client) knishio_client_destroy(client);

    stub_t s;
    if (stub_start(&s, STUB_GENESIS)) {
        client = client_at(&s, 2, secret);
        err = client ? knishio_client_get_source_wallet_continuid(client, "USER", &source) : KNISHIO_ERROR_INVALID_STATE;
        check(err == KNISHIO_SUCCESS && source && source->ots_version == 2 && strlen(source->private_key) == 8192,
              "without a ContinuID, the source wallet is a v2 key at a v2 client", NULL);
        if (source) knishio_wallet_free(source);
        source = NULL;

        knishio_request_profile_auth_token_params_t auth = { .secret = secret, .encrypt = false };
        knishio_request_profile_auth_token_result_t *result = NULL;
        err = client ? knishio_client_request_profile_auth_token(client, &auth, &result) : KNISHIO_ERROR_INVALID_STATE;
        knishio_molecule_t *m = proposed_molecule(&s, s.proposal_count ? s.proposal_count - 1 : 0);
        const knishio_atom_t *a0 = (m && m->atom_count > 0) ? m->atoms[0] : NULL;
        check(err == KNISHIO_SUCCESS && a0 && same(a0->token, "AUTH")
                  && address_at(secret, "AUTH", a0->position, KNISHIO_OTS_V2, a0->wallet_address),
              "without a ContinuID, a v2 client signs in from a v2 AUTH wallet", NULL);
        knishio_molecule_free_deep(m);
        knishio_request_profile_auth_token_result_free(result);
        knishio_create_wallet_params_t cw = { .token = "OTSV2" };
        knishio_create_wallet_result_t *created = NULL;
        err = client ? knishio_client_create_wallet(client, &cw, &created) : KNISHIO_ERROR_INVALID_STATE;
        snprintf(detail, sizeof(detail), "err=%d (%s)", (int)err, knishio_error_to_string(err));
        check(err == KNISHIO_SUCCESS && created && created->wallet && created->wallet->ots_version == 2
                  && strlen(created->wallet->private_key) == 8192,
              "create_wallet at a v2 client creates a v2 wallet", detail);
        knishio_create_wallet_result_free(created);

        if (client) knishio_client_destroy(client);
        stub_stop(&s);
    } else {
        check(false, "stub server starts", NULL);
    }

    /* ContinuID head derived at v2, read by a v1 client */
    char *head_v2 = continuid_reply(position, w2->address);
    if (stub_start(&s, head_v2)) {
        client = client_at(&s, 1, secret);
        err = client ? knishio_client_get_source_wallet_continuid(client, "USER", &source) : KNISHIO_ERROR_INVALID_STATE;
        check(err == KNISHIO_SUCCESS && source && source->ots_version == 2 && same(source->private_key, w2->private_key),
              "a v1 client derives a v2 ContinuID head at version 2", NULL);
        if (source) knishio_wallet_free(source);
        source = NULL;

        knishio_request_profile_auth_token_params_t auth = { .secret = secret, .encrypt = false };
        knishio_request_profile_auth_token_result_t *result = NULL;
        err = client ? knishio_client_request_profile_auth_token(client, &auth, &result) : KNISHIO_ERROR_INVALID_STATE;
        knishio_molecule_t *m = proposed_molecule(&s, 0);
        const knishio_atom_t *a0 = (m && m->atom_count > 0) ? m->atoms[0] : NULL;
        check(err == KNISHIO_SUCCESS && a0 && same(a0->token, "USER") && same(a0->position, position)
                  && same(a0->wallet_address, w2->address),
              "a v1 client signs in from the v2 login pointer", NULL);
        const knishio_atom_t *head = isotope_atom(m, KNISHIO_ISOTOPE_I);
        check(err == KNISHIO_SUCCESS && head && same(head->token, "USER") && !same(head->position, position)
                  && address_at(secret, "USER", head->position, KNISHIO_OTS_V2, head->wallet_address),
              "(m3) a v1 client keeps a v2 identity's new ContinuID head at v2", NULL);
        knishio_molecule_free_deep(m);
        knishio_request_profile_auth_token_result_free(result);
        if (client) knishio_client_destroy(client);
        stub_stop(&s);
    } else {
        check(false, "stub server starts", NULL);
    }
    cJSON_free(head_v2);

    /* A v1 ContinuID head: a v2 client signs from the v1 key and creates every wallet the
     * molecule needs for its signer (ContinuID head, token remainder) at v2; a v1 client stays
     * at v1. */
    char token_position[65];
    memset(token_position, 'd', 64); token_position[64] = '\0';
    knishio_wallet_t *t1 = NULL;
    char *head_v1 = continuid_reply(position, w1->address);
    char *balance_v1 = knishio_wallet_create_with_ots_version(&t1, secret, "OTSV2", token_position, KNISHIO_OTS_V1)
        ? balance_reply(t1->bundle_hash, "OTSV2", token_position, t1->address, "10") : NULL;
    if (balance_v1 && stub_start_with_balance(&s, head_v1, balance_v1)) {
        client = client_at(&s, 2, secret);
        knishio_request_profile_auth_token_params_t auth = { .secret = secret, .encrypt = false };
        knishio_request_profile_auth_token_result_t *result = NULL;
        err = client ? knishio_client_request_profile_auth_token(client, &auth, &result) : KNISHIO_ERROR_INVALID_STATE;
        knishio_molecule_t *m = proposed_molecule(&s, 0);
        const knishio_atom_t *a0 = (m && m->atom_count > 0) ? m->atoms[0] : NULL;
        const knishio_atom_t *head = isotope_atom(m, KNISHIO_ISOTOPE_I);
        check(err == KNISHIO_SUCCESS && a0 && same(a0->token, "USER") && same(a0->position, position)
                  && same(a0->wallet_address, w1->address)
                  && head && same(head->token, "USER") && !same(head->position, position)
                  && address_at(secret, "USER", head->position, KNISHIO_OTS_V2, head->wallet_address),
              "(m1) a v2 client signs in from a v1 ContinuID head and registers a v2 head", NULL);
        knishio_molecule_free_deep(m);
        knishio_request_profile_auth_token_result_free(result);

        char meta_key[] = "k", meta_value[] = "v";
        knishio_meta_t pair = { .key = meta_key, .value = meta_value };
        knishio_meta_t *pairs[] = { &pair };
        char *hash = NULL;
        err = client ? knishio_client_create_meta(client, "ots-v2", "successor", pairs, 1, &hash)
                     : KNISHIO_ERROR_INVALID_STATE;
        snprintf(detail, sizeof(detail), "err=%d (%s)", (int)err, knishio_error_to_string(err));
        m = proposed_molecule(&s, 1);
        a0 = (m && m->atom_count > 0) ? m->atoms[0] : NULL;
        head = isotope_atom(m, KNISHIO_ISOTOPE_I);
        check(err == KNISHIO_SUCCESS && a0 && same(a0->position, position) && same(a0->wallet_address, w1->address)
                  && head && !same(head->position, position)
                  && address_at(secret, "USER", head->position, KNISHIO_OTS_V2, head->wallet_address),
              "(m2) create_meta at a v2 client signs from the v1 head and registers a v2 head", detail);
        knishio_molecule_free_deep(m);
        knishio_free(hash);

        knishio_transfer_params_t transfer = { .recipient = unrelated, .token = "OTSV2", .amount = 4 };
        knishio_transfer_result_t *sent = NULL;
        err = client ? knishio_client_transfer_tokens(client, &transfer, &sent) : KNISHIO_ERROR_INVALID_STATE;
        snprintf(detail, sizeof(detail), "err=%d (%s)", (int)err, knishio_error_to_string(err));
        m = proposed_molecule(&s, 2);
        a0 = (m && m->atom_count > 0) ? m->atoms[0] : NULL;
        const knishio_atom_t *change = NULL;
        for (size_t i = 0; m && i < m->atom_count; i++) {
            const knishio_atom_t *atom = m->atoms[i];
            if (atom->isotope == KNISHIO_ISOTOPE_V && atom->position && atom->position[0]
                && !same(atom->position, token_position)) {
                change = atom;
            }
        }
        check(err == KNISHIO_SUCCESS && a0 && same(a0->position, token_position)
                  && same(a0->wallet_address, t1->address)
                  && change && address_at(secret, "OTSV2", change->position, KNISHIO_OTS_V2, change->wallet_address),
              "(m6) transfer at a v2 client spends a v1 balance wallet into a v2 remainder", detail);
        knishio_molecule_free_deep(m);
        knishio_transfer_result_free(sent);
        if (client) knishio_client_destroy(client);

        client = client_at(&s, 1, secret);
        result = NULL;
        err = client ? knishio_client_request_profile_auth_token(client, &auth, &result) : KNISHIO_ERROR_INVALID_STATE;
        m = proposed_molecule(&s, 3);
        head = isotope_atom(m, KNISHIO_ISOTOPE_I);
        check(err == KNISHIO_SUCCESS && head && !same(head->position, position)
                  && address_at(secret, "USER", head->position, KNISHIO_OTS_V1, head->wallet_address),
              "a v1 client keeps a v1 identity's new ContinuID head at v1", NULL);
        knishio_molecule_free_deep(m);
        knishio_request_profile_auth_token_result_free(result);
        if (client) knishio_client_destroy(client);
        stub_stop(&s);
    } else {
        check(false, "stub server starts", NULL);
    }
    cJSON_free(head_v1);
    cJSON_free(balance_v1);
    if (t1) knishio_wallet_free(t1);

    /* ContinuID head this secret does not derive */
    char *head_unrelated = continuid_reply(position, unrelated);
    if (stub_start(&s, head_unrelated)) {
        client = client_at(&s, 1, secret);
        g_last_error[0] = '\0';
        err = client ? knishio_client_get_source_wallet_continuid(client, "USER", &source) : KNISHIO_ERROR_INVALID_STATE;
        check(err == KNISHIO_ERROR_WALLET_MISMATCH && source == NULL && same(g_last_error, expected),
              "a ContinuID head neither version derives is refused", g_last_error);
        if (source) knishio_wallet_free(source);
        source = NULL;

        knishio_request_profile_auth_token_params_t auth = { .secret = secret, .encrypt = false };
        knishio_request_profile_auth_token_result_t *result = NULL;
        err = client ? knishio_client_request_profile_auth_token(client, &auth, &result) : KNISHIO_ERROR_INVALID_STATE;
        knishio_molecule_t *m = proposed_molecule(&s, 0);
        const knishio_atom_t *a0 = (m && m->atom_count > 0) ? m->atoms[0] : NULL;
        check(err == KNISHIO_SUCCESS && s.proposal_count == 1 && a0 && same(a0->token, "AUTH"),
              "the login falls back to an AUTH wallet for an underivable pointer", NULL);
        knishio_molecule_free_deep(m);
        knishio_request_profile_auth_token_result_free(result);

        knishio_wallet_list_result_t *list = NULL;
        err = client ? knishio_client_query_wallets(client, NULL, &list) : KNISHIO_ERROR_INVALID_STATE;
        snprintf(detail, sizeof(detail), "err=%d (%s)", (int)err, knishio_error_to_string(err));
        check(err == KNISHIO_SUCCESS && list && list->success,
              "query_wallets (a bundle-only read) succeeds with an underivable ContinuID head", detail);
        knishio_wallet_list_result_free(list);
        if (client) knishio_client_destroy(client);
        stub_stop(&s);
    } else {
        check(false, "stub server starts", NULL);
    }
    cJSON_free(head_unrelated);

    /* (g)/(h) AuthToken snapshot */
    knishio_wallet_t *session = NULL;
    knishio_auth_token_t *token = NULL;
    char *snapshot = NULL;
    knishio_auth_token_config_t token_config = { .token = "t", .expires_at = 0, .encrypt = false, .pubkey = NULL };
    bool made = knishio_wallet_create_with_ots_version(&session, secret, "USER", position, KNISHIO_OTS_V2)
        && knishio_auth_token_create_with_wallet(&token, &token_config, session) == KNISHIO_SUCCESS
        && knishio_auth_token_get_snapshot(token, &snapshot) == KNISHIO_SUCCESS;
    cJSON *snap = made ? cJSON_Parse(snapshot) : NULL;
    cJSON *snap_wallet = snap ? cJSON_GetObjectItemCaseSensitive(snap, "wallet") : NULL;
    cJSON *snap_version = snap_wallet ? cJSON_GetObjectItemCaseSensitive(snap_wallet, "otsVersion") : NULL;
    check(cJSON_IsNumber(snap_version) && cJSON_GetNumberValue(snap_version) == 2.0,
          "an AuthToken snapshot carries wallet.otsVersion 2", NULL);

    knishio_auth_token_t *restored = NULL;
    err = made ? knishio_auth_token_restore(&restored, snapshot, secret) : KNISHIO_ERROR_INVALID_STATE;
    const knishio_wallet_t *rw = restored ? knishio_auth_token_get_wallet(restored) : NULL;
    check(err == KNISHIO_SUCCESS && rw && rw->ots_version == 2 && same(rw->address, session->address),
          "restore keeps the session wallet's version and address", NULL);
    free_token(restored);
    restored = NULL;

    if (snap_wallet) cJSON_DeleteItemFromObjectCaseSensitive(snap_wallet, "otsVersion");
    char *legacy = snap ? cJSON_PrintUnformatted(snap) : NULL;
    err = legacy ? knishio_auth_token_restore(&restored, legacy, secret) : KNISHIO_ERROR_INVALID_STATE;
    rw = restored ? knishio_auth_token_get_wallet(restored) : NULL;
    check(err == KNISHIO_SUCCESS && rw && rw->ots_version == 1, "a snapshot without otsVersion restores at 1", NULL);
    free_token(restored);
    restored = NULL;
    cJSON_free(legacy);

    if (snap_wallet) cJSON_AddNumberToObject(snap_wallet, "otsVersion", 3);
    char *unsupported = snap ? cJSON_PrintUnformatted(snap) : NULL;
    g_last_error[0] = '\0';
    err = unsupported ? knishio_auth_token_restore(&restored, unsupported, secret) : KNISHIO_ERROR_INVALID_STATE;
    check(err == KNISHIO_ERROR_INVALID_ARGS && restored == NULL
              && same(g_last_error, "KnishIO: unsupported OTS version 3; expected 1 or 2."),
          "a snapshot with otsVersion 3 is refused with the unsupported-version message", g_last_error);
    free_token(restored);
    cJSON_free(unsupported);

    if (snap) cJSON_Delete(snap);
    free(snapshot);
    if (token) knishio_auth_token_cleanup(token);  /* the token does not own `session` */
    if (session) knishio_wallet_free(session);
    knishio_wallet_free(w1);
    knishio_wallet_free(w2);
    knishio_set_error_handler(NULL, NULL);
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
    if (argc == 2 && strcmp(argv[1], "client") == 0) return run_client();
    if (argc == 2 && strcmp(argv[1], "vectors") == 0) return run_vectors();
    fprintf(stderr, "usage: %s roundtrip|client|vectors\n", argv[0]);
    return 2;
}
