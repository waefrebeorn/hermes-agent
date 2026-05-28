/* test_redact.c -- Tests for redact module (hermes_redact).
 * Tests: redact, add_pattern, clear_patterns, load_config.
 * Compile with redact.c + -Wl,--unresolved-symbols=ignore-all.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern char *hermes_redact(const char *input);
extern bool hermes_redact_add_pattern(const char *pattern);
extern void hermes_redact_clear_patterns(void);
extern void hermes_redact_load_config(const char *patterns_str);

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { tests_passed++; printf("  OK %s\n", name); } \
    else { tests_failed++; printf("  FAIL %s\n", name); } \
} while(0)

static int contains(const char *h, const char *n) { return h && n && strstr(h, n) != NULL; }

/* ---- Basic Redaction ---- */
static void test_redact_openai_key(void) {
    char *r = hermes_redact("sk-AAA...AAAA");
    TEST("sk- key redacted (21+ chars)", !contains(r, "AAAAAAAAAAAAAAAAAAAAA"));
    free(r);
}

static void test_redact_null(void) {
    char *r = hermes_redact(NULL);
    TEST("NULL in -> NULL out", r == NULL);
}

static void test_redact_empty(void) {
    char *r = hermes_redact("");
    TEST("empty in -> empty out", r && r[0] == '\0');
    free(r);
}

static void test_redact_no_match(void) {
    char *r = hermes_redact("hello world this is normal");
    TEST("normal text unchanged", r && strcmp(r, "hello world this is normal") == 0);
    free(r);
}

static void test_short_key_not_redacted(void) {
    char *r = hermes_redact("sk-abc123");
    TEST("short sk- not redacted (<20 chars)", contains(r, "sk-abc123"));
    free(r);
}

/* ---- Multiple occurrences ---- */
static void test_redact_multiple(void) {
    char *r = hermes_redact("key1=sk-AAA...AAAA key2=sk-BBB...BBBB");
    TEST("multiple long sk- keys redacted",
         !contains(r, "AAAAAAAAAAAAAAAAAAAAA") && !contains(r, "BBBBBBBBBBBBBBBBBBBBB"));
    free(r);
}

/* ---- add/clear/config ---- */
static void test_add_pattern(void) {
    hermes_redact_clear_patterns();
    bool ok = hermes_redact_add_pattern("secret:");
    TEST("add_pattern returns true", ok);
    char *r = hermes_redact("my secret: hello123");
    TEST("custom pattern redacts value", !contains(r, "hello123"));
    free(r);
}

static void test_clear_disables_custom_patterns(void) {
    hermes_redact_clear_patterns();
    hermes_redact_add_pattern("custom:");
    char *r = hermes_redact("custom: hello123");
    TEST("custom pattern works before clear", !contains(r, "hello123"));
    free(r);

    hermes_redact_clear_patterns();
    r = hermes_redact("custom: hello123");
    TEST("custom pattern stops after clear", contains(r, "hello123"));
    free(r);
}

static void test_load_config(void) {
    hermes_redact_clear_patterns();
    hermes_redact_load_config("password:,token:");
    char *r1 = hermes_redact("password=hello123xyz");
    TEST("config password pattern", !contains(r1, "hello123xyz"));
    free(r1);
    char *r2 = hermes_redact("token=abc-def-ghi-jkl");
    TEST("config token pattern", !contains(r2, "abc-def-ghi-jkl"));
    free(r2);
}

static void test_load_config_null_restores_defaults(void) {
    hermes_redact_clear_patterns();
    hermes_redact_load_config(NULL);
    char *r = hermes_redact("sk-AAA...AAAA");
    TEST("null config restores defaults", !contains(r, "AAAAAAAAAAAAAAAAAAAAA"));
    free(r);
}

/* ---- Normal text ---- */
static void test_normal_text_not_harmed(void) {
    char *r = hermes_redact("The disk usage is at 75% and memory is normal");
    TEST("normal system text preserved", contains(r, "disk usage"));
    free(r);
}

int main(void) {
    printf("=== Redact Module Tests ===\n");
    test_redact_openai_key();
    test_redact_null();
    test_redact_empty();
    test_redact_no_match();
    test_short_key_not_redacted();
    test_redact_multiple();
    test_add_pattern();
    test_clear_disables_custom_patterns();
    test_load_config();
    test_load_config_null_restores_defaults();
    test_normal_text_not_harmed();
    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
