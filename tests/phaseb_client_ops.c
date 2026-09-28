/**
 * @file phaseb_client_ops.c
 * @brief The Phase B client operations send the molecules validator 0.6.x accepts.
 *
 * Drives the public client API against a scripted loopback GraphQL endpoint (an HTTP/1.1
 * server on an ephemeral 127.0.0.1 port, in a thread; no external service) and inspects the
 * requests the client actually sent:
 *   - replenishToken proposes C(action "add") + I crediting the identity's existing wallet,
 *     never the RequestTokens faucet (contract 9.1);
 *   - fuseToken proposes V V F V spending the identity's stackable wallet, and refuses a
 *     single-unit fusion without sending anything (contract 9.2);
 *   - claimShadowWallet without a batch id claims the first SHADOW wallet queryWallets lists,
 *     even when a regular wallet is listed first, and fails locally when there is none (11.2b);
 *   - withdrawBufferToken spends Balance(type: "buffer") and sends the remainder to a fresh
 *     position (contract 9.6);
 *   - a built molecule that fails the SDK's own check (USER-signed, no I atom) is refused with
 *     KNISHIO_ERROR_ATOMS_MISSING and nothing is sent, while the raw propose path sends the same
 *     molecule unchanged; createMeta sends M + I (contract 9.7).
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
#include "knishio/client.h"
#include "knishio/client_ops.h"
#include "knishio/graphql.h"
#include "knishio/meta.h"
#include "knishio/molecule.h"
#include "knishio/operations/auth.h"
#include "knishio/operations/identity.h"
#include "knishio/operations/token.h"
#include "knishio/operations/transfer.h"
#include "knishio/wallet.h"

static int g_failures = 0;

static void check(bool ok, const char *name, const char *detail) {
    printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name, detail ? " — " : "", detail ? detail : "");
    if (!ok) g_failures++;
}

/* ------------------------------------------------------------------------------------------ */
/* Scripted loopback GraphQL endpoint                                                          */
/* ------------------------------------------------------------------------------------------ */

#define MAX_REQUESTS 32

typedef struct {
    int listen_fd;
    unsigned short port;
    pthread_t thread;
    pthread_mutex_t lock;
    bool stop;
    /* Replies by operation. */
    const char *balance_reply;         /* Balance without type (regular) */
    const char *buffer_balance_reply;  /* Balance(type: "buffer") */
    const char *wallet_list_reply;
    char *requests[MAX_REQUESTS];
    size_t request_count;
} stub_server_t;

static const char *const GENESIS_REPLY = "{\"data\":{\"ContinuId\":null}}";
static const char *const ACCEPTED_REPLY =
    "{\"data\":{\"ProposeMolecule\":{\"molecularHash\":\"h\",\"status\":\"accepted\","
    "\"reason\":null,\"payload\":\"{\\\"token\\\":\\\"jwt-accepted\\\"}\",\"createdAt\":\"0\"}}}";
static const char *const NULL_BALANCE_REPLY = "{\"data\":{\"Balance\":null}}";

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
static char *read_request(int fd) {
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

static const char *route(const stub_server_t *s, const char *body) {
    if (strstr(body, "QueryContinuId")) return GENESIS_REPLY;
    if (strstr(body, "QueryBalance")) {
        const char *r = strstr(body, "\"type\":\"buffer\"") ? s->buffer_balance_reply : s->balance_reply;
        return r ? r : NULL_BALANCE_REPLY;
    }
    if (strstr(body, "QueryWalletList")) {
        return s->wallet_list_reply ? s->wallet_list_reply : "{\"data\":{\"Wallet\":[]}}";
    }
    if (strstr(body, "ProposeMolecule")) return ACCEPTED_REPLY;
    return "{\"errors\":[{\"message\":\"unscripted operation\"}]}";
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
        char *body = read_request(fd);
        const char *reply = "{\"errors\":[{\"message\":\"unreadable request\"}]}";
        pthread_mutex_lock(&s->lock);
        if (body) {
            reply = route(s, body);
            if (s->request_count < MAX_REQUESTS) {
                s->requests[s->request_count++] = body;
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
    for (size_t i = 0; i < s->request_count; i++) free(s->requests[i]);
    pthread_mutex_destroy(&s->lock);
}

/* Requests recorded so far whose body contains `needle`. */
static size_t count_requests(stub_server_t *s, const char *needle) {
    size_t n = 0;
    pthread_mutex_lock(&s->lock);
    for (size_t i = 0; i < s->request_count; i++) {
        if (strstr(s->requests[i], needle)) n++;
    }
    pthread_mutex_unlock(&s->lock);
    return n;
}

/* The molecule of the last ProposeMolecule request (the login is the first one). */
static knishio_molecule_t *last_proposal(stub_server_t *s) {
    knishio_molecule_t *m = NULL;
    pthread_mutex_lock(&s->lock);
    for (size_t i = s->request_count; i-- > 0;) {
        if (!strstr(s->requests[i], "ProposeMolecule")) continue;
        cJSON *root = cJSON_Parse(s->requests[i]);
        cJSON *vars = root ? cJSON_GetObjectItemCaseSensitive(root, "variables") : NULL;
        cJSON *mol = vars ? cJSON_GetObjectItemCaseSensitive(vars, "molecule") : NULL;
        char *text = mol ? cJSON_PrintUnformatted(mol) : NULL;
        if (text) {
            if (knishio_molecule_from_json(text, &m) != KNISHIO_SUCCESS) m = NULL;
            cJSON_free(text);
        }
        if (root) cJSON_Delete(root);
        break;
    }
    pthread_mutex_unlock(&s->lock);
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

/* ------------------------------------------------------------------------------------------ */
/* Fixtures                                                                                    */
/* ------------------------------------------------------------------------------------------ */

static char *g_secret = NULL;
static char *g_bundle = NULL;

static const char *const POS_REGULAR =
    "a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1";
static const char *const POS_BUFFER =
    "b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2";

static char *derived_address(const char *token, const char *position) {
    knishio_wallet_t *w = NULL;
    char *address = NULL;
    if (knishio_wallet_create_simple(&w, g_secret, token, position) == KNISHIO_SUCCESS) {
        address = strdup(w->address);
    }
    knishio_wallet_free(w);
    return address;
}

/* {"data":{"Balance":{...}}} for this identity's wallet of `token` at `position`. */
static char *balance_reply(const char *token, const char *position, const char *amount,
                           const char *const *unit_ids, size_t unit_count) {
    char *address = derived_address(token, position);
    cJSON *root = cJSON_CreateObject();
    cJSON *bal = cJSON_AddObjectToObject(cJSON_AddObjectToObject(root, "data"), "Balance");
    cJSON_AddStringToObject(bal, "address", address);
    cJSON_AddStringToObject(bal, "bundleHash", g_bundle);
    cJSON_AddStringToObject(bal, "tokenSlug", token);
    cJSON_AddNullToObject(bal, "batchId");
    cJSON_AddStringToObject(bal, "position", position);
    cJSON_AddStringToObject(bal, "amount", amount);
    cJSON_AddStringToObject(bal, "characters", "BASE64");
    cJSON *units = cJSON_AddArrayToObject(bal, "tokenUnits");
    for (size_t i = 0; i < unit_count; i++) {
        cJSON *u = cJSON_CreateObject();
        cJSON_AddStringToObject(u, "id", unit_ids[i]);
        cJSON_AddStringToObject(u, "name", unit_ids[i]);
        cJSON_AddNullToObject(u, "metas");
        cJSON_AddItemToArray(units, u);
    }
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(address);
    return text;
}

/* A fresh client logged in through the stub (first login: AUTH wallet, genesis ContinuID). */
static knishio_client_t *logged_in_client(stub_server_t *s) {
    char uri[64];
    snprintf(uri, sizeof(uri), "http://127.0.0.1:%u/graphql", (unsigned)s->port);
    knishio_client_config_t config = {0};
    config.uri = uri;
    config.cell_slug = "public";
    knishio_client_t *client = NULL;
    if (knishio_client_create(&client, &config) != KNISHIO_SUCCESS) return NULL;
    knishio_request_profile_auth_token_params_t params = { .secret = g_secret, .encrypt = false };
    knishio_request_profile_auth_token_result_t *result = NULL;
    knishio_error_t err = knishio_client_request_profile_auth_token(client, &params, &result);
    bool ok = err == KNISHIO_SUCCESS && result && result->success;
    knishio_request_profile_auth_token_result_free(result);
    if (!ok) {
        knishio_client_destroy(client);
        return NULL;
    }
    return client;
}

/* ------------------------------------------------------------------------------------------ */
/* Cases                                                                                       */
/* ------------------------------------------------------------------------------------------ */

static void case_replenish(void) {
    printf("replenishToken proposes C(action add) + I crediting the existing wallet\n");
    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    char *bal = balance_reply("RTOK", POS_REGULAR, "1000", NULL, 0);
    s.balance_reply = bal;
    knishio_client_t *client = logged_in_client(&s);
    check(client != NULL, "login through the stub", NULL);
    if (client) {
        knishio_request_tokens_result_t *res = NULL;
        knishio_error_t err = knishio_client_replenish_token(client, "RTOK", 500, NULL, 0, &res);
        char detail[160];
        snprintf(detail, sizeof(detail), "err=%d", (int)err);
        check(err == KNISHIO_SUCCESS && res && res->success, "the replenish is submitted and accepted", detail);
        check(count_requests(&s, "RequestTokens") == 0, "the RequestTokens faucet is not used", NULL);

        knishio_molecule_t *m = last_proposal(&s);
        const knishio_atom_t *c = (m && m->atom_count == 2) ? m->atoms[0] : NULL;
        const knishio_atom_t *i = (m && m->atom_count == 2) ? m->atoms[1] : NULL;
        char *address = derived_address("RTOK", POS_REGULAR);
        check(c && c->isotope == KNISHIO_ISOTOPE_C && i && i->isotope == KNISHIO_ISOTOPE_I,
              "atoms are [C, I]", NULL);
        check(c && c->value && strcmp(c->value, "500") == 0 && c->meta_type && strcmp(c->meta_type, "token") == 0
                  && c->meta_id && strcmp(c->meta_id, "RTOK") == 0 && c->token && strcmp(c->token, "USER") == 0,
              "C atom: USER-signed, value 500, metaType token, metaId RTOK", NULL);
        check(c && c->meta_count >= 1 && strcmp(c->meta[0]->key, "action") == 0
                  && strcmp(c->meta[0]->value, "add") == 0,
              "C atom's first meta is action = add", NULL);
        const char *addr = meta_value(c, "address");
        const char *pos = meta_value(c, "position");
        check(addr && address && strcmp(addr, address) == 0 && pos && strcmp(pos, POS_REGULAR) == 0,
              "the credited wallet is the identity's existing RTOK wallet", NULL);
        check(m && knishio_molecule_check(m, NULL) == KNISHIO_SUCCESS, "the sent molecule passes the SDK check", NULL);

        knishio_error_t neg = knishio_client_replenish_token(client, "RTOK", 0, NULL, 0, &res);
        check(neg == KNISHIO_ERROR_NEGATIVE_AMOUNT, "a zero amount is refused (NegativeAmount)", NULL);
        free(address);
        knishio_molecule_free_deep(m);
        knishio_request_tokens_result_free(res);
        knishio_client_destroy(client);
    }
    server_stop(&s);
    cJSON_free(bal);
}

static void case_fuse(void) {
    printf("fuseToken proposes V V F V from the identity's stackable wallet\n");
    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    const char *units[] = { "u1", "u2", "u3", "u4", "u5" };
    char *bal = balance_reply("STK", POS_REGULAR, "5", units, 5);
    s.balance_reply = bal;
    knishio_client_t *client = logged_in_client(&s);
    check(client != NULL, "login through the stub", NULL);
    if (client) {
        const char *fuse[] = { "u2", "u4", "u5" };
        knishio_request_tokens_result_t *res = NULL;
        knishio_error_t err = knishio_client_fuse_token(client, NULL, "STK", "FUSED", fuse, 3, &res);
        check(err == KNISHIO_SUCCESS && res && res->success, "the fusion is submitted and accepted", NULL);
        knishio_molecule_t *m = last_proposal(&s);
        char *address = derived_address("STK", POS_REGULAR);
        bool shape = m && m->atom_count == 4 && m->atoms[0]->isotope == KNISHIO_ISOTOPE_V
                     && m->atoms[1]->isotope == KNISHIO_ISOTOPE_V && m->atoms[2]->isotope == KNISHIO_ISOTOPE_F
                     && m->atoms[3]->isotope == KNISHIO_ISOTOPE_V;
        check(shape, "atoms are [V, V, F, V] (no I atom)", NULL);
        check(shape && strcmp(m->atoms[0]->wallet_address, address) == 0
                  && strcmp(m->atoms[0]->position, POS_REGULAR) == 0 && strcmp(m->atoms[0]->value, "-5") == 0,
              "atom 0 debits the whole stackable wallet from its own position", NULL);
        check(shape && m->atoms[2]->meta_id && strcmp(m->atoms[2]->meta_id, g_bundle) == 0,
              "the fused unit goes to the identity's own bundle", NULL);
        check(m && knishio_molecule_check(m, NULL) == KNISHIO_SUCCESS, "the sent molecule passes the SDK check", NULL);
        free(address);
        knishio_molecule_free_deep(m);
        knishio_request_tokens_result_free(res);

        size_t before = count_requests(&s, "\"query\"");
        const char *one[] = { "u1" };
        res = NULL;
        err = knishio_client_fuse_token(client, NULL, "STK", "FUSED", one, 1, &res);
        check(err == KNISHIO_ERROR_TRANSFER_BALANCE && !res, "a single-unit fusion is refused (TransferBalance)", NULL);
        check(count_requests(&s, "\"query\"") == before, "nothing is sent for the refused fusion", NULL);
        knishio_client_destroy(client);
    }
    server_stop(&s);
    cJSON_free(bal);
}

static void case_claim(void) {
    printf("claimShadowWallet without a batch id claims the first shadow wallet\n");
    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    char *regular_address = derived_address("CLM", POS_REGULAR);
    char list[1024];
    snprintf(list, sizeof(list),
             "{\"data\":{\"Wallet\":["
             "{\"address\":\"%s\",\"bundleHash\":\"%s\",\"tokenSlug\":\"CLM\",\"amount\":\"5\",\"position\":\"%s\","
             "\"batchId\":\"batch-regular\",\"isShadow\":false,\"tokenUnits\":[]},"
             "{\"address\":null,\"bundleHash\":\"%s\",\"tokenSlug\":\"CLM\",\"amount\":\"10\",\"position\":null,"
             "\"batchId\":\"batch-shadow-1\",\"isShadow\":true,\"tokenUnits\":[]}]}}",
             regular_address, g_bundle, POS_REGULAR, g_bundle);
    s.wallet_list_reply = list;
    knishio_client_t *client = logged_in_client(&s);
    check(client != NULL, "login through the stub", NULL);
    if (client) {
        knishio_claim_shadow_wallet_params_t params = { .token = "CLM", .batch_id = NULL };
        knishio_claim_shadow_wallet_result_t *res = NULL;
        knishio_error_t err = knishio_client_claim_shadow_wallet(client, &params, &res);
        check(err == KNISHIO_SUCCESS && res && res->success, "the claim is submitted and accepted", NULL);
        knishio_molecule_t *m = last_proposal(&s);
        const knishio_atom_t *c = (m && m->atom_count > 0) ? m->atoms[0] : NULL;
        const char *batch = meta_value(c, "walletBatchId");
        char detail[96];
        snprintf(detail, sizeof(detail), "walletBatchId=%s", batch ? batch : "(none)");
        check(c && c->isotope == KNISHIO_ISOTOPE_C && batch && strcmp(batch, "batch-shadow-1") == 0
                  && c->batch_id && strcmp(c->batch_id, "batch-shadow-1") == 0,
              "the claim carries the shadow wallet's batch id, not the regular wallet's", detail);
        knishio_molecule_free_deep(m);
        knishio_claim_shadow_wallet_result_free(res);

        s.wallet_list_reply = "{\"data\":{\"Wallet\":[]}}";
        size_t proposals = count_requests(&s, "ProposeMolecule");
        res = NULL;
        err = knishio_client_claim_shadow_wallet(client, &params, &res);
        check(err == KNISHIO_ERROR_WALLET_SHADOW && !res, "no shadow wallet -> WalletShadow, locally", NULL);
        check(count_requests(&s, "ProposeMolecule") == proposals, "nothing is proposed without a shadow wallet", NULL);
        knishio_client_destroy(client);
    }
    server_stop(&s);
    free(regular_address);
}

static void case_withdraw(void) {
    printf("withdrawBufferToken spends the buffer wallet and remainders to a fresh position\n");
    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    char *regular = balance_reply("BTOK", POS_REGULAR, "900", NULL, 0);
    char *buffer = balance_reply("BTOK", POS_BUFFER, "50", NULL, 0);
    s.balance_reply = regular;
    s.buffer_balance_reply = buffer;
    knishio_client_t *client = logged_in_client(&s);
    check(client != NULL, "login through the stub", NULL);
    if (client) {
        knishio_transfer_result_t *res = NULL;
        knishio_error_t err = knishio_client_withdraw_buffer_token(client, "BTOK", 20, g_bundle, &res);
        check(err == KNISHIO_SUCCESS && res && res->success, "the withdraw is submitted and accepted", NULL);
        check(count_requests(&s, "\"type\":\"buffer\"") >= 1, "the source comes from Balance(type: buffer)", NULL);
        knishio_molecule_t *m = last_proposal(&s);
        char *buffer_address = derived_address("BTOK", POS_BUFFER);
        bool shape = m && m->atom_count == 3 && m->atoms[0]->isotope == KNISHIO_ISOTOPE_B
                     && m->atoms[1]->isotope == KNISHIO_ISOTOPE_V && m->atoms[2]->isotope == KNISHIO_ISOTOPE_B;
        check(shape, "atoms are [B, V, B]", NULL);
        char detail[160];
        snprintf(detail, sizeof(detail), "source position=%.16s… value=%s", shape ? m->atoms[0]->position : "",
                 shape ? m->atoms[0]->value : "");
        check(shape && strcmp(m->atoms[0]->position, POS_BUFFER) == 0
                  && strcmp(m->atoms[0]->wallet_address, buffer_address) == 0
                  && strcmp(m->atoms[0]->value, "-50") == 0,
              "atom 0 debits the buffer wallet (-50), not the regular wallet", detail);
        check(shape && strcmp(m->atoms[2]->position, POS_BUFFER) != 0 && strcmp(m->atoms[2]->position, POS_REGULAR) != 0
                  && strcmp(m->atoms[2]->value, "30") == 0,
              "the remainder (+30) goes to a fresh position", NULL);
        check(m && knishio_molecule_check(m, NULL) == KNISHIO_SUCCESS, "the sent molecule passes the SDK check", NULL);
        free(buffer_address);
        knishio_molecule_free_deep(m);
        knishio_transfer_result_free(res);
        knishio_client_destroy(client);
    }
    server_stop(&s);
    cJSON_free(regular);
    cJSON_free(buffer);
}

static void case_presubmit_check(void) {
    printf("a built molecule that fails the SDK check is never sent; the raw path sends as given\n");
    stub_server_t s;
    if (!server_start(&s)) { check(false, "stub server starts", NULL); return; }
    knishio_client_t *client = logged_in_client(&s);
    check(client != NULL, "login through the stub", NULL);
    if (client) {
        /* A USER-signed meta molecule whose builder dropped the ContinuID I atom. */
        knishio_wallet_t *source = NULL, *remainder = NULL;
        knishio_wallet_create_simple(&source, g_secret, "USER", KNISHIO_FIXED_POSITION);
        knishio_wallet_create_simple(&remainder, g_secret, "USER", POS_REGULAR);
        knishio_molecule_t *m = NULL;
        knishio_molecule_create(&m, g_secret, g_bundle, source, remainder, "public", "V4");
        const char *keys[] = { "appRole" };
        const char *vals[] = { "admin" };
        knishio_molecule_init_meta(m, "WalletBundle", g_bundle, keys, vals, 1);
        knishio_atom_t *dropped = (m->atom_count == 2) ? m->atoms[1] : NULL;
        m->atom_count = 1;
        knishio_molecule_generate_hash(m);
        knishio_molecule_sign(m, g_bundle, false, true);

        size_t proposals = count_requests(&s, "ProposeMolecule");
        knishio_graphql_response_t *response = NULL;
        knishio_error_t err = knishio_client_submit_molecule(
            client, m, "CreateMeta",
            "mutation CreateMeta($molecule: MoleculeInput!) { ProposeMolecule(molecule: $molecule) { molecularHash status } }",
            &response);
        char detail[64];
        snprintf(detail, sizeof(detail), "err=%d", (int)err);
        check(err == KNISHIO_ERROR_ATOMS_MISSING && !response,
              "the checked submit refuses it with AtomsMissing", detail);
        check(count_requests(&s, "ProposeMolecule") == proposals, "no request is sent", NULL);

        char *hash = NULL;
        err = knishio_client_propose_molecule(client, m, &hash);
        check(err == KNISHIO_SUCCESS && count_requests(&s, "ProposeMolecule") == proposals + 1,
              "the raw propose path sends the same molecule", NULL);
        knishio_molecule_t *sent = last_proposal(&s);
        check(sent && sent->atom_count == 1 && sent->molecular_hash && m->molecular_hash
                  && strcmp(sent->molecular_hash, m->molecular_hash) == 0,
              "unchanged: one M atom, same molecular hash", NULL);
        knishio_molecule_free_deep(sent);
        free(hash);

        /* createMeta builds M + I and passes the check, so it is sent. */
        knishio_meta_t *meta[1] = { NULL };
        knishio_meta_create(&meta[0], "appRole", "admin");
        hash = NULL;
        err = knishio_client_create_meta(client, "WalletBundle", g_bundle, meta, 1, &hash);
        knishio_molecule_t *created = last_proposal(&s);
        check(err == KNISHIO_SUCCESS && created && created->atom_count == 2
                  && created->atoms[0]->isotope == KNISHIO_ISOTOPE_M && created->atoms[1]->isotope == KNISHIO_ISOTOPE_I,
              "createMeta sends M + I", NULL);
        knishio_molecule_free_deep(created);
        knishio_meta_free(meta[0]);
        free(hash);

        m->atom_count = dropped ? 2 : 1;
        knishio_molecule_free_deep(m);
        knishio_wallet_free(source);
        knishio_wallet_free(remainder);
        knishio_client_destroy(client);
    }
    server_stop(&s);
}

int main(void) {
    printf("Phase B client operations (SDK %s)\n", KNISHIO_VERSION_STRING);
    knishio_init();
    if (!knishio_generate_secret("phaseb-client-ops-test", 2048, &g_secret)
        || !knishio_generate_bundle_hash(g_secret, NULL, NULL, &g_bundle)) {
        fprintf(stderr, "phaseb_client_ops: cannot generate a secret\n");
        return 1;
    }
    case_replenish();
    case_fuse();
    case_claim();
    case_withdraw();
    case_presubmit_check();
    printf("%s: %d failure(s)\n", g_failures ? "FAIL" : "PASS", g_failures);
    knishio_free(g_secret);
    knishio_free(g_bundle);
    return g_failures ? 1 : 0;
}
