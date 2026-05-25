/*
 * test_skill_preprocessing.c — Tests for skill_preprocessing module
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/* External declarations */
char *skill_substitute_template_vars(const char *content,
                                      const char *skill_dir,
                                      const char *session_id);
char *skill_expand_inline_shell(const char *content, int timeout_sec);
char *skill_preprocess_content(const char *content,
                                const char *skill_dir,
                                const char *session_id,
                                bool template_vars,
                                bool inline_shell,
                                int inline_shell_timeout);

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  TEST: %s\n", name); } while(0)
#define PASS() do { printf("    PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("    FAIL: %s\n", msg); fail++; } while(0)

static void check_str(const char *label, const char *got, const char *expected) {
    if (!got && !expected) { PASS(); return; }
    if (!got || !expected) { FAIL(label); printf("    got=%s expected=%s\n", got ? got : "NULL", expected ? expected : "NULL"); return; }
    if (strcmp(got, expected) == 0) { PASS(); return; }
    FAIL(label);
    printf("    got:      \"%s\"\n", got);
    printf("    expected: \"%s\"\n", expected);
}

static void test_no_tokens(void) {
    TEST("no template tokens");
    char *r = skill_substitute_template_vars("hello world", "/skills/dir", "sess-1");
    check_str("no tokens", r, "hello world");
    free(r);
}

static void test_skill_dir_token(void) {
    TEST("${HERMES_SKILL_DIR}");
    char *r = skill_substitute_template_vars("path: ${HERMES_SKILL_DIR}/file.txt", "/skills/my-skill", NULL);
    check_str("skill dir", r, "path: /skills/my-skill/file.txt");
    free(r);
}

static void test_session_id_token(void) {
    TEST("${HERMES_SESSION_ID}");
    char *r = skill_substitute_template_vars("session: ${HERMES_SESSION_ID}", NULL, "abc-123");
    check_str("session id", r, "session: abc-123");
    free(r);
}

static void test_both_tokens(void) {
    TEST("both tokens");
    char *r = skill_substitute_template_vars("${HERMES_SKILL_DIR}/${HERMES_SESSION_ID}", "/dir", "sid-99");
    check_str("both", r, "/dir/sid-99");
    free(r);
}

static void test_unresolved_token(void) {
    TEST("unresolved token left in place");
    char *r = skill_substitute_template_vars("hello ${UNKNOWN_TOKEN} world", NULL, NULL);
    check_str("unresolved", r, "hello ${UNKNOWN_TOKEN} world");
    free(r);
}

static void test_no_closing_brace(void) {
    TEST("no closing brace");
    char *r = skill_substitute_template_vars("hello ${", NULL, NULL);
    check_str("no close", r, "hello ${");
    free(r);
}

static void test_inline_shell_basic(void) {
    TEST("inline shell: !`echo hello`");
    char *r = skill_expand_inline_shell("prefix: !`echo hello` suffix", 5);
    if (!r) { FAIL("NULL"); return; }
    /* Should contain "hello" (trimmed) */
    if (strstr(r, "hello")) PASS();
    else { FAIL("missing echo output"); printf("    got: %s\n", r); }
    free(r);
}

static void test_inline_shell_no_marker(void) {
    TEST("inline shell: no !` marker");
    char *r = skill_expand_inline_shell("plain text without markers", 5);
    check_str("no marker", r, "plain text without markers");
    free(r);
}

static void test_inline_shell_empty(void) {
    TEST("inline shell: empty input");
    char *r = skill_expand_inline_shell("", 5);
    check_str("empty", r, "");
    free(r);
}

static void test_preprocess_basic(void) {
    TEST("preprocess: template only");
    char *r = skill_preprocess_content("dir: ${HERMES_SKILL_DIR}", "/my-dir", NULL, true, false, 5);
    check_str("preprocess template", r, "dir: /my-dir");
    free(r);
}

static void test_preprocess_all_off(void) {
    TEST("preprocess: all disabled");
    char *r = skill_preprocess_content("hello world", NULL, NULL, false, false, 5);
    check_str("all off", r, "hello world");
    free(r);
}

static void test_preprocess_null(void) {
    TEST("preprocess: null input");
    char *r = skill_preprocess_content(NULL, NULL, NULL, false, false, 5);
    if (!r) { PASS(); return; }
    FAIL("expected NULL");
    free(r);
}

int main(void) {
    printf("=== Skill Preprocessing Tests ===\n\n");

    test_no_tokens();
    test_skill_dir_token();
    test_session_id_token();
    test_both_tokens();
    test_unresolved_token();
    test_no_closing_brace();
    test_inline_shell_basic();
    test_inline_shell_no_marker();
    test_inline_shell_empty();
    test_preprocess_basic();
    test_preprocess_all_off();
    test_preprocess_null();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
