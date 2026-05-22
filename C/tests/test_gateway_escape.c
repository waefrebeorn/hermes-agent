/*
 * test_gateway_escape.c — M07: Gateway Telegram escape modes
 *
 * Tests gw_markdown_to_html, gw_markdown_v2_escape, gw_truncate_message
 * for edge cases: HTML/MarkdownV2 escaping, truncation boundaries.
 */
#include "hermes_gateway.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

int main(void) {
    printf("=== M07: Gateway escape modes ===\n\n");

    /* ================================================================
     * 1. gw_markdown_to_html
     * ================================================================ */
    printf("--- gw_markdown_to_html ---\n");

    /* NULL / empty */
    {
        char *r = gw_markdown_to_html(NULL);
        TEST("NULL input returns NULL", r == NULL);
    }
    {
        char *r = gw_markdown_to_html("");
        TEST("empty input", r && strcmp(r, "") == 0);
        free(r);
    }
    {
        char *r = gw_markdown_to_html("hello");
        TEST("plain text", r && strcmp(r, "hello") == 0);
        free(r);
    }

    /* Bold */
    {
        char *r = gw_markdown_to_html("**bold**");
        TEST("bold **text**", r && strcmp(r, "<b>bold</b>") == 0);
        free(r);
    }
    {
        char *r = gw_markdown_to_html("before **bold** after");
        TEST("bold with context", r && strcmp(r, "before <b>bold</b> after") == 0);
        free(r);
    }

    /* Italic */
    {
        char *r = gw_markdown_to_html("*italic*");
        TEST("italic *text*", r && strcmp(r, "<i>italic</i>") == 0);
        free(r);
    }
    {
        char *r = gw_markdown_to_html("before *italic* after");
        TEST("italic with context", r && strcmp(r, "before <i>italic</i> after") == 0);
        free(r);
    }

    /* Code */
    {
        char *r = gw_markdown_to_html("`code`");
        TEST("code `text`", r && strcmp(r, "<code>code</code>") == 0);
        free(r);
    }
    {
        char *r = gw_markdown_to_html("before `code` after");
        TEST("code with context", r && strcmp(r, "before <code>code</code> after") == 0);
        free(r);
    }

    /* HTML entity escaping */
    {
        char *r = gw_markdown_to_html("<tag>");
        TEST("HTML < escaped", r && strstr(r, "&lt;") && strstr(r, "&gt;"));
        free(r);
    }
    {
        char *r = gw_markdown_to_html("a & b");
        TEST("HTML & escaped", r && strstr(r, "&amp;"));
        free(r);
    }

    /* Mixed formatting */
    {
        char *r = gw_markdown_to_html("**bold** and *italic* and `code`");
        TEST("mixed formatting",
             r && strstr(r, "<b>bold</b>") &&
             strstr(r, "<i>italic</i>") &&
             strstr(r, "<code>code</code>"));
        free(r);
    }

    /* Nested bold + italic */
    {
        char *r = gw_markdown_to_html("***bold+italic***");
        /* The parser sees ** then *, so it's **bold+italic** with a trailing * */
        TEST("nested ***text*** does not crash", r != NULL);
        free(r);
    }

    /* ================================================================
     * 2. gw_markdown_v2_escape
     * ================================================================ */
    printf("\n--- gw_markdown_v2_escape ---\n");

    /* NULL / empty */
    {
        char *r = gw_markdown_v2_escape(NULL);
        TEST("NULL input returns NULL", r == NULL);
    }
    {
        char *r = gw_markdown_v2_escape("");
        TEST("empty input", r && strcmp(r, "") == 0);
        free(r);
    }
    {
        char *r = gw_markdown_v2_escape("no special chars");
        TEST("plain text", r && strcmp(r, "no special chars") == 0);
        free(r);
    }

    /* Reserved chars that need escaping in MarkdownV2 */
    {
        char *r = gw_markdown_v2_escape("_*[]()~`>#+-=|{}.!");
        TEST("all reserved chars escaped",
             r && strcmp(r, "\\_\\*\\[\\]\\(\\)\\~\\`\\>\\#\\+\\-\\=\\|\\{\\}\\.\\!") == 0);
        free(r);
    }
    {
        char *r = gw_markdown_v2_escape("hello_world");
        TEST("underscore _ escaped", r && strstr(r, "hello\\_world"));
        free(r);
    }
    {
        char *r = gw_markdown_v2_escape("**bold**");
        TEST("asterisks escaped", r && strstr(r, "\\*\\*bold\\*\\*"));
        free(r);
    }
    {
        char *r = gw_markdown_v2_escape("[link](url)");
        TEST("brackets escaped", r && strstr(r, "\\[link\\]\\(url\\)"));
        free(r);
    }
    {
        char *r = gw_markdown_v2_escape("some `code` here");
        TEST("backtick escaped", r && strstr(r, "\\`code\\`"));
        free(r);
    }

    /* ================================================================
     * 3. gw_truncate_message
     * ================================================================ */
    printf("\n--- gw_truncate_message ---\n");

    /* NULL / empty */
    {
        char *r = gw_truncate_message(NULL, 100);
        TEST("NULL input returns NULL", r == NULL);
    }
    {
        char *r = gw_truncate_message("", 100);
        TEST("empty input", r && strcmp(r, "") == 0);
        free(r);
    }
    {
        char *r = gw_truncate_message("short", 100);
        TEST("short text not truncated", r && strcmp(r, "short") == 0);
        free(r);
    }

    /* Truncation at boundary */
    {
        char *r = gw_truncate_message("hello world", 5);
        TEST("truncated to 5 with ellipsis", r && strcmp(r, "hello...") == 0);
        free(r);
    }
    {
        char *r = gw_truncate_message("hello world", 11);
        TEST("exact fit", r && strcmp(r, "hello world") == 0);
        free(r);
    }
    {
        char *r = gw_truncate_message("hello world", 12);
        TEST("above exact", r && strcmp(r, "hello world") == 0);
        free(r);
    }
    {
        char *r = gw_truncate_message("hello bro", 5);
        TEST("word boundary break", r && strcmp(r, "hello...") == 0);
        free(r);
    }

    /* Unicode handling */
    {
        char *r = gw_truncate_message("héllo wörld", 100);
        TEST("unicode preserved", r && strcmp(r, "héllo wörld") == 0);
        free(r);
    }

    /* Truncation does NOT strip markdown (that's done by the send pipeline) */
    {
        char *r = gw_truncate_message("**bold**", 100);
        TEST("truncation preserves markdown", r && strstr(r, "**bold**") != NULL);
        free(r);
    }

    /* Summary */
    printf("\n=== M07 Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
