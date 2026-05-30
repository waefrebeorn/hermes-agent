/*
 * test_telegram_network.c — Tests for telegram_network helpers.
 *
 * Tests: telegram_resolve_system_dns.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include \
 *       tests/test_telegram_network.c \
 *       src/gateway/platforms/telegram_network.c \
 *       -o /tmp/hermes_test_telegram_network -lm
 *
 * Run:
 *   /tmp/hermes_test_telegram_network
 */

#include "hermes_telegram_network.h"
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
    /* Use a hostname that's extremely unlikely to resolve */
    bool ok = telegram_resolve_system_dns("zzz-nonexistent-host-xyz.test.", &ips, &cnt);
    /* Should return true (not an error) with empty list */
    if (ok && cnt == 0 && ips == NULL) { PASS(); return; }
    /* Some resolvers may return results for any query */
    telegram_free_ip_list(ips, cnt);
    if (ok && cnt > 0) {
        /* Actually resolved — not expected but possible with wildcard DNS */
        PASS();
        return;
    }
    FAIL("expected true with empty results");
}

static void test_resolve_ipv4_only(void) {
    TEST("resolve returns only IPv4 addresses");
    char **ips = NULL;
    size_t cnt = 0;
    bool ok = telegram_resolve_system_dns("localhost", &ips, &cnt);
    if (!ok || cnt == 0) { FAIL("no results"); telegram_free_ip_list(ips, cnt); return; }
    for (size_t i = 0; i < cnt; i++) {
        /* Check it's a valid IPv4 address (no colons = no IPv6) */
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

    printf("\n%d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
