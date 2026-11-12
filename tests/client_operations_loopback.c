/**
 * @file client_operations_loopback.c
 * @brief Client operations against a GraphQL stub on 127.0.0.1 (no validator is contacted).
 *
 * Sections (argv[1]):
 *   o1  Every operation that used to cast knishio_client_t to knishio_graphql_client_t (a
 *       different struct) reaches the server through knishio_client_execute_graphql.
 *   s2  Variables and molecule JSON are sized from their content: a 5000-character token,
 *       a 4096-character meta value, a 100-atom molecule with large meta and a 3 MiB base64
 *       round trip stay intact.
 *   o2  create_identifier / create_policy / create_rule send real signed C+I / R+I / R+I
 *       molecules that parse back and pass knishio_molecule_check (no placeholder secret).
 *   o3  The guest fingerprint is random per process and stable within it; guest auth sends
 *       the guest wallet's ML-KEM public key (1568 bytes for a 1024 client) and binds the
 *       returned token, its expiry and the validator key to the client.
 *   fingerprint  (helper) prints knishio_get_fingerprint() for the o3 two-process check.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
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
#include "knishio/auth_token.h"
#include "knishio/client.h"
#include "knishio/client_ops.h"
#include "knishio/fingerprint.h"
#include "knishio/meta.h"
#include "knishio/molecule.h"
#include "knishio/wallet.h"
#include "knishio/operations/auth.h"
#include "knishio/operations/identity.h"
#include "knishio/operations/policy.h"
#include "knishio/operations/query.h"
#include "knishio/operations/token.h"
#include "knishio/operations/wallet.h"
#include "knishio/utils/encoding.h"
#include "knishio/utils/memory.h"

#define MAX_BODIES 256
#define GUEST_EXPIRES_AT 4102444800LL

static int g_failures = 0;

static void check(bool ok, const char* name) {
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) g_failures++;
}

/* ---------------------------------------------------------------- stub server */

typedef struct {
    int listen_fd;
    int port;
    volatile int stop;
    pthread_t thread;
    pthread_mutex_t lock;
    char* bodies[MAX_BODIES];
    size_t count;
    const char* server_pubkey;  /* advertised by AccessToken */
} stub_t;

static stub_t g_stub;

static bool read_all(int fd, char* buf, size_t len) {
    size_t have = 0;
    while (have < len) {
        struct pollfd p = { .fd = fd, .events = POLLIN };
        if (poll(&p, 1, 5000) != 1) return false;
        ssize_t n = read(fd, buf + have, len - have);
        if (n <= 0) return false;
        have += (size_t)n;
    }
    return true;
}

/* One HTTP request; *body_out is the malloc'd body. False on EOF, timeout or a bad request. */
static bool read_request(int fd, char** body_out) {
    char head[16384];
    size_t len = 0;
    char* end = NULL;
    while (!end) {
        if (len == sizeof head - 1) return false;
        struct pollfd p = { .fd = fd, .events = POLLIN };
        if (poll(&p, 1, 5000) != 1) return false;
        ssize_t n = read(fd, head + len, sizeof head - 1 - len);
        if (n <= 0) return false;
        len += (size_t)n;
        head[len] = '\0';
        end = strstr(head, "\r\n\r\n");
    }
    size_t content_length = 0;
    bool expect_continue = false;
    for (char* line = head; line && line < end; line = strstr(line, "\r\n")) {
        if (line != head) line += 2;
        if (strncasecmp(line, "Content-Length:", 15) == 0) content_length = strtoul(line + 15, NULL, 10);
        if (strncasecmp(line, "Expect: 100-continue", 20) == 0) expect_continue = true;
    }
    char* body = malloc(content_length + 1);
    if (!body) return false;
    size_t already = len - (size_t)(end + 4 - head);
    if (already > content_length) already = content_length;
    memcpy(body, end + 4, already);
    if (expect_continue && already < content_length) {
        static const char cont[] = "HTTP/1.1 100 Continue\r\n\r\n";
        if (write(fd, cont, sizeof cont - 1) < 0) { free(body); return false; }
    }
    if (!read_all(fd, body + already, content_length - already)) { free(body); return false; }
    body[content_length] = '\0';
    *body_out = body;
    return true;
}

static char* answer_for(const char* body) {
    cJSON* root = cJSON_CreateObject();
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    if (strstr(body, "AccessToken")) {
        cJSON* t = cJSON_AddObjectToObject(data, "AccessToken");
        cJSON_AddStringToObject(t, "token", "guest-token");
        cJSON_AddStringToObject(t, "pubkey", g_stub.server_pubkey ? g_stub.server_pubkey : "");
        cJSON_AddNumberToObject(t, "expiresAt", (double)GUEST_EXPIRES_AT);
        cJSON_AddBoolToObject(t, "encrypt", false);
    } else if (strstr(body, "ContinuId")) {
        cJSON_AddNullToObject(data, "ContinuId");
    } else if (strstr(body, "ProposeMolecule")) {
        cJSON* p = cJSON_AddObjectToObject(data, "ProposeMolecule");
        cJSON_AddStringToObject(p, "molecularHash", "stubhash");
        cJSON_AddStringToObject(p, "status", "accepted");
        cJSON_AddNullToObject(p, "reason");
        cJSON_AddNullToObject(p, "payload");
    }
    char* text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return text;
}

static void* serve(void* arg) {
    (void)arg;
    struct pollfd fds[17];
    int nconn = 0;
    fds[0].fd = g_stub.listen_fd;
    fds[0].events = POLLIN;
    while (!g_stub.stop) {
        if (poll(fds, (nfds_t)(1 + nconn), 100) <= 0) continue;
        if ((fds[0].revents & POLLIN) && nconn < 16) {
            int c = accept(g_stub.listen_fd, NULL, NULL);
            if (c >= 0) {
                fds[1 + nconn].fd = c;
                fds[1 + nconn].events = POLLIN;
                fds[1 + nconn].revents = 0;
                nconn++;
            }
        }
        for (int i = 1; i <= nconn; i++) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            char* body = NULL;
            if (!read_request(fds[i].fd, &body)) {
                close(fds[i].fd);
                fds[i] = fds[nconn];
                nconn--;
                i--;
                continue;
            }
            char* answer = answer_for(body);
            pthread_mutex_lock(&g_stub.lock);
            if (g_stub.count < MAX_BODIES) g_stub.bodies[g_stub.count++] = body; else free(body);
            pthread_mutex_unlock(&g_stub.lock);
            char header[256];
            int hl = snprintf(header, sizeof header,
                              "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n",
                              strlen(answer));
            if (write(fds[i].fd, header, (size_t)hl) < 0 || write(fds[i].fd, answer, strlen(answer)) < 0) {
                /* the client gave up; the next poll drops the connection */
            }
            cJSON_free(answer);
        }
    }
    for (int i = 1; i <= nconn; i++) close(fds[i].fd);
    return NULL;
}

static bool stub_start(void) {
    memset(&g_stub, 0, sizeof g_stub);
    pthread_mutex_init(&g_stub.lock, NULL);
    g_stub.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_stub.listen_fd < 0) return false;
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = 0 };
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addr_len = sizeof addr;
    if (bind(g_stub.listen_fd, (struct sockaddr*)&addr, sizeof addr) != 0
        || listen(g_stub.listen_fd, 16) != 0
        || getsockname(g_stub.listen_fd, (struct sockaddr*)&addr, &addr_len) != 0) {
        return false;
    }
    g_stub.port = ntohs(addr.sin_port);
    return pthread_create(&g_stub.thread, NULL, serve, NULL) == 0;
}

static void stub_stop(void) {
    g_stub.stop = 1;
    pthread_join(g_stub.thread, NULL);
    close(g_stub.listen_fd);
    for (size_t i = 0; i < g_stub.count; i++) free(g_stub.bodies[i]);
    pthread_mutex_destroy(&g_stub.lock);
}

/* The most recent request body containing `needle` (borrowed), or NULL. */
static const char* last_body_with(const char* needle) {
    const char* found = NULL;
    pthread_mutex_lock(&g_stub.lock);
    for (size_t i = 0; i < g_stub.count; i++) {
        if (strstr(g_stub.bodies[i], needle)) found = g_stub.bodies[i];
    }
    pthread_mutex_unlock(&g_stub.lock);
    return found;
}

/* A client of the stub; with_token gives it an auth token (mutations require one). */
static knishio_client_t* stub_client(const char* secret, bool with_token) {
    char url[64];
    snprintf(url, sizeof url, "http://127.0.0.1:%d/graphql", g_stub.port);
    knishio_client_config_t config = { 0 };
    config.uri = url;
    config.cell_slug = "public";
    knishio_client_t* client = NULL;
    if (knishio_client_create(&client, &config) != KNISHIO_SUCCESS) return NULL;
    if (secret && knishio_client_set_secret(client, secret) != KNISHIO_SUCCESS) {
        knishio_client_destroy(client);
        return NULL;
    }
    if (with_token) {
        knishio_auth_token_config_t token_config = { .token = "stub-token", .expires_at = GUEST_EXPIRES_AT,
                                                     .encrypt = false, .pubkey = "" };
        knishio_auth_token_t* token = NULL;
        if (knishio_auth_token_create(&token, &token_config) != KNISHIO_SUCCESS
            || knishio_client_set_auth_token(client, token) != KNISHIO_SUCCESS) {
            knishio_client_destroy(client);
            return NULL;
        }
    }
    return client;
}

/* The molecule a captured ProposeMolecule request carried (variables.molecule), parsed. */
static knishio_molecule_t* captured_molecule(const char* body) {
    if (!body) return NULL;
    cJSON* root = cJSON_Parse(body);
    cJSON* vars = root ? cJSON_GetObjectItem(root, "variables") : NULL;
    cJSON* mol = vars ? cJSON_GetObjectItem(vars, "molecule") : NULL;
    char* text = mol ? cJSON_PrintUnformatted(mol) : NULL;
    knishio_molecule_t* molecule = NULL;
    if (text) knishio_molecule_from_json(text, &molecule);
    cJSON_free(text);
    cJSON_Delete(root);
    return molecule;
}

static const char* atom_meta(const knishio_atom_t* atom, const char* key) {
    for (size_t i = 0; i < atom->meta_count; i++) {
        if (atom->meta[i] && atom->meta[i]->key && strcmp(atom->meta[i]->key, key) == 0) return atom->meta[i]->value;
    }
    return NULL;
}

/* ---------------------------------------------------------------- o1 */

static void section_o1(void) {
    printf("o1: operations reach the server through the client transport\n");
    knishio_client_t* client = stub_client(NULL, true);
    check(client != NULL, "client created");
    if (!client) return;

    char* out = NULL;
    check(knishio_client_query_meta(client, "profile", "p1", &out) == KNISHIO_SUCCESS && out
          && last_body_with("QueryMeta"), "query_meta");
    knishio_free(out);
    out = NULL;
    check(knishio_client_execute_query(client, "query Q { __typename }", NULL, &out) == KNISHIO_SUCCESS
          && last_body_with("query Q"), "execute_query");
    knishio_free(out);
    out = NULL;
    check(knishio_client_execute_mutation(client, "mutation M { __typename }", "{}", &out) == KNISHIO_SUCCESS
          && last_body_with("mutation M"), "execute_mutation");
    knishio_free(out);
    out = NULL;
    check(knishio_client_query_batch(client, "batch-1", &out) == KNISHIO_SUCCESS
          && last_body_with("QueryBatch("), "query_batch");
    knishio_free(out);
    out = NULL;
    check(knishio_client_query_batch_history(client, "batch-1", &out) == KNISHIO_SUCCESS
          && last_body_with("QueryBatchHistory"), "query_batch_history");
    knishio_free(out);

    knishio_query_atom_params_t qa = { 0 };
    qa.token_slug = "TEST";
    qa.latest = true;
    knishio_query_atom_result_t* qa_result = NULL;
    check(knishio_client_query_atom(client, &qa, &qa_result) == KNISHIO_SUCCESS
          && last_body_with("QueryAtom"), "query_atom");
    knishio_query_atom_result_free(qa_result);

    knishio_query_active_session_params_t qs = { .bundle_hash = "b", .meta_type = "t", .meta_id = "i" };
    knishio_query_active_session_result_t* qs_result = NULL;
    check(knishio_client_query_active_session(client, &qs, &qs_result) == KNISHIO_SUCCESS
          && last_body_with("QueryActiveSession"), "query_active_session");
    knishio_query_active_session_result_free(qs_result);

    knishio_query_policy_params_t qp = { .meta_type = "profile", .meta_id = "p1" };
    knishio_query_policy_result_t* qp_result = NULL;
    check(knishio_client_query_policy(client, &qp, &qp_result) == KNISHIO_SUCCESS
          && last_body_with("QueryPolicy"), "query_policy");
    knishio_query_policy_result_free(qp_result);

    knishio_wallet_list_result_t* bundle_result = NULL;
    check(knishio_client_query_bundle(client, "b", &bundle_result) == KNISHIO_SUCCESS
          && last_body_with("QueryWalletBundle"), "query_bundle");
    knishio_wallet_list_result_free(bundle_result);

    knishio_active_session_params_t as = { .bundle = "b", .meta_type = "t", .meta_id = "i" };
    knishio_active_session_result_t* as_result = NULL;
    check(knishio_client_active_session(client, &as, &as_result) == KNISHIO_SUCCESS
          && last_body_with("ActiveSession("), "active_session");
    knishio_active_session_result_free(as_result);

    knishio_client_destroy(client);
}

/* ---------------------------------------------------------------- s2 */

static void section_s2(void) {
    printf("s2: variables and molecule JSON are sized from their content\n");
    char* long_token = malloc(5001);
    memset(long_token, 'T', 5000);
    long_token[5000] = '\0';

    knishio_client_t* client = stub_client(NULL, true);
    check(client != NULL, "client created");
    if (client) {
        const char* units[] = { "unit-1", "unit\"2" };
        knishio_request_tokens_params_t rt = { 0 };
        rt.token = long_token;
        rt.requested_amount = "10";
        rt.requested_units = units;
        rt.unit_count = 2;
        knishio_request_tokens_result_t* rt_result = NULL;
        knishio_error_t e = knishio_client_request_tokens(client, &rt, &rt_result);
        const char* body = last_body_with("RequestTokens");
        cJSON* root = body ? cJSON_Parse(body) : NULL;
        cJSON* vars = root ? cJSON_GetObjectItem(root, "variables") : NULL;
        cJSON* tok = vars ? cJSON_GetObjectItem(vars, "token") : NULL;
        cJSON* us = vars ? cJSON_GetObjectItem(vars, "units") : NULL;
        check(e == KNISHIO_SUCCESS && cJSON_IsString(tok) && strcmp(tok->valuestring, long_token) == 0
              && cJSON_GetArraySize(us) == 2
              && strcmp(cJSON_GetArrayItem(us, 1)->valuestring, "unit\"2") == 0,
              "request_tokens: 5000-character token and a quoted unit arrive intact");
        cJSON_Delete(root);
        knishio_request_tokens_result_free(rt_result);

        knishio_query_atom_params_t qa = { 0 };
        qa.token_slug = long_token;
        qa.limit = 15;
        knishio_query_atom_result_t* qa_result = NULL;
        e = knishio_client_query_atom(client, &qa, &qa_result);
        body = last_body_with("QueryAtom");
        root = body ? cJSON_Parse(body) : NULL;
        vars = root ? cJSON_GetObjectItem(root, "variables") : NULL;
        tok = vars ? cJSON_GetObjectItem(vars, "tokenSlug") : NULL;
        check(e == KNISHIO_SUCCESS && cJSON_IsString(tok) && strcmp(tok->valuestring, long_token) == 0
              && cJSON_GetNumberValue(cJSON_GetObjectItem(vars, "limit")) == 15,
              "query_atom: 5000-character tokenSlug arrives intact");
        cJSON_Delete(root);
        knishio_query_atom_result_free(qa_result);

        char* out = NULL;
        e = knishio_client_query_meta(client, long_token, "id\"1", &out);
        body = last_body_with("QueryMeta");
        root = body ? cJSON_Parse(body) : NULL;
        vars = root ? cJSON_GetObjectItem(root, "variables") : NULL;
        tok = vars ? cJSON_GetObjectItem(vars, "metaType") : NULL;
        check(e == KNISHIO_SUCCESS && cJSON_IsString(tok) && strcmp(tok->valuestring, long_token) == 0
              && strcmp(cJSON_GetObjectItem(vars, "metaId")->valuestring, "id\"1") == 0,
              "query_meta: long and quoted variables arrive intact");
        cJSON_Delete(root);
        knishio_free(out);
        knishio_client_destroy(client);
    }

    /* meta array JSON (C meta values are capped at KNISHIO_MAX_META_VALUE_LENGTH) */
    char* long_value = malloc(KNISHIO_MAX_META_VALUE_LENGTH + 1);
    memset(long_value, 'v', KNISHIO_MAX_META_VALUE_LENGTH);
    long_value[KNISHIO_MAX_META_VALUE_LENGTH] = '\0';
    {
        knishio_meta_array_t* array = NULL;
        knishio_meta_array_create(&array);
        knishio_meta_t* m = NULL;
        knishio_meta_create(&m, "name\"x", long_value);
        knishio_meta_array_add(array, m);
        char* json = NULL;
        knishio_error_t e = knishio_meta_array_to_json(array, &json);
        cJSON* parsed = json ? cJSON_Parse(json) : NULL;
        cJSON* first = cJSON_GetArrayItem(parsed, 0);
        check(e == KNISHIO_SUCCESS && first
              && strcmp(cJSON_GetObjectItem(first, "key")->valuestring, "name\"x") == 0
              && strcmp(cJSON_GetObjectItem(first, "value")->valuestring, long_value) == 0,
              "meta_array_to_json: a 4096-character value and a quoted key stay intact");
        cJSON_Delete(parsed);
        knishio_free(json);
        knishio_meta_array_free(array, true);
    }

    /* 100-atom molecule with large meta */
    {
        knishio_molecule_t* molecule = NULL;
        knishio_molecule_create(&molecule, NULL, NULL, NULL, NULL, "public", NULL);
        bool built = molecule != NULL;
        for (size_t i = 0; built && i < 100; i++) {
            knishio_atom_t* atom = NULL;
            built = knishio_atom_create(&atom,
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                KNISHIO_ISOTOPE_M, "USER", NULL, NULL) == KNISHIO_SUCCESS;
            if (built) {
                static const char* const keys[] = { "blob1", "blob2", "blob3" };
                atom->meta = malloc(3 * sizeof(knishio_meta_t*));
                atom->meta_count = 3;
                for (size_t k = 0; k < 3; k++) {
                    atom->meta[k] = NULL;
                    knishio_meta_create(&atom->meta[k], keys[k], long_value);
                }
                atom->index = (int)i;
                built = knishio_molecule_add_atom(molecule, atom) == KNISHIO_SUCCESS;
            }
        }
        char* json = NULL;
        knishio_error_t e = built ? knishio_molecule_to_json(molecule, &json) : KNISHIO_ERROR_MEMORY;
        cJSON* parsed = json ? cJSON_Parse(json) : NULL;
        cJSON* atoms = parsed ? cJSON_GetObjectItem(parsed, "atoms") : NULL;
        bool intact = cJSON_GetArraySize(atoms) == 100;
        cJSON* atom = NULL;
        cJSON_ArrayForEach(atom, atoms) {
            cJSON* metas = cJSON_GetObjectItem(atom, "meta");
            intact = intact && cJSON_GetArraySize(metas) == 3;
            cJSON* meta = NULL;
            cJSON_ArrayForEach(meta, metas) {
                cJSON* value = cJSON_GetObjectItem(meta, "value");
                intact = intact && cJSON_IsString(value) && strcmp(value->valuestring, long_value) == 0;
            }
        }
        check(e == KNISHIO_SUCCESS && intact,
              "molecule_to_json: 100 atoms with three 4096-character metas each parse back whole");
        cJSON_Delete(parsed);
        knishio_free(json);
        knishio_molecule_free_deep(molecule);
    }

    /* 3 MiB base64 round trip */
    {
        size_t n = 3u * 1024u * 1024u;
        unsigned char* data = malloc(n);
        for (size_t i = 0; i < n; i++) data[i] = (unsigned char)(i * 131u + 7u);
        char* b64 = NULL;
        unsigned char* back = NULL;
        size_t back_len = 0;
        bool ok = knishio_base64_encode(data, n, &b64)
            && knishio_base64_decode(b64, &back, &back_len)
            && back_len == n && memcmp(back, data, n) == 0;
        check(ok, "base64: 3 MiB round trip");
        free(data);
        free(b64);
        free(back);
    }
    free(long_value);
    free(long_token);
}

/* ---------------------------------------------------------------- o2 */

static void expect_molecule(const char* label, const char* needle, const char* first_isotope_meta_key,
                            knishio_isotope_t first_isotope) {
    char name[160];
    const char* body = last_body_with(needle);
    knishio_molecule_t* m = captured_molecule(body);
    snprintf(name, sizeof name, "%s: request carries a parsable molecule", label);
    check(m != NULL, name);
    if (!m) return;
    snprintf(name, sizeof name, "%s: no placeholder secret or hash in the request", label);
    check(strstr(body, "placeholder") == NULL && strstr(body, "\"secret\"") == NULL, name);
    snprintf(name, sizeof name, "%s: two atoms, the operation atom first and the ContinuID I-atom second", label);
    bool shape = m->atom_count == 2 && m->atoms[0]->isotope == first_isotope
        && m->atoms[1]->isotope == KNISHIO_ISOTOPE_I && atom_meta(m->atoms[0], first_isotope_meta_key) != NULL;
    check(shape, name);
    snprintf(name, sizeof name, "%s: knishio_molecule_check passes on the sent molecule", label);
    check(knishio_molecule_check(m, NULL) == KNISHIO_SUCCESS, name);
    knishio_molecule_free_deep(m);
}

static void section_o2(void) {
    printf("o2: identifier, policy and rule molecules are real\n");
    char* secret = NULL;
    knishio_generate_secret("fleet-audit-2026-11-o2", 2048, &secret);
    knishio_client_t* client = stub_client(secret, true);
    check(client != NULL, "client created");
    if (!client) { knishio_free(secret); return; }

    knishio_create_identifier_params_t ip = { .type = "email", .contact = "  alice@example.com ", .code = "1234" };
    knishio_create_identifier_result_t* ir = NULL;
    knishio_error_t e = knishio_client_create_identifier(client, &ip, &ir);
    check(e == KNISHIO_SUCCESS && ir && ir->success, "create_identifier returns success");
    knishio_create_identifier_result_free(ir);
    expect_molecule("identifier", "\"identifier\"", "hash", KNISHIO_ISOTOPE_C);
    {
        const char* body = last_body_with("\"identifier\"");
        knishio_molecule_t* m = captured_molecule(body);
        char* expected = NULL;
        knishio_generate_bundle_hash("alice@example.com", NULL, NULL, &expected);
        check(m && m->atom_count > 0 && expected && atom_meta(m->atoms[0], "hash")
              && strcmp(atom_meta(m->atoms[0], "hash"), expected) == 0
              && body && strstr(body, "alice@example.com") == NULL,
              "identifier: meta hash = SHAKE256(contact.trim()), contact not sent");
        knishio_free(expected);
        knishio_molecule_free_deep(m);
    }

    knishio_create_policy_params_t pp = { .meta_type = "profile", .meta_id = "p1",
                                          .policy_json = "{\"write\":{\"name\":[\"self\"]},\"read\":{\"name\":[\"all\"]}}" };
    knishio_create_policy_result_t* pr = NULL;
    e = knishio_client_create_policy(client, &pp, &pr);
    check(e == KNISHIO_SUCCESS && pr && pr->success, "create_policy returns success");
    knishio_create_policy_result_free(pr);
    expect_molecule("policy", "CreatePolicy", "policy", KNISHIO_ISOTOPE_R);

    knishio_create_rule_params_t rp = { 0 };
    rp.meta_type = "profile";
    rp.meta_id = "p1";
    rp.rule_json = "[{\"condition\":[{\"key\":\"amount\",\"value\":\"100\",\"comparison\":\">=\"}],"
                   "\"callback\":[{\"action\":\"reject\"}]}]";
    rp.policy_json = "{\"read\":{\"name\":[\"all\"]}}";
    knishio_create_rule_result_t* rr = NULL;
    e = knishio_client_create_rule(client, &rp, &rr);
    check(e == KNISHIO_SUCCESS && rr && rr->success, "create_rule returns success");
    knishio_create_rule_result_free(rr);
    expect_molecule("rule", "CreateRule", "rule", KNISHIO_ISOTOPE_R);

    knishio_client_destroy(client);
    knishio_free(secret);
}

/* ---------------------------------------------------------------- o3 */

static bool is_hex64(const char* s) {
    if (!s || strlen(s) != 64) return false;
    for (const char* p = s; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) return false;
    }
    return true;
}

static bool fingerprint_of_child(const char* self, char out[128]) {
    char cmd[4096];
    snprintf(cmd, sizeof cmd, "'%s' fingerprint", self);
    FILE* p = popen(cmd, "r");
    if (!p) return false;
    bool ok = fgets(out, 128, p) != NULL;
    pclose(p);
    if (ok) out[strcspn(out, "\r\n")] = '\0';
    return ok;
}

static void section_o3(const char* self) {
    printf("o3: guest fingerprint and guest auth\n");
    char* f1 = NULL;
    char* f2 = NULL;
    knishio_get_fingerprint(&f1);
    knishio_get_fingerprint(&f2);
    check(is_hex64(f1), "fingerprint is 64 lowercase hex characters");
    check(f1 && f2 && strcmp(f1, f2) == 0, "fingerprint is stable within one process");
    char c1[128] = { 0 };
    char c2[128] = { 0 };
    check(fingerprint_of_child(self, c1) && fingerprint_of_child(self, c2)
          && is_hex64(c1) && is_hex64(c2) && strcmp(c1, c2) != 0 && f1 && strcmp(c1, f1) != 0,
          "two other processes produce two different fingerprints");
    free(f1);
    free(f2);

    /* The validator's advertised key: any ML-KEM-1024 public key. */
    char* server_secret = NULL;
    knishio_generate_secret("fleet-audit-2026-11-server", 2048, &server_secret);
    knishio_wallet_t* server = NULL;
    knishio_wallet_create_with_mlkem_param(&server, server_secret, "USER",
        "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", 2, KNISHIO_MLKEM_1024);
    g_stub.server_pubkey = server ? server->pubkey : NULL;

    knishio_client_t* client = stub_client(NULL, false);
    knishio_request_guest_auth_token_params_t gp = { .cell_slug = "public", .encrypt = false };
    knishio_request_guest_auth_token_result_t* gr = NULL;
    knishio_error_t e = client ? knishio_client_request_guest_auth_token(client, &gp, &gr) : KNISHIO_ERROR_MEMORY;
    check(e == KNISHIO_SUCCESS && gr && gr->success && gr->token && strcmp(gr->token, "guest-token") == 0,
          "guest auth returns the token");

    const char* body = last_body_with("AccessToken");
    cJSON* root = body ? cJSON_Parse(body) : NULL;
    cJSON* vars = root ? cJSON_GetObjectItem(root, "variables") : NULL;
    cJSON* pubkey = vars ? cJSON_GetObjectItem(vars, "pubkey") : NULL;
    unsigned char* raw = NULL;
    size_t raw_len = 0;
    bool decoded = cJSON_IsString(pubkey) && knishio_base64_decode(pubkey->valuestring, &raw, &raw_len);
    check(decoded && raw_len == 1568, "guest auth sends a 1568-byte ML-KEM-1024 public key");
    check(cJSON_IsString(cJSON_GetObjectItem(vars, "cellSlug"))
          && cJSON_IsBool(cJSON_GetObjectItem(vars, "encrypt")), "guest auth sends cellSlug and encrypt");
    free(raw);
    cJSON_Delete(root);

    knishio_auth_token_t* bound = client ? knishio_client_get_auth_token(client) : NULL;
    check(bound && bound->token && strcmp(bound->token, "guest-token") == 0, "the client holds the guest token");
    check(bound && bound->expires_at == GUEST_EXPIRES_AT, "the token's expires_at comes from the response");
    knishio_request_guest_auth_token_result_free(gr);

    /* With the validator key bound, an encrypted client sends its next query as CipherHash. */
    if (client) {
        knishio_client_set_encryption(client, true);
        char* out = NULL;
        knishio_client_query_meta(client, "profile", "p1", &out);
        check(last_body_with("CipherHash") != NULL, "the validator key is bound: the next query goes out as CipherHash");
        knishio_free(out);
    }

    knishio_client_destroy(client);
    knishio_wallet_free(server);
    knishio_free(server_secret);
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);  /* keep the progress lines when a sanitizer aborts */
    const char* section = argc > 1 ? argv[1] : "";
    if (strcmp(section, "fingerprint") == 0) {
        char* f = NULL;
        if (knishio_get_fingerprint(&f) != KNISHIO_SUCCESS || !f) return 1;
        printf("%s\n", f);
        free(f);
        return 0;
    }
    if (!stub_start()) {
        printf("FAIL: loopback stub did not start\n");
        return 1;
    }
    if (strcmp(section, "o1") == 0) section_o1();
    else if (strcmp(section, "s2") == 0) section_s2();
    else if (strcmp(section, "o2") == 0) section_o2();
    else if (strcmp(section, "o3") == 0) section_o3(argv[0]);
    else { printf("usage: %s o1|s2|o2|o3|fingerprint\n", argv[0]); g_failures = 1; }
    stub_stop();
    printf("%s (%d failure%s)\n", g_failures ? "FAIL" : "PASS", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
