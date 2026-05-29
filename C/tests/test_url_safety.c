/*
 * test_url_safety.c — URL safety module tests.
 * Tests SSRF protection, IP blocking, blocklist, secret detection.
 *
 * Build:
 *   gcc -O2 -g -Wall -I include tests/test_url_safety.c src/tools/url_safety.c \
 *       lib/libjson/json.c lib/libhttp/http.c -o /tmp/hermes_test_url_safety \
 *       -lssl -lcrypto -lz -lm -Wl,--unresolved-symbols=ignore-all
 */
#include "hermes.h"
#include "hermes_url_safety.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== URL Safety Tests ===\n");

    /* 1. extract_hostname */
    TEST("extract hostname from https URL",
         strcmp(url_extract_hostname("https://example.com/path"), "example.com") == 0);
    TEST("extract hostname from http URL with port",
         strcmp(url_extract_hostname("http://hostname:8080/path"), "hostname") == 0);
    TEST("extract hostname from URL without path",
         strcmp(url_extract_hostname("https://google.com"), "google.com") == 0);

    /* 2. url_host_matches */
    TEST("host matches exact",
         url_host_matches("https://example.com/path", "example.com"));
    TEST("host matches subdomain",
         url_host_matches("https://sub.example.com/path", "example.com"));
    TEST("host does not match different",
         !url_host_matches("https://other.com/path", "example.com"));

    /* 3. url_has_valid_scheme */
    TEST("valid https scheme",
         url_has_valid_scheme("https://example.com"));
    TEST("valid http scheme",
         url_has_valid_scheme("http://example.com"));
    TEST("data scheme not valid for network safety",
         !url_has_valid_scheme("data:image/png;base64,iVBOR"));
    TEST("invalid scheme",
         !url_has_valid_scheme("ftp://example.com"));
    TEST("no scheme (path only)",
         !url_has_valid_scheme("/local/path/file.txt"));

    /* 4. url_has_secret */
    TEST("detect sk- secret in URL",
         url_has_secret("https://example.com?key=sk-proj-abc123") != NULL);
    TEST("detect ghp_ secret in URL",
         url_has_secret("https://example.com?token=ghp_abc123def456") != NULL);
    TEST("detect AIza secret in URL",
         url_has_secret("https://example.com?api=AIzaSyABC123DEF456") != NULL);
    TEST("no secret in clean URL",
         url_has_secret("https://example.com/page.html") == NULL);
    TEST("no secret in URL with safe query",
         url_has_secret("https://example.com/search?q=hello") == NULL);

    /* 5. url_is_always_blocked */
    TEST("localhost always blocked",
         url_is_always_blocked("http://localhost:8080"));
    TEST("127.0.0.1 always blocked",
         url_is_always_blocked("http://127.0.0.1/api"));
    TEST("169.254 link-local blocked",
         url_is_always_blocked("http://169.254.1.1"));
    TEST("10.x private blocked",
         url_is_always_blocked("http://10.0.0.1"));
    TEST("172.16 private blocked",
         url_is_always_blocked("http://172.16.0.1"));
    TEST("192.168 private blocked",
         url_is_always_blocked("http://192.168.1.1"));
    TEST("public IP not blocked",
         !url_is_always_blocked("http://93.184.216.34"));
    TEST("public domain not blocked",
         !url_is_always_blocked("https://example.com"));

    /* 6. Blocklist */
    url_blocklist_add_domain("malware.com");
    url_blocklist_add_domain("phishing.test");
    TEST("blocklisted domain detected",
         url_is_always_blocked("https://malware.com/evil"));
    TEST("blocklisted subdomain detected",
         url_is_always_blocked("https://sub.phishing.test/page"));
    TEST("non-blocklisted domain not blocked",
         !url_is_always_blocked("https://safe.com"));
    url_blocklist_remove_domain("malware.com");
    TEST("removed from blocklist no longer blocked",
         !url_is_always_blocked("https://malware.com"));
    url_blocklist_clear();
    TEST("cleared blocklist allows previously added",
         !url_is_always_blocked("https://phishing.test"));

    /* 7. Allow private */
    url_reset_allow_private();

    /* 8. url_safe_for_log */
    {
        char *s;
        s = url_safe_for_log(NULL, 80);
        TEST("safe_for_log NULL returns NULL", s == NULL);
        free(s);
        s = url_safe_for_log("", 80);
        TEST("safe_for_log empty returns NULL", s == NULL);
        s = url_safe_for_log("https://example.com/path/to/file.json", 80);
        TEST("safe_for_log normal URL path", s && strstr(s, "example.com/.../file.json"));
        free(s);
        s = url_safe_for_log("https://user:pass@api.example.com/data", 80);
        TEST("safe_for_log strips userinfo", s && strstr(s, "api.example.com/.../data") && !strstr(s, "user:pass"));
        free(s);
        s = url_safe_for_log("https://example.com", 80);
        TEST("safe_for_log bare domain", s && strcmp(s, "https://example.com") == 0);
        free(s);
        s = url_safe_for_log("just a plain string without protocol", 80);
        TEST("safe_for_log non-URL string truncated", s != NULL);
        free(s);
        s = url_safe_for_log("https://host:8443/path", 80);
        TEST("safe_for_log preserves port", s && strstr(s, "host:8443"));
        free(s);
        s = url_safe_for_log("https://api.openai.com/v1/chat/completions", 30);
        TEST("safe_for_log truncation", s && strlen(s) <= 30);
        free(s);
        s = url_safe_for_log("https://api.example.com/", 80);
        TEST("safe_for_log trailing slash", s && strstr(s, "example.com"));
        free(s);
    }

    /* 9. url_is_network_accessible */
    TEST("network_access localhost is not accessible",
         !url_is_network_accessible("127.0.0.1"));
    TEST("network_access 127.1 not accessible",
         !url_is_network_accessible("127.0.0.2"));
    TEST("network_access ::1 not accessible",
         !url_is_network_accessible("::1"));
    TEST("network_access public IP accessible",
         url_is_network_accessible("93.184.216.34"));
    TEST("network_access NULL returns true (fail closed)",
         url_is_network_accessible(NULL));
    TEST("network_access empty returns true (fail closed)",
         url_is_network_accessible(""));

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
