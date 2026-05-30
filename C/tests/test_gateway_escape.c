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

    /* ================================================================
     * 4. gw_utf16_len
     * ================================================================ */
    printf("\n--- gw_utf16_len ---\n");

    {
        size_t r = gw_utf16_len(NULL);
        TEST("NULL input returns 0", r == 0);
    }
    {
        size_t r = gw_utf16_len("");
        TEST("empty string", r == 0);
    }
    {
        size_t r = gw_utf16_len("hello");
        TEST("ASCII only 1:1", r == 5);
    }
    {
        size_t r = gw_utf16_len("héllo");  /* é = U+00E9, 2 UTF-8 bytes, 1 UTF-16 unit */
        TEST("accented 2-byte char", r == 5);
    }
    {
        size_t r = gw_utf16_len("\xe4\xb8\xad\xe6\x96\x87");  /* 中文 — each is 3 UTF-8 bytes, 1 UTF-16 unit */
        TEST("CJK 3-byte chars", r == 2);
    }
    {
        /* 😀 (U+1F600) = 4 UTF-8 bytes, 2 UTF-16 units (surrogate pair) */
        size_t r = gw_utf16_len("\xf0\x9f\x98\x80");
        TEST("emoji 4-byte = 2 units", r == 2);
    }
    {
        /* hello😀world — emoji = 2 units */
        size_t r = gw_utf16_len("hello\xf0\x9f\x98\x80world");
        TEST("emoji in context counts 2", r == 12);  /* 5 + 2 + 5 */
    }
    {
        /* Two emoji: 😀😎 (U+1F600, U+1F60E) */
        size_t r = gw_utf16_len("\xf0\x9f\x98\x80\xf0\x9f\x98\x8e");
        TEST("two emoji = 4 units", r == 4);
    }
    {
        size_t r = gw_utf16_len("a\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80" "b");
        /* a(1) + é(1) + 中(1) + 😀(2) + b(1) = 6 */
        TEST("mixed ASCII+2byte+3byte+4byte", r == 6);
    }

    /* ================================================================
     * 5. gw_prefix_within_utf16_limit
     * ================================================================ */
    printf("\n--- gw_prefix_within_utf16_limit ---\n");

    {
        char *r = gw_prefix_within_utf16_limit(NULL, 100);
        TEST("NULL input returns NULL", r == NULL);
    }
    {
        char *r = gw_prefix_within_utf16_limit("hello", 0);
        TEST("limit 0 returns NULL", r == NULL);
    }
    {
        char *r = gw_prefix_within_utf16_limit("hello", 100);
        TEST("within limit returns full string", r && strcmp(r, "hello") == 0);
        free(r);
    }
    {
        char *r = gw_prefix_within_utf16_limit("hello", 5);
        TEST("exact limit returns full string", r && strcmp(r, "hello") == 0);
        free(r);
    }
    {
        char *r = gw_prefix_within_utf16_limit("hello world", 5);
        TEST("truncate ASCII at 5", r && strcmp(r, "hello") == 0);
        free(r);
    }
    {
        /* é = 1 UTF-16 unit, limit 4 should include it */
        char *r = gw_prefix_within_utf16_limit("h\xc3\xa9llo", 4);
        TEST("truncate at accented boundary", r && strcmp(r, "h\xc3\xa9ll") == 0);
        free(r);
    }
    {
        /* 😀 = 2 UTF-16 units, limit 6 with "hi😀" (hi=2 + 😀=2 = 4) should allow "wo" as well */
        char *r = gw_prefix_within_utf16_limit("hi\xf0\x9f\x98\x80world", 6);
        TEST("emoji within 6-unit limit", r && strcmp(r, "hi\xf0\x9f\x98\x80wo") == 0);
        free(r);
    }
    {
        /* 😀 = 2 UTF-16 units, limit 4, "hi😀" = 4 */
        char *r = gw_prefix_within_utf16_limit("hi\xf0\x9f\x98\x80world", 4);
        TEST("emoji at exact limit", r && strcmp(r, "hi\xf0\x9f\x98\x80") == 0);
        free(r);
    }
    {
        /* 😀 = 2 UTF-16 units, limit 3, "hi" = 2 < 3, next 😀 would be 4 > 3 */
        char *r = gw_prefix_within_utf16_limit("hi\xf0\x9f\x98\x80world", 3);
        TEST("emoji pushes over limit, stops before", r && strcmp(r, "hi") == 0);
        free(r);
    }
    {
        /* 中文 each = 1 UTF-16 unit, limit 1 = "中" */
        char *r = gw_prefix_within_utf16_limit("\xe4\xb8\xad\xe6\x96\x87", 1);
        TEST("truncate CJK at 1", r && strcmp(r, "\xe4\xb8\xad") == 0);
        free(r);
    }

    /*
     * 6. gw_custom_unit_to_cp
     */
    printf("\n--- gw_custom_unit_to_cp ---\n");
    {
        /* Simple char-counting len_fn */
        int count_char(const char *s, int n) { return n; } /* counts characters = returns n */

        TEST("custom full match", gw_custom_unit_to_cp("hello", 5, 5, count_char) == 5);
        TEST("custom budget 3", gw_custom_unit_to_cp("hello", 5, 3, count_char) == 3);
        TEST("custom zero budget", gw_custom_unit_to_cp("hello", 5, 0, count_char) == 0);
        TEST("custom negative budget", gw_custom_unit_to_cp("hello", 5, -1, count_char) == 0);
        TEST("custom NULL input", gw_custom_unit_to_cp(NULL, 5, 5, count_char) == 0);
        TEST("custom NULL fn", gw_custom_unit_to_cp("hello", 5, 5, NULL) == 0);
        TEST("custom empty string", gw_custom_unit_to_cp("", 0, 5, count_char) == 0);
    }

    /* Summary */
    printf("\n=== M07 Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
