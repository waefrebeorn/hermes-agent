/*
 * test_yaml.c — Expanded test suite for YAML parser.
 * Tests: parse, get_string/bool/int, lists, map_keys, to_json_string,
 *        multi-document, file parsing, NULL safety, edge cases.
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

static int write_test_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    size_t n = fwrite(content, 1, strlen(content), f);
    fclose(f);
    return (int)n;
}

int main(void) {
    /* ── Basic parse ── */
    const char *input =
        "server:\n"
        "  host: localhost\n"
        "  port: 8080\n"
        "name: test-app\n"
        "debug: true\n"
        "list:\n"
        "  - alpha\n"
        "  - beta\n"
        "  - gamma\n"
        "nested:\n"
        "  level1:\n"
        "    level2:\n"
        "      value: deep\n"
        "booleans:\n"
        "  yes_val: yes\n"
        "  no_val: no\n"
        "  on_val: on\n"
        "  off_val: off\n"
        "  true_val: true\n"
        "  false_val: false\n"
        "numbers:\n"
        "  neg: -42\n"
        "  zero: 0\n"
        "comment_test: val # this is a comment\n"
        "empty_str:\n"
        "quoted_key: \"hello:world\"\n"
        "nested_map:\n"
        "  sub1:\n"
        "    inner: x\n"
        "  sub2:\n"
        "    inner: y\n";

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(input, &err);
    TEST("yaml_parse success", doc != NULL && err == NULL);
    if (err) free(err);

    if (!doc) {
        printf("\nSOME TESTS FAILED (parse failed)\n");
        return 1;
    }

    /* ── Basic getters ── */
    TEST("yaml_get_string server.host",
         ({ const char *v = yaml_get_string(doc, "server.host");
            v && strcmp(v, "localhost") == 0; }));
    TEST("yaml_get_int server.port == 8080",
         yaml_get_int(doc, "server.port", -1) == 8080);
    TEST("yaml_get_string name",
         ({ const char *v = yaml_get_string(doc, "name");
            v && strcmp(v, "test-app") == 0; }));
    TEST("yaml_get_bool debug true",
         yaml_get_bool(doc, "debug", false) == true);

    /* ── Defaults for missing ── */
    TEST("yaml_get_string missing NULL",
         yaml_get_string(doc, "nonexistent") == NULL);
    TEST("yaml_get_int default 42",
         yaml_get_int(doc, "server.nonexistent", 42) == 42);
    TEST("yaml_get_bool missing default true",
         yaml_get_bool(doc, "no_such_key", true) == true);

    /* ── Iterate top-level keys ── */
    int count = 0;
    yaml_iterate(doc, count_cb, &count);
    TEST("yaml_iterate count > 0", count > 0);

    /* ── List operations ── */
    TEST("yaml_list_count list == 3",
         yaml_list_count(doc, "list") == 3);
    TEST("yaml_list_get list[0] alpha",
         ({ const char *v = yaml_list_get(doc, "list", 0);
            v && strcmp(v, "alpha") == 0; }));
    TEST("yaml_list_get list[2] gamma",
         ({ const char *v = yaml_list_get(doc, "list", 2);
            v && strcmp(v, "gamma") == 0; }));
    TEST("yaml_list_get out of range NULL",
         yaml_list_get(doc, "list", 99) == NULL);
    TEST("yaml_list_count on non-list == 0",
         yaml_list_count(doc, "name") == 0);

    /* ── Map keys ── */
    size_t key_count = 0;
    char **keys = yaml_map_keys(doc, "nested_map", &key_count);
    TEST("yaml_map_keys count == 2", keys != NULL && key_count == 2);
    if (keys) {
        TEST("yaml_map_keys key[0] sub1 or sub2",
             strcmp(keys[0], "sub1") == 0 || strcmp(keys[0], "sub2") == 0);
        TEST("yaml_map_keys key[1] other sub",
             (strcmp(keys[0], "sub1") == 0 && strcmp(keys[1], "sub2") == 0) ||
             (strcmp(keys[0], "sub2") == 0 && strcmp(keys[1], "sub1") == 0));
        for (size_t i = 0; i < key_count; i++) free(keys[i]);
        free(keys);
    }
    TEST("yaml_map_keys non-existent NULL",
         ({ size_t c = 99; char **k = yaml_map_keys(doc, "no_such", &c);
            bool r = (k == NULL && c == 0); free(k); r; }));
    TEST("yaml_map_keys on scalar NULL",
         ({ size_t c = 99; char **k = yaml_map_keys(doc, "name", &c);
            bool r = (k == NULL && c == 0); free(k); r; }));

    /* ── JSON serialization ── */
    char *json = yaml_to_json_string(doc, "server");
    TEST("yaml_to_json_string server not NULL", json != NULL);
    if (json) {
        TEST("yaml_to_json_string contains host",
             strstr(json, "localhost") != NULL);
        TEST("yaml_to_json_string contains 8080",
             strstr(json, "8080") != NULL);
        free(json);
    }
    TEST("yaml_to_json_string non-existent NULL",
         yaml_to_json_string(doc, "no_such") == NULL);

    /* ── Deep nesting ── */
    TEST("yaml_get_string 5-level deep",
         ({ const char *v = yaml_get_string(doc, "nested.level1.level2.value");
            v && strcmp(v, "deep") == 0; }));

    /* ── Boolean variants ── */
    TEST("yaml_get_bool yes=true",
         yaml_get_bool(doc, "booleans.yes_val", false) == true);
    TEST("yaml_get_bool no=false",
         yaml_get_bool(doc, "booleans.no_val", true) == false);
    TEST("yaml_get_bool on=true",
         yaml_get_bool(doc, "booleans.on_val", false) == true);
    TEST("yaml_get_bool off=false",
         yaml_get_bool(doc, "booleans.off_val", true) == false);
    TEST("yaml_get_bool true=true",
         yaml_get_bool(doc, "booleans.true_val", false) == true);
    TEST("yaml_get_bool false=false",
         yaml_get_bool(doc, "booleans.false_val", true) == false);

    /* ── Number edge cases ── */
    TEST("yaml_get_int negative -42",
         yaml_get_int(doc, "numbers.neg", 0) == -42);
    TEST("yaml_get_int zero 0",
         yaml_get_int(doc, "numbers.zero", -1) == 0);

    /* ── Comment handling ── */
    TEST("yaml_get_string comment stripped",
         ({ const char *v = yaml_get_string(doc, "comment_test");
            v && strcmp(v, "val") == 0; }));

    /* ── Empty string value ── */
    TEST("yaml_get_string empty str",
         ({ const char *v = yaml_get_string(doc, "empty_str");
            v && strcmp(v, "") == 0; }));

    /* ── Quoted value with colon ── */
    TEST("yaml_get_string quoted colon",
         ({ const char *v = yaml_get_string(doc, "quoted_key");
            v && strcmp(v, "\"hello:world\"") == 0; }));

    /* ── Multi-document ── */
    const char *multi_input =
        "doc1: first\n---\ndoc2: second\n";
    size_t mcount = 0;
    yaml_doc_t **mdocs = yaml_parse_multi(multi_input, &mcount, NULL);
    TEST("yaml_parse_multi 2 docs", mdocs != NULL && mcount == 2);
    if (mdocs && mcount == 2) {
        TEST("yaml_get_string doc1 first",
             ({ const char *v = yaml_get_string(mdocs[0], "doc1");
                v && strcmp(v, "first") == 0; }));
        TEST("yaml_get_string doc2 second",
             ({ const char *v = yaml_get_string(mdocs[1], "doc2");
                v && strcmp(v, "second") == 0; }));
        for (size_t i = 0; i < mcount; i++) yaml_free(mdocs[i]);
        free(mdocs);
    }

    /* ── File parsing ── */
    const char *tmp = "/tmp/hermes_test_yaml.yml";
    if (write_test_file(tmp, "from_file: okay\n") > 0) {
        char *ferr = NULL;
        yaml_doc_t *fdoc = yaml_parse_file(tmp, &ferr);
        TEST("yaml_parse_file success", fdoc != NULL && ferr == NULL);
        if (fdoc) {
            TEST("yaml_get_string from_file",
                 ({ const char *v = yaml_get_string(fdoc, "from_file");
                    v && strcmp(v, "okay") == 0; }));
            yaml_free(fdoc);
        }
        if (ferr) free(ferr);
        remove(tmp);
    }
    /* File parsing: non-existent file → NULL + error */
    char *ferr2 = NULL;
    yaml_doc_t *fdoc2 = yaml_parse_file("/tmp/nonexistent_XXXX.yml", &ferr2);
    TEST("yaml_parse_file missing returns NULL", fdoc2 == NULL);
    TEST("yaml_parse_file error set", ferr2 != NULL);
    if (ferr2) free(ferr2);

    /* ── Parse error ── */
    char *err2 = NULL;
    yaml_doc_t *bad = yaml_parse("{{{{invalid", &err2);
    TEST("yaml_parse error NULL", bad == NULL);
    TEST("yaml_parse error msg set", err2 != NULL);
    free(err2);

    /* ── Empty document ── */
    char *eerr = NULL;
    yaml_doc_t *edoc = yaml_parse("", &eerr);
    TEST("yaml_parse empty string", edoc != NULL && eerr == NULL);
    if (edoc) {
        TEST("yaml_get_string on empty doc NULL",
             yaml_get_string(edoc, "anything") == NULL);
        int ec = 0;
        yaml_iterate(edoc, count_cb, &ec);
        TEST("yaml_iterate empty doc == 0", ec == 0);
        yaml_free(edoc);
    }
    if (eerr) free(eerr);

    /* ── Whitespace-only document (returns NULL, no error) ── */
    char *werr = NULL;
    yaml_doc_t *wdoc = yaml_parse("   \n  \n", &werr);
    TEST("yaml_parse whitespace only returns NULL", wdoc == NULL);
    if (werr) free(werr);

    /* ── NULL safety ── */
    TEST("yaml_get_string NULL doc returns NULL",
         yaml_get_string(NULL, "key") == NULL);
    TEST("yaml_get_string NULL path returns NULL",
         yaml_get_string(doc, NULL) == NULL);
    TEST("yaml_get_bool NULL doc default",
         yaml_get_bool(NULL, "key", true) == true);
    TEST("yaml_get_int NULL doc default",
         yaml_get_int(NULL, "key", 42) == 42);
    TEST("yaml_list_count NULL doc", yaml_list_count(NULL, "key") == 0);
    TEST("yaml_list_get NULL doc", yaml_list_get(NULL, "key", 0) == NULL);
    TEST("yaml_map_keys NULL doc",
         ({ size_t c = 99; char **k = yaml_map_keys(NULL, "path", &c);
            bool r = (k == NULL && c == 0); free(k); r; }));
    TEST("yaml_to_json_string NULL doc",
         yaml_to_json_string(NULL, "key") == NULL);

    /* ── Multi-document single doc ── */
    const char *single_multi = "single: doc\n";
    size_t scount = 0;
    yaml_doc_t **sdocs = yaml_parse_multi(single_multi, &scount, NULL);
    TEST("yaml_parse_multi single doc", sdocs != NULL && scount == 1);
    if (sdocs && scount == 1) {
        TEST("yaml_get_string single content",
             ({ const char *v = yaml_get_string(sdocs[0], "single");
                v && strcmp(v, "doc") == 0; }));
        yaml_free(sdocs[0]);
        free(sdocs);
    }

    yaml_free(doc);

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All YAML tests PASSED");
    return failures ? 1 : 0;
}
