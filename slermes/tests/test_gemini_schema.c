/* test_gemini_schema.c — Tests for Gemini schema sanitizer. */

#include "gemini_schema.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (line %d)\\n", name, __LINE__); \
        fail++; \
    } else { \
        pass++; \
    } \
} while(0)

static void test_simple_schema(void) {
    const char *input = "{\"type\":\"string\",\"description\":\"A name\",\"$schema\":\"http://...\"}";
    char *result = sanitize_gemini_schema(input);
    TEST("simple: result not null", result != NULL);
    TEST("simple: contains type", strstr(result, "\"type\"") != NULL);
    TEST("simple: contains description", strstr(result, "\"description\"") != NULL);
    TEST("simple: no $schema (stripped)", strstr(result, "$schema") == NULL);
    free(result);
}

static void test_nested_properties(void) {
    const char *input = "{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"name\":{\"type\":\"string\",\"$schema\":\"bad\"},"
            "\"age\":{\"type\":\"integer\",\"minimum\":0}"
        "},"
        "\"required\":[\"name\"]"
    "}";
    char *result = sanitize_gemini_schema(input);
    TEST("nested: result not null", result != NULL);
    TEST("nested: has type", strstr(result, "\"object\"") != NULL);
    TEST("nested: has name prop", strstr(result, "\"name\"") != NULL);
    TEST("nested: has age prop", strstr(result, "\"age\"") != NULL);
    TEST("nested: has minimum", strstr(result, "\"minimum\"") != NULL);
    TEST("nested: no $schema in name prop", strstr(result, "$schema") == NULL);
    TEST("nested: has required", strstr(result, "\"required\"") != NULL);
    free(result);
}

static void test_strip_typed_enum(void) {
    /* integer enum with non-string values should drop enum */
    const char *input = "{\"type\":\"integer\",\"enum\":[60,1440,4320,10080]}";
    char *result = sanitize_gemini_schema(input);
    TEST("typed enum: enum stripped for integer", strstr(result, "\"enum\"") == NULL);
    TEST("typed enum: type preserved", strstr(result, "\"integer\"") != NULL);
    free(result);
}

static void test_keep_string_enum(void) {
    /* string enum should be preserved */
    const char *input = "{\"type\":\"string\",\"enum\":[\"red\",\"green\",\"blue\"]}";
    char *result = sanitize_gemini_schema(input);
    TEST("string enum: result not null", result != NULL);
    TEST("string enum: has type", strstr(result, "\"string\"") != NULL);
    TEST("string enum: has enum", strstr(result, "\"enum\"") != NULL);
    TEST("string enum: has colors", strstr(result, "red") != NULL);
    free(result);
}

static void test_items_sanitized(void) {
    const char *input = "{"
        "\"type\":\"array\","
        "\"items\":{\"type\":\"string\",\"$schema\":\"bad\"}"
    "}";
    char *result = sanitize_gemini_schema(input);
    TEST("items: result not null", result != NULL);
    TEST("items: has items", strstr(result, "\"items\"") != NULL);
    TEST("items: has type string in items", strstr(result, "\"string\"") != NULL);
    TEST("items: no $schema", strstr(result, "$schema") == NULL);
    free(result);
}

static void test_anyOf_sanitized(void) {
    const char *input = "{"
        "\"anyOf\":["
            "{\"type\":\"string\",\"$schema\":\"x\"},"
            "{\"type\":\"integer\"}"
        "]"
    "}";
    char *result = sanitize_gemini_schema(input);
    TEST("anyOf: result not null", result != NULL);
    TEST("anyOf: has anyOf", strstr(result, "\"anyOf\"") != NULL);
    TEST("anyOf: has string type", strstr(result, "\"string\"") != NULL);
    TEST("anyOf: has integer type", strstr(result, "\"integer\"") != NULL);
    TEST("anyOf: no $schema", strstr(result, "$schema") == NULL);
    free(result);
}

static void test_null_input(void) {
    char *result = sanitize_gemini_schema(NULL);
    TEST("null: result not null", result != NULL);
    free(result);
}

static void test_empty_input(void) {
    char *result = sanitize_gemini_schema("");
    TEST("empty: result not null", result != NULL);
    free(result);
}

static void test_tool_parameters_empty(void) {
    char *result = sanitize_gemini_tool_parameters("{}");
    TEST("params empty: has fallback", strstr(result, "\"object\"") != NULL);
    free(result);
}

static void test_tool_parameters_normal(void) {
    const char *input = "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}}}";
    char *result = sanitize_gemini_tool_parameters(input);
    TEST("params normal: has object", strstr(result, "\"object\"") != NULL);
    TEST("params normal: has name", strstr(result, "\"name\"") != NULL);
    free(result);
}

static void test_tool_parameters_null(void) {
    char *result = sanitize_gemini_tool_parameters(NULL);
    TEST("params null: has fallback", strstr(result, "\"object\"") != NULL);
    free(result);
}

int main(void) {
    test_simple_schema();
    test_nested_properties();
    test_strip_typed_enum();
    test_keep_string_enum();
    test_items_sanitized();
    test_anyOf_sanitized();
    test_null_input();
    test_empty_input();
    test_tool_parameters_empty();
    test_tool_parameters_normal();
    test_tool_parameters_null();

    fprintf(stderr, "gemini_schema: %d/%d pass\\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
