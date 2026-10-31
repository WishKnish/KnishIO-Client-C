/*
 * The HTTP client reuses its TCP connection across requests.
 *
 * Up to 1.6.0 every request created and destroyed its own libcurl handle, and with it a TCP
 * connection. A client submitting at a sustained rate left so many sockets in TIME_WAIT that it ran
 * out of local ports ("Cannot assign requested address"). These cases count the connections a
 * keep-alive server on 127.0.0.1 actually accepts.
 */
#include "unity.h"
#include "knishio/http.h"
#include "knishio/knishio.h"
#include "knishio/graphql.h"

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

#define SERVER_WAIT_MS 5000

typedef struct {
    int listen_fd;
    int port;
    int expected_requests;
    bool close_after_response; /* drop the connection after each answer, as an idle timeout does */
    int accepts;
    int requests;
    pthread_t thread;
} test_server_t;

static bool wait_readable(int fd) {
    struct pollfd p = {.fd = fd, .events = POLLIN};
    return poll(&p, 1, SERVER_WAIT_MS) == 1;
}

/* Reads one HTTP request (headers plus Content-Length body). Returns false on EOF or timeout. */
static bool read_request(int fd) {
    char buf[65536];
    size_t len = 0;
    char* end = NULL;
    while (!end) {
        if (len == sizeof buf - 1 || !wait_readable(fd)) return false;
        ssize_t n = read(fd, buf + len, sizeof buf - 1 - len);
        if (n <= 0) return false;
        len += (size_t)n;
        buf[len] = '\0';
        end = strstr(buf, "\r\n\r\n");
    }
    size_t body = 0;
    for (char* line = buf; line && line < end; line = strstr(line, "\r\n")) {
        if (line != buf) line += 2;
        if (strncasecmp(line, "Content-Length:", 15) == 0) body = strtoul(line + 15, NULL, 10);
    }
    size_t have = len - (size_t)(end + 4 - buf);
    while (have < body) {
        if (!wait_readable(fd)) return false;
        ssize_t n = read(fd, buf, sizeof buf);
        if (n <= 0) return false;
        have += (size_t)n;
    }
    return true;
}

static void* serve(void* arg) {
    test_server_t* s = arg;
    static const char answer[] =
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 11\r\n\r\n{\"data\":{}}";
    int conn = -1;
    while (s->requests < s->expected_requests) {
        if (conn < 0) {
            if (!wait_readable(s->listen_fd)) break;
            conn = accept(s->listen_fd, NULL, NULL);
            if (conn < 0) break;
            s->accepts++;
        }
        if (!read_request(conn)) {
            close(conn);
            conn = -1;
            continue;
        }
        if (write(conn, answer, sizeof answer - 1) != (ssize_t)(sizeof answer - 1)) break;
        s->requests++;
        if (s->close_after_response) {
            close(conn);
            conn = -1;
        }
    }
    if (conn >= 0) close(conn);
    return NULL;
}

static void server_start(test_server_t* s, int expected_requests, bool close_after_response) {
    memset(s, 0, sizeof *s);
    s->expected_requests = expected_requests;
    s->close_after_response = close_after_response;
    s->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT_TRUE(s->listen_fd >= 0);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = 0};
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    TEST_ASSERT_EQUAL_INT(0, bind(s->listen_fd, (struct sockaddr*)&addr, sizeof addr));
    TEST_ASSERT_EQUAL_INT(0, listen(s->listen_fd, 8));
    socklen_t addr_len = sizeof addr;
    TEST_ASSERT_EQUAL_INT(0, getsockname(s->listen_fd, (struct sockaddr*)&addr, &addr_len));
    s->port = ntohs(addr.sin_port);
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&s->thread, NULL, serve, s));
}

static void server_join(test_server_t* s) {
    pthread_join(s->thread, NULL);
    close(s->listen_fd);
}

static knishio_http_client_t* client_for(const test_server_t* s) {
    char url[64];
    snprintf(url, sizeof url, "http://127.0.0.1:%d/graphql", s->port);
    knishio_http_client_t* client = NULL;
    TEST_ASSERT_EQUAL_INT(KNISHIO_SUCCESS, knishio_http_client_create(&client, url));
    return client;
}

static void post_ok(knishio_http_client_t* client) {
    knishio_http_response_t* response = NULL;
    TEST_ASSERT_EQUAL_INT(KNISHIO_SUCCESS,
                          knishio_http_post_graphql(client, "query { __typename }", NULL, &response));
    TEST_ASSERT_EQUAL_INT(200, knishio_http_response_get_status(response));
    TEST_ASSERT_EQUAL_STRING("{\"data\":{}}", knishio_http_response_get_data(response));
    knishio_http_response_free(response);
}

static void test_sequential_posts_share_one_connection(void) {
    test_server_t server;
    server_start(&server, 5, false);
    knishio_http_client_t* client = client_for(&server);
    for (int i = 0; i < 5; i++) post_ok(client);
    knishio_http_client_free(client);
    server_join(&server);
    TEST_ASSERT_EQUAL_INT(5, server.requests);
    TEST_ASSERT_EQUAL_INT(1, server.accepts);
}

static void test_a_connection_the_server_closed_is_not_reused(void) {
    test_server_t server;
    server_start(&server, 2, true);
    knishio_http_client_t* client = client_for(&server);
    post_ok(client);
    usleep(200 * 1000); /* the server's close (FIN) has arrived before the next request */
    post_ok(client);
    knishio_http_client_free(client);
    server_join(&server);
    TEST_ASSERT_EQUAL_INT(2, server.requests);
    TEST_ASSERT_EQUAL_INT(2, server.accepts);
}

/* 1.6.1 pooled handles in knishio_http_client_t, but knishio_client_execute_graphql still created
 * and freed a fresh knishio_graphql_client_t (and its own http_client) on every operation. */
static void test_client_execute_graphql_reuses_one_connection(void) {
    test_server_t server;
    server_start(&server, 5, false);
    char url[64];
    snprintf(url, sizeof url, "http://127.0.0.1:%d/graphql", server.port);
    knishio_client_config_t cfg = {.uri = url, .cell_slug = "test"};
    knishio_client_t* client = NULL;
    TEST_ASSERT_EQUAL_INT(KNISHIO_SUCCESS, knishio_client_create(&client, &cfg));
    knishio_graphql_operation_t op = {
        .name = "ContinuId",
        .query = "query { __typename }",
        .variables_json = NULL,
        .requires_auth = false,
        .is_mutation = false
    };
    for (int i = 0; i < 5; i++) {
        knishio_graphql_response_t* response = NULL;
        TEST_ASSERT_EQUAL_INT(KNISHIO_SUCCESS, knishio_client_execute_graphql(client, &op, &response));
        TEST_ASSERT_NOT_NULL(response);
        TEST_ASSERT_EQUAL_INT(200, (int)response->status_code);
        knishio_graphql_response_free(response);
    }
    knishio_client_destroy(client);
    server_join(&server);
    TEST_ASSERT_EQUAL_INT(5, server.requests);
    TEST_ASSERT_EQUAL_INT(1, server.accepts);
}

void test_http_connection_reuse_suite(void) {
    RUN_TEST(test_sequential_posts_share_one_connection);
    RUN_TEST(test_a_connection_the_server_closed_is_not_reused);
    RUN_TEST(test_client_execute_graphql_reuses_one_connection);
}
