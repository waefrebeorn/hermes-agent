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

int main(void) {
    test_missing_type_fix();
    test_anyOf_strips_parent_type();
    test_enum_strips_null_empty();
    test_ref_strips_siblings();
    test_tool_parameters_null();

    fprintf(stderr, "moonshot_schema: %d/%d pass\\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
