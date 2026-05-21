/*
 * test_dotenv.c — Smoke test for .env parser.
 */
#include "dotenv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    const char *input =
        "# This is a comment\n"
        "API_KEY=abc123\n"
        "DATABASE_URL=postgres://localhost:5432/db\n"
        "DEBUG=true\n"
        "EMPTY=\n"
        "QUOTED=\"hello world\"\n";

    /* Test 1: parse from string */
    char *err = NULL;
    env_t *env = dotenv_parse(input, &err);
    TEST("dotenv_parse success", env != NULL && err == NULL);
    free(err);

    if (env) {
        /* Test 2: get values */
        const char *key = dotenv_get(env, "API_KEY");
        TEST("dotenv_get API_KEY", key && strcmp(key, "abc123") == 0);

        const char *url = dotenv_get(env, "DATABASE_URL");
        TEST("dotenv_get DATABASE_URL", url && strcmp(url, "postgres://localhost:5432/db") == 0);

        const char *debug = dotenv_get(env, "DEBUG");
        TEST("dotenv_get DEBUG", debug && strcmp(debug, "true") == 0);

        const char *empty = dotenv_get(env, "EMPTY");
        TEST("dotenv_get EMPTY (empty value)", empty && strlen(empty) == 0);

        const char *quoted = dotenv_get(env, "QUOTED");
        TEST("dotenv_get QUOTED (strips quotes)", quoted && strcmp(quoted, "hello world") == 0);

        /* Test 3: missing key returns NULL */
        const char *missing = dotenv_get(env, "NONEXISTENT");
        TEST("dotenv_get missing returns NULL", missing == NULL);

        /* Test 4: count */
        size_t cnt = dotenv_count(env);
        TEST("dotenv_count", cnt == 5);

        /* Test 5: iteration */
        size_t idx = 0;
        const char *k, *v;
        int iterated = 0;
        while (dotenv_iter(env, &idx, &k, &v)) {
            iterated++;
            TEST("dotenv_iter key non-NULL", k != NULL);
            TEST("dotenv_iter value non-NULL", v != NULL);
        }
        TEST("dotenv_iter count matches", (size_t)iterated == cnt);

        /* Test 6: set override */
        bool set_ok = dotenv_set(env, "API_KEY", "newkey");
        TEST("dotenv_set success", set_ok == true);
        const char *updated = dotenv_get(env, "API_KEY");
        TEST("dotenv_get after set", updated && strcmp(updated, "newkey") == 0);

        dotenv_free(env);
    }

    /* Test 7: parse empty input */
    env_t *empty_env = dotenv_parse("", NULL);
    TEST("dotenv_parse empty string", empty_env != NULL);
    if (empty_env) {
        TEST("dotenv_count empty", dotenv_count(empty_env) == 0);
        dotenv_free(empty_env);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All dotenv tests PASSED");
    return failures ? 1 : 0;
}
