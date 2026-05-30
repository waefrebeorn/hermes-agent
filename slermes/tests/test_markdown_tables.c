/* Markdown tables tests — verify CJK-aware table realignment.
 *
 * Tests:
 * 1. split_table_row: basic 3 cells
 * 2. split_table_row: leading pipe
 * 3. split_table_row: trailing empty
 * 4. is_table_divider: basic
 * 5. is_table_divider: with colons
 * 6. is_table_divider: not divider (text row)
 * 7. looks_like_table_row: yes
 * 8. looks_like_table_row: no pipes
 * 9. looks_like_table_row: 2+ pipes without leading
 * 10. disp_width: ASCII
 * 11. disp_width: empty/NULL
 * 12. realign: basic table
 * 13. realign: passthrough non-table text
 * 14. realign: table with CJK characters
 * 15. realign: table wide enough to trigger vertical fallback
 */

#include "hermes_markdown_tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s\n", name); \
    } else { \
        passed++; \
    } \
} while(0)

int main(void)
{
    size_t n;
    char **cells;

    /* Test 1: split basic */
    cells = hermes_split_table_row("| a | b | c |", &n);
    TEST("split 3 cells", cells && n == 3);
    if (cells) {
        TEST("cell0='a'", strcmp(cells[0], "a") == 0);
        TEST("cell1='b'", strcmp(cells[1], "b") == 0);
        TEST("cell2='c'", strcmp(cells[2], "c") == 0);
        for (size_t i = 0; i < n; i++) free(cells[i]);
        free(cells);
    }

    /* Test 2: split with leading pipe only */
    cells = hermes_split_table_row("a | b", &n);
    TEST("split 2 cells no pipes", cells && n == 2);
    if (cells) {
        TEST("no-lead cell0='a'", strcmp(cells[0], "a") == 0);
        TEST("no-lead cell1='b'", strcmp(cells[1], "b") == 0);
        for (size_t i = 0; i < n; i++) free(cells[i]);
        free(cells);
    }

    /* Test 3: split divider */
    cells = hermes_split_table_row("|---|---|", &n);
    TEST("divider split n=2", cells && n == 2);
    if (cells) {
        TEST("divider0", strcmp(cells[0], "---") == 0);
        TEST("divider1", strcmp(cells[1], "---") == 0);
        for (size_t i = 0; i < n; i++) free(cells[i]);
        free(cells);
    }

    /* Test 4: is_table_divider */
    TEST("divider yes", hermes_is_table_divider("|---|---|") == true);
    TEST("divider with colons", hermes_is_table_divider("|:---|---:|") == true);
    TEST("divider text row", hermes_is_table_divider("| a | b |") == false);
    TEST("divider empty", hermes_is_table_divider("") == false);

    /* Test 5: looks_like_table_row */
    TEST("looks like pipe-start", hermes_looks_like_table_row("| a | b |") == true);
    TEST("looks like no pipes", hermes_looks_like_table_row("hello") == false);
    TEST("looks like 2+ pipes", hermes_looks_like_table_row("a | b | c") == true);
    TEST("looks like single pipe", hermes_looks_like_table_row("a | b") == false);
    TEST("looks like empty", hermes_looks_like_table_row("") == false);
    TEST("looks like NULL", hermes_looks_like_table_row(NULL) == false);

    /* Test 6: disp_width */
    TEST("disp ASCII", hermes_disp_width("hello") == 5);
    TEST("disp empty", hermes_disp_width("") == 0);
    TEST("disp NULL", hermes_disp_width(NULL) == 0);

    /* Test 7: realign simple table */
    {
        char *r = hermes_realign_markdown_tables(
            "| A | B |\n|---|---|\n| 1 | 2 |\n", 0);
        TEST("realign result", r != NULL);
        if (r) {
            TEST("realign header 'A'", strstr(r, "A") != NULL);
            TEST("realign data '1'", strstr(r, "1") != NULL);
            TEST("realign divider has dashes", strchr(r, '-') != NULL);
            free(r);
        }
    }

    /* Test 8: realign passthrough non-table */
    {
        char *r = hermes_realign_markdown_tables("Hello world\nNo pipes here", 0);
        TEST("passthrough", r && strcmp(r, "Hello world\nNo pipes here") == 0);
        free(r);
    }

    /* Test 9: realign CJK table */
    {
        char *r = hermes_realign_markdown_tables(
            "| 名称 | 值 |\n|------|----|\n| 测试 | 123 |\n", 0);
        TEST("CJK table", r != NULL);
        if (r) {
            TEST("CJK header", strstr(r, "名称") != NULL);
            TEST("CJK data", strstr(r, "测试") != NULL);
            free(r);
        }
    }

    /* Test 10: realign NULL input */
    {
        char *r = hermes_realign_markdown_tables(NULL, 0);
        TEST("NULL input", r && strcmp(r, "") == 0);
        free(r);
    }

    /* Test 11: realign empty string */
    {
        char *r = hermes_realign_markdown_tables("", 0);
        TEST("empty input", r && strcmp(r, "") == 0);
        free(r);
    }

    /* Test 12: realign table one-row */
    {
        char *r = hermes_realign_markdown_tables(
            "| X |\n|---|\n| 1 |\n", 0);
        TEST("single column", r != NULL);
        free(r);
    }

    /* Test 13: realign with very narrow width (vertical fallback) */
    {
        char *r = hermes_realign_markdown_tables(
            "| ColA | ColB | ColC |\n|------|------|------|\n| val1 | val2 | val3 |\n", 20);
        TEST("narrow width", r != NULL);
        free(r);
    }

    printf("markdown_tables: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
