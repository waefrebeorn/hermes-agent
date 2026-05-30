/*
 * test_telegram_network.c — Tests for telegram_network helpers.
 *
 * Tests: telegram_resolve_system_dns, telegram_parse_doh_response.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_telegram_network.c \
 *       src/gateway/platforms/telegram_network.c \
 *       lib/libjson/json.c \
 *       -o /tmp/hermes_test_telegram_network -lm
 *
 * Run:
 *   /tmp/hermes_test_telegram_network
 */

#include "hermes_telegram_network.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 *  telegram_resolve_system_dns tests
 * ================================================================ */

static void test_resolve_null_hostname(void) {
    TEST("resolve null hostname returns false");
    char **ips = NULL;
    size_t cnt = 999;
    bool ok = telegram_resolve_system_dns(NULL, &ips, &cnt);
    if (!ok && ips == NULL && cnt == 0) { PASS(); return; }
    FAIL("expected false, NULL, 0");
}

static void test_resolve_empty_hostname(void) {
    TEST("resolve empty hostname returns false");
    char **ips = NULL;
    size_t cnt = 999;
    bool ok = telegram_resolve_system_dns("", &ips, &cnt);
    if (!ok && ips == NULL && cnt == 0) { PASS(); return; }
    FAIL("expected false, NULL, 0");
}

static void test_resolve_null_out_params(void) {
    TEST("resolve null out params returns false");
    bool ok = telegram_resolve_system_dns("localhost", NULL, NULL);
    if (!ok) { PASS(); return; }
    FAIL("expected false");
}

static void test_resolve_localhost(void) {
    TEST("resolve 'localhost' returns 127.0.0.1");
    char **ips = NULL;
    size_t cnt = 0;
    bool ok = telegram_resolve_system_dns("localhost", &ips, &cnt);
    if (!ok) { FAIL("returned false"); return; }
    if (cnt == 0) { FAIL("no results"); goto cleanup; }
    bool found_loopback = false;
    for (size_t i = 0; i < cnt; i++) {
        if (strcmp(ips[i], "127.0.0.1") == 0) { found_loopback = true; break; }
    }
    if (found_loopback) { PASS(); goto cleanup; }
    FAIL("127.0.0.1 not found among results");
cleanup:
    telegram_free_ip_list(ips, cnt);
}

static void test_resolve_nonexistent(void) {
    TEST("resolve nonexistent host returns empty (not error)");
    char **ips = NULL;
    size_t cnt = 999;
    bool ok = telegram_resolve_system_dns("zzz-nonexistent-host-xyz.test.", &ips, &cnt);
    if (ok && cnt == 0 && ips == NULL) { PASS(); return; }
    telegram_free_ip_list(ips, cnt);
    if (ok && cnt > 0) { PASS(); return; }
    FAIL("expected true with empty results");
}

static void test_resolve_ipv4_only(void) {
    TEST("resolve returns only IPv4 addresses");
    char **ips = NULL;
    size_t cnt = 0;
    bool ok = telegram_resolve_system_dns("localhost", &ips, &cnt);
    if (!ok || cnt == 0) { FAIL("no results"); telegram_free_ip_list(ips, cnt); return; }
    for (size_t i = 0; i < cnt; i++) {
        if (strchr(ips[i], ':')) {
            FAIL("Got IPv6 address");
            telegram_free_ip_list(ips, cnt);
            return;
        }
    }
    PASS();
    telegram_free_ip_list(ips, cnt);
}

static void test_free_null(void) {
    TEST("free NULL list is safe");
    telegram_free_ip_list(NULL, 0);
    telegram_free_ip_list(NULL, 10);
    PASS();
}

/* ================================================================
 *  telegram_parse_doh_response tests
 * ================================================================ */

static void test_parse_doh_null(void) {
    TEST("parse null response returns false");
    char **ips = NULL;
    size_t cnt = 999;
    bool ok = telegram_parse_doh_response(NULL, &ips, &cnt);
    if (!ok && ips == NULL && cnt == 0) { PASS(); return; }
    FAIL("expected false");
}

static void test_parse_doh_empty(void) {
    TEST("parse empty response returns false");
    char **ips = NULL;
    size_t cnt = 999;
    bool ok = telegram_parse_doh_response("", &ips, &cnt);
    if (!ok && ips == NULL && cnt == 0) { PASS(); return; }
    FAIL("expected false");
}

static void test_parse_doh_invalid_json(void) {
    TEST("parse invalid JSON returns false");
    char **ips = NULL;
    size_t cnt = 999;
    bool ok = telegram_parse_doh_response("not json", &ips, &cnt);
    if (!ok && ips == NULL && cnt == 0) { PASS(); return; }
    FAIL("expected false");
}

static void test_parse_doh_no_answer(void) {
    TEST("parse response with no Answer key returns empty");
    char **ips = NULL;
    size_t cnt = 999;
    bool ok = telegram_parse_doh_response("{\"status\":0}", &ips, &cnt);
    if (ok && cnt == 0 && ips == NULL) { PASS(); return; }
    FAIL("expected true, empty");
}

static void test_parse_doh_empty_answer(void) {
    TEST("parse response with empty Answer array returns empty");
    char **ips = NULL;
    size_t cnt = 999;
    bool ok = telegram_parse_doh_response("{\"Answer\":[]}", &ips, &cnt);
    if (ok && cnt == 0 && ips == NULL) { PASS(); return; }
    FAIL("expected true, empty");
}

static void test_parse_doh_single_a(void) {
    TEST("parse single A record");
    char **ips = NULL;
    size_t cnt = 0;
    bool ok = telegram_parse_doh_response(
        "{\"Answer\":[{\"name\":\"api.telegram.org\",\"type\":1,\"data\":\"149.154.167.220\"}]}",
        &ips, &cnt);
    if (!ok) { FAIL("returned false"); return; }
    if (cnt != 1) { FAIL("expected 1 IP"); telegram_free_ip_list(ips, cnt); return; }
    if (strcmp(ips[0], "149.154.167.220") == 0) { PASS(); telegram_free_ip_list(ips, cnt); return; }
    FAIL("wrong IP"); telegram_free_ip_list(ips, cnt);
}

static void test_parse_doh_multiple_a(void) {
    TEST("parse multiple A records");
    char **ips = NULL;
    size_t cnt = 0;
    bool ok = telegram_parse_doh_response(
        "{\"Answer\":["
        "{\"name\":\"api.telegram.org\",\"type\":1,\"data\":\"149.154.167.220\"},"
        "{\"name\":\"api.telegram.org\",\"type\":1,\"data\":\"149.154.167.221\"},"
        "{\"name\":\"api.telegram.org\",\"type\":1,\"data\":\"149.154.167.222\"}"
        "]}",
        &ips, &cnt);
    if (!ok) { FAIL("returned false"); return; }
    if (cnt != 3) { FAIL("expected 3 IPs"); telegram_free_ip_list(ips, cnt); return; }
    if (strcmp(ips[0], "149.154.167.220") == 0 &&
        strcmp(ips[1], "149.154.167.221") == 0 &&
        strcmp(ips[2], "149.154.167.222") == 0) { PASS(); telegram_free_ip_list(ips, cnt); return; }
    FAIL("wrong IPs"); telegram_free_ip_list(ips, cnt);
}

static void test_parse_doh_filters_non_a(void) {
    TEST("parse filters non-A records");
    char **ips = NULL;
    size_t cnt = 0;
    bool ok = telegram_parse_doh_response(
        "{\"Answer\":["
        "{\"name\":\"api.telegram.org\",\"type\":1,\"data\":\"149.154.167.220\"},"
        "{\"name\":\"api.telegram.org\",\"type\":28,\"data\":\"2001:db8::1\"},"
        "{\"name\":\"api.telegram.org\",\"type\":5,\"data\":\"cname.example.com\"}"
        "]}",
        &ips, &cnt);
    if (!ok) { FAIL("returned false"); return; }
    if (cnt != 1) { FAIL("expected 1 A record"); telegram_free_ip_list(ips, cnt); return; }
    if (strcmp(ips[0], "149.154.167.220") == 0) { PASS(); telegram_free_ip_list(ips, cnt); return; }
    FAIL("wrong IP"); telegram_free_ip_list(ips, cnt);
}

static void test_parse_doh_null_out_params(void) {
    TEST("parse null out params returns false");
    bool ok = telegram_parse_doh_response("{\"Answer\":[]}", NULL, NULL);
    if (!ok) { PASS(); return; }
    FAIL("expected false");
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== telegram_resolve_system_dns ===\n");
    test_resolve_null_hostname();
    test_resolve_empty_hostname();
    test_resolve_null_out_params();
    test_resolve_localhost();
    test_resolve_nonexistent();
    test_resolve_ipv4_only();
    test_free_null();

    printf("=== telegram_parse_doh_response ===\n");
    test_parse_doh_null();
    test_parse_doh_empty();
    test_parse_doh_invalid_json();
    test_parse_doh_no_answer();
    test_parse_doh_empty_answer();
    test_parse_doh_single_a();
    test_parse_doh_multiple_a();
    test_parse_doh_filters_non_a();
    test_parse_doh_null_out_params();

    printf("\n%d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
