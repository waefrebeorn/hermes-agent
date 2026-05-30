/*
 * test_tool_result_storage.c — Tests for result_storage preview generation.
 * Tests: full preview, newline-aware truncation, edge cases.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        tests_passed++; \
        printf("  \xe2\x9c\x85 %s\n", name); \
    } else { \
        tests_failed++; \
        printf("  \xe2\x9d\x8c %s\n", name); \
    } \
} while(0)

int main(void)
{
    printf("=== Tool Result Storage Tests ===\n\n");

    /* ── Short content (within limit) ── */
    printf("-- Within Limit --\n");
    {
        bool has_more = false;
        char *preview = tool_result_generate_preview("Hello world", 100, &has_more);
        TEST("short content returned as-is", strcmp(preview, "Hello world") == 0);
        TEST("has_more is false", has_more == false);
        free(preview);
    }

    /* ── Long content with no newlines ── */
    printf("\n-- No Newlines --\n");
    {
        /* Build a long string with no newlines */
        char long_str[300];
        memset(long_str, 'A', 250);
        long_str[250] = '\0';

        bool has_more = false;
        char *preview = tool_result_generate_preview(long_str, 100, &has_more);
        TEST("truncated to exact max_chars", (int)strlen(preview) == 100);
        TEST("has_more is true", has_more == true);
        /* Should start with A... */
        TEST("starts with A", preview[0] == 'A');
        free(preview);
    }

    /* ── Long content with newline at boundary ── */
    printf("\n-- Newline at Boundary --\n");
    {
        /* Content: 50 chars + \n + 200 more chars.
         * Newline at pos 50, max_chars=100. 50 > 50 == false → truncate at 100. */
        char content[300];
        memset(content, 'X', 50);
        content[50] = '\n';
        memset(content + 51, 'Y', 200);
        content[251] = '\0';

        bool has_more = false;
        char *preview = tool_result_generate_preview(content, 100, &has_more);
        TEST("truncated to max_chars (newline at exact halfway)", (int)strlen(preview) == 100);
        TEST("has_more is true", has_more == true);
        free(preview);
    }

    /* ── Newline before halfway point (should not use it) ── */
    printf("\n-- Newline Before Halfway --\n");
    {
        /* \n at position 10, max_chars=100 */
        char content[300];
        memset(content, 'Z', 10);
        content[10] = '\n';
        memset(content + 11, 'W', 200);
        content[211] = '\0';

        bool has_more = false;
        char *preview = tool_result_generate_preview(content, 100, &has_more);
        TEST("truncated to max_chars (newline too early)", (int)strlen(preview) == 100);
        TEST("has_more is true", has_more == true);
        free(preview);
    }

    /* ── Exact fit ── */
    printf("\n-- Exact Fit --\n");
    {
        char exact[50];
        memset(exact, 'B', 49);
        exact[49] = '\0';

        bool has_more = false;
        char *preview = tool_result_generate_preview(exact, 50, &has_more);
        TEST("exact fit returns original", strcmp(preview, exact) == 0);
        TEST("has_more is false", has_more == false);
        free(preview);
    }

    /* ── NULL input ── */
    printf("\n-- NULL Input --\n");
    {
        bool has_more = true;
        char *preview = tool_result_generate_preview(NULL, 100, &has_more);
        TEST("NULL returns empty string", strcmp(preview, "") == 0);
        TEST("has_more is false", has_more == false);
        free(preview);
    }

    /* ── Multiple newlines at boundary ── */
    printf("\n-- Multiple Newlines --\n");
    {
        char content[200];
        int pos = 0;
        /* line 1 */
        memset(content + pos, 'A', 30); pos += 30;
        content[pos++] = '\n';
        /* line 2 */
        memset(content + pos, 'B', 40); pos += 40;
        content[pos++] = '\n';
        /* line 3 (past limit) */
        memset(content + pos, 'C', 100); pos += 100;
        content[pos] = '\0';

        bool has_more = false;
        char *preview = tool_result_generate_preview(content, 80, &has_more);
        /* Last newline at pos 71, halfway=40, 71>40=true → truncate at 72 (includes \n) */
        TEST("truncated at 2nd newline (72 chars)", (int)strlen(preview) == 72);
        TEST("has_more is true", has_more == true);
        free(preview);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
