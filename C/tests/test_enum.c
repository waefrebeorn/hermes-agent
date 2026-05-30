/* test_enum.c — J14: libenum tests (ENUM_DECLARE, ENUM_NAME, ENUM_PARSE, ENUM_COUNT).
 * Expanded: large enum, edge values, all-positions parse, zero-value type,
 * macro isolation with multiple types. */
#include "enum.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Test enum with custom names */
#define COLOR_ENUM(X) \
    X(COLOR_RED, "red") \
    X(COLOR_GREEN, "green") \
    X(COLOR_BLUE, "blue") \
    X(COLOR_BLACK, "black")
ENUM_DECLARE(color_t, COLOR_ENUM);

/* Test single-value enum */
#define SINGLE_ENUM(X) \
    X(SINGLE_ONLY, "only")
ENUM_DECLARE(single_t, SINGLE_ENUM);

/* Large enum for scalability testing */
#define LARGE_ENUM(X) \
    X(L0, "zero") \
    X(L1, "one") \
    X(L2, "two") \
    X(L3, "three") \
    X(L4, "four") \
    X(L5, "five") \
    X(L6, "six") \
    X(L7, "seven") \
    X(L8, "eight") \
    X(L9, "nine") \
    X(L10, "ten") \
    X(L11, "eleven") \
    X(L12, "twelve") \
    X(L13, "thirteen") \
    X(L14, "fourteen") \
    X(L15, "fifteen") \
    X(L16, "sixteen") \
    X(L17, "seventeen") \
    X(L18, "eighteen") \
    X(L19, "nineteen") \
    X(L20, "twenty")
ENUM_DECLARE(large_t, LARGE_ENUM);

/* Enum using macro-name fallback (ENUM_STR_NAME would give "L0" not "zero") */
/* Here we use explicit strings to test round-trip */

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
    printf("=== J14: libenum Tests ===\n\n");

    /* --- ENUM_DECLARE / type --- */
    printf("--- ENUM_DECLARE ---\n");
    TEST("COLOR_RED is 0", COLOR_RED == 0);
    TEST("COLOR_GREEN is 1", COLOR_GREEN == 1);
    TEST("COLOR_BLUE is 2", COLOR_BLUE == 2);
    TEST("COLOR_BLACK is 3", COLOR_BLACK == 3);

    /* --- ENUM_NAME --- */
    printf("\n--- ENUM_NAME ---\n");
    TEST("COLOR_RED name", strcmp(ENUM_NAME(color_t, COLOR_ENUM, COLOR_RED), "red") == 0);
    TEST("COLOR_GREEN name", strcmp(ENUM_NAME(color_t, COLOR_ENUM, COLOR_GREEN), "green") == 0);
    TEST("COLOR_BLUE name", strcmp(ENUM_NAME(color_t, COLOR_ENUM, COLOR_BLUE), "blue") == 0);
    TEST("COLOR_BLACK name", strcmp(ENUM_NAME(color_t, COLOR_ENUM, COLOR_BLACK), "black") == 0);
    TEST("out of range returns NULL", ENUM_NAME(color_t, COLOR_ENUM, 999) == NULL);
    TEST("negative returns NULL", ENUM_NAME(color_t, COLOR_ENUM, -1) == NULL);
    TEST("value == count returns NULL", ENUM_NAME(color_t, COLOR_ENUM, 4) == NULL);
    TEST("value just below count works",
         strcmp(ENUM_NAME(color_t, COLOR_ENUM, 3), "black") == 0);

    /* --- ENUM_PARSE --- */
    printf("\n--- ENUM_PARSE ---\n");
    TEST("parse 'red' -> COLOR_RED", ENUM_PARSE(color_t, COLOR_ENUM, "red", COLOR_BLUE) == COLOR_RED);
    TEST("parse 'green' -> COLOR_GREEN", ENUM_PARSE(color_t, COLOR_ENUM, "green", COLOR_BLUE) == COLOR_GREEN);
    TEST("parse 'blue' -> COLOR_BLUE", ENUM_PARSE(color_t, COLOR_ENUM, "blue", COLOR_BLUE) == COLOR_BLUE);
    TEST("parse 'black' -> COLOR_BLACK", ENUM_PARSE(color_t, COLOR_ENUM, "black", COLOR_RED) == COLOR_BLACK);
    TEST("parse 'yellow' -> default COLOR_BLUE", ENUM_PARSE(color_t, COLOR_ENUM, "yellow", COLOR_BLUE) == COLOR_BLUE);
    TEST("parse NULL -> default", ENUM_PARSE(color_t, COLOR_ENUM, NULL, COLOR_RED) == COLOR_RED);
    TEST("parse 'BLACK' -> default (case sensitive)", ENUM_PARSE(color_t, COLOR_ENUM, "BLACK", COLOR_RED) == COLOR_RED);
    TEST("parse empty string -> default",
         ENUM_PARSE(color_t, COLOR_ENUM, "", COLOR_GREEN) == COLOR_GREEN);
    TEST("parse partial 're' -> default",
         ENUM_PARSE(color_t, COLOR_ENUM, "re", COLOR_RED) == COLOR_RED);
    TEST("parse all valid values round-trip",
         ENUM_PARSE(color_t, COLOR_ENUM, "red", -1) == 0 &&
         ENUM_PARSE(color_t, COLOR_ENUM, "green", -1) == 1 &&
         ENUM_PARSE(color_t, COLOR_ENUM, "blue", -1) == 2 &&
         ENUM_PARSE(color_t, COLOR_ENUM, "black", -1) == 3);

    /* --- ENUM_COUNT --- */
    printf("\n--- ENUM_COUNT ---\n");
    TEST("color count is 4", ENUM_COUNT(color_t, COLOR_ENUM) == 4);
    TEST("single count is 1", ENUM_COUNT(single_t, SINGLE_ENUM) == 1);
    TEST("large count is 21", ENUM_COUNT(large_t, LARGE_ENUM) == 21);

    /* --- Edge: single value --- */
    printf("\n--- Single value ---\n");
    TEST("SINGLE_ONLY name", strcmp(ENUM_NAME(single_t, SINGLE_ENUM, SINGLE_ONLY), "only") == 0);
    TEST("SINGLE_ONLY is 0", SINGLE_ONLY == 0);
    TEST("single enum value==count returns NULL",
         ENUM_NAME(single_t, SINGLE_ENUM, 1) == NULL);

    /* --- Large enum --- */
    printf("\n--- Large enum (21 values) ---\n");
    TEST("L0 name zero", strcmp(ENUM_NAME(large_t, LARGE_ENUM, L0), "zero") == 0);
    TEST("L10 name ten", strcmp(ENUM_NAME(large_t, LARGE_ENUM, L10), "ten") == 0);
    TEST("L20 name twenty", strcmp(ENUM_NAME(large_t, LARGE_ENUM, L20), "twenty") == 0);
    TEST("large out of range NULL",
         ENUM_NAME(large_t, LARGE_ENUM, 21) == NULL);
    TEST("large negative NULL",
         ENUM_NAME(large_t, LARGE_ENUM, -5) == NULL);
    TEST("parse first 'zero' -> 0",
         ENUM_PARSE(large_t, LARGE_ENUM, "zero", -1) == 0);
    TEST("parse middle 'ten' -> 10",
         ENUM_PARSE(large_t, LARGE_ENUM, "ten", -1) == 10);
    TEST("parse last 'twenty' -> 20",
         ENUM_PARSE(large_t, LARGE_ENUM, "twenty", -1) == 20);
    TEST("parse missing in large -> default",
         ENUM_PARSE(large_t, LARGE_ENUM, "missing", 5) == 5);

    /* Verify all positions round-trip for large enum */
    int all_ok = 1;
    const char *large_names[] = {"zero","one","two","three","four","five","six",
        "seven","eight","nine","ten","eleven","twelve","thirteen","fourteen",
        "fifteen","sixteen","seventeen","eighteen","nineteen","twenty"};
    for (int i = 0; i < 21; i++) {
        const char *got = ENUM_NAME(large_t, LARGE_ENUM, i);
        if (!got || strcmp(got, large_names[i]) != 0) { all_ok = 0; break; }
    }
    TEST("all 21 large ENUM_NAME values correct", all_ok);

    /* Verify all parse round-trip for large */
    all_ok = 1;
    for (int i = 0; i < 21; i++) {
        int parsed = ENUM_PARSE(large_t, LARGE_ENUM, large_names[i], -1);
        if (parsed != i) { all_ok = 0; break; }
    }
    TEST("all 21 large ENUM_PARSE round-trip correct", all_ok);

    /* --- Practical: switch-friendly --- */
    printf("\n--- Practical use ---\n");
    color_t c = COLOR_GREEN;
    const char *name = ENUM_NAME(color_t, COLOR_ENUM, c);
    TEST("name lookup from variable", name && strcmp(name, "green") == 0);

    int parsed = ENUM_PARSE(color_t, COLOR_ENUM, "blue", COLOR_RED);
    TEST("parse to variable", parsed == COLOR_BLUE);

    /* Switch over all enum values */
    int switch_ok = 1;
    for (int i = 0; i < 4; i++) {
        const char *n = ENUM_NAME(color_t, COLOR_ENUM, i);
        if (!n) { switch_ok = 0; break; }
        int back = ENUM_PARSE(color_t, COLOR_ENUM, n, -1);
        if (back != i) { switch_ok = 0; break; }
    }
    TEST("round-trip all 4 colors", switch_ok);

    /* --- Macro isolation --- */
    printf("\n--- Macro isolation (multiple types in one file) ---\n");
    TEST("color_t count still 4", ENUM_COUNT(color_t, COLOR_ENUM) == 4);
    TEST("single_t count still 1", ENUM_COUNT(single_t, SINGLE_ENUM) == 1);
    TEST("large_t count still 21", ENUM_COUNT(large_t, LARGE_ENUM) == 21);
    TEST("isolated color name", strcmp(ENUM_NAME(color_t, COLOR_ENUM, 1), "green") == 0);
    TEST("isolated large name", strcmp(ENUM_NAME(large_t, LARGE_ENUM, 5), "five") == 0);

    /* --- ENUM_NAME const-correctness --- */
    printf("\n--- Const correctness ---\n");
    const char *ro_name = ENUM_NAME(color_t, COLOR_ENUM, COLOR_BLUE);
    TEST("const name pointer valid", ro_name && strcmp(ro_name, "blue") == 0);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
