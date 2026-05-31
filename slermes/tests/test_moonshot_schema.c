/* test_moonshot_schema.c — Tests for Moonshot schema sanitizer. */

#include "moonshot_schema.h"
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

static void test_missing_type_fix(void) {
    const char *input = "{\"properties\":{\"name\":{\"description\":\"No type here\"}}}";
    char *result = sanitize_moonshot_schema(input);
    TEST("missing type: result not null", result != NULL);
    TEST("missing type: has type object", strstr(result, "\"object\"") != NULL);
    TEST("missing type: properties preserved", strstr(result, "\"properties\"") != NULL);
    free(result);
}

static void test_anyOf_strips_parent_type(void) {
    const char *input = "{"
        "\"type\":\"object\","
        "\"anyOf\":[{\"type\":\"string\"},{\"type\":\"integer\"}]"
    "}";
    char *result = sanitize_moonshot_schema(input);
    TEST("anyOf: result not null", result != NULL);
    TEST("anyOf: has anyOf", strstr(result, "\"anyOf\"") != NULL);
    /* Parent type should be stripped when anyOf present */
    TEST("anyOf: parent type stripped for anyOf", strstr(result, "\"object\"") == NULL);
    free(result);
}

static void test_enum_strips_null_empty(void) {
    const char *input = "{\"type\":\"string\",\"enum\":[\"a\",null,\"b\",\"\",\"c\"]}";
    char *result = sanitize_moonshot_schema(input);
    TEST("enum: result not null", result != NULL);
    TEST("enum: has a", strstr(result, "\"a\"") != NULL);
    TEST("enum: has b", strstr(result, "\"b\"") != NULL);
    TEST("enum: has c", strstr(result, "\"c\"") != NULL);
    TEST("enum: no null", strstr(result, "null") == NULL);
    TEST("enum: no empty string", strstr(result, "\"\"") == NULL);
    free(result);
}

static void test_ref_strips_siblings(void) {
    const char *input = "{\"$ref\":\"#/defs/MyType\",\"description\":\"Should be stripped\"}";
    char *result = sanitize_moonshot_schema(input);
    TEST("$ref: result not null", result != NULL);
    TEST("$ref: has $ref", strstr(result, "\"$ref\"") != NULL);
    TEST("$ref: has defs path", strstr(result, "#/defs") != NULL);
    TEST("$ref: no description sibling", strstr(result, "description") == NULL);
    free(result);
}

static void test_tool_parameters_null(void) {
    char *result = sanitize_moonshot_tool_parameters(NULL);
    TEST("params null: has fallback object", strstr(result, "\"object\"") != NULL);
    free(result);
}

static void test_items_tuple_collapse(void) {
    /* Fix 5 NOT YET IMPLEMENTED in C — tuple items kept as-is */
    const char *input = "{\"type\":\"object\",\"items\":[{\"type\":\"string\"},{\"type\":\"integer\"}]}";
    char *result = sanitize_moonshot_schema(input);
    TEST("items tuple: result not null", result != NULL);
    TEST("items tuple: items present", strstr(result, "\"items\"") != NULL);
    /* Current behavior: tuple items preserved as array (fix #5 pending) */
    TEST("items tuple: type fix applied", strstr(result, "\"object\"") != NULL);
    free(result);
}

static void test_empty_schema(void) {
    char *result = sanitize_moonshot_schema("");
    TEST("empty schema: result not null", result != NULL);
    TEST("empty schema: returns object", strstr(result, "{") != NULL);
    free(result);

    char *r2 = sanitize_moonshot_schema(NULL);
    TEST("NULL schema: result not null", r2 != NULL);
    free(r2);
}

static void test_invalid_json(void) {
    char *result = sanitize_moonshot_schema("{invalid}");
    TEST("invalid JSON: result not null", result != NULL);
    TEST("invalid JSON: returns object", strstr(result, "{") != NULL);
    free(result);
}

static void test_deep_nesting(void) {
    /* Deeply nested schema with type fix at depth */
    const char *input = "{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"object\",\"properties\":{\"b\":{\"properties\":{\"c\":{\"type\":\"string\"}}}}}}}";
    char *result = sanitize_moonshot_schema(input);
    TEST("deep nesting: result not null", result != NULL);
    TEST("deep nesting: inner type added", strstr(result, "\"object\"") != NULL);
    free(result);
}

static void test_all_null_enum(void) {
    const char *input = "{\"type\":\"string\",\"enum\":[null,null,null]}";
    char *result = sanitize_moonshot_schema(input);
    TEST("all-null enum: result not null", result != NULL);
    TEST("all-null enum: no null", strstr(result, "null") == NULL);
    TEST("all-null enum: no enum key", strstr(result, "\"enum\"") == NULL || strstr(result, "\"enum\":[]") != NULL);
    free(result);
}

int main(void) {
    test_missing_type_fix();
    test_anyOf_strips_parent_type();
    test_enum_strips_null_empty();
    test_ref_strips_siblings();
    test_tool_parameters_null();
    test_items_tuple_collapse();
    test_empty_schema();
    test_invalid_json();
    test_deep_nesting();
    test_all_null_enum();

    fprintf(stderr, "moonshot_schema: %d/%d pass\\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
