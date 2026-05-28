/*
 * test_ansi.c — ANSI library tests (J22).
 * Tests: wrap, strip, string constants, terminal detection.
 */
#include "ansi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

/* ================================================================
 * 1. String constants
 * ================================================================ */

static void test_reset_defined(void) {
    TEST("ANSI_RESET defined");
    assert(ANSI_RESET != NULL);
    assert(strcmp(ANSI_RESET, "\033[0m") == 0);
    PASS();
}

static void test_color_constants(void) {
    TEST("ANSI_RED defined");
    assert(strcmp(ANSI_RED, "\033[31m") == 0);
    PASS();
}

static void test_bright_colors(void) {
    TEST("ANSI_BRIGHT_GREEN defined");
    assert(strcmp(ANSI_BRIGHT_GREEN, "\033[92m") == 0);
    PASS();
}

static void test_bg_colors(void) {
    TEST("ANSI_BG_BLUE defined");
    assert(strcmp(ANSI_BG_BLUE, "\033[44m") == 0);
    PASS();
}

static void test_style_constants(void) {
    TEST("ANSI_BOLD defined");
    assert(strcmp(ANSI_BOLD, "\033[1m") == 0);
    assert(strcmp(ANSI_UNDERLINE, "\033[4m") == 0);
    PASS();
}

/* ================================================================
 * 2. ansi_wrap
 * ================================================================ */

static void test_wrap_fg_only(void) {
    TEST("ansi_wrap with foreground color only");
    char *w = ansi_wrap("hello", ANSI_RED, NULL);
    assert(w != NULL);
    assert(strstr(w, "hello") != NULL);
    assert(strstr(w, ANSI_RED) != NULL);
    assert(strstr(w, ANSI_RESET) != NULL);
    free(w);
    PASS();
}

static void test_wrap_style_only(void) {
    TEST("ansi_wrap with style only");
    char *w = ansi_wrap("bold", NULL, ANSI_BOLD);
    assert(w != NULL);
    assert(strstr(w, ANSI_BOLD) != NULL);
    assert(strstr(w, ANSI_RESET) != NULL);
    free(w);
    PASS();
}

static void test_wrap_both(void) {
    TEST("ansi_wrap with fg + style");
    char *w = ansi_wrap("red_bold", ANSI_RED, ANSI_BOLD);
    assert(w != NULL);
    assert(strstr(w, ANSI_RED) != NULL);
    assert(strstr(w, ANSI_BOLD) != NULL);
    assert(strstr(w, ANSI_RESET) != NULL);
    free(w);
    PASS();
}

static void test_wrap_null_text(void) {
    TEST("ansi_wrap(NULL) returns NULL");
    assert(ansi_wrap(NULL, ANSI_RED, NULL) == NULL);
    PASS();
}

static void test_wrap_empty(void) {
    TEST("ansi_wrap empty string");
    char *w = ansi_wrap("", ANSI_GREEN, NULL);
    assert(w != NULL);
    assert(strstr(w, ANSI_GREEN) != NULL);
    assert(strstr(w, ANSI_RESET) != NULL);
    free(w);
    PASS();
}

/* ================================================================
 * 3. ansi_strip
 * ================================================================ */

static void test_strip_plain(void) {
    TEST("ansi_strip plain text unchanged");
    char *s = ansi_strip("hello world");
    assert(s != NULL);
    assert(strcmp(s, "hello world") == 0);
    free(s);
    PASS();
}

static void test_strip_single_color(void) {
    TEST("ansi_strip removes single color code");
    char input[64];
    snprintf(input, sizeof(input), "%sred%s", ANSI_RED, ANSI_RESET);
    char *s = ansi_strip(input);
    assert(s != NULL);
    assert(strcmp(s, "red") == 0);
    free(s);
    PASS();
}

static void test_strip_multiple_codes(void) {
    TEST("ansi_strip removes multiple codes");
    char input[128];
    snprintf(input, sizeof(input), "%s%sbold%s", ANSI_RED, ANSI_BOLD, ANSI_RESET);
    char *s = ansi_strip(input);
    assert(s != NULL);
    assert(strcmp(s, "bold") == 0);
    free(s);
    PASS();
}

static void test_strip_mixed(void) {
    TEST("ansi_strip mixed plain + ANSI");
    char input[128];
    snprintf(input, sizeof(input), "%shello %sworld%s", ANSI_RED, ANSI_BOLD, ANSI_RESET);
    char *s = ansi_strip(input);
    assert(s != NULL);
    assert(strcmp(s, "hello world") == 0);
    free(s);
    PASS();
}

static void test_strip_null(void) {
    TEST("ansi_strip(NULL) returns NULL");
    assert(ansi_strip(NULL) == NULL);
    PASS();
}

/* ================================================================
 * 4. Terminal support detection
 * ================================================================ */

static void test_supported_not_null(void) {
    TEST("ansi_supported returns a value (true/false)");
    /* Can be true or false depending on environment — just check no crash */
    bool r = ansi_supported();
    (void)r;
    PASS();
}

static void test_term_width_valid(void) {
    TEST("ansi_term_width returns positive value");
    int w = ansi_term_width();
    assert(w > 0);
    PASS();
}

static void test_term_height_valid(void) {
    TEST("ansi_term_height returns positive value");
    int h = ansi_term_height();
    assert(h > 0);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== ANSI Library Tests (J22) ===\n");

    test_reset_defined();
    test_color_constants();
    test_bright_colors();
    test_bg_colors();
    test_style_constants();
    test_wrap_fg_only();
    test_wrap_style_only();
    test_wrap_both();
    test_wrap_null_text();
    test_wrap_empty();
    test_strip_plain();
    test_strip_single_color();
    test_strip_multiple_codes();
    test_strip_mixed();
    test_strip_null();
    test_supported_not_null();
    test_term_width_valid();
    test_term_height_valid();

    printf("\n%d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
