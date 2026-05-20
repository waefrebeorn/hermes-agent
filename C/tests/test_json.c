/*
 * test_json.c — Quick smoke test for JSON parser.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    /* Test 1: basic parse + serialize roundtrip */
    const char *input = "{\"name\":\"hermes\",\"version\":0.14,\"features\":[\"json\",\"yaml\"],\"enabled\":true,\"count\":42,\"empty\":null}";
    char *err = NULL;
    json_node_t *root = json_parse(input, &err);
    assert(root != NULL && "parse should succeed");
    assert(err == NULL);

    assert(strcmp(json_object_get_string(root, "name", ""), "hermes") == 0);
    assert(json_object_get_number(root, "version", 0) == 0.14);
    assert(json_object_get_bool(root, "enabled", false) == true);
    assert(json_object_get_bool(root, "missing", true) == true);

    json_node_t *features = json_object_get(root, "features");
    assert(features != NULL && features->type == JSON_ARRAY);
    assert(json_array_count(features) == 2);

    char *serialized = json_serialize(root);
    assert(serialized != NULL);
    printf("Test 1 PASS: %s\n", serialized);
    free(serialized);

    /* Test 2: pretty print */
    char *pretty = json_serialize_pretty(root, 0);
    assert(pretty != NULL);
    printf("Test 2 PASS (pretty):\n%s\n", pretty);
    free(pretty);

    /* Test 3: error handling */
    char *err2 = NULL;
    json_node_t *bad = json_parse("{invalid}", &err2);
    assert(bad == NULL);
    assert(err2 != NULL);
    printf("Test 3 PASS: error = '%s'\n", err2);
    free(err2);

    /* Test 4: array operations */
    json_node_t *arr = json_new_array();
    json_array_append(arr, json_new_string("a"));
    json_array_append(arr, json_new_number(2));
    json_array_append(arr, json_new_bool(true));
    assert(json_array_count(arr) == 3);
    assert(strcmp(json_array_get(arr, 0)->str_val, "a") == 0);
    char *arr_s = json_serialize(arr);
    printf("Test 4 PASS: %s\n", arr_s);
    free(arr_s);
    json_free(arr);

    /* Test 5: copy */
    json_node_t *copy = json_copy(root);
    assert(copy != NULL);
    assert(strcmp(json_object_get_string(copy, "name", ""), "hermes") == 0);
    char *copy_s = json_serialize(copy);
    printf("Test 5 PASS (copy): %s\n", copy_s);
    free(copy_s);
    json_free(copy);

    json_free(root);
    printf("\nAll JSON tests PASSED.\n");
    return 0;
}
