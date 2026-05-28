/*
 * test_csv.c — J06: CSV library unit tests.
 *
 * Tests all public functions in libcsv/csv.h including RFC 4180 edge cases:
 * quoting, escaping, embedded delimiters, embedded newlines, TSV, multi-line.
 */

#include "csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    tests_run++; \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", name); \
        tests_passed++; \
    } \
} while(0)

#define TEST_STR(name, actual, expected) do { \
    tests_run++; \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if (!_a || strcmp(_a, _e) != 0) { \
        printf("  FAIL: %s (got \"%s\", expected \"%s\")\n", name, _a ? _a : "NULL", _e); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", name); \
        tests_passed++; \
    } \
} while(0)

int main(void) {
    printf("=== J06: CSV library tests ===\n\n");

    /* ─── csv_escape() ────────────────────────────────────── */
    printf("--- csv_escape() ---\n");
    {
        char *e = csv_escape("hello", ',');
        TEST_STR("plain text unquoted", e, "hello");
        free(e);

        e = csv_escape("he,llo", ',');
        TEST_STR("embedded comma quoted", e, "\"he,llo\"");
        free(e);

        e = csv_escape("he\"llo", ',');
        TEST_STR("embedded quote escaped", e, "\"he\"\"llo\"");
        free(e);

        e = csv_escape("he\nllo", ',');
        TEST_STR("embedded newline quoted", e, "\"he\nllo\"");
        free(e);

        e = csv_escape(NULL, ',');
        TEST_STR("NULL input", e, "");
        free(e);

        e = csv_escape("", ',');
        TEST_STR("empty string", e, "");
        free(e);
    }

    /* ─── csv_unescape() ──────────────────────────────────── */
    printf("\n--- csv_unescape() ---\n");
    {
        char *u = csv_unescape("hello", ',');
        TEST_STR("plain unquoted unchanged", u, "hello");
        free(u);

        u = csv_unescape("\"hello\"", ',');
        TEST_STR("remove surrounding quotes", u, "hello");
        free(u);

        u = csv_unescape("\"he\"\"llo\"", ',');
        TEST_STR("unescape doubled quotes", u, "he\"llo");
        free(u);

        u = csv_unescape(NULL, ',');
        TEST_STR("NULL input", u, "");
        free(u);
    }

    /* ─── csv_parse_line() ────────────────────────────────── */
    printf("\n--- csv_parse_line() ---\n");
    {
        int count = 0;
        char **fields;

        /* Simple */
        fields = csv_parse_line("a,b,c", ',', &count);
        TEST("simple parse returns 3 fields", count == 3);
        if (fields && count >= 3) {
            TEST_STR("field[0] a", fields[0], "a");
            TEST_STR("field[1] b", fields[1], "b");
            TEST_STR("field[2] c", fields[2], "c");
        }
        csv_free_fields(fields, count);

        /* Quoted field with comma */
        fields = csv_parse_line("a,\"b,c\",d", ',', &count);
        TEST("quoted field parse returns 3", count == 3);
        if (fields && count >= 3) {
            TEST_STR("quoted field[1] b,c", fields[1], "b,c");
        }
        csv_free_fields(fields, count);

        /* Escaped quotes */
        fields = csv_parse_line("\"a\"\"b\",c", ',', &count);
        TEST("escaped quotes returns 2", count == 2);
        if (fields && count >= 1) {
            TEST_STR("unescaped field", fields[0], "a\"b");
        }
        csv_free_fields(fields, count);

        /* Single field */
        fields = csv_parse_line("hello", ',', &count);
        TEST("single field returns 1", count == 1);
        if (fields && count >= 1) {
            TEST_STR("single field", fields[0], "hello");
        }
        csv_free_fields(fields, count);

        /* Empty fields */
        fields = csv_parse_line("a,,c", ',', &count);
        TEST("empty middle field returns 3", count == 3);
        if (fields && count >= 3) {
            TEST_STR("field[1] empty", fields[1], "");
        }
        csv_free_fields(fields, count);

        /* TSV */
        fields = csv_parse_line("a\tb\tc", '\t', &count);
        TEST("tsv returns 3 fields", count == 3);
        if (fields && count >= 3) {
            TEST_STR("tsv field[0]", fields[0], "a");
            TEST_STR("tsv field[1]", fields[1], "b");
        }
        csv_free_fields(fields, count);

        /* NULL input */
        fields = csv_parse_line(NULL, ',', &count);
        TEST("NULL line returns NULL", fields == NULL);

        /* Empty line */
        fields = csv_parse_line("", ',', &count);
        TEST("empty line returns NULL or empty", fields == NULL || count == 0);
        csv_free_fields(fields, count);
    }

    /* ─── csv_reader_open_string() + read_row() ───────────── */
    printf("\n--- csv_reader string mode ---\n");
    {
        const char *data = "a,b,c\n1,2,3\nx,y,z\n";
        csv_reader_t *r = csv_reader_open_string(data, 0, ',');
        TEST("reader string open non-NULL", r != NULL);

        if (r) {
            int count = 0;
            char **row;

            row = csv_reader_read_row(r, &count);
            TEST("row1 has 3 fields", count == 3);
            if (row && count >= 3) {
                TEST_STR("row1 field[0]", row[0], "a");
                TEST_STR("row1 field[1]", row[1], "b");
                TEST_STR("row1 field[2]", row[2], "c");
            }
            csv_free_fields(row, count);

            row = csv_reader_read_row(r, &count);
            TEST("row2 has 3 fields", count == 3);
            if (row && count >= 3) {
                TEST_STR("row2 field[0]", row[0], "1");
                TEST_STR("row2 field[1]", row[1], "2");
            }
            csv_free_fields(row, count);

            row = csv_reader_read_row(r, &count);
            TEST("row3 has 3 fields", count == 3);
            csv_free_fields(row, count);

            row = csv_reader_read_row(r, &count);
            TEST("row4 EOF", row == NULL);

            TEST("row_number is 3", csv_reader_row_number(r) == 3);

            csv_reader_close(r);
        }

        /* Quoted fields in string mode */
        const char *qdata = "name,value\nalice,\"hello, world\"\n";
        r = csv_reader_open_string(qdata, 0, ',');
        if (r) {
            int count = 0;
            char **row;

            row = csv_reader_read_row(r, &count);  /* header */
            csv_free_fields(row, count);

            row = csv_reader_read_row(r, &count);
            TEST("quoted row has 2 fields", count == 2);
            if (row && count >= 2) {
                TEST_STR("quoted value", row[1], "hello, world");
            }
            csv_free_fields(row, count);

            csv_reader_close(r);
        }
    }

    /* ─── csv_writer + file I/O ───────────────────────────── */
    printf("\n--- csv_writer + file I/O ---\n");
    {
        const char *tmp_path = "/tmp/hermes_test_csv.csv";
        csv_writer_t *w = csv_writer_open(tmp_path, ',');
        TEST("writer open non-NULL", w != NULL);

        if (w) {
            TEST("write simple row", csv_writer_write_row_va(w, 3, "a", "b", "c"));
            TEST("write quoted row", csv_writer_write_row_va(w, 2, "hello", "wo,rld"));
            TEST("write empty row", csv_writer_write_row_va(w, 0));
            TEST("write single", csv_writer_write_row_va(w, 1, "x"));
            csv_writer_close(w);
        }

        /* Now read it back */
        csv_reader_t *r = csv_reader_open(tmp_path, ',');
        TEST("reader file open non-NULL", r != NULL);

        if (r) {
            int count = 0;
            char **row;

            row = csv_reader_read_row(r, &count);
            TEST("file row1 has 3 fields", count == 3);
            csv_free_fields(row, count);

            row = csv_reader_read_row(r, &count);
            TEST("file row2 has 2 fields", count == 2);
            if (row && count >= 2) {
                TEST_STR("file row2 quoted value", row[1], "wo,rld");
            }
            csv_free_fields(row, count);

            csv_reader_close(r);
        }

        /* Cleanup */
        remove(tmp_path);
    }

    /* ─── NULL/edge safety ────────────────────────────────── */
    printf("\n--- NULL/edge safety ---\n");
    {
        csv_writer_close(NULL);
        csv_reader_close(NULL);
        csv_free_fields(NULL, 0);
        TEST("writer open NULL path", csv_writer_open(NULL, ',') == NULL);
        TEST("writer open empty path", csv_writer_open("", ',') == NULL);
        TEST("reader open NULL path", csv_reader_open(NULL, ',') == NULL);
        TEST("reader open empty path", csv_reader_open("", ',') == NULL);
        TEST("reader open_string NULL", csv_reader_open_string(NULL, 0, ',') == NULL);
    }

    /* ─── Summary ──────────────────────────────────────────── */
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
