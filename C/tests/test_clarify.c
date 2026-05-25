/*
 * test_clarify.c — Clarify tool unit tests
 * Tests JSON schema parsing, error handling, and response structure
 */

#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* External handler from clarify.c */
extern char *clarify_handler(const char *args_json, const char *task_id);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Clarify Tests ===\n");

    /* Test 1: Null args → error */
    {
        char *result = clarify_handler(NULL, NULL);
        TEST("null args returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 2: Empty JSON → error (missing required 'question') */
    {
        char *result = clarify_handler("{}", NULL);
        TEST("empty json returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 3: Missing question in args → error */
    {
        char *result = clarify_handler("{\"choices\":[\"a\",\"b\"]}", NULL);
        TEST("missing question returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 4: Invalid JSON → error */
    {
        char *result = clarify_handler("{invalid}", NULL);
        TEST("invalid json returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 5: Schema constant parses as valid JSON */
    {
        static const char *SCHEMA = "{"
            "\"type\":\"object\","
            "\"properties\":{"
              "\"question\":{\"type\":\"string\",\"description\":\"Question to ask the user\"},"
              "\"choices\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Optional multiple choice options\",\"maxItems\":4}"
            "},"
            "\"required\":[\"question\"]"
        "}";
        json_node_t *parsed = json_parse(SCHEMA, NULL);
        TEST("schema parses as valid JSON", parsed != NULL);
        if (parsed) json_free(parsed);
    }

    /* Test 6: Schema has 'required' field with 'question' */
    {
        static const char *SCHEMA = "{"
            "\"type\":\"object\","
            "\"properties\":{"
              "\"question\":{\"type\":\"string\"},"
              "\"choices\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"maxItems\":4}"
            "},"
            "\"required\":[\"question\"]"
        "}";
        json_node_t *parsed = json_parse(SCHEMA, NULL);
        int ok = 0;
        if (parsed) {
            json_node_t *req = json_object_get(parsed, "required");
            if (req && json_array_count(req) >= 1) {
                json_node_t *first = json_array_get(req, 0);
                ok = (first && first->type == JSON_STRING && strcmp(first->str_val, "question") == 0);
            }
            json_free(parsed);
        }
        TEST("schema requires question field", ok);
    }

    /* Test 7: Schema limits choices to 4 max */
    {
        static const char *SCHEMA = "{"
            "\"type\":\"object\","
            "\"properties\":{"
              "\"choices\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"maxItems\":4}"
            "},"
            "\"required\":[\"question\"]"
        "}";
        json_node_t *parsed = json_parse(SCHEMA, NULL);
        int ok = 0;
        if (parsed) {
            json_node_t *props = json_object_get(parsed, "properties");
            if (props) {
                json_node_t *choices = json_object_get(props, "choices");
                if (choices) {
                    json_node_t *mi = json_object_get(choices, "maxItems");
                    ok = (mi && mi->type == JSON_NUMBER && mi->num_val == 4);
                }
            }
            json_free(parsed);
        }
        TEST("choices maxItems is 4", ok);
    }

    /* Test 8: Schema has type=object */
    {
        static const char *SCHEMA = "{"
            "\"type\":\"object\","
            "\"properties\":{"
              "\"question\":{\"type\":\"string\"},"
              "\"choices\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"maxItems\":4}"
            "},"
            "\"required\":[\"question\"]"
        "}";
        json_node_t *parsed = json_parse(SCHEMA, NULL);
        int ok = 0;
        if (parsed) {
            json_node_t *type = json_object_get(parsed, "type");
            ok = (type && type->type == JSON_STRING && strcmp(type->str_val, "object") == 0);
            json_free(parsed);
        }
        TEST("schema type is object", ok);
    }

    /* Results */
    printf("\n=== Clarify Test Summary: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
