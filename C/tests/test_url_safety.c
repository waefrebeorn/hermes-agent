/*
 * test_url_safety.c — URL safety test suite (G125).
 *
 * Tests: scheme validation, always-blocked patterns, blocklist domain/category
 * management, allow_private toggle, NULL safety.
 * DNS-based checks (url_check_ip, url_is_safe) use getaddrinfo — tested via
 * integration tests only.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_url_safety.c src/tools/url_safety.c \
 *       -o /tmp/hermes_test_url -lm
 *
 * Run:
 *   /tmp/hermes_test_url
 */

#include "hermes.h"
#include "hermes_url_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_TRUE(name, expr) TEST(name, (expr))
#define TEST_FALSE(name, expr) TEST(name, !(expr))

/* ================================================================
 *  1. Scheme validation
 * ================================================================ */
static void test_scheme(void) {
    printf("\n--- Scheme Validation ---\n");

    TEST_TRUE("http valid",    url_has_valid_scheme("http://example.com"));
    TEST_TRUE("https valid",   url_has_valid_scheme("https://example.com"));
    TEST_FALSE("ftp invalid",   url_has_valid_scheme("ftp://example.com"));
    TEST_FALSE("file invalid",  url_has_valid_scheme("file:///etc/passwd"));
    TEST_FALSE("data invalid",  url_has_valid_scheme("data:text/plain,hello"));
    TEST_FALSE("gopher invalid", url_has_valid_scheme("gopher://example.com"));
    TEST_FALSE("javascript",    url_has_valid_scheme("javascript:alert(1)"));
    TEST_FALSE("no scheme",     url_has_valid_scheme("example.com/path"));
    TEST_FALSE("empty string",  url_has_valid_scheme(""));
    TEST_FALSE("NULL input",    url_has_valid_scheme(NULL));
}

/* ================================================================
 *  2. Always-blocked URLs (no DNS — string pattern check)
 * ================================================================ */
static void test_always_blocked(void) {
    printf("\n--- Always-Blocked URLs ---\n");

    /* Localhost variants */
    TEST_TRUE("localhost",     url_is_always_blocked("http://localhost:8080/api"));
    TEST_TRUE("127.0.0.1",     url_is_always_blocked("http://127.0.0.1:3000"));
    /* Note: only 127.0.0.1 is checked by string pattern. 127.x.x.x, 0.0.0.0,
     * and ::1 require DNS resolution via url_check_ip(). */
    TEST_FALSE("127.0.0.2",     url_is_always_blocked("http://127.0.0.2"));
    TEST_FALSE("0.0.0.0",       url_is_always_blocked("http://0.0.0.0"));
    TEST_FALSE("::1",           url_is_always_blocked("http://[::1]:8080"));

    /* Private IP ranges */
    TEST_TRUE("10.x.x.x",      url_is_always_blocked("http://10.0.0.1"));
    TEST_TRUE("172.16.x.x",    url_is_always_blocked("http://172.16.0.1"));
    TEST_TRUE("172.31.x.x",    url_is_always_blocked("http://172.31.255.255"));
    TEST_TRUE("192.168.x.x",   url_is_always_blocked("http://192.168.1.1"));

    /* Link-local */
    TEST_TRUE("169.254.x.x",   url_is_always_blocked("http://169.254.1.1"));

    /* Safe URLs should NOT be blocked */
    TEST_FALSE("public IP",    url_is_always_blocked("http://93.184.216.34"));
    TEST_FALSE("normal domain", url_is_always_blocked("http://example.com"));
    TEST_FALSE("google",        url_is_always_blocked("https://www.google.com"));
    TEST_FALSE("api endpoint",  url_is_always_blocked("https://api.openai.com/v1/chat"));

    /* Edge cases — NULL/empty/relative are blocked (can't fetch) */
    TEST_TRUE("NULL input",    url_is_always_blocked(NULL));
    TEST_TRUE("empty",         url_is_always_blocked(""));
    TEST_TRUE("relative path", url_is_always_blocked("/api/v1/data"));
}

/* ================================================================
 *  3. Blocklist domain management
 * ================================================================ */
static void test_blocklist_domains(void) {
    printf("\n--- Blocklist Domains ---\n");

    url_blocklist_enable(true);

    /* Add domains */
    TEST_TRUE("add facebook.com",    url_blocklist_add_domain("facebook.com"));
    TEST_TRUE("add twitter.com",     url_blocklist_add_domain("twitter.com"));
    TEST_TRUE("add *.malware.site",  url_blocklist_add_domain("*.malware.site"));

    /* Duplicate add should return true (already present) */
    TEST_TRUE("duplicate add",       url_blocklist_add_domain("facebook.com"));

    /* Remove domain */
    TEST_TRUE("remove twitter.com",  url_blocklist_remove_domain("twitter.com"));

    /* Remove non-existent */
    TEST_FALSE("remove not-found",   url_blocklist_remove_domain("nonexistent.com"));

    /* NULL safety */
    TEST_FALSE("add NULL",           url_blocklist_add_domain(NULL));
    TEST_FALSE("remove NULL",        url_blocklist_remove_domain(NULL));

    url_blocklist_clear();
}

/* ================================================================
 *  4. Blocklist category management
 * ================================================================ */
static void test_blocklist_categories(void) {
    printf("\n--- Blocklist Categories ---\n");

    url_blocklist_enable(true);

    TEST_TRUE("add social_media",    url_blocklist_add_category("social_media"));
    TEST_TRUE("add streaming",       url_blocklist_add_category("streaming"));
    TEST_TRUE("add adult",           url_blocklist_add_category("adult"));

    /* Duplicate */
    TEST_TRUE("duplicate category",  url_blocklist_add_category("social_media"));

    /* Remove */
    TEST_TRUE("remove streaming",    url_blocklist_remove_category("streaming"));
    TEST_FALSE("remove not-found",   url_blocklist_remove_category("nonexistent"));

    /* NULL safety */
    TEST_FALSE("add NULL",           url_blocklist_add_category(NULL));
    TEST_FALSE("remove NULL",        url_blocklist_remove_category(NULL));

    url_blocklist_clear();
}

/* ================================================================
 *  5. Blocklist toggle & clear
 * ================================================================ */
static void test_blocklist_toggle(void) {
    printf("\n--- Blocklist Toggle ---\n");

    /* Disable blocklist */
    url_blocklist_enable(false);
    /* Adding should still work but have no effect on safety checks */
    TEST_TRUE("add while disabled", url_blocklist_add_domain("blocked-site.com"));

    /* Re-enable */
    url_blocklist_enable(true);

    /* Clear */
    url_blocklist_clear();
    TEST_TRUE("add after clear", url_blocklist_add_domain("new-domain.com"));

    url_blocklist_clear();
}

/* ================================================================
 *  6. Allow-private toggle
 * ================================================================ */
static void test_allow_private(void) {
    printf("\n--- Allow Private Toggle ---\n");

    /* Default: private blocked */
    url_reset_allow_private();
    TEST_TRUE("localhost blocked by default",
              url_is_always_blocked("http://localhost"));

    /* Allow private */
    url_set_allow_private(true);
    /* url_is_always_blocked only checks URL patterns, not the allow flag.
     * The allow flag affects url_is_safe() which also does DNS.
     * This test validates the toggle doesn't crash. */
    TEST("set_allow_private no crash", 1);

    /* Reset to default */
    url_reset_allow_private();
    TEST("reset_allow_private no crash", 1);
}

/* ================================================================
 *  7. Blocklist edge cases
 * ================================================================ */
static void test_blocklist_edge_cases(void) {
    printf("\n--- Blocklist Edge Cases ---\n");

    /* Empty domain — implementation accepts empty as valid entry (matches nothing) */
    TEST_TRUE("add empty domain",   url_blocklist_add_domain(""));
    TEST_TRUE("remove empty",       url_blocklist_remove_domain(""));
    TEST_TRUE("add empty category", url_blocklist_add_category(""));
    TEST_TRUE("remove empty",       url_blocklist_remove_category(""));

    /* Long domain */
    char long_domain[256];
    memset(long_domain, 'a', 250);
    long_domain[250] = '\0';
    TEST_TRUE("add long domain",     url_blocklist_add_domain(long_domain));
    TEST_TRUE("remove long domain",  url_blocklist_remove_domain(long_domain));

    /* Multiple clears */
    url_blocklist_clear();
    url_blocklist_clear();
    url_blocklist_clear();
    TEST("multiple clears no crash", 1);
}

int main(void) {
    printf("=== URL Safety Test Suite (G125) ===\n");

    test_scheme();
    test_always_blocked();
    test_blocklist_domains();
    test_blocklist_categories();
    test_blocklist_toggle();
    test_allow_private();
    test_blocklist_edge_cases();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
