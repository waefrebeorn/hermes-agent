/*
 * test_markdown_render.c — Markdown render/strip test suite (J22).
 *
 * Tests: hermes_markdown_render(), hermes_markdown_strip().
 * Covers headers, bold, italic, inline code, fenced code blocks,
 * links, lists, blockquotes, horizontal rules, edge cases.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include \
 *       tests/test_markdown_render.c src/agent/markdown_render.c \
 *       -o /tmp/hermes_test_markdown_render -lm
 *
 * Run:
 *   /tmp/hermes_test_markdown_render
 */

#include "hermes_markdown.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_STRSTR(name, haystack, needle) TEST(name, haystack && strstr(haystack, needle) != NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_EMPTY(name, s) TEST(name, s && s[0] != '\0')

/* ================================================================
 *  1. hermes_markdown_render — basic text
 * ================================================================ */
static void test_render_plain(void) {
    printf("\n--- render: plain text ---\n");

    char *r = hermes_markdown_render("hello world", 80);
    TEST_NOT_NULL("plain text returns non-NULL", r);
    TEST_STRSTR("plain text contains 'hello'", r, "hello");
    TEST_STRSTR("plain text contains 'world'", r, "world");
    free(r);

    r = hermes_markdown_render("", 80);
    TEST_NOT_NULL("empty string returns non-NULL", r);
    free(r);

    r = hermes_markdown_render("\n\n", 80);
    TEST_NOT_NULL("whitespace-only returns non-NULL", r);
    free(r);
}

/* ================================================================
 *  2. hermes_markdown_render — headers
 * ================================================================ */
static void test_render_headers(void) {
    printf("\n--- render: headers ---\n");

    /* Each header level emits bold + color */
    char *r = hermes_markdown_render("# H1\n## H2\n### H3\n", 80);
    TEST_NOT_NULL("multiple headers returns non-NULL", r);
    TEST_STRSTR("h1 contains H1 text", r, "H1");
    TEST_STRSTR("h2 contains H2 text", r, "H2");
    TEST_STRSTR("h3 contains H3 text", r, "H3");
    TEST_STRSTR("h1 has bold ANSI", r, "[1m");  /* ANSI bold */
    free(r);

    r = hermes_markdown_render("###### H6", 80);
    TEST_NOT_NULL("h6 returns non-NULL", r);
    TEST_STRSTR("h6 contains H6 text", r, "H6");
    TEST_STRSTR("h6 has bold ANSI", r, "[1m");
    free(r);

    /* Invalid header — no space after ### */
    r = hermes_markdown_render("###not a header", 80);
    TEST_NOT_NULL("no-space header returns non-NULL", r);
    TEST_STRSTR("no-space header contains text", r, "###not a header");
    free(r);
}

/* ================================================================
 *  3. hermes_markdown_render — bold and italic
 * ================================================================ */
static void test_render_bold_italic(void) {
    printf("\n--- render: bold & italic ---\n");

    char *r = hermes_markdown_render("**bold** text *italic*", 80);
    TEST_NOT_NULL("bold+italic returns non-NULL", r);
    TEST_STRSTR("bold+italic contains 'bold'", r, "bold");
    TEST_STRSTR("bold+italic contains 'italic'", r, "italic");
    TEST_STRSTR("bold+italic contains 'text'", r, "text");
    /* Bold marks with ANSI bold */
    TEST_STRSTR("bold has ANSI bold around it", r, "[1m");
    /* Italic marks with ANSI italic */
    TEST_STRSTR("italic has ANSI italic", r, "[3m");
    free(r);

    /* Unclosed bold */
    r = hermes_markdown_render("**unclosed", 80);
    TEST_NOT_NULL("unclosed bold returns non-NULL", r);
    TEST_STRSTR("unclosed bold contains text", r, "unclosed");
    free(r);
}

/* ================================================================
 *  4. hermes_markdown_render — inline code
 * ================================================================ */
static void test_render_inline_code(void) {
    printf("\n--- render: inline code ---\n");

    char *r = hermes_markdown_render("Use `code` here", 80);
    TEST_NOT_NULL("inline code returns non-NULL", r);
    TEST_STRSTR("inline code contains 'code'", r, "code");
    TEST_STRSTR("inline code has background ANSI", r, "[48;5;236");
    free(r);

    /* Escaped backtick */
    r = hermes_markdown_render("not \\`code", 80);
    TEST_NOT_NULL("escaped backtick returns non-NULL", r);
    TEST_STRSTR("escaped backtick shows backtick", r, "`");
    free(r);
}

/* ================================================================
 *  5. hermes_markdown_render — links
 * ================================================================ */
static void test_render_links(void) {
    printf("\n--- render: links ---\n");

    char *r = hermes_markdown_render("[text](https://example.com)", 80);
    TEST_NOT_NULL("link returns non-NULL", r);
    TEST_STRSTR("link contains 'text'", r, "text");
    /* Link is underlined blue */
    TEST_STRSTR("link has underline ANSI", r, "[4;38;5;33");
    free(r);

    /* Unclosed link (no closing paren) — should render as-is */
    r = hermes_markdown_render("[broken](url", 80);
    TEST_NOT_NULL("broken link returns non-NULL", r);
    TEST_STRSTR("broken link contains 'broken'", r, "broken");
    free(r);
}

/* ================================================================
 *  6. hermes_markdown_render — fenced code blocks
 * ================================================================ */
static void test_render_code_blocks(void) {
    printf("\n--- render: fenced code blocks ---\n");

    char *r = hermes_markdown_render("```c\nint x = 1;\n```\n", 80);
    TEST_NOT_NULL("code block returns non-NULL", r);
    TEST_STRSTR("code block contains 'int x = 1'", r, "int x = 1");
    TEST_STRSTR("code block has language 'c'", r, "c");
    TEST_STRSTR("code block has background ANSI", r, "[48;5;235m");
    free(r);

    /* Code block with no closing fence — all content inside */
    r = hermes_markdown_render("```\nopen content", 80);
    TEST_NOT_NULL("unclosed code block returns non-NULL", r);
    TEST_STRSTR("unclosed code block contains 'open content'", r, "open content");
    free(r);
}

/* ================================================================
 *  7. hermes_markdown_render — blockquotes
 * ================================================================ */
static void test_render_blockquotes(void) {
    printf("\n--- render: blockquotes ---\n");

    char *r = hermes_markdown_render("> quoted text", 80);
    TEST_NOT_NULL("blockquote returns non-NULL", r);
    TEST_STRSTR("blockquote contains 'quoted text'", r, "quoted text");
    /* Blockquote has │ prefix */
    TEST_STRSTR("blockquote has pipe prefix", r, "\xE2\x94\x82");  /* UTF-8 │ */
    free(r);

    r = hermes_markdown_render(">nospace", 80);
    TEST_NOT_NULL("no-space blockquote returns non-NULL", r);
    TEST_STRSTR("no-space blockquote contains 'nospace'", r, "nospace");
    free(r);
}

/* ================================================================
 *  8. hermes_markdown_render — lists
 * ================================================================ */
static void test_render_lists(void) {
    printf("\n--- render: lists ---\n");

    char *r = hermes_markdown_render("- item one\n- item two\n", 80);
    TEST_NOT_NULL("list returns non-NULL", r);
    TEST_STRSTR("list contains 'item one'", r, "item one");
    TEST_STRSTR("list contains 'item two'", r, "item two");
    /* List has bullet character */
    TEST_STRSTR("list has bullet", r, "\xE2\x80\xA2");  /* UTF-8 • */
    free(r);

    /* Star and plus lists */
    r = hermes_markdown_render("* star item\n+ plus item\n", 80);
    TEST_NOT_NULL("star/plus list returns non-NULL", r);
    TEST_STRSTR("star list contains 'star item'", r, "star item");
    TEST_STRSTR("plus list contains 'plus item'", r, "plus item");
    free(r);
}

/* ================================================================
 *  9. hermes_markdown_render — horizontal rules
 * ================================================================ */
static void test_render_hr(void) {
    printf("\n--- render: horizontal rules ---\n");

    char *r = hermes_markdown_render("---\n", 80);
    TEST_NOT_NULL("HR (---) returns non-NULL", r);
    TEST_STRSTR("HR has separator line", r, "\xE2\x94\x80");  /* UTF-8 ─ */
    free(r);

    r = hermes_markdown_render("***\n", 80);
    TEST_NOT_NULL("HR (***) returns non-NULL", r);
    free(r);

    r = hermes_markdown_render("___\n", 80);
    TEST_NOT_NULL("HR (___) returns non-NULL", r);
    free(r);
}

/* ================================================================
 *  10. hermes_markdown_render — mixed content
 * ================================================================ */
static void test_render_mixed(void) {
    printf("\n--- render: mixed content ---\n");

    const char *md =
        "# Title\n\n"
        "This is **bold** and *italic* and `code`.\n\n"
        "> A quote\n\n"
        "- list item\n\n"
        "```\ncode block\n```\n";

    char *r = hermes_markdown_render(md, 80);
    TEST_NOT_NULL("mixed content returns non-NULL", r);
    TEST_STRSTR("mixed contains 'Title'", r, "Title");
    TEST_STRSTR("mixed contains 'bold'", r, "bold");
    TEST_STRSTR("mixed contains 'italic'", r, "italic");
    TEST_STRSTR("mixed contains 'code'", r, "code");
    TEST_STRSTR("mixed contains 'A quote'", r, "A quote");
    TEST_STRSTR("mixed contains 'list item'", r, "list item");
    TEST_STRSTR("mixed contains 'code block'", r, "code block");
    free(r);
}

/* ================================================================
 *  11. hermes_markdown_render — edge cases
 * ================================================================ */
static void test_render_edge(void) {
    printf("\n--- render: edge cases ---\n");

    TEST_NULL("NULL input returns NULL", hermes_markdown_render(NULL, 80));

    /* Very long single line */
    char long_line[4096];
    memset(long_line, 'a', 4000);
    long_line[4000] = '\0';
    char *r = hermes_markdown_render(long_line, 80);
    TEST_NOT_NULL("long line returns non-NULL", r);
    free(r);
}

/* ================================================================
 *  12. hermes_markdown_strip — plain text
 * ================================================================ */
static void test_strip_plain(void) {
    printf("\n--- strip: plain text ---\n");

    char *s = hermes_markdown_strip("hello world");
    TEST_STR_EQ("strip plain text", s, "hello world");
    free(s);

    TEST_NULL("strip NULL returns NULL", hermes_markdown_strip(NULL));

    s = hermes_markdown_strip("");
    TEST_STR_EQ("strip empty string", s, "");
    free(s);
}

/* ================================================================
 *  13. hermes_markdown_strip — headers
 * ================================================================ */
static void test_strip_headers(void) {
    printf("\n--- strip: headers ---\n");

    char *s = hermes_markdown_strip("# H1\n## H2\n### H3\n");
    TEST_NOT_EMPTY("strip headers returns non-empty", s);
    TEST_STRSTR("strip headers contains H1", s, "H1");
    TEST_STRSTR("strip headers contains H2", s, "H2");
    TEST_STRSTR("strip headers contains H3", s, "H3");
    /* No # markers in output */
    TEST("strip headers removes # markup", strchr(s, '#') == NULL);
    free(s);
}

/* ================================================================
 *  14. hermes_markdown_strip — bold and italic
 * ================================================================ */
static void test_strip_bold_italic(void) {
    printf("\n--- strip: bold & italic ---\n");

    char *s = hermes_markdown_strip("**bold** and *italic*");
    TEST_STR_EQ("strip bold+italic", s, "bold and italic");
    free(s);

    s = hermes_markdown_strip("**_both_** and *_mix_*");
    TEST_NOT_EMPTY("strip mixed bold+italic", s);
    TEST("strip mixed has no asterisks", strchr(s, '*') == NULL);
    free(s);
}

/* ================================================================
 *  15. hermes_markdown_strip — inline code
 * ================================================================ */
static void test_strip_inline_code(void) {
    printf("\n--- strip: inline code ---\n");

    char *s = hermes_markdown_strip("Use `code` here");
    /* Inline code strips backticks but keeps content */
    TEST_STR_EQ("strip inline code", s, "Use code here");
    free(s);
}

/* ================================================================
 *  16. hermes_markdown_strip — links
 * ================================================================ */
static void test_strip_links(void) {
    printf("\n--- strip: links ---\n");

    char *s = hermes_markdown_strip("[text](https://example.com)");
    TEST_STR_EQ("strip link keeps text, drops URL", s, "text");
    free(s);
}

/* ================================================================
 *  17. hermes_markdown_strip — code blocks
 * ================================================================ */
static void test_strip_code_blocks(void) {
    printf("\n--- strip: code blocks ---\n");

    char *s = hermes_markdown_strip("```c\nint x = 1;\n```\npost");
    TEST_NOT_NULL("strip code block", s);
    TEST_STRSTR("strip code block preserves content", s, "int x = 1");
    TEST_STRSTR("strip code block preserves post", s, "post");
    free(s);
}

/* ================================================================
 *  18. hermes_markdown_strip — list markers
 * ================================================================ */
static void test_strip_lists(void) {
    printf("\n--- strip: lists ---\n");

    char *s = hermes_markdown_strip("- item one\n- item two\n");
    TEST_NOT_NULL("strip list", s);
    TEST_STRSTR("strip list contains 'item one'", s, "item one");
    TEST_STRSTR("strip list contains 'item two'", s, "item two");
    free(s);
}

/* ================================================================
 *  19. hermes_markdown_strip — image syntax
 * ================================================================ */
static void test_strip_images(void) {
    printf("\n--- strip: images ---\n");

    char *s = hermes_markdown_strip("![alt](img.png) text");
    TEST_STR_EQ("strip image removes markup, keeps alt+text", s, "alt text");
    free(s);
}

/* ================================================================
 *  20. hermes_markdown_strip — mixed markdown
 * ================================================================ */
static void test_strip_mixed(void) {
    printf("\n--- strip: mixed ---\n");

    const char *md =
        "# Title\n\n"
        "This is **bold** and `code`.\n\n"
        "[link](url) here\n\n"
        "> quote\n\n"
        "- list\n";

    char *s = hermes_markdown_strip(md);
    TEST_NOT_NULL("strip mixed", s);
    TEST_STRSTR("strip mixed contains 'Title'", s, "Title");
    TEST_STRSTR("strip mixed contains 'bold'", s, "bold");
    TEST_STRSTR("strip mixed contains 'code'", s, "code");
    TEST_STRSTR("strip mixed contains 'link'", s, "link");
    TEST_STRSTR("strip mixed contains 'here'", s, "here");
    TEST_STRSTR("strip mixed contains 'quote'", s, "quote");
    TEST_STRSTR("strip mixed contains 'list'", s, "list");
    free(s);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Markdown Render Test Suite ===\n");

    test_render_plain();
    test_render_headers();
    test_render_bold_italic();
    test_render_inline_code();
    test_render_links();
    test_render_code_blocks();
    test_render_blockquotes();
    test_render_lists();
    test_render_hr();
    test_render_mixed();
    test_render_edge();
    test_strip_plain();
    test_strip_headers();
    test_strip_bold_italic();
    test_strip_inline_code();
    test_strip_links();
    test_strip_code_blocks();
    test_strip_lists();
    test_strip_images();
    test_strip_mixed();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
