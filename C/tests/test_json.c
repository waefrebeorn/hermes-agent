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

/* --- JSON Pointer (RFC 6901) tests --- */
static void test_json_pointer(void) {
    json_t *obj = json_parse("{\"foo\":{\"bar\":42,\"arr\":[1,\"two\",{\"n\":3}]}}", NULL);
    json_t *v;

    v = json_pointer_get(obj, "");
    TEST("pointer root", v == obj);

    v = json_pointer_get(obj, "/foo");
    TEST("pointer /foo", v && v->type == JSON_OBJECT);

    v = json_pointer_get(obj, "/foo/bar");
    TEST("pointer /foo/bar", v && v->type == JSON_NUMBER && v->num_val == 42.0);

    v = json_pointer_get(obj, "/foo/arr");
    TEST("pointer /foo/arr", v && v->type == JSON_ARRAY && json_len(v) == 3);

    v = json_pointer_get(obj, "/foo/arr/0");
    TEST("pointer /foo/arr/0", v && v->type == JSON_NUMBER && v->num_val == 1.0);

    v = json_pointer_get(obj, "/foo/arr/1");
    TEST("pointer /foo/arr/1", v && v->type == JSON_STRING && strcmp(v->str_val, "two") == 0);

    v = json_pointer_get(obj, "/foo/arr/2/n");
    TEST("pointer /foo/arr/2/n", v && v->type == JSON_NUMBER && v->num_val == 3.0);

    v = json_pointer_get(obj, "/nonexistent");
    TEST("pointer missing key", v == NULL);

    v = json_pointer_get(obj, "/foo/arr/999");
    TEST("pointer missing index", v == NULL);

    v = json_pointer_get(obj, "bad");
    TEST("pointer no leading slash", v == NULL);

    v = json_pointer_get(NULL, "/foo");
    TEST("pointer null root", v == NULL);

    json_free(obj);
}

/* --- Unicode surrogate pair parsing tests --- */
static void test_surrogate_pair(void) {
    /* U+1F389 PARTY POPPER 🎉 = \uD83C\uDF89 */
    json_t *obj = json_parse("{\"emoji\":\"\\uD83C\\uDF89\"}", NULL);
    TEST("surrogate pair parse", obj != NULL);
    if (obj) {
        json_t *v = json_obj_get(obj, "emoji");
        TEST("surrogate pair value exists", v != NULL);
        /* Expected UTF-8: F0 9F 8E 89 */
        TEST("surrogate pair correct UTF-8", v && v->type == JSON_STRING
             && v->str_val[0] == (char)0xF0
             && v->str_val[1] == (char)0x9F
             && v->str_val[2] == (char)0x8E
             && v->str_val[3] == (char)0x89
             && v->str_val[4] == '\0');
        json_free(obj);
    }
}

static void test_lone_high_surrogate(void) {
    /* Lone high surrogate should become U+FFFD */
    json_t *obj = json_parse("{\"x\":\"\\uD800\"}", NULL);
    TEST("lone high surrogate parse", obj != NULL);
    if (obj) {
        json_t *v = json_obj_get(obj, "x");
        /* U+FFFD encodes as EF BF BD */
        TEST("lone high surrogate becomes U+FFFD", v && v->type == JSON_STRING
             && v->str_val[0] == (char)0xEF
             && v->str_val[1] == (char)0xBF
             && v->str_val[2] == (char)0xBD
             && v->str_val[3] == '\0');
        json_free(obj);
    }
}

static void test_lone_low_surrogate(void) {
    /* Lone low surrogate should become U+FFFD */
    json_t *obj = json_parse("{\"x\":\"\\uDC00\"}", NULL);
    TEST("lone low surrogate parse", obj != NULL);
    if (obj) {
        json_t *v = json_obj_get(obj, "x");
        TEST("lone low surrogate becomes U+FFFD", v && v->type == JSON_STRING
             && v->str_val[0] == (char)0xEF
             && v->str_val[1] == (char)0xBF
             && v->str_val[2] == (char)0xBD
             && v->str_val[3] == '\0');
        json_free(obj);
    }
}

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

    /* Test 2: builder API */
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

    /* Test 5: JSON Pointer */
    test_json_pointer();

    /* Test 6: Unicode surrogate pairs */
    test_surrogate_pair();
    test_lone_high_surrogate();
    test_lone_low_surrogate();

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All JSON tests PASSED");
    return failures ? 1 : 0;
}
