/*
 * test_xai_retirement.c — Tests for retired xAI model detection (L04).
 *
 * Tests: normalize, looks_like, format_issue. Pure logic, no I/O.
 * Uses -Wl,--unresolved-symbols=ignore-all for unused hermes.h deps.
 */
#include "hermes_xai_retirement.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)

/* == Normalize Tests == */

static void test_normalize_basic(void) {
    TEST_STR_EQ("normalize grok-4", hermes_xai_normalize("grok-4"), "grok-4");
    TEST_STR_EQ("normalize xai/grok-4", hermes_xai_normalize("xai/grok-4"), "grok-4");
    TEST_STR_EQ("normalize x-ai/grok-4", hermes_xai_normalize("x-ai/grok-4"), "grok-4");
    TEST_STR_EQ("normalize mixed case", hermes_xai_normalize("X-AI/Grok-4-Fast"), "grok-4-fast");
    TEST_STR_EQ("normalize empty string", hermes_xai_normalize(""), "");
    TEST_STR_EQ("normalize NULL", hermes_xai_normalize(NULL), "");
}

static void test_normalize_prefix_strip(void) {
    TEST_STR_EQ("xai/ prefix stripped", hermes_xai_normalize("xai/grok-4-0709"), "grok-4-0709");
    TEST_STR_EQ("x-ai/ prefix stripped", hermes_xai_normalize("x-ai/grok-4-fast"), "grok-4-fast");
    TEST_STR_EQ("not xai model unchanged", hermes_xai_normalize("gpt-4"), "gpt-4");
    TEST_STR_EQ("xai in middle kept", hermes_xai_normalize("xai"), "xai");
}

static void test_normalize_trim(void) {
    TEST_STR_EQ("leading spaces", hermes_xai_normalize("  grok-4"), "grok-4");
    TEST_STR_EQ("trailing spaces", hermes_xai_normalize("grok-4  "), "grok-4");
    TEST_STR_EQ("both sides", hermes_xai_normalize("  xai/grok-4  "), "grok-4");
}

/* == Looks Like Tests == */

static void test_looks_like(void) {
    TEST("grok-4 looks like xAI", hermes_xai_looks_like("grok-4"));
    TEST("xai/grok-4 looks like xAI", hermes_xai_looks_like("xai/grok-4"));
    TEST("x-ai/grok-4-fast looks like xAI", hermes_xai_looks_like("x-ai/grok-4-fast"));
    TEST("gpt-4 does not look like xAI", !hermes_xai_looks_like("gpt-4"));
    TEST("NULL does not look like xAI", !hermes_xai_looks_like(NULL));
    TEST("empty does not look like xAI", !hermes_xai_looks_like(""));
    TEST("lowercase works", hermes_xai_looks_like("grok-code-fast-1"));
}

/* == Format Issue Tests == */

static void test_format_issue_simple(void) {
    xai_retirement_issue_t issue = {0};
    snprintf(issue.config_path, sizeof(issue.config_path), "principal.model");
    snprintf(issue.current_model, sizeof(issue.current_model), "grok-4");
    snprintf(issue.replacement, sizeof(issue.replacement), "grok-4.3");

    char *formatted = hermes_xai_format_issue(&issue);
    TEST("format issue non-NULL", formatted != NULL);
    TEST("format contains config_path", formatted && strstr(formatted, "principal.model"));
    TEST("format contains replacement", formatted && strstr(formatted, "grok-4.3"));
    TEST("format contains retirement date", formatted && strstr(formatted, "May 15, 2026"));
    TEST("format contains migration URL", formatted && strstr(formatted, "docs.x.ai"));
    free(formatted);
}

static void test_format_issue_with_reasoning(void) {
    xai_retirement_issue_t issue = {0};
    snprintf(issue.config_path, sizeof(issue.config_path), "principal.model");
    snprintf(issue.current_model, sizeof(issue.current_model), "grok-4-fast-non-reasoning");
    snprintf(issue.replacement, sizeof(issue.replacement), "grok-4.3");
    snprintf(issue.reasoning_effort, sizeof(issue.reasoning_effort), "none");

    char *formatted = hermes_xai_format_issue(&issue);
    TEST("format with reasoning non-NULL", formatted != NULL);
    TEST("format contains reasoning_effort", formatted && strstr(formatted, "reasoning_effort=none"));
    TEST("format contains replacement", formatted && strstr(formatted, "grok-4.3"));
    free(formatted);
}

static void test_format_issue_with_note(void) {
    xai_retirement_issue_t issue = {0};
    snprintf(issue.config_path, sizeof(issue.config_path), "auxiliary.web_extract.model");
    snprintf(issue.current_model, sizeof(issue.current_model), "grok-4-0709");
    snprintf(issue.replacement, sizeof(issue.replacement), "grok-4.3");
    snprintf(issue.note, sizeof(issue.note), "ambiguous variant");

    char *formatted = hermes_xai_format_issue(&issue);
    TEST("format with note non-NULL", formatted != NULL);
    TEST("format contains note", formatted && strstr(formatted, "ambiguous variant"));
    TEST("format contains config_path", formatted && strstr(formatted, "auxiliary.web_extract.model"));
    free(formatted);
}

static void test_format_issue_with_both(void) {
    xai_retirement_issue_t issue = {0};
    snprintf(issue.config_path, sizeof(issue.config_path), "principal.model");
    snprintf(issue.current_model, sizeof(issue.current_model), "grok-4-fast-non-reasoning");
    snprintf(issue.replacement, sizeof(issue.replacement), "grok-4.3");
    snprintf(issue.reasoning_effort, sizeof(issue.reasoning_effort), "none");
    snprintf(issue.note, sizeof(issue.note), "non-reasoning variant");

    char *formatted = hermes_xai_format_issue(&issue);
    TEST("format with both non-NULL", formatted != NULL);
    TEST("format contains reasoning_effort", formatted && strstr(formatted, "reasoning_effort=none"));
    TEST("format contains note", formatted && strstr(formatted, "non-reasoning variant"));
    free(formatted);
}

static void test_format_issue_null(void) {
    char *formatted = hermes_xai_format_issue(NULL);
    TEST("format NULL returns NULL", formatted == NULL);
}

int main(void) {
    printf("=== xAI Retirement Detection Tests ===\n");

    printf("--- Normalize ---\n");
    test_normalize_basic();
    test_normalize_prefix_strip();
    test_normalize_trim();

    printf("--- Looks Like ---\n");
    test_looks_like();

    printf("--- Format Issue ---\n");
    test_format_issue_simple();
    test_format_issue_with_reasoning();
    test_format_issue_with_note();
    test_format_issue_with_both();
    test_format_issue_null();

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
