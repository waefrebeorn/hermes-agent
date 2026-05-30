/* test_mixture_of_agents.c — Tests for Mixture of Agents tool.
 * Tests error paths and argument validation without network access.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Declare the handler (non-static in mixture_of_agents.c) */
extern char *handle_mixture_of_agents(const char *args_json, const char *task_id);

static int pass = 0, fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        pass++; \
    } \
} while(0)

static void test_null_args(void) {
    char *result = handle_mixture_of_agents(NULL, NULL);
    TEST("null args: result not null", result != NULL);
    TEST("null args: contains error", strstr(result, "error") != NULL);
    json_t *j = json_parse(result, NULL);
    TEST("null args: valid JSON", j != NULL);
    if (j) json_free(j);
    free(result);
}

static void test_invalid_json(void) {
    const char *args = "{bad json here";
    char *result = handle_mixture_of_agents(args, NULL);
    TEST("bad json: result not null", result != NULL);
    TEST("bad json: contains error", strstr(result, "error") != NULL);
    free(result);
}

static void test_empty_args(void) {
    const char *args = "{}";
    char *result = handle_mixture_of_agents(args, NULL);
    TEST("empty args: result not null", result != NULL);
    json_t *j = json_parse(result, NULL);
    TEST("empty args: valid JSON", j != NULL);
    if (j) {
        /* Should mention missing user_prompt */
        const char *err = json_get_str(j, "error", "");
        TEST("empty args: error mentions missing parameter",
             strstr(err, "missing") != NULL);
        json_free(j);
    }
    free(result);
}

static void test_missing_user_prompt(void) {
    /* user_prompt missing from args */
    const char *args = "{\"other_param\":\"value\"}";
    char *result = handle_mixture_of_agents(args, NULL);
    TEST("missing prompt: result not null", result != NULL);
    json_t *j = json_parse(result, NULL);
    TEST("missing prompt: valid JSON", j != NULL);
    if (j) {
        const char *err = json_get_str(j, "error", "");
        TEST("missing prompt: error mentions missing parameter",
             strstr(err, "missing") != NULL);
        json_free(j);
    }
    free(result);
}

int main(void) {
    test_null_args();
    test_invalid_json();
    test_empty_args();
    test_missing_user_prompt();

    fprintf(stderr, "mixture_of_agents: %d/%d pass\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
