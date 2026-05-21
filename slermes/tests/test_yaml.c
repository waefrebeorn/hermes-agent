/*
 * test_yaml.c — Smoke test for YAML parser.
 */
#include "yaml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

static void count_cb(const char *k, const char *v, void *user) {
    (void)k; (void)v;
    (*(int *)user)++;
}

int main(void) {
    const char *input =
        "server:\n"
        "  host: localhost\n"
        "  port: 8080\n"
        "name: test-app\n"
        "debug: true\n";

    /* Test 1: parse and get top-level keys */
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(input, &err);
    TEST("yaml_parse success", doc != NULL && err == NULL);
    if (err) free(err);

    if (doc) {
        /* Test 2: nested key via dotted path */
        const char *host = yaml_get_string(doc, "server.host");
        TEST("yaml_get_string server.host", host && strcmp(host, "localhost") == 0);

        int port = yaml_get_int(doc, "server.port", -1);
        TEST("yaml_get_int server.port", port == 8080);

        const char *name = yaml_get_string(doc, "name");
        TEST("yaml_get_string name", name && strcmp(name, "test-app") == 0);

        bool debug = yaml_get_bool(doc, "debug", false);
        TEST("yaml_get_bool debug", debug == true);

        /* Test 3: defaults for missing keys */
        const char *missing = yaml_get_string(doc, "nonexistent");
        TEST("yaml_get_string missing returns NULL", missing == NULL);

        int missing_int = yaml_get_int(doc, "server.nonexistent", 42);
        TEST("yaml_get_int default", missing_int == 42);

        /* Test 4: iterate top-level keys */
        int count = 0;
        yaml_iterate(doc, count_cb, &count);
        TEST("yaml_iterate count > 0", count > 0);

        yaml_free(doc);
    }

    /* Test 5: parse error */
    char *err2 = NULL;
    yaml_doc_t *bad = yaml_parse("{{{{invalid", &err2);
    TEST("yaml_parse error returns NULL", bad == NULL);
    TEST("yaml_parse error message set", err2 != NULL);
    free(err2);

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All YAML tests PASSED");
    return failures ? 1 : 0;
}
