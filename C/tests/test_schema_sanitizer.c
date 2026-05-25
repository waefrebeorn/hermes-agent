/*
 * test_schema_sanitizer.c — M-series tests for schema_sanitizer.
 *
 * Tests sanitize_tool_schemas, strip_pattern_and_format, strip_slash_enum.
 */

#include "hermes_json.h"
#include "schema_sanitizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (test_##name()) { \
        tests_passed++; \
        printf("  PASS: %s\n", #name); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", #name); \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("    assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

#define ASSERT_STR_CONTAINS(str, substr, msg) do { \
    if (!(str) || !strstr((str), (substr))) { \
        printf("    assertion failed: %s -- expected '%s' in '%s' (%s:%d)\n", \
               msg, (substr), (str) ? (str) : "(null)", __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

/* Helper: check JSON output for a specific key=value pattern */
static bool json_has_key_value(const char *json_str, const char *key,
                                const char *value)
{
    if (!json_str || !key || !value) return false;
    /* Build expected substring: "key": "value"  or  "key": value */
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"%s\"", key, value);
    if (strstr(json_str, pattern)) return true;
    snprintf(pattern, sizeof(pattern), "\"%s\":%s", key, value);
    return strstr(json_str, pattern) != NULL;
}

/* ─── Tests: sanitize_tool_schemas ──────────────────────────────────── */

static bool test_sanitize_null_input(void)
{
    char *result = sanitize_tool_schemas(NULL);
    ASSERT(result != NULL, "null input returns []");
    ASSERT(strcmp(result, "[]") == 0, "null input returns '[]'");
    free(result);
    return true;
}

static bool test_sanitize_empty_string(void)
{
    char *result = sanitize_tool_schemas("");
    ASSERT(result != NULL, "empty input returns []");
    ASSERT(strcmp(result, "[]") == 0, "empty input returns '[]'");
    free(result);
    return true;
}

static bool test_sanitize_empty_array(void)
{
    char *result = sanitize_tool_schemas("[]");
    ASSERT(result != NULL, "[] input returns []");
    ASSERT(strcmp(result, "[]") == 0, "[] input returns '[]'");
    free(result);
    return true;
}

static bool test_sanitize_basic_tool(void)
{
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"test_tool\",\"parameters\":{"
          "\"type\":\"object\","
          "\"properties\":{"
            "\"name\":{\"type\":\"string\"}"
          "}"
        "}"
      "}}]";

    char *result = sanitize_tool_schemas(input);
    ASSERT(result != NULL, "basic tool sanitizes");
    ASSERT_STR_CONTAINS(result, "test_tool", "name preserved");
    ASSERT_STR_CONTAINS(result, "\"type\":\"string\"", "type preserved");
    free(result);
    return true;
}

static bool test_sanitize_bare_string_schema(void)
{
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"bare\",\"parameters\":\"object\""
      "}}]";

    char *result = sanitize_tool_schemas(input);
    ASSERT(result != NULL, "bare string schema sanitizes");
    /* Bare "object" should become {"type":"object","properties":{}} */
    ASSERT_STR_CONTAINS(result, "\"type\":\"object\"", "type:object emitted");
    ASSERT_STR_CONTAINS(result, "\"properties\":{}", "properties injected");
    free(result);
    return true;
}

static bool test_sanitize_array_type(void)
{
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"arr_type\",\"parameters\":{"
          "\"type\":\"object\","
          "\"properties\":{"
            "\"name\":{\"type\":[\"string\",\"null\"]}"
          "}"
        "}"
      "}}]";

    char *result = sanitize_tool_schemas(input);
    ASSERT(result != NULL, "array type sanitizes");
    /* Property type should be single string */
    ASSERT_STR_CONTAINS(result, "\"type\":\"string\"", "type normalized");
    ASSERT_STR_CONTAINS(result, "\"nullable\":true", "nullable hint set");
    /* Should NOT contain the array form */
    bool has_array_type = strstr(result, "\"type\":[\"string\",\"null\"]") != NULL;
    ASSERT(!has_array_type, "array type form removed");
    free(result);
    return true;
}

static bool test_sanitize_object_no_properties(void)
{
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"no_props\",\"parameters\":{\"type\":\"object\"}"
      "}}]";

    char *result = sanitize_tool_schemas(input);
    ASSERT(result != NULL, "object without properties sanitizes");
    ASSERT_STR_CONTAINS(result, "\"properties\":{}", "empty properties injected");
    free(result);
    return true;
}

static bool test_sanitize_missing_params(void)
{
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"no_params\""
      "}}]";

    char *result = sanitize_tool_schemas(input);
    ASSERT(result != NULL, "missing params sanitizes");
    /* Should inject minimal parameters */
    ASSERT_STR_CONTAINS(result, "\"parameters\":{", "parameters injected");
    free(result);
    return true;
}

static bool test_sanitize_strip_top_combinators(void)
{
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"with_combo\",\"parameters\":{"
          "\"type\":\"object\","
          "\"properties\":{\"x\":{\"type\":\"string\"}},"
          "\"allOf\":[{\"if\":{\"properties\":{\"x\":{\"const\":\"a\"}}}}]"
        "}"
      "}}]";

    char *result = sanitize_tool_schemas(input);
    ASSERT(result != NULL, "top-level combinator stripped");
    /* allOf should be removed from top level */
    bool has_allof = strstr(result, "allOf") != NULL ||
                     strstr(result, "\"allOf\"") != NULL;
    ASSERT(!has_allof, "allOf removed from top level");
    /* type/object/properties should still be there */
    ASSERT_STR_CONTAINS(result, "\"type\":\"object\"", "type preserved");
    free(result);
    return true;
}

/* ─── Tests: strip_pattern_and_format ───────────────────────────────── */

static bool test_strip_pattern_basic(void)
{
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"with_pat\",\"parameters\":{"
          "\"type\":\"object\","
          "\"properties\":{"
            "\"email\":{\"type\":\"string\",\"pattern\":\"^.+@.+\\\\.com$\",\"format\":\"email\"}"
          "}"
        "}"
      "}}]";

    int stripped = -1;
    char *result = strip_pattern_and_format(input, &stripped);
    ASSERT(result != NULL, "pattern stripped");
    ASSERT(stripped == 2, "2 keywords stripped (pattern + format)");
    /* Should NOT contain pattern or format */
    bool has_pattern = strstr(result, "pattern") != NULL;
    bool has_format = strstr(result, "\"format\"") != NULL;
    ASSERT(!has_pattern, "pattern removed");
    ASSERT(!has_format, "format removed");
    /* type/string should still be there */
    ASSERT_STR_CONTAINS(result, "\"type\":\"string\"", "type preserved");
    free(result);
    return true;
}

static bool test_strip_pattern_no_schema_node(void)
{
    /* A property literally named "pattern" should NOT be stripped */
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"search\",\"parameters\":{"
          "\"type\":\"object\","
          "\"properties\":{"
            "\"pattern\":{\"type\":\"string\",\"description\":\"search pattern\"}"
          "}"
        "}"
      "}}]";

    int stripped = -1;
    char *result = strip_pattern_and_format(input, &stripped);
    ASSERT(result != NULL, "literal 'pattern' property preserved");
    ASSERT(stripped == 0, "0 keywords stripped (not a schema sibling)");
    /* The PROPERTY named "pattern" should still exist */
    ASSERT_STR_CONTAINS(result, "\"pattern\"", "property key preserved");
    free(result);
    return true;
}

static bool test_strip_pattern_null_input(void)
{
    int stripped = -1;
    char *result = strip_pattern_and_format(NULL, &stripped);
    ASSERT(result != NULL, "null input returns []");
    ASSERT(stripped == 0, "stripped count is 0");
    free(result);
    return true;
}

/* ─── Tests: strip_slash_enum ───────────────────────────────────────── */

static bool test_strip_slash_basic(void)
{
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"model_picker\",\"parameters\":{"
          "\"type\":\"object\","
          "\"properties\":{"
            "\"model\":{\"type\":\"string\",\"enum\":[\"Qwen/Qwen3.5\",\"gpt-4o\"]}"
          "}"
        "}"
      "}}]";

    int stripped = -1;
    char *result = strip_slash_enum(input, &stripped);
    ASSERT(result != NULL, "slash enum stripped");
    ASSERT(stripped == 1, "1 enum stripped");
    /* The "enum" should be removed from the model property */
    bool has_enum = strstr(result, "\"enum\"") != NULL;
    ASSERT(!has_enum, "enum removed");
    /* Type should still be there */
    ASSERT_STR_CONTAINS(result, "\"type\":\"string\"", "type preserved");
    ASSERT_STR_CONTAINS(result, "\"name\":\"model_picker\"", "name preserved");
    free(result);
    return true;
}

static bool test_strip_slash_no_slash(void)
{
    const char *input = "[{\"type\":\"function\",\"function\":{"
        "\"name\":\"picker\",\"parameters\":{"
          "\"type\":\"object\","
          "\"properties\":{"
            "\"color\":{\"type\":\"string\",\"enum\":[\"red\",\"green\",\"blue\"]}"
          "}"
        "}"
      "}}]";

    int stripped = -1;
    char *result = strip_slash_enum(input, &stripped);
    ASSERT(result != NULL, "no-slash enum preserved");
    ASSERT(stripped == 0, "0 enums stripped");
    /* Enum should still be present */
    ASSERT_STR_CONTAINS(result, "\"enum\":[\"red\",\"green\",\"blue\"]", "enum preserved");
    free(result);
    return true;
}

static bool test_strip_slash_responses_format(void)
{
    const char *input = "[{"
        "\"name\":\"responses_tool\","
        "\"parameters\":{"
          "\"type\":\"object\","
          "\"properties\":{"
            "\"id\":{\"type\":\"string\",\"enum\":[\"org/Qwen\"]}"
          "}"
        "}"
      "}]";

    int stripped = -1;
    char *result = strip_slash_enum(input, &stripped);
    ASSERT(result != NULL, "responses-format works");
    ASSERT(stripped == 1, "1 enum stripped in responses format");
    free(result);
    return true;
}

/* ─── Main ──────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== Schema Sanitizer Library Tests ===\n");

    TEST(sanitize_null_input);
    TEST(sanitize_empty_string);
    TEST(sanitize_empty_array);
    TEST(sanitize_basic_tool);
    TEST(sanitize_bare_string_schema);
    TEST(sanitize_array_type);
    TEST(sanitize_object_no_properties);
    TEST(sanitize_missing_params);
    TEST(sanitize_strip_top_combinators);

    TEST(strip_pattern_basic);
    TEST(strip_pattern_no_schema_node);
    TEST(strip_pattern_null_input);

    TEST(strip_slash_basic);
    TEST(strip_slash_no_slash);
    TEST(strip_slash_responses_format);

    printf("\nResults: %d passed, %d failed, %d total\n",
           tests_passed, tests_failed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
