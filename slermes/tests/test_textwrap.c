/* test_textwrap.c — J11: Python textwrap port tests (wrap, fill, dedent, shorten). */
#include "textwrap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

int main(void) {
    printf("=== J11: libtextwrap Tests ===\n\n");

    /* --- wrap --- */
    printf("--- textwrap_wrap ---\n");
    textwrap_result_t r = textwrap_wrap("hello world", 80);
    TEST("single line fits", r.count == 1);
    TEST("contains both words", strcmp(r.lines[0], "hello world") == 0);
    for (int i = 0; i < r.count; i++) free(r.lines[i]);

    r = textwrap_wrap("hello world foo bar baz", 10);
    TEST("wrap breaks at 10", r.count >= 2);
    if (r.count > 0) {
        TEST("first line ≤10", strlen(r.lines[0]) <= 10);
    }
    for (int i = 0; i < r.count; i++) free(r.lines[i]);

    r = textwrap_wrap(NULL, 80);
    TEST("NULL text → empty", r.count == 0);

    r = textwrap_wrap("", 80);
    TEST("empty text → empty", r.count == 0);

    r = textwrap_wrap("singleword", 3);
    TEST("long word forced to new line", r.count >= 1);
    if (r.count > 0) TEST("long word placed", strlen(r.lines[0]) > 0);
    for (int i = 0; i < r.count; i++) free(r.lines[i]);

    r = textwrap_wrap("width≤0 returns single line", 0);
    TEST("width 0 returns raw", r.count == 1);
    for (int i = 0; i < r.count; i++) free(r.lines[i]);

    /* --- wrap with newlines --- */
    printf("\n--- textwrap_wrap (newlines) ---\n");
    r = textwrap_wrap("line1\nline2", 80);
    TEST("two lines from \\n", r.count == 2);
    if (r.count >= 2) {
        TEST("first line is line1", strcmp(r.lines[0], "line1") == 0);
        TEST("second line is line2", strcmp(r.lines[1], "line2") == 0);
    }
    for (int i = 0; i < r.count; i++) free(r.lines[i]);

    r = textwrap_wrap("aaa bbb ccc ddd eee fff", 7);
    TEST("wrap at 7 has 3+ lines", r.count >= 3);
    for (int i = 0; i < r.count; i++) {
        TEST("each line ≤7", strlen(r.lines[i]) <= 7);
        free(r.lines[i]);
    }

    /* --- fill --- */
    printf("\n--- textwrap_fill ---\n");
    char *filled = textwrap_fill("hello world", 80);
    TEST("fill single line", filled && strcmp(filled, "hello world") == 0);
    free(filled);

    filled = textwrap_fill("short", 80);
    TEST("fill short", filled && strcmp(filled, "short") == 0);
    free(filled);

    filled = textwrap_fill("", 80);
    TEST("fill empty", filled && strcmp(filled, "") == 0);
    free(filled);

    filled = textwrap_fill(NULL, 80);
    TEST("fill NULL returns empty", filled && strcmp(filled, "") == 0);
    free(filled);

    filled = textwrap_fill("a b c d e", 3);
    TEST("fill wraps at 3", filled != NULL);
    if (filled) {
        /* Each line should be ≤3 chars */
        TEST("fill has newlines", strchr(filled, '\n') != NULL);
    }
    free(filled);

    /* --- dedent --- */
    printf("\n--- textwrap_dedent ---\n");
    char *dedented = textwrap_dedent("  hello\n  world");
    TEST("dedent 2 spaces", dedented && strcmp(dedented, "hello\nworld") == 0);
    free(dedented);

    dedented = textwrap_dedent("    hello\n  world");
    TEST("dedent min indent (2)", dedented && strcmp(dedented, "  hello\nworld") == 0);
    free(dedented);

    dedented = textwrap_dedent("no indent here");
    TEST("dedent no indent", dedented && strcmp(dedented, "no indent here") == 0);
    free(dedented);

    dedented = textwrap_dedent("");
    TEST("dedent empty", dedented && strcmp(dedented, "") == 0);
    free(dedented);

    dedented = textwrap_dedent(NULL);
    TEST("dedent NULL", dedented && strcmp(dedented, "") == 0);
    free(dedented);

    dedented = textwrap_dedent("  \n  ");
    TEST("dedent blank lines (no non-blank → passthrough)", dedented && strcmp(dedented, "  \n  ") == 0);
    free(dedented);

    /* --- shorten --- */
    printf("\n--- textwrap_shorten ---\n");
    char *shortened = textwrap_shorten("hello world", 5);
    TEST("shorten to 5", shortened && strcmp(shortened, "he...") == 0);
    free(shortened);

    shortened = textwrap_shorten("hello world", 11);
    TEST("shorten fits exactly", shortened && strcmp(shortened, "hello world") == 0);
    free(shortened);

    shortened = textwrap_shorten("hello world", 80);
    TEST("shorten well within", shortened && strcmp(shortened, "hello world") == 0);
    free(shortened);

    shortened = textwrap_shorten("ab", 2);
    TEST("shorten exact short", shortened && strcmp(shortened, "ab") == 0);
    free(shortened);

    shortened = textwrap_shorten(NULL, 10);
    TEST("shorten NULL", shortened && strcmp(shortened, "") == 0);
    free(shortened);

    shortened = textwrap_shorten("test", 0);
    TEST("shorten max_len 0", shortened && strcmp(shortened, "") == 0);
    free(shortened);

    shortened = textwrap_shorten("hello world", 3);
    TEST("shorten to 3 (0 chars + ...)", shortened && strcmp(shortened, "...") == 0);
    free(shortened);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
