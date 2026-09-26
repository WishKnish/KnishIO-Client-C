/**
 * @file profile_auth_continuid.c
 * @brief A returning user's profile login is signed from the identity's ContinuID pointer.
 *
 * Validator 0.5.0 marks an auth token proven only when atoms[0] of the U molecule sits at the
 * bundle's ContinuID position and belongs to the USER wallet registered there. A login signed from
 * a fresh AUTH wallet is accepted but unproven, so the user is a guest for permissioned and private
 * cells. knishio_client_request_profile_auth_token() must therefore:
 *   - query ContinuId(bundle, token USER) first, on a client that holds no auth token yet;
 *   - sign from the USER wallet at the returned position when the validator's address matches;
 *   - keep the fresh AUTH wallet for a first login (no pointer), a non-USER pointer or a mismatch;
 *   - fall back to that AUTH login ONCE when the pointer-signed proposal is rejected, so one login
 *     sends at most two authorization molecules (testnet allows 3 auths/min/IP).
 *
 * The SDK has no transport seam, so this binary serves the GraphQL endpoint itself: an HTTP/1.1
 * server on an ephemeral 127.0.0.1 port, in a thread, answering from a per-case script and
 * recording every request. No external service is contacted.
 *
 * The same binary also pins two AUTH-token assumptions the pointer login makes wrong:
 * knishio_molecule_check() must accept a U atom with token USER (and still reject any other
 * token), and an auth-token snapshot must restore its wallet with the token it was bound to.
 *
 * Finally it drives a mutation (createToken) on an encrypted session: the stub opens the
 * CipherHash request with its ML-KEM key and answers with a reply encrypted to the pointer USER
 * wallet, so the client must read the result from the decrypted reply, not from the envelope.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
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
#include "knishio/client_auth.h"
#include "knishio/client.h"
#include "knishio/client_ops.h"
#include "knishio/meta.h"
#include "knishio/molecule.h"
#include "knishio/operations/auth.h"
#include "knishio/operations/token.h"
#include "knishio/crypto/aes_gcm.h"
#include "knishio/crypto/cipher_hash.h"
#include "knishio/crypto/mlkem.h"
#include "knishio/utils/encoding.h"
#include "knishio/wallet.h"

static int g_failures = 0;

static void check(bool ok, const char *name, const char *detail) {
    printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail ? " — " : "", detail ? detail : "");
    if (!ok) g_failures++;
}

/* ------------------------------------------------------------------------------------------ */
/* Scripted loopback GraphQL endpoint                                                          */
/* ------------------------------------------------------------------------------------------ */

#define MAX_REQUESTS 8

typedef struct {
    char *body;
    bool has_auth_header;
} recorded_request_t;

typedef struct {
    int listen_fd;
    unsigned short port;
    pthread_t thread;
    pthread_mutex_t lock;
    bool stop;

    /* Script: the ContinuId reply, then one reply per ProposeMolecule in order. */
    const char *continuid_reply;
    const char *propose_replies[MAX_REQUESTS];
    size_t propose_reply_count;
    /* Builds the malloc'd reply to a CipherHash (encrypted) request from its body. */
    char *(*cipher_reply)(const char *body, void *ctx);
    void *cipher_ctx;

    recorded_request_t requests[MAX_REQUESTS];
    size_t request_count;
    size_t propose_count;
    size_t cipher_count;
} stub_server_t;

static const char *const REJECTED_REPLY =
    "{\"data\":{\"ProposeMolecule\":{\"molecularHash\":\"x\",\"status\":\"rejected\","
    "\"reason\":\"unscripted proposal\",\"payload\":null,\"createdAt\":null}}}";

static bool send_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n <= 0) return false;
        buf += n;
        len -= (size_t)n;
    }
    return true;
}

/* Reads one HTTP request (headers + Content-Length body). Returns the body, or NULL. */
static char *read_request(int fd, bool *has_auth_header) {
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
    *has_auth_header = false;
    for (char *line = buf; line < header_end;) {
        char *eol = strstr(line, "\r\n");
        if (!eol) break;
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            content_length = (size_t)strtoul(line + 15, NULL, 10);
        } else if (strncasecmp(line, "Expect:", 7) == 0 && strstr(line, "100-continue")) {
            expect_continue = true;
        } else if (strncasecmp(line, "X-Auth-Token:", 13) == 0) {
            *has_auth_header = true;
        }
        line = eol + 2;
    }
    if (expect_continue && len == header_len) {
        const char *cont = "HTTP/1.1 100 Continue\r\n\r\n";
        if (!send_all(fd, cont, strlen(cont))) { free(buf); return NULL; }
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

static void *serve(void *arg) {
    stub_server_t *s = arg;
    for (;;) {
        int fd = accept(s->listen_fd, NULL, NULL);
        pthread_mutex_lock(&s->lock);
        bool stop = s->stop;
        pthread_mutex_unlock(&s->lock);
        if (fd < 0 || stop) {
            if (fd >= 0) close(fd);
            break;
        }
        bool has_auth = false;
        char *body = read_request(fd, &has_auth);
        const char *reply = "{\"errors\":[{\"message\":\"unreadable request\"}]}";
        char *owned_reply = NULL;
        pthread_mutex_lock(&s->lock);
        if (body) {
            if (strstr(body, "CipherHash") && s->cipher_reply) {
                owned_reply = s->cipher_reply(body, s->cipher_ctx);
                if (owned_reply) reply = owned_reply;
                s->cipher_count++;
            } else if (strstr(body, "QueryContinuId")) {
                reply = s->continuid_reply;
            } else if (strstr(body, "ProposeMolecule")) {
                reply = (s->propose_count < s->propose_reply_count)
                    ? s->propose_replies[s->propose_count] : REJECTED_REPLY;
                s->propose_count++;
            }
            if (s->request_count < MAX_REQUESTS) {
                s->requests[s->request_count].body = body;
                s->requests[s->request_count].has_auth_header = has_auth;
                s->request_count++;
                body = NULL;
            }
        }
        pthread_mutex_unlock(&s->lock);
        free(body);
        char head[160];
        int head_len = snprintf(head, sizeof(head),
                                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                                "Content-Length: %zu\r\nConnection: close\r\n\r\n", strlen(reply));
        if (send_all(fd, head, (size_t)head_len)) {
            send_all(fd, reply, strlen(reply));
        }
        free(owned_reply);
        close(fd);
    }
    return NULL;
}

static bool server_start(stub_server_t *s) {
    memset(s, 0, sizeof(*s));
    pthread_mutex_init(&s->lock, NULL);
    s->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->listen_fd < 0) return false;
    int one = 1;
    setsockopt(s->listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    socklen_t addr_len = sizeof(addr);
    if (bind(s->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0
        || listen(s->listen_fd, 8) != 0
        || getsockname(s->listen_fd, (struct sockaddr *)&addr, &addr_len) != 0) {
        close(s->listen_fd);
        return false;
    }
    s->port = ntohs(addr.sin_port);
    return pthread_create(&s->thread, NULL, serve, s) == 0;
}

static void server_stop(stub_server_t *s) {
    pthread_mutex_lock(&s->lock);
    s->stop = true;
    pthread_mutex_unlock(&s->lock);
    /* Wake the blocking accept(). */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
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
    for (size_t i = 0; i < s->request_count; i++) free(s->requests[i].body);
    pthread_mutex_destroy(&s->lock);
}

/* ------------------------------------------------------------------------------------------ */
/* Fixtures                                                                                    */
/* ------------------------------------------------------------------------------------------ */

static const char *const POINTER_POSITION =
    "a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1";

static const char *const ACCEPTED_REPLY =
    "{\"data\":{\"ProposeMolecule\":{\"molecularHash\":\"h\",\"status\":\"accepted\","
    "\"reason\":null,\"payload\":\"{\\\"token\\\":\\\"jwt-accepted\\\"}\",\"createdAt\":\"0\"}}}";

static const char *const POINTER_REJECTED_REPLY =
    "{\"data\":{\"ProposeMolecule\":{\"molecularHash\":\"h\",\"status\":\"rejected\","
    "\"reason\":\"ContinuID position already consumed\",\"payload\":null,\"createdAt\":null}}}";

static const char *const GENESIS_REPLY = "{\"data\":{\"ContinuId\":null}}";

static char *g_secret = NULL;

/* {"data":{"ContinuId":{...}}} for a wallet with the given token/position/address. */
static char *continuid_reply(const char *token, const char *position, const char *address,
                             const char *bundle) {
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_AddObjectToObject(root, "data");
    cJSON *cid = cJSON_AddObjectToObject(data, "ContinuId");
    cJSON_AddStringToObject(cid, "position", position);
    cJSON_AddStringToObject(cid, "address", address);
    cJSON_AddStringToObject(cid, "tokenSlug", token);
    cJSON_AddStringToObject(cid, "bundleHash", bundle);
    cJSON_AddNullToObject(cid, "pubkey");
    cJSON_AddNullToObject(cid, "characters");
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return text;
}

static knishio_client_t *make_client(const stub_server_t *s) {
    char uri[64];
    snprintf(uri, sizeof(uri), "http://127.0.0.1:%u/graphql", (unsigned)s->port);
    knishio_client_config_t config = {0};
    config.uri = uri;
    config.cell_slug = "public";
    knishio_client_t *client = NULL;
    if (knishio_client_create(&client, &config) != KNISHIO_SUCCESS) return NULL;
    return client;
}

/* Parses the molecule out of a recorded ProposeMolecule request body. */
static knishio_molecule_t *proposed_molecule(const char *body) {
    cJSON *root = cJSON_Parse(body);
    cJSON *vars = root ? cJSON_GetObjectItemCaseSensitive(root, "variables") : NULL;
    cJSON *mol = vars ? cJSON_GetObjectItemCaseSensitive(vars, "molecule") : NULL;
    char *text = mol ? cJSON_PrintUnformatted(mol) : NULL;
    knishio_molecule_t *m = NULL;
    if (text) {
        if (knishio_molecule_from_json(text, &m) != KNISHIO_SUCCESS) m = NULL;
        cJSON_free(text);
    }
    if (root) cJSON_Delete(root);
    return m;
}

static const char *meta_value(const knishio_atom_t *atom, const char *key) {
    for (size_t i = 0; atom && i < atom->meta_count; i++) {
        if (atom->meta[i] && atom->meta[i]->key && strcmp(atom->meta[i]->key, key) == 0) {
            return atom->meta[i]->value;
        }
    }
    return NULL;
}

/* The i-th recorded ProposeMolecule request body (i counts proposals only). */
static const char *proposal_body(const stub_server_t *s, size_t i) {
    size_t seen = 0;
    for (size_t r = 0; r < s->request_count; r++) {
        if (strstr(s->requests[r].body, "ProposeMolecule")) {
            if (seen == i) return s->requests[r].body;
            seen++;
        }
    }
    return NULL;
}

static const char *atom0_token(const stub_server_t *s, size_t i, char *out, size_t out_len) {
    out[0] = '\0';
    const char *body = proposal_body(s, i);
    knishio_molecule_t *m = body ? proposed_molecule(body) : NULL;
    if (m && m->atom_count > 0 && m->atoms[0]->token) {
        snprintf(out, out_len, "%s", m->atoms[0]->token);
    }
    knishio_molecule_free_deep(m);
    return out;
}

/* ------------------------------------------------------------------------------------------ */
/* Cases                                                                                       */
/* ------------------------------------------------------------------------------------------ */

static void case_pointer_signed(const char *bundle) {
    printf("pointer returned: the login is signed from the USER wallet at the pointer\n");
    knishio_wallet_t *w = NULL;
    knishio_wallet_create_simple(&w, g_secret, "USER", POINTER_POSITION);
    char *cid = continuid_reply("USER", POINTER_POSITION, w->address, bundle);

    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    s.continuid_reply = cid;
    s.propose_replies[0] = ACCEPTED_REPLY;
    s.propose_reply_count = 1;

    knishio_client_t *client = make_client(&s);
    knishio_request_profile_auth_token_params_t params = { .secret = g_secret, .encrypt = false };
    knishio_request_profile_auth_token_result_t *result = NULL;
    knishio_error_t err = knishio_client_request_profile_auth_token(client, &params, &result);

    char detail[256];
    snprintf(detail, sizeof(detail), "err=%d success=%d token=%s", (int)err,
             result ? (int)result->success : -1, (result && result->token) ? result->token : "(null)");
    check(err == KNISHIO_SUCCESS && result && result->success && result->token
              && strcmp(result->token, "jwt-accepted") == 0,
          "the pointer-signed login yields the validator's token", detail);

    check(s.request_count >= 1 && strstr(s.requests[0].body, "QueryContinuId") != NULL,
          "the ContinuID query is sent before the proposal", NULL);
    cJSON *q = s.request_count >= 1 ? cJSON_Parse(s.requests[0].body) : NULL;
    cJSON *qv = q ? cJSON_GetObjectItemCaseSensitive(q, "variables") : NULL;
    const char *qtoken = qv ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(qv, "token")) : NULL;
    const char *qbundle = qv ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(qv, "bundle")) : NULL;
    snprintf(detail, sizeof(detail), "token=%s", qtoken ? qtoken : "(null)");
    check(qtoken && strcmp(qtoken, "USER") == 0 && qbundle && strcmp(qbundle, bundle) == 0,
          "the ContinuID query carries token USER and the secret's bundle", detail);
    if (q) cJSON_Delete(q);
    check(s.request_count >= 1 && !s.requests[0].has_auth_header,
          "the ContinuID query needs no auth token (fresh client sends none)", NULL);

    snprintf(detail, sizeof(detail), "proposals=%zu", s.propose_count);
    check(s.propose_count == 1, "exactly one authorization molecule is proposed", detail);

    const char *body = proposal_body(&s, 0);
    knishio_molecule_t *m = body ? proposed_molecule(body) : NULL;
    const knishio_atom_t *a0 = (m && m->atom_count > 0) ? m->atoms[0] : NULL;
    const knishio_atom_t *a1 = (m && m->atom_count > 1) ? m->atoms[1] : NULL;
    snprintf(detail, sizeof(detail), "isotope=%d token=%s position=%.16s…",
             a0 ? (int)a0->isotope : -1, (a0 && a0->token) ? a0->token : "(null)",
             (a0 && a0->position) ? a0->position : "(null)");
    check(a0 && a0->isotope == KNISHIO_ISOTOPE_U && a0->token && strcmp(a0->token, "USER") == 0
              && a0->position && strcmp(a0->position, POINTER_POSITION) == 0,
          "atoms[0] is a U atom with token USER at the pointer position", detail);
    check(a0 && a0->wallet_address && strcmp(a0->wallet_address, w->address) == 0,
          "atoms[0].walletAddress is the USER wallet derived from the secret at the pointer", NULL);
    const char *prev = meta_value(a1, "previousPosition");
    check(a1 && a1->isotope == KNISHIO_ISOTOPE_I && prev && strcmp(prev, POINTER_POSITION) == 0,
          "atoms[1] is the I atom with previousPosition = the pointer", NULL);
    check(a1 && a1->token && strcmp(a1->token, "USER") == 0 && a1->position
              && strcmp(a1->position, POINTER_POSITION) != 0,
          "atoms[1] moves the pointer to a fresh USER position", NULL);
    knishio_molecule_free_deep(m);

    knishio_auth_token_t *tok = knishio_client_get_auth_token(client);
    const char *pub = tok ? knishio_auth_token_get_pubkey(tok) : NULL;
    check(pub && strcmp(pub, w->address) == 0,
          "the client auth token is bound to the pointer wallet", NULL);

    knishio_request_profile_auth_token_result_free(result);
    knishio_client_destroy(client);
    server_stop(&s);
    cJSON_free(cid);
    knishio_wallet_free(w);
}

/* No usable pointer: exactly one AUTH-signed proposal. */
static void case_auth_path(const char *label, const char *cid_reply) {
    printf("%s: the login is signed from a fresh AUTH wallet\n", label);
    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    s.continuid_reply = cid_reply;
    s.propose_replies[0] = ACCEPTED_REPLY;
    s.propose_reply_count = 1;

    knishio_client_t *client = make_client(&s);
    knishio_request_profile_auth_token_params_t params = { .secret = g_secret, .encrypt = false };
    knishio_request_profile_auth_token_result_t *result = NULL;
    knishio_error_t err = knishio_client_request_profile_auth_token(client, &params, &result);

    char token[32];
    char detail[128];
    snprintf(detail, sizeof(detail), "err=%d proposals=%zu atoms[0].token=%s", (int)err,
             s.propose_count, atom0_token(&s, 0, token, sizeof(token)));
    check(err == KNISHIO_SUCCESS && result && result->success && s.propose_count == 1
              && strcmp(token, "AUTH") == 0,
          "exactly one proposal, atoms[0].token AUTH", detail);

    knishio_request_profile_auth_token_result_free(result);
    knishio_client_destroy(client);
    server_stop(&s);
}

/* A client with the encrypted transport switched on but no keys yet must still send the ContinuID
 * query in plaintext: it is part of the auth bootstrap, like the U-isotope proposal. */
static void case_encryption_enabled_client(const char *bundle) {
    printf("encryption enabled before login: the ContinuID query stays plaintext\n");
    knishio_wallet_t *w = NULL;
    knishio_wallet_create_simple(&w, g_secret, "USER", POINTER_POSITION);
    char *cid = continuid_reply("USER", POINTER_POSITION, w->address, bundle);

    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    s.continuid_reply = cid;
    s.propose_replies[0] = ACCEPTED_REPLY;
    s.propose_reply_count = 1;

    knishio_client_t *client = make_client(&s);
    knishio_client_set_encryption(client, true);
    knishio_request_profile_auth_token_params_t params = { .secret = g_secret, .encrypt = true };
    knishio_request_profile_auth_token_result_t *result = NULL;
    knishio_error_t err = knishio_client_request_profile_auth_token(client, &params, &result);

    char token[32];
    char detail[160];
    snprintf(detail, sizeof(detail), "err=%d (%s) requests=%zu atoms[0].token=%s", (int)err,
             knishio_error_to_string(err), s.request_count, atom0_token(&s, 0, token, sizeof(token)));
    check(err == KNISHIO_SUCCESS && result && result->success && s.request_count == 2
              && strstr(s.requests[0].body, "QueryContinuId") && !strstr(s.requests[0].body, "CipherHash")
              && strcmp(token, "USER") == 0,
          "the plaintext ContinuID query and the pointer-signed proposal both go out", detail);

    if (result) knishio_request_profile_auth_token_result_free(result);
    knishio_client_destroy(client);
    server_stop(&s);
    cJSON_free(cid);
    knishio_wallet_free(w);
}

static void case_pointer_rejected(const char *bundle, bool fallback_accepted) {
    printf("pointer-signed login rejected, fallback %s\n", fallback_accepted ? "accepted" : "rejected");
    knishio_wallet_t *w = NULL;
    knishio_wallet_create_simple(&w, g_secret, "USER", POINTER_POSITION);
    char *cid = continuid_reply("USER", POINTER_POSITION, w->address, bundle);

    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    s.continuid_reply = cid;
    s.propose_replies[0] = POINTER_REJECTED_REPLY;
    s.propose_replies[1] = fallback_accepted ? ACCEPTED_REPLY : POINTER_REJECTED_REPLY;
    s.propose_replies[2] = ACCEPTED_REPLY;  /* a third proposal must never be sent */
    s.propose_reply_count = 3;

    knishio_client_t *client = make_client(&s);
    /* knishio_client_authenticate is the login entry point: it maps a rejected authorization to
     * KNISHIO_ERROR_AUTHORIZATION_REJECTED. */
    knishio_error_t err = knishio_client_authenticate(client, g_secret, false);

    char t0[32], t1[32];
    char detail[160];
    snprintf(detail, sizeof(detail), "err=%d proposals=%zu tokens=%s,%s", (int)err, s.propose_count,
             atom0_token(&s, 0, t0, sizeof(t0)), atom0_token(&s, 1, t1, sizeof(t1)));
    check(s.propose_count == 2 && strcmp(t0, "USER") == 0 && strcmp(t1, "AUTH") == 0,
          "exactly two proposals: the pointer-signed one, then one AUTH fallback", detail);
    if (fallback_accepted) {
        check(err == KNISHIO_SUCCESS, "the AUTH fallback's token logs the user in", detail);
    } else {
        check(err == KNISHIO_ERROR_AUTHORIZATION_REJECTED,
              "a rejected fallback raises KNISHIO_ERROR_AUTHORIZATION_REJECTED", detail);
    }

    knishio_client_destroy(client);
    server_stop(&s);
    cJSON_free(cid);
    knishio_wallet_free(w);
}

/* knishio_molecule_check() verdict for a signed auth molecule whose U atom carries `token`. */
static knishio_error_t check_auth_molecule(const char *token) {
    knishio_wallet_t *source = NULL, *remainder = NULL;
    char *remainder_position = NULL;
    knishio_molecule_t *m = NULL;
    knishio_error_t err = knishio_wallet_create_simple(&source, g_secret, token, POINTER_POSITION);
    if (err == KNISHIO_SUCCESS && !knishio_generate_position(&remainder_position)) err = KNISHIO_ERROR_CRYPTO;
    if (err == KNISHIO_SUCCESS) err = knishio_wallet_create_simple(&remainder, g_secret, "USER", remainder_position);
    if (err == KNISHIO_SUCCESS) {
        err = knishio_molecule_create(&m, source->secret, source->bundle_hash, source, remainder,
                                      "public", "V4");
    }
    if (err == KNISHIO_SUCCESS) err = knishio_molecule_init_authorization(m, false);
    if (err == KNISHIO_SUCCESS) err = knishio_molecule_generate_hash(m);
    if (err == KNISHIO_SUCCESS) err = knishio_molecule_sign(m, source->bundle_hash, false, true);
    if (err == KNISHIO_SUCCESS) err = knishio_molecule_check(m, NULL);
    knishio_molecule_free_deep(m);
    knishio_wallet_free(source);
    knishio_wallet_free(remainder);
    free(remainder_position);
    return err;
}

static void case_isotope_u_token(void) {
    printf("knishio_molecule_check: U atom token\n");
    char detail[96];
    knishio_error_t v = check_auth_molecule("AUTH");
    snprintf(detail, sizeof(detail), "%d (%s)", (int)v, knishio_error_to_string(v));
    check(v == KNISHIO_SUCCESS, "a U atom with token AUTH passes", detail);
    v = check_auth_molecule("USER");
    snprintf(detail, sizeof(detail), "%d (%s)", (int)v, knishio_error_to_string(v));
    check(v == KNISHIO_SUCCESS, "a U atom with token USER passes", detail);
    v = check_auth_molecule("TEST");
    snprintf(detail, sizeof(detail), "%d (%s)", (int)v, knishio_error_to_string(v));
    check(v == KNISHIO_ERROR_INVALID_STATE, "a U atom with token TEST is rejected", detail);
}

static void case_snapshot_restore(void) {
    printf("auth token snapshot / restore keeps the bound wallet's token\n");
    knishio_wallet_t *w = NULL;
    knishio_wallet_create_simple(&w, g_secret, "USER", POINTER_POSITION);
    knishio_auth_token_config_t cfg = { .token = "jwt", .expires_at = 0, .encrypt = false,
                                        .pubkey = w->address };
    knishio_auth_token_t *tok = NULL;
    knishio_auth_token_create_with_wallet(&tok, &cfg, w);
    char *snapshot = NULL;
    knishio_error_t err = tok ? knishio_auth_token_get_snapshot(tok, &snapshot) : KNISHIO_ERROR_MEMORY;

    cJSON *snap = snapshot ? cJSON_Parse(snapshot) : NULL;
    cJSON *snap_wallet = snap ? cJSON_GetObjectItemCaseSensitive(snap, "wallet") : NULL;
    const char *snap_token = snap_wallet
        ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(snap_wallet, "token")) : NULL;
    check(err == KNISHIO_SUCCESS && snap_token && strcmp(snap_token, "USER") == 0,
          "the snapshot records wallet.token of the bound wallet", snapshot ? NULL : "no snapshot");

    knishio_auth_token_t *restored = NULL;
    err = snapshot ? knishio_auth_token_restore(&restored, snapshot, g_secret) : KNISHIO_ERROR_MEMORY;
    knishio_wallet_t *rw = restored ? knishio_auth_token_get_wallet(restored) : NULL;
    char detail[160];
    snprintf(detail, sizeof(detail), "err=%d token=%s", (int)err, (rw && rw->token) ? rw->token : "(null)");
    check(rw && rw->token && strcmp(rw->token, "USER") == 0
              && rw->address && strcmp(rw->address, w->address) == 0
              && rw->pubkey && w->pubkey && strcmp(rw->pubkey, w->pubkey) == 0,
          "restore rebuilds the USER wallet with the same address and ML-KEM pubkey", detail);
    if (rw) knishio_wallet_free(rw);
    if (restored) knishio_auth_token_cleanup(restored);

    /* An old snapshot (no wallet.token) still restores an AUTH wallet. */
    knishio_auth_token_t *legacy = NULL;
    knishio_wallet_t *lw = NULL;
    if (snap_wallet) {
        cJSON_DeleteItemFromObjectCaseSensitive(snap_wallet, "token");
        char *legacy_text = cJSON_PrintUnformatted(snap);
        err = knishio_auth_token_restore(&legacy, legacy_text, g_secret);
        cJSON_free(legacy_text);
        lw = legacy ? knishio_auth_token_get_wallet(legacy) : NULL;
    }
    snprintf(detail, sizeof(detail), "err=%d token=%s", (int)err, (lw && lw->token) ? lw->token : "(null)");
    check(lw && lw->token && strcmp(lw->token, "AUTH") == 0,
          "a snapshot without wallet.token restores an AUTH wallet", detail);
    if (lw) knishio_wallet_free(lw);
    if (legacy) knishio_auth_token_cleanup(legacy);

    if (snap) cJSON_Delete(snap);
    free(snapshot);
    if (tok) knishio_auth_token_cleanup(tok);
    knishio_wallet_free(w);
}

/* ------------------------------------------------------------------------------------------ */
/* Mutations on an encrypted session                                                           */
/* ------------------------------------------------------------------------------------------ */

typedef enum {
    REPLY_ACCEPTED,       /* the decrypted reply accepts the proposed molecule */
    REPLY_GRAPHQL_ERROR,  /* the decrypted reply carries GraphQL errors */
    REPLY_UNDECRYPTABLE   /* data.CipherHash.hash is not a CipherHash map */
} cipher_reply_kind_t;

typedef struct {
    const knishio_wallet_t *validator;  /* holds the ML-KEM key the login advertised */
    const char *recipient_pubkey;       /* the pointer USER wallet the login bound */
    cipher_reply_kind_t kind;
    bool request_opened;   /* the request opened under the validator key and is a ProposeMolecule */
    char *molecular_hash;  /* of the molecule inside the encrypted request */
    char *inner;           /* the plaintext reply the stub encrypted */
    char *envelope;        /* the data.CipherHash.hash the stub sent */
} cipher_script_t;

/* The validator's CipherHash reply map: the reply OBJECT's bytes encrypted as-is and keyed by
 * hashShare(recipient pubkey). knishio_cipher_hash_encrypt() encrypts a request body as a JSON
 * string literal instead, which the client would decrypt to a quoted string. */
static char *encrypt_reply(const char *inner, const char *pubkey_b64) {
    unsigned char *pub = NULL;
    size_t pub_len = 0;
    uint8_t *sealed = NULL;
    size_t sealed_len = 0;
    char *ct_b64 = NULL, *msg_b64 = NULL, *share = NULL, *out = NULL;
    knishio_mlkem_ciphertext_t kem_ct;
    knishio_mlkem_shared_secret_t shared;
    if (knishio_base64_decode(pubkey_b64, &pub, &pub_len)
        && knishio_mlkem_encapsulate(pub, pub_len, &kem_ct, &shared) == KNISHIO_SUCCESS
        && knishio_aes_gcm_encrypt((const uint8_t *)inner, strlen(inner), shared.shared_secret,
                                   &sealed, &sealed_len) == KNISHIO_SUCCESS
        && knishio_base64_encode(kem_ct.ciphertext, kem_ct.ciphertext_len, &ct_b64)
        && knishio_base64_encode(sealed, sealed_len, &msg_b64)
        && knishio_cipher_hash_share(pubkey_b64, &share) == KNISHIO_SUCCESS) {
        cJSON *map = cJSON_CreateObject();
        cJSON *entry = cJSON_AddObjectToObject(map, share);
        cJSON_AddStringToObject(entry, "cipherText", ct_b64);
        cJSON_AddStringToObject(entry, "encryptedMessage", msg_b64);
        out = cJSON_PrintUnformatted(map);
        cJSON_Delete(map);
    }
    free(pub);
    free(sealed);
    free(ct_b64);
    free(msg_b64);
    free(share);
    return out;
}

/* Opens the CipherHash request like the validator (the envelope holds the request body as a JSON
 * string literal), records the proposed molecule's hash, and answers per the script. */
static char *scripted_cipher_reply(const char *body, void *ctx) {
    cipher_script_t *c = ctx;
    cJSON *root = cJSON_Parse(body);
    cJSON *vars = root ? cJSON_GetObjectItemCaseSensitive(root, "variables") : NULL;
    const char *hash_var = vars ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(vars, "Hash")) : NULL;
    char *plain = NULL;
    if (hash_var && knishio_cipher_hash_decrypt(hash_var, c->validator, &plain) == KNISHIO_SUCCESS
        && plain) {
        cJSON *literal = cJSON_Parse(plain);
        cJSON *request = cJSON_IsString(literal) ? cJSON_Parse(cJSON_GetStringValue(literal)) : NULL;
        cJSON *rvars = request ? cJSON_GetObjectItemCaseSensitive(request, "variables") : NULL;
        cJSON *mol = rvars ? cJSON_GetObjectItemCaseSensitive(rvars, "molecule") : NULL;
        const char *query = request ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(request, "query")) : NULL;
        const char *mh = mol ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(mol, "molecularHash")) : NULL;
        if (query && strstr(query, "ProposeMolecule") && mh) {
            c->request_opened = true;
            free(c->molecular_hash);
            c->molecular_hash = strdup(mh);
        }
        cJSON_Delete(request);
        cJSON_Delete(literal);
    }
    free(plain);
    cJSON_Delete(root);

    free(c->inner);
    c->inner = NULL;
    free(c->envelope);
    c->envelope = NULL;
    if (c->kind == REPLY_UNDECRYPTABLE) {
        c->envelope = strdup("malformed-cipher-hash");
    } else {
        cJSON *reply = cJSON_CreateObject();
        if (c->kind == REPLY_ACCEPTED) {
            cJSON *pm = cJSON_AddObjectToObject(cJSON_AddObjectToObject(reply, "data"), "ProposeMolecule");
            cJSON_AddStringToObject(pm, "molecularHash", c->molecular_hash ? c->molecular_hash : "");
            cJSON_AddStringToObject(pm, "status", "accepted");
            cJSON_AddNullToObject(pm, "reason");
            cJSON_AddNullToObject(pm, "payload");
        } else {
            cJSON_AddNullToObject(reply, "data");
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "message", "x");
            cJSON_AddItemToArray(cJSON_AddArrayToObject(reply, "errors"), err);
        }
        c->inner = cJSON_PrintUnformatted(reply);
        cJSON_Delete(reply);
        c->envelope = c->inner ? encrypt_reply(c->inner, c->recipient_pubkey) : NULL;
    }

    cJSON *out = cJSON_CreateObject();
    cJSON *cipher = cJSON_AddObjectToObject(cJSON_AddObjectToObject(out, "data"), "CipherHash");
    cJSON_AddStringToObject(cipher, "hash", c->envelope ? c->envelope : "");
    char *text = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    return text;
}

/* An accepted login reply whose payload advertises the validator's ML-KEM key (payload.key). */
static char *accepted_reply_with_key(const char *validator_pubkey) {
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "token", "jwt-accepted");
    cJSON_AddStringToObject(payload, "key", validator_pubkey);
    char *payload_text = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    cJSON *root = cJSON_CreateObject();
    cJSON *pm = cJSON_AddObjectToObject(cJSON_AddObjectToObject(root, "data"), "ProposeMolecule");
    cJSON_AddStringToObject(pm, "molecularHash", "h");
    cJSON_AddStringToObject(pm, "status", "accepted");
    cJSON_AddNullToObject(pm, "reason");
    cJSON_AddStringToObject(pm, "payload", payload_text ? payload_text : "");
    cJSON_AddStringToObject(pm, "createdAt", "0");
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(payload_text);
    return text;
}

static void case_encrypted_session_mutation(const char *bundle, cipher_reply_kind_t kind,
                                            const char *label) {
    printf("encrypted session: createToken, %s\n", label);
    knishio_wallet_t *w = NULL;
    knishio_wallet_create_simple(&w, g_secret, "USER", POINTER_POSITION);
    char *validator_secret = NULL;
    knishio_generate_secret("profile-auth-continuid-validator", 2048, &validator_secret);
    knishio_wallet_t *validator = NULL;
    knishio_wallet_create_simple(&validator, validator_secret, "AUTH", POINTER_POSITION);
    char *cid = continuid_reply("USER", POINTER_POSITION, w->address, bundle);
    char *login_reply = accepted_reply_with_key(validator->pubkey);

    cipher_script_t script = { .validator = validator, .recipient_pubkey = w->pubkey, .kind = kind };
    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    s.continuid_reply = cid;
    s.propose_replies[0] = login_reply;
    s.propose_reply_count = 1;
    s.cipher_reply = scripted_cipher_reply;
    s.cipher_ctx = &script;

    knishio_client_t *client = make_client(&s);
    knishio_request_profile_auth_token_params_t params = { .secret = g_secret, .encrypt = true };
    knishio_request_profile_auth_token_result_t *login = NULL;
    knishio_error_t err = knishio_client_request_profile_auth_token(client, &params, &login);
    check(err == KNISHIO_SUCCESS && login && login->success, "the encrypted login is accepted", NULL);

    knishio_create_token_params_t token_params = {
        .token = "ENCTOKEN", .name = "Encrypted session token", .amount = 1000,
        .fungibility = KNISHIO_TOKEN_FUNGIBLE
    };
    knishio_create_token_result_t *created = NULL;
    err = knishio_client_create_token(client, &token_params, &created);

    char detail[256];
    snprintf(detail, sizeof(detail), "cipher requests=%zu plaintext proposals=%zu opened=%d",
             s.cipher_count, s.propose_count, (int)script.request_opened);
    check(s.cipher_count == 1 && s.propose_count == 1 && script.request_opened,
          "createToken goes out as one CipherHash envelope (no plaintext proposal)", detail);

    if (kind != REPLY_UNDECRYPTABLE) {
        char *roundtrip = NULL;
        knishio_error_t de = script.envelope
            ? knishio_cipher_hash_decrypt(script.envelope, w, &roundtrip) : KNISHIO_ERROR_INVALID_ARGS;
        check(de == KNISHIO_SUCCESS && roundtrip && script.inner && strcmp(roundtrip, script.inner) == 0,
              "the stub's reply decrypts to its plaintext reply under the pointer USER wallet", NULL);
        free(roundtrip);
    }

    snprintf(detail, sizeof(detail), "err=%d success=%d hash=%.16s… error=%.120s", (int)err,
             created ? (int)created->success : -1,
             (created && created->molecular_hash) ? created->molecular_hash : "(null)",
             (created && created->error_message) ? created->error_message : "(null)");
    switch (kind) {
    case REPLY_ACCEPTED:
        check(err == KNISHIO_SUCCESS && created && created->success && created->molecular_hash
                  && script.molecular_hash && strcmp(created->molecular_hash, script.molecular_hash) == 0,
              "createToken succeeds with the proposed molecule's hash from the decrypted reply", detail);
        break;
    case REPLY_GRAPHQL_ERROR:
        check(err == KNISHIO_SUCCESS && created && !created->success && created->error_message
                  && strstr(created->error_message, "\"message\":\"x\""),
              "createToken fails with the decrypted reply's GraphQL error", detail);
        break;
    case REPLY_UNDECRYPTABLE:
        check(err == KNISHIO_SUCCESS && created && !created->success && created->error_message
                  && strstr(created->error_message, "CipherHash response could not be decrypted"),
              "an undecryptable reply fails closed", detail);
        break;
    }

    knishio_create_token_result_free(created);
    knishio_request_profile_auth_token_result_free(login);
    knishio_client_destroy(client);
    server_stop(&s);
    free(script.molecular_hash);
    free(script.inner);
    free(script.envelope);
    free(login_reply);
    cJSON_free(cid);
    knishio_wallet_free(validator);
    free(validator_secret);
    knishio_wallet_free(w);
}

int main(void) {
    printf("profile auth from the ContinuID pointer (SDK %s)\n", KNISHIO_VERSION_STRING);
    if (!knishio_generate_secret("profile-auth-continuid-test", 2048, &g_secret)) {
        fprintf(stderr, "profile_auth_continuid: cannot generate a secret\n");
        return 2;
    }
    char *bundle = NULL;
    if (!knishio_generate_bundle_hash(g_secret, "USER", POINTER_POSITION, &bundle) || !bundle) {
        fprintf(stderr, "profile_auth_continuid: cannot derive the bundle hash\n");
        return 2;
    }

    case_pointer_signed(bundle);
    case_encryption_enabled_client(bundle);

    case_auth_path("no ContinuID (first login)", GENESIS_REPLY);

    knishio_wallet_t *w = NULL;
    knishio_wallet_create_simple(&w, g_secret, "AUTH", POINTER_POSITION);
    char *non_user = continuid_reply("AUTH", POINTER_POSITION, w->address, bundle);
    case_auth_path("ContinuID wallet is not USER", non_user);
    cJSON_free(non_user);
    knishio_wallet_free(w);

    char *mismatch = continuid_reply(
        "USER", POINTER_POSITION,
        "0000000000000000000000000000000000000000000000000000000000000000", bundle);
    case_auth_path("ContinuID address differs from the secret's wallet", mismatch);
    cJSON_free(mismatch);

    case_pointer_rejected(bundle, true);
    case_pointer_rejected(bundle, false);

    case_isotope_u_token();
    case_snapshot_restore();

    case_encrypted_session_mutation(bundle, REPLY_ACCEPTED, "the decrypted reply accepts");
    case_encrypted_session_mutation(bundle, REPLY_GRAPHQL_ERROR, "the decrypted reply has errors");
    case_encrypted_session_mutation(bundle, REPLY_UNDECRYPTABLE, "the reply cannot be decrypted");

    free(bundle);
    free(g_secret);
    printf("%s (%d failure%s)\n", g_failures ? "FAIL" : "PASS", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
