/*
 * test_cron.c — Cron job tool unit tests (M36).
 *
 * Tests: action dispatch, schedule validation, error paths.
 * Provides stubs for external cron scheduler functions.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       -I lib/libcron tests/test_cron.c src/tools/cronjob.c \
 *       lib/libcron/cron.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_cron -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_cron
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declaration from cronjob.c */
char *cronjob_handler(const char *args_json, const char *task_id);

/* ================================================================
 * Stubs for external cron scheduler functions
 * These replace the real scheduler for isolated testing.
 * ================================================================ */
bool cron_add_job(const char *name, const char *schedule, const char *command) {
    (void)name; (void)schedule; (void)command;
    return true;
}

void cron_remove_job(const char *name) {
    (void)name;
}

char *cron_list_jobs(void) {
    return strdup("[]");
}

bool cron_job_set_retry(const char *job_name, int max_retries, int backoff_sec) {
    (void)job_name; (void)max_retries; (void)backoff_sec;
    return true;
}

bool cron_notify_on_complete(const char *job_name, bool enabled) {
    (void)job_name; (void)enabled;
    return true;
}

bool cron_notify_on_failure(const char *job_name, bool enabled) {
    (void)job_name; (void)enabled;
    return true;
}

bool cron_chain_set_context(const char *job_name, const char *context_from) {
    (void)job_name; (void)context_from;
    return true;
}

/* ================================================================
 * Test framework
 * ================================================================ */
static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

static int json_contains(const char *json_str, const char *sub) {
    if (!json_str || !sub) return 0;
    return strstr(json_str, sub) != NULL;
}

static int has_error(const char *json_str) {
    if (!json_str) return 0;
    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return 1; }
    const char *e = json_get_str(root, "error", NULL);
    int rv = (e != NULL);
    json_free(root);
    return rv;
}

int main(void) {
    printf("=== Cron Tool Tests ===\n");

    /* Test 1: Null args */
    {
        char *res = cronjob_handler(NULL, NULL);
        TEST("null args returns error", has_error(res));
        free(res);
    }

    /* Test 2: Invalid JSON */
    {
        char *res = cronjob_handler("{bad json}", NULL);
        TEST("invalid JSON returns error", has_error(res));
        free(res);
    }

    /* Test 3: Empty args (default action=list) */
    {
        char *res = cronjob_handler("{}", NULL);
        TEST("empty args defaults to list", res != NULL);
        if (res) {
            TEST("list returns jobs array", json_contains(res, "\"jobs\""));
        }
        free(res);
    }

    /* Test 4: Unknown action */
    {
        char *res = cronjob_handler("{\"action\":\"unknown\"}", NULL);
        TEST("unknown action returns error", has_error(res));
        free(res);
    }

    /* Test 5: Add missing name */
    {
        char *res = cronjob_handler("{\"action\":\"add\"}", NULL);
        TEST("add without name returns error", has_error(res));
        free(res);
    }

    /* Test 6: Add missing schedule */
    {
        char *res = cronjob_handler("{\"action\":\"add\",\"name\":\"test\"}", NULL);
        TEST("add without schedule returns error", has_error(res));
        free(res);
    }

    /* Test 7: Add with invalid schedule */
    {
        char *res = cronjob_handler(
            "{\"action\":\"add\",\"name\":\"test\",\"schedule\":\"not-a-cron\"}", NULL);
        TEST("add with invalid schedule returns error", has_error(res));
        free(res);
    }

    /* Test 8: Add with valid @-schedule */
    {
        char *res = cronjob_handler(
            "{\"action\":\"add\",\"name\":\"test\",\"schedule\":\"@hourly\",\"command\":\"echo hi\"}", NULL);
        TEST("add with @hourly returns non-NULL", res != NULL);
        if (res) {
            TEST("add @hourly returns status added", json_contains(res, "\"status\""));
        }
        free(res);
    }

    /* Test 9: Add with valid cron expression */
    {
        char *res = cronjob_handler(
            "{\"action\":\"add\",\"name\":\"test_cron\",\"schedule\":\"0 * * * *\",\"command\":\"ls\"}", NULL);
        TEST("add with cron expression returns non-NULL", res != NULL);
        free(res);
    }

    /* Test 10: Add with notification and retry */
    {
        char *res = cronjob_handler(
            "{\"action\":\"add\",\"name\":\"test_retry\",\"schedule\":\"@daily\","
            "\"command\":\"backup\",\"notify_on_complete\":true,\"notify_on_failure\":false,"
            "\"retry\":3,\"backoff\":30}", NULL);
        TEST("add with notify+retry returns non-NULL", res != NULL);
        if (res) {
            TEST("add with retry has retry field", json_contains(res, "\"retry\""));
        }
        free(res);
    }

    /* Test 11: Add with context_from chaining */
    {
        char *res = cronjob_handler(
            "{\"action\":\"add\",\"name\":\"chained\",\"schedule\":\"@weekly\","
            "\"command\":\"process\",\"context_from\":\"upstream_job\"}", NULL);
        TEST("add with context_from returns non-NULL", res != NULL);
        if (res) {
            TEST("add chained has context_from", json_contains(res, "\"context_from\""));
        }
        free(res);
    }

    /* Test 12: Remove without name */
    {
        char *res = cronjob_handler("{\"action\":\"remove\"}", NULL);
        TEST("remove without name returns error", has_error(res));
        free(res);
    }

    /* Test 13: Remove with name */
    {
        char *res = cronjob_handler("{\"action\":\"remove\",\"name\":\"test\"}", NULL);
        TEST("remove with name returns non-NULL", res != NULL);
        if (res) {
            TEST("remove returns status", json_contains(res, "\"status\""));
        }
        free(res);
    }

    /* Test 14: Config without name */
    {
        char *res = cronjob_handler("{\"action\":\"config\"}", NULL);
        TEST("config without name returns error", has_error(res));
        free(res);
    }

    /* Test 15: Config with notify_on_complete */
    {
        char *res = cronjob_handler(
            "{\"action\":\"config\",\"name\":\"test\",\"notify_on_complete\":true}", NULL);
        TEST("config with notify returns non-NULL", res != NULL);
        if (res) {
            TEST("config returns status configured", json_contains(res, "\"status\""));
        }
        free(res);
    }

    /* Test 16: Config with retry settings */
    {
        char *res = cronjob_handler(
            "{\"action\":\"config\",\"name\":\"test\",\"retry\":5,\"backoff\":120}", NULL);
        TEST("config with retry returns non-NULL", res != NULL);
        free(res);
    }

    /* Test 17: Valid @-schedule expressions */
    {
        char *res;
        /* Test each @-schedule */
        const char *schedules[] = {"@hourly", "@daily", "@weekly", "@monthly", "@yearly"};
        int n = sizeof(schedules)/sizeof(schedules[0]);
        bool all_valid = true;
        for (int i = 0; i < n; i++) {
            char args[512];
            snprintf(args, sizeof(args),
                "{\"action\":\"add\",\"name\":\"t%d\",\"schedule\":\"%s\",\"command\":\"x\"}", i, schedules[i]);
            res = cronjob_handler(args, NULL);
            if (!res || has_error(res)) all_valid = false;
            free(res);
        }
        TEST("all @-schedules valid", all_valid);
    }

    /* Test 18: Explicit list action */
    {
        char *res = cronjob_handler("{\"action\":\"list\"}", NULL);
        TEST("explicit list returns non-NULL", res != NULL);
        if (res) {
            TEST("list action has jobs", json_contains(res, "\"jobs\""));
        }
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All cron tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
