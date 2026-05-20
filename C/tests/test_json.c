/*
 * test_json.c — Smoke test for JSON parser/serializer.
 */
#include "json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    /* Test 1: parse + serialize roundtrip */
    char *err = NULL;
    json_t *doc = json_parse("{\"name\":\"hermes\",\"count\":42,\"ok\":true,\"empty\":null}", &err);
    TEST("json_parse success", doc != NULL && err == NULL);
    if (err) free(err);
    if (doc) {
        TEST("json_get_str", strcmp(json_get_str(doc, "name", ""), "hermes") == 0);
        TEST("json_get_num", json_get_num(doc, "count", 0) == 42.0);
        TEST("json_get_bool", json_get_bool(doc, "ok", false) == true);
        TEST("json_get_bool default", json_get_bool(doc, "missing", true) == true);
        char *s = json_serialize(doc);
        TEST("json_serialize", s != NULL && strlen(s) > 0);
        free(s);
        json_free(doc);
    }

    /* Test 2: builder API — json_string, json_array, json_object */
    json_t *arr = json_array();
    json_append(arr, json_string("a"));
    json_append(arr, json_string("b"));
    json_append(arr, json_string("c"));
    TEST("json_len", json_len(arr) == 3);
    json_t *first = json_get(arr, 0);
    TEST("json_get", first != NULL && first->type == JSON_STRING);
    char *arr_s = json_serialize(arr);
    TEST("json_serialize array", arr_s != NULL);
    free(arr_s);
    json_free(arr);

    json_t *obj = json_object();
    json_set(obj, "key", json_string("value"));
    json_set(obj, "num", json_number(3.14));
    TEST("json_obj_get", json_obj_get(obj, "key") != NULL);
    TEST("json_get_str obj", strcmp(json_get_str(obj, "key", ""), "value") == 0);
    char *obj_s = json_serialize(obj);
    TEST("json_serialize object", obj_s != NULL);
    free(obj_s);
    json_free(obj);

    /* Test 3: error handling */
    char *err2 = NULL;
    json_t *bad = json_parse("{invalid}", &err2);
    TEST("json_parse error", bad == NULL);
    TEST("error message set", err2 != NULL);
    free(err2);

    /* Test 4: copy */
    json_t *orig = json_parse("{\"x\":1}", NULL);
    json_t *cpy = json_copy(orig);
    TEST("json_copy", cpy != NULL);
    TEST("copy content", json_get_num(cpy, "x", 0) == 1.0);
    json_free(cpy);
    json_free(orig);

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All JSON tests PASSED");
    return failures ? 1 : 0;
}
