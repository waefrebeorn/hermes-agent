/*
 * test_website_policy.c — Tests for website access policy checking.
 * Port of Python tools/website_policy.py tests.
 */

#include "website_policy.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests = 0;
static int passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* ─── Host Extraction Tests ─────────────────────────────── */

static void test_extract_host(void)
{
    char *h;

    h = website_extract_host("https://example.com/path");
    TEST("https host", h && strcmp(h, "example.com") == 0);
    free(h);

    h = website_extract_host("http://www.example.com");
    TEST("www stripped", h && strcmp(h, "example.com") == 0);
    free(h);

    h = website_extract_host("https://sub.example.com:8080/path?q=1");
    TEST("port stripped", h && strcmp(h, "sub.example.com") == 0);
    free(h);

    h = website_extract_host("example.com");
    TEST("bare domain", h && strcmp(h, "example.com") == 0);
    free(h);

    h = website_extract_host("");
    TEST("empty string", h == NULL);

    h = website_extract_host("//cdn.example.com/file.js");
    TEST("protocol relative", h && strcmp(h, "cdn.example.com") == 0);
    free(h);

    h = website_extract_host("https://EXAMPLE.COM");
    TEST("lowercase host", h && strcmp(h, "example.com") == 0);
    free(h);

    h = website_extract_host(NULL);
    TEST("NULL input", h == NULL);
}

/* ─── Pattern Matching Tests ────────────────────────────── */

static void test_match_host(void)
{
    TEST("exact match", website_match_host("example.com", "example.com"));
    TEST("subdomain match", website_match_host("sub.example.com", "example.com"));
    TEST("no match different", !website_match_host("other.com", "example.com"));
    TEST("wildcard prefix", website_match_host("sub.example.com", "*.example.com"));
    TEST("wildcard bare domain", website_match_host("example.com", "*.example.com"));
    TEST("wildcard deep sub", website_match_host("a.b.example.com", "*.example.com"));
    TEST("wildcard no match", !website_match_host("other.com", "*.example.com"));
    TEST("case insensitive", website_match_host("Example.COM", "example.com"));
    TEST("NULL host", !website_match_host(NULL, "example.com"));
    TEST("NULL pattern", !website_match_host("example.com", NULL));
}

/* ─── Policy Management Tests ───────────────────────────── */

static void test_policy_init(void)
{
    website_policy_t policy;
    website_policy_init(&policy);
    TEST("policy enabled by default", policy.enabled == true);
    TEST("policy starts empty", policy.rule_count == 0);
    website_policy_free(&policy);
}

static void test_policy_add_rule(void)
{
    website_policy_t policy;
    website_policy_init(&policy);

    int ret = website_add_rule(&policy, "*.malware.com", "config");
    TEST("add rule succeeds", ret == 0);
    TEST("rule count incremented", policy.rule_count == 1);
    TEST("pattern stored", strcmp(policy.rules[0].pattern, "*.malware.com") == 0);
    TEST("source stored", strcmp(policy.rules[0].source, "config") == 0);

    /* Add without source */
    website_add_rule(&policy, "badhost.org", NULL);
    TEST("rule without source", policy.rule_count == 2);
    TEST("null source", policy.rules[1].source == NULL);

    website_policy_free(&policy);
}

static void test_policy_free(void)
{
    website_policy_t policy;
    website_policy_init(&policy);
    website_add_rule(&policy, "test.com", "test");
    website_policy_free(&policy);
    TEST("freed clean", 1);  /* no crash */
}

/* ─── Check Access Tests ────────────────────────────────── */

static void test_check_access_blocked(void)
{
    website_policy_t policy;
    website_policy_init(&policy);
    website_add_rule(&policy, "*.malware.com", "config");

    website_block_t *block = website_check_access(
        "https://evil.malware.com/page", &policy);
    TEST("blocked url", block != NULL);
    TEST("blocked url preserved", block && strcmp(block->url,
        "https://evil.malware.com/page") == 0);
    TEST("blocked host extracted", block && strcmp(block->host, "evil.malware.com") == 0);
    TEST("blocked rule matched", block && strcmp(block->rule, "*.malware.com") == 0);
    TEST("blocked source", block && strcmp(block->source, "config") == 0);
    TEST("block message not empty", block && block->message && strlen(block->message) > 0);
    website_block_free(block);

    website_policy_free(&policy);
}

static void test_check_access_allowed(void)
{
    website_policy_t policy;
    website_policy_init(&policy);
    website_add_rule(&policy, "*.blocked.com", "config");

    website_block_t *block = website_check_access(
        "https://good-site.com", &policy);
    TEST("allowed url", block == NULL);
    website_block_free(block);

    website_policy_free(&policy);
}

static void test_check_access_disabled(void)
{
    website_policy_t policy;
    website_policy_init(&policy);
    policy.enabled = false;
    website_add_rule(&policy, "*.blocked.com", "config");

    website_block_t *block = website_check_access(
        "https://evil.blocked.com", &policy);
    TEST("disabled policy allows", block == NULL);
    website_block_free(block);

    website_policy_free(&policy);
}

static void test_check_access_no_rules(void)
{
    website_policy_t policy;
    website_policy_init(&policy);

    website_block_t *block = website_check_access(
        "https://anything.com", &policy);
    TEST("no rules allows", block == NULL);
    website_block_free(block);

    website_policy_free(&policy);
}

/* ─── File Loading Tests ────────────────────────────────── */

static void test_load_rules_from_file(void)
{
    /* Create a temporary blocklist file */
    const char *tmp_path = "/tmp/test_blocklist.txt";
    FILE *f = fopen(tmp_path, "w");
    if (!f) { TEST("can't create temp file", 0); return; }
    fprintf(f, "# Website blocklist\n");
    fprintf(f, "*.malware.com\n");
    fprintf(f, "badhost.org\n");
    fprintf(f, "\n");
    fprintf(f, "*.phishing.com\n");
    fclose(f);

    website_policy_t policy;
    website_policy_init(&policy);

    int loaded = website_load_rules_from_file(&policy, tmp_path);
    TEST("loaded 3 rules", loaded == 3);
    TEST("rule count matches", policy.rule_count == 3);
    TEST("first rule", strcmp(policy.rules[0].pattern, "*.malware.com") == 0);
    TEST("second rule", strcmp(policy.rules[1].pattern, "badhost.org") == 0);
    TEST("third rule", strcmp(policy.rules[2].pattern, "*.phishing.com") == 0);
    TEST("source is file path", strcmp(policy.rules[0].source, tmp_path) == 0);

    website_policy_free(&policy);
    remove(tmp_path);
}

static void test_load_rules_missing_file(void)
{
    website_policy_t policy;
    website_policy_init(&policy);

    int ret = website_load_rules_from_file(&policy, "/tmp/nonexistent.txt");
    TEST("missing file returns -1", ret == -1);

    website_policy_free(&policy);
}

/* ─── Main ──────────────────────────────────────────────── */

int main(void)
{
    test_extract_host();
    test_match_host();
    test_policy_init();
    test_policy_add_rule();
    test_policy_free();
    test_check_access_blocked();
    test_check_access_allowed();
    test_check_access_disabled();
    test_check_access_no_rules();
    test_load_rules_from_file();
    test_load_rules_missing_file();

    printf("website_policy: %d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
