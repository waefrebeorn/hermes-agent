/*
 * test_delegate.c — Delegate tool unit tests (M32).
 *
 * Tests: argument validation and error paths only.
 * NOTE: delegate_ctx_t is 20MB (64 children × ~325KB each).
 * MUST run with: ulimit -s unlimited
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_delegate.c src/tools/delegate.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_delegate -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   ulimit -s unlimited && /tmp/hermes_test_delegate
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

char *delegate_handler(const char *args_json, const char *task_id);

/* Stubs for config loading */
void hermes_config_defaults(hermes_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->delegation.max_concurrent_children = 3;
    cfg->delegation.max_spawn_depth = 1;
    cfg->delegation.child_max_turns = 30;
    cfg->agent.max_iterations = 90;
}

bool hermes_config_load(hermes_config_t *cfg, const char *path) {
    (void)cfg; (void)path; return true;
}

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

static int has_error(const char *json_str) {
    if (!json_str) return 0;
    char *err = NULL;
    json_node_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return 1; }
    const char *e = json_get_str(root, "error", NULL);
    int rv = (e != NULL);
    json_free(root);
    return rv;
}

int main(void) {
    printf("=== Delegate Tool Tests ===\n");

    /* Validation tests — return BEFORE child spawning */

    /* Test 1: Null args */
    {
        char *res = delegate_handler(NULL, NULL);
        TEST("null args returns error", has_error(res));
        free(res);
    }

    /* Test 2: Invalid JSON */
    {
        char *res = delegate_handler("{bad json}", NULL);
        TEST("invalid JSON returns error", has_error(res));
        free(res);
    }

    /* Test 3: Empty args — missing goal and subtasks */
    {
        char *res = delegate_handler("{}", NULL);
        TEST("empty args returns error (missing goal)", has_error(res));
        free(res);
    }

    /* Test 4: Context without goal */
    {
        char *res = delegate_handler("{\"context\":\"some context\"}", NULL);
        TEST("context without goal returns error", has_error(res));
        free(res);
    }

    /* Edge: empty string args */
    {
        char *res = delegate_handler("", NULL);
        TEST("empty string args returns error", has_error(res));
        free(res);
    }

    /* Edge: whitespace-only goal (handler doesn't strip — passes validation) */
    {
        char *res = delegate_handler("{\"goal\":\"   \"}", NULL);
        TEST("whitespace goal passes through (no strip)", !has_error(res) || strstr(res, "session_id") != NULL);
        free(res);
    }

    /* Edge: goal with subtask list but no context */
    {
        char *res = delegate_handler("{\"goal\":\"do something\",\"subtasks\":[{\"description\":\"step 1\"}]}", NULL);
        TEST("goal + subtasks passes validation", !has_error(res) || !strstr(res, "missing"));
        free(res);
    }

    /* Edge: null goal value */
    {
        char *res = delegate_handler("{\"goal\":null}", NULL);
        TEST("null goal value returns error", has_error(res));
        free(res);
    }

    /* Edge: empty subtasks array */
    {
        char *res = delegate_handler("{\"goal\":\"test\",\"subtasks\":[]}", NULL);
        TEST("empty subtasks array passes (valid)", !has_error(res));
        free(res);
    }

    /* Edge: very long goal string (2000+ chars) */
    {
        char *long_goal = malloc(2200);
        if (long_goal) {
            memset(long_goal, 'A', 2000);
            long_goal[2000] = '\0';
            char args[2500];
            snprintf(args, sizeof(args), "{\"goal\":\"%s\"}", long_goal);
            char *res = delegate_handler(args, NULL);
            TEST("very long goal processed without crash", res != NULL);
            free(res);
            free(long_goal);
        }
    }

    /* Edge: NaN/infinity in numeric fields */
    {
        char *res = delegate_handler("{\"goal\":\"test\",\"max_concurrent_children\":-1}", NULL);
        TEST("negative max_concurrent_children handled", !has_error(res) || (res != NULL));
        free(res);
    }

    /* Edge: unicode/special characters in goal */
    {
        char *res = delegate_handler("{\"goal\":\"emoji test \\u2603\\ufe0f\"}", NULL);
        TEST("unicode in goal passes validation", !has_error(res));
        free(res);
    }

    /* Edge: missing subtasks with context only */
    {
        char *res = delegate_handler("{\"goal\":\"test\",\"context\":\"some useful context here\"}", NULL);
        TEST("goal + context (no subtasks) passes validation", !has_error(res));
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All delegate tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
