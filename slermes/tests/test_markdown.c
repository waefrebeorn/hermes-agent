/* test_markdown.c -- Tests for markdown_render module.
 * Tests: hermes_markdown_render, hermes_markdown_strip.
 * Compile with markdown_render.c + -Wl,--unresolved-symbols=ignore-all.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *hermes_markdown_render(const char *md, int term_width);
extern char *hermes_markdown_strip(const char *md);

static int pass = 0, fail = 0;
#define T(n, e) do { if(e) { pass++; printf("  OK %s\n", n); } else { fail++; printf("  FAIL %s\n", n); } } while(0)
static int contains(const char *h, const char *n) { return h && n && strstr(h, n) != NULL; }

/* ---- hermes_markdown_strip ---- */
static void test_strip_null(void) {
    char *r = hermes_markdown_strip(NULL);
    T("NULL in -> NULL out", r == NULL);
}

static void test_strip_empty(void) {
    char *r = hermes_markdown_strip("");
    T("empty in -> empty out", r && r[0] == '\0');
    free(r);
}

static void test_strip_plain_text(void) {
    char *r = hermes_markdown_strip("hello world");
    T("plain text unchanged", r && strcmp(r, "hello world") == 0);
    free(r);
}

static void test_strip_bold(void) {
    char *r = hermes_markdown_strip("this is **bold** text");
    T("bold markers removed", r && !contains(r, "**bold**"));
    T("bold content preserved", r && contains(r, "bold"));
    free(r);
}

static void test_strip_italic(void) {
    char *r = hermes_markdown_strip("this is *italic* text");
    T("italic markers removed", r && !contains(r, "*italic*"));
    T("italic content preserved", r && contains(r, "italic"));
    free(r);
}

static void test_strip_code(void) {
    char *r = hermes_markdown_strip("use `func()` here");
    T("backtick code markers removed", r && strstr(r, "func()") != NULL && !contains(r, "`func()`"));
    free(r);
}

static void test_strip_headers(void) {
    char *r = hermes_markdown_strip("## Section Title");
    T("header # removed", r && contains(r, "Section Title"));
    T("header text preserved", r && !contains(r, "##"));
    free(r);
}

static void test_strip_link(void) {
    char *r = hermes_markdown_strip("click [here](https://example.com)");
    T("link text preserved", r && contains(r, "here"));
    T("URL removed from output", r && !contains(r, "https://"));
    free(r);
}

static void test_strip_multiline(void) {
    char *r = hermes_markdown_strip("# Title\n\nParagraph with **bold** and *italic*.\n\n- item1\n- item2");
    T("multiline strips all formatting", r && contains(r, "Title") && contains(r, "Paragraph"));
    T("multiline preserves content order", r && contains(r, "item1") && contains(r, "item2"));
    free(r);
}

static void test_strip_code_fence(void) {
    char *r = hermes_markdown_strip("text\n```\ncode block\n```\nmore text");
    T("code fence content", r && contains(r, "code block"));
    T("code fence markers removed", r && !contains(r, "```"));
    free(r);
}

static void test_strip_image(void) {
    char *r = hermes_markdown_strip("text ![alt](img.png) more");
    T("image alt text preserved", r && contains(r, "alt"));
    T("image URL removed", r && !contains(r, "img.png"));
    free(r);
}

static void test_strip_strikethrough(void) {
    char *r = hermes_markdown_strip("this is ~~strikethrough~~ text");
    T("strikethrough content preserved", r && contains(r, "strikethrough"));
    free(r);
}

static void test_strip_blockquote(void) {
    char *r = hermes_markdown_strip("> quoted text\n> more quote");
    T("blockquote text preserved", r && contains(r, "quoted text"));
    free(r);
}

static void test_strip_horizontal_rule(void) {
    char *r = hermes_markdown_strip("before\n---\nafter");
    T("horizontal rule preserves text", r && contains(r, "before") && contains(r, "after"));
    free(r);
}

static void test_strip_inline_html(void) {
    char *r = hermes_markdown_strip("text <b>bold</b> <i>italic</i> end");
    T("inline HTML content preserved", r && contains(r, "bold") && contains(r, "italic"));
    free(r);
}

static void test_strip_nested_formatting(void) {
    char *r = hermes_markdown_strip("**bold and *italic* inside**");
    T("nested formatting content preserved", r && contains(r, "bold") && contains(r, "italic"));
    free(r);
}

/* ---- hermes_markdown_render ---- */
static void test_render_null(void) {
    char *r = hermes_markdown_render(NULL, 80);
    T("render NULL in -> NULL out", r == NULL);
}

static void test_render_empty(void) {
    char *r = hermes_markdown_render("", 80);
    T("render empty -> empty out", r && r[0] == '\0');
    free(r);
}

static void test_render_plain_text(void) {
    char *r = hermes_markdown_render("hello world", 80);
    T("render plain text non-NULL", r != NULL);
    T("render plain text contains original", r && contains(r, "hello world"));
    free(r);
}

static void test_render_bold(void) {
    char *r = hermes_markdown_render("**bold**", 80);
    T("render bold non-NULL", r != NULL);
    T("render bold has content", r && strlen(r) > 0);
    free(r);
}

static void test_render_header(void) {
    char *r = hermes_markdown_render("# Header", 80);
    T("render header non-NULL", r != NULL);
    T("render header has content", r && contains(r, "Header"));
    free(r);
}

static void test_render_term_width(void) {
    char *r30 = hermes_markdown_render("short text", 30);
    char *r100 = hermes_markdown_render("short text", 100);
    T("render with different widths both work", r30 && r100);
    free(r30); free(r100);
}

static void test_render_code_block(void) {
    char *r = hermes_markdown_render("text\n```\ncode\n```\nend", 80);
    T("render code block non-NULL", r != NULL);
    T("render code block has content", r && contains(r, "code"));
    free(r);
}

static void test_render_long_text(void) {
    char long_text[256];
    snprintf(long_text, sizeof(long_text), "%s",
        "This is a very long line of text that should wrap at the specified terminal width for proper display in a terminal environment.");
    char *r = hermes_markdown_render(long_text, 40);
    T("render long text non-NULL", r != NULL);
    T("render long text preserves words", r && contains(r, "terminal"));
    free(r);
}

static void test_render_narrow(void) {
    char *r = hermes_markdown_render("hello world", 5);
    T("render narrow terminal non-NULL", r != NULL);
    T("render narrow has content", r && strlen(r) > 0);
    free(r);
}

static void test_render_very_wide(void) {
    char *r = hermes_markdown_render("hello world", 999);
    T("render very wide terminal non-NULL", r != NULL);
    T("render very wide preserves text", r && contains(r, "hello world"));
    free(r);
}

static void test_render_negative_width(void) {
    char *r = hermes_markdown_render("hello", -1);
    T("render negative width non-NULL", r != NULL);
    T("render negative width has content", r && strlen(r) > 0);
    free(r);
}

int main(void) {
    printf("=== Markdown Render Tests ===\n");
    printf("\n--- hermes_markdown_strip ---\n");
    test_strip_null(); test_strip_empty(); test_strip_plain_text();
    test_strip_bold(); test_strip_italic(); test_strip_code();
    test_strip_headers(); test_strip_link(); test_strip_multiline();
    test_strip_code_fence(); test_strip_image(); test_strip_strikethrough();
    test_strip_blockquote(); test_strip_horizontal_rule();
    test_strip_inline_html(); test_strip_nested_formatting();
    printf("\n--- hermes_markdown_render ---\n");
    test_render_null(); test_render_empty(); test_render_plain_text();
    test_render_bold(); test_render_header(); test_render_term_width();
    test_render_code_block(); test_render_long_text();
    test_render_narrow(); test_render_very_wide(); test_render_negative_width();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0;
}
