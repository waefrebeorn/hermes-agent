/*
 * fuzz_harness.c — Fuzz test harness for Hermes C (T08).
 *
 * Exercises tool JSON parsing, provider config parsing, and other
 * input-processing functions with random/corrupted inputs.
 * Build with: make fuzz
 * Run with:   ./hermes-fuzz [iterations]
 */

#include "hermes.h"
#include "hermes_json.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Number of fuzz iterations per run */
#define DEFAULT_ITERATIONS 10000

/* Max input length */
#define MAX_INPUT 4096

/* ─── Test cases ──────────────────────────────────────────── */

static int passed = 0;
static int failed = 0;

#define TEST(expr, name) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Fuzz JSON parsing with random inputs */
static void fuzz_json_parse(void) {
    unsigned char buf[MAX_INPUT];
    for (int i = 0; i < 1000; i++) {
        /* Generate random-length input */
        int len = rand() % MAX_INPUT;
        for (int j = 0; j < len; j++)
            buf[j] = (unsigned char)(rand() & 0xFF);

        /* Ensure null-terminated */
        buf[len] = '\0';

        /* Try parsing — should never crash */
        char *err = NULL;
        json_t *root = json_parse((const char *)buf, &err);
        free(err);
        if (root) json_free(root);
    }
    passed++;
    printf("  PASS: fuzz_json_parse (1000 iterations, no crashes)\n");
}

/* Fuzz tool argument parsing with malformed JSON */
static void fuzz_tool_args(void) {
    const char *inputs[] = {
        "null",
        "{}",
        "{\"key\": \"value\"}",
        "{\"key\": 123}",
        "{\"key\": []}",
        "{\"key\": {}}",
        "\"just a string\"",
        "[1, 2, 3]",
        "{\"nested\": {\"deeply\": {\"buried\": \"value\"}}}",
        "{\"empty\": \"\", \"spaces\": \"   \", \"newlines\": \"\n\n\"}",
        "{\"unicode\": \"\u00e9\u00e0\u00fc\u00f1\"}",
        "{\"escaped\": \"tab\there\nand\\\"quotes\\\"\"}",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        char *err = NULL;
        json_t *args = json_parse(inputs[i], &err);
        free(err);
        if (args) {
            /* Smoke test: try reading various types */
            (void)json_get_str(args, "key", "");
            (void)json_get_num(args, "key", 0);
            (void)json_get_bool(args, "key", false);
            json_free(args);
        }
    }
    passed++;
    printf("  PASS: fuzz_tool_args (%zu inputs)\n",
           sizeof(inputs)/sizeof(inputs[0]) - 1);
}

/* Fuzz path resolution edge cases */
static void fuzz_paths(void) {
    const char *paths[] = {
        "",
        "/",
        ".",
        "..",
        "/../",
        "/./",
        "foo/bar/../baz",
        "///foo///bar///",
        "/../../../../etc/passwd",
        "a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p",
        "....",
        "..foo..",
        "foo..bar",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        /* These should never crash */
        bool traversal = path_has_traversal(paths[i]);
        (void)traversal;
        char *normalized = path_normalize(paths[i]);
        free(normalized);
    }
    passed++;
    printf("  PASS: fuzz_paths (%zu paths)\n",
           sizeof(paths)/sizeof(paths[0]) - 1);
}

/* ─── Main ────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    srand((unsigned int)time(NULL));

    printf("=== Fuzz Test Harness ===\n\n");

    printf("-- JSON parsing --\n");
    fuzz_json_parse();

    printf("\n-- Tool arguments --\n");
    fuzz_tool_args();

    printf("\n-- Path resolution --\n");
    fuzz_paths();

    printf("\n%s\n", failed > 0 ? "SOME FUZZ TESTS FAILED" : "ALL FUZZ TESTS PASSED");
    printf("Results: %d passed, %d failed\n", passed, failed);

    return failed > 0 ? 1 : 0;
}
