/*
 * test_tool_coerce.c — Tests for tool argument type coercion.
 * Port of Python model_tools _coerce_number / _coerce_boolean.
 *
 * Tests: number coercion (integer, float, edge cases, nan/inf),
 * boolean coercion (true/false variants, case, whitespace, edge cases).
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include \
 *       tests/test_tool_coerce.c src/tools/tool_coerce.c \
 *       -o /tmp/hermes_test_tool_coerce -lm
 *
 * Run:
 *   /tmp/hermes_test_tool_coerce
 */

#include "hermes_tool_coerce.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* ================================================================
 * 1. Number coercion
 * ================================================================ */

static void test_number_integer(void) {
    printf("\n--- Number: integers ---\n");
    double v; bool is_int;

    TEST("42", tool_coerce_number("42", &v, &is_int, false) && v == 42.0 && is_int);
    TEST("-17", tool_coerce_number("-17", &v, &is_int, false) && v == -17.0 && is_int);
    TEST("0", tool_coerce_number("0", &v, &is_int, false) && v == 0.0 && is_int);
    TEST("  +5  ", tool_coerce_number("  +5  ", &v, &is_int, false) && v == 5.0 && is_int);
}

static void test_number_float(void) {
    printf("\n--- Number: floats ---\n");
    double v; bool is_int;

    TEST("3.14", tool_coerce_number("3.14", &v, &is_int, false) && fabs(v - 3.14) < 0.001 && !is_int);
    TEST("-2.5", tool_coerce_number("-2.5", &v, &is_int, false) && v == -2.5 && !is_int);
    TEST("1e10", tool_coerce_number("1e10", &v, &is_int, false) && v == 1e10 && is_int);
    TEST("1.5e2", tool_coerce_number("1.5e2", &v, &is_int, false) && v == 150.0 && is_int);
}

static void test_number_integer_only(void) {
    printf("\n--- Number: integer_only ---\n");
    double v; bool is_int;

    TEST("42 integer_only", tool_coerce_number("42", &v, &is_int, true) && v == 42.0);
    TEST("3.14 rejected integer_only", !tool_coerce_number("3.14", &v, &is_int, true));
    TEST("0 integer_only", tool_coerce_number("0", &v, &is_int, true) && v == 0.0);
}

static void test_number_edge_cases(void) {
    printf("\n--- Number: edge cases ---\n");
    double v; bool is_int;

    TEST("NULL", !tool_coerce_number(NULL, &v, &is_int, false));
    TEST("empty", !tool_coerce_number("", &v, &is_int, false));
    TEST("not a number", !tool_coerce_number("hello", &v, &is_int, false));
    TEST("trailing text", !tool_coerce_number("42abc", &v, &is_int, false));
    TEST("nan", !tool_coerce_number("nan", &v, &is_int, false));
    TEST("inf", !tool_coerce_number("inf", &v, &is_int, false));
    TEST("-inf", !tool_coerce_number("-inf", &v, &is_int, false));
    TEST("whitespace only", !tool_coerce_number("   ", &v, &is_int, false));
    TEST("just minus", !tool_coerce_number("-", &v, &is_int, false));
    TEST("just plus", !tool_coerce_number("+", &v, &is_int, false));
    TEST("just decimal", !tool_coerce_number(".", &v, &is_int, false));
    TEST("just minus decimal", !tool_coerce_number("-.", &v, &is_int, false));
    TEST("leading zeros", tool_coerce_number("00042", &v, &is_int, false) && v == 42.0 && is_int);
    TEST("null output ptr", !tool_coerce_number("42", NULL, &is_int, false));
    TEST("null is_int ptr", !tool_coerce_number("42", &v, NULL, false));
}

/* ================================================================
 * 2. Boolean coercion
 * ================================================================ */

static void test_bool_true(void) {
    printf("\n--- Boolean: true ---\n");
    bool v;

    TEST("\"true\"", tool_coerce_boolean("true", &v) && v == true);
    TEST("\"True\"", tool_coerce_boolean("True", &v) && v == true);
    TEST("\"TRUE\"", tool_coerce_boolean("TRUE", &v) && v == true);
    TEST("\" true \"", tool_coerce_boolean(" true ", &v) && v == true);
}

static void test_bool_false(void) {
    printf("\n--- Boolean: false ---\n");
    bool v;

    TEST("\"false\"", tool_coerce_boolean("false", &v) && v == false);
    TEST("\"False\"", tool_coerce_boolean("False", &v) && v == false);
    TEST("\"FALSE\"", tool_coerce_boolean("FALSE", &v) && v == false);
    TEST("\" false \"", tool_coerce_boolean(" false ", &v) && v == false);
}

static void test_bool_edge_cases(void) {
    printf("\n--- Boolean: edge cases ---\n");
    bool v;

    TEST("NULL", !tool_coerce_boolean(NULL, &v));
    TEST("empty", !tool_coerce_boolean("", &v));
    TEST("whitespace", !tool_coerce_boolean("   ", &v));
    TEST("yes", !tool_coerce_boolean("yes", &v));
    TEST("no", !tool_coerce_boolean("no", &v));
    TEST("1", !tool_coerce_boolean("1", &v));
    TEST("0", !tool_coerce_boolean("0", &v));
    TEST("random", !tool_coerce_boolean("random", &v));
}

int main(void) {
    printf("=== Tool Argument Coercion Tests ===\n");

    /* Number tests */
    test_number_integer();
    test_number_float();
    test_number_integer_only();
    test_number_edge_cases();

    /* Boolean tests */
    test_bool_true();
    test_bool_false();
    test_bool_edge_cases();

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
