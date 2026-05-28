/* test_enum.c — J14: libenum tests (ENUM_DECLARE, ENUM_NAME, ENUM_PARSE, ENUM_COUNT). */
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

    /* --- ENUM_PARSE --- */
    printf("\n--- ENUM_PARSE ---\n");
    TEST("parse 'red' -> COLOR_RED", ENUM_PARSE(color_t, COLOR_ENUM, "red", COLOR_BLUE) == COLOR_RED);
    TEST("parse 'green' -> COLOR_GREEN", ENUM_PARSE(color_t, COLOR_ENUM, "green", COLOR_BLUE) == COLOR_GREEN);
    TEST("parse 'blue' -> COLOR_BLUE", ENUM_PARSE(color_t, COLOR_ENUM, "blue", COLOR_BLUE) == COLOR_BLUE);
    TEST("parse 'yellow' -> default COLOR_BLUE", ENUM_PARSE(color_t, COLOR_ENUM, "yellow", COLOR_BLUE) == COLOR_BLUE);
    TEST("parse NULL -> default", ENUM_PARSE(color_t, COLOR_ENUM, NULL, COLOR_RED) == COLOR_RED);
    TEST("parse 'BLACK' -> default (case sensitive)", ENUM_PARSE(color_t, COLOR_ENUM, "BLACK", COLOR_RED) == COLOR_RED);

    /* --- ENUM_COUNT --- */
    printf("\n--- ENUM_COUNT ---\n");
    TEST("color count is 4", ENUM_COUNT(color_t, COLOR_ENUM) == 4);
    TEST("single count is 1", ENUM_COUNT(single_t, SINGLE_ENUM) == 1);

    /* --- Edge: single value --- */
    printf("\n--- Single value ---\n");
    TEST("SINGLE_ONLY name", strcmp(ENUM_NAME(single_t, SINGLE_ENUM, SINGLE_ONLY), "only") == 0);
    TEST("SINGLE_ONLY is 0", SINGLE_ONLY == 0);

    /* --- Practical: switch-friendly --- */
    printf("\n--- Practical use ---\n");
    color_t c = COLOR_GREEN;
    const char *name = ENUM_NAME(color_t, COLOR_ENUM, c);
    TEST("name lookup from variable", name && strcmp(name, "green") == 0);

    int parsed = ENUM_PARSE(color_t, COLOR_ENUM, "blue", COLOR_RED);
    TEST("parse to variable", parsed == COLOR_BLUE);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
