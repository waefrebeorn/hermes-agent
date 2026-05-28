/* test_website.c -- Tests for libwebsite (website_policy) module.
 * Tests: policy init/free, add_rule, check_access, extract_host, match_host.
 * Compile with website_policy.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "website_policy.h"

static int pass = 0, fail = 0;
#define T(n, e) do { if(e) { pass++; printf("  OK %s\n", n); } else { fail++; printf("  FAIL %s\n", n); } } while(0)

static int contains(const char *h, const char *n) { return h && n && strstr(h, n) != NULL; }

static void test_init_free(void) {
    website_policy_t p;
    website_policy_init(&p);
    T("policy init sets enabled=true", p.enabled == true);
    T("policy init sets rule_count=0", p.rule_count == 0);
    website_policy_free(&p);
    T("policy free does not crash", 1);
}

static void test_extract_host(void) {
    char *h = website_extract_host("https://www.example.com/path?q=1");
    T("extracts host without www", h && strcmp(h, "example.com") == 0);
    free(h);

    h = website_extract_host("http://example.com");
    T("extracts host from http url", h && strcmp(h, "example.com") == 0);
    free(h);

    h = website_extract_host("not-a-url");
    T("non-url returns input", h && strcmp(h, "not-a-url") == 0);
    free(h);

    h = website_extract_host(NULL);
    T("NULL returns NULL", h == NULL);
}

static void test_match_host(void) {
    T("exact match", website_match_host("example.com", "example.com"));
    T("wildcard match", website_match_host("sub.example.com", "*.example.com"));
    T("wildcard no match", !website_match_host("other.com", "*.example.com"));
    T("full wildcard", website_match_host("anything.here", "*"));
}

static void test_add_rule_and_check(void) {
    website_policy_t p;
    website_policy_init(&p);
    p.enabled = true;

    int r = website_add_rule(&p, "*.example.com", "test");
    T("add_rule returns 0", r == 0);

    website_block_t *b = website_check_access("https://sub.example.com/page", &p);
    T("blocked URL returns block", b != NULL);
    if (b) {
        T("block url preserved", contains(b->url, "sub.example.com"));
        T("block pattern matches", b->rule && strcmp(b->rule, "*.example.com") == 0);
        website_block_free(b);
    }

    b = website_check_access("https://allowed.com/page", &p);
    T("allowed URL returns NULL", b == NULL);

    website_policy_free(&p);
}

static void test_disabled_policy(void) {
    website_policy_t p;
    website_policy_init(&p);
    p.enabled = false; /* override default (enabled=true) */
    website_add_rule(&p, "*.blocked.com", "test");

    website_block_t *b = website_check_access("https://sub.blocked.com/", &p);
    T("disabled policy allows all", b == NULL);

    website_policy_free(&p);
}

int main(void) {
    printf("=== Website Policy Tests ===\n\n");
    printf("--- Init/Free ---\n"); test_init_free();
    printf("\n--- Extract Host ---\n"); test_extract_host();
    printf("\n--- Match Host ---\n"); test_match_host();
    printf("\n--- Blocklist ---\n"); test_add_rule_and_check();
    printf("\n--- Disabled Policy ---\n"); test_disabled_policy();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0;
}
