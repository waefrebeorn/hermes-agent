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

/* Stub for SQLite-backed cron store used by list action */
struct cron_sqlite_store_t { int _; };
struct cron_sqlite_store_t *g_cron_store = NULL;

/* Initialize the cron store for testing */
static void cron_store_init(void) {
    if (!g_cron_store) {
        g_cron_store = calloc(1, sizeof(struct cron_sqlite_store_t));
    }
}

static void cron_store_cleanup(void) {
    free(g_cron_store);
    g_cron_store = NULL;
}

char *cron_sqlite_list_to_json(struct cron_sqlite_store_t *store) {
    (void)store;
    return strdup("[]");
}

bool cron_sqlite_update_job(struct cron_sqlite_store_t *store, const char *name,
                             const char *field, const char *value) {
    (void)store; (void)name; (void)field; (void)value;
    return true;
}

char *cron_sqlite_get_command(struct cron_sqlite_store_t *store, const char *name) {
    (void)store; (void)name;
    return strdup("echo test");
}

/* Stub for cron_run_job — called by "run" action */
void cron_run_job(const char *name, const char *command) {
    (void)name; (void)command;
}

/* Stub for cron.h functions needed by cronjob.c */

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
    cron_store_init();

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

    /* --- Edge case expansion: 20 new assertions --- */

    /* Test 19: Empty action string should error -- not same as missing */
    {
        char *res = cronjob_handler("{\"action\":\"\"}", NULL);
        TEST("empty action returns error", has_error(res));
        free(res);
    }

    /* Test 20: Update without name */
    {
        char *res = cronjob_handler("{\"action\":\"update\"}", NULL);
        TEST("update without name returns error", has_error(res));
        free(res);
    }

    /* Test 21: Update with schedule */
    {
        char *res = cronjob_handler(
            "{\"action\":\"update\",\"name\":\"myjob\",\"schedule\":\"@weekly\"}", NULL);
        TEST("update with schedule returns non-NULL", res != NULL);
        if (res) {
            TEST("update returns status", json_contains(res, "\"status\""));
        }
        free(res);
    }

    /* Test 22: Update with command + retry */
    {
        char *res = cronjob_handler(
            "{\"action\":\"update\",\"name\":\"myjob\",\"command\":\"echo new\","
            "\"retry\":2,\"backoff\":15}", NULL);
        TEST("update with command+retry returns non-NULL", res != NULL);
        free(res);
    }

    /* Test 23: Pause without name */
    {
        char *res = cronjob_handler("{\"action\":\"pause\"}", NULL);
        TEST("pause without name returns error", has_error(res));
        free(res);
    }

    /* Test 24: Pause with name */
    {
        char *res = cronjob_handler("{\"action\":\"pause\",\"name\":\"myjob\"}", NULL);
        TEST("pause with name returns non-NULL", res != NULL);
        if (res) {
            TEST("pause returns status paused", json_contains(res, "\"paused\""));
        }
        free(res);
    }

    /* Test 25: Resume without name */
    {
        char *res = cronjob_handler("{\"action\":\"resume\"}", NULL);
        TEST("resume without name returns error", has_error(res));
        free(res);
    }

    /* Test 26: Resume with name */
    {
        char *res = cronjob_handler("{\"action\":\"resume\",\"name\":\"myjob\"}", NULL);
        TEST("resume with name returns non-NULL", res != NULL);
        if (res) {
            TEST("resume returns status resumed", json_contains(res, "\"resumed\""));
        }
        free(res);
    }

    /* Test 27: Run without name */
    {
        char *res = cronjob_handler("{\"action\":\"run\"}", NULL);
        TEST("run without name returns error", has_error(res));
        free(res);
    }

    /* Test 28: Run with name */
    {
        char *res = cronjob_handler("{\"action\":\"run\",\"name\":\"myjob\"}", NULL);
        TEST("run with name returns non-NULL", res != NULL);
        if (res) {
            TEST("run returns status triggered", json_contains(res, "\"triggered\""));
        }
        free(res);
    }

    /* Test 29: History without name */
    {
        char *res = cronjob_handler("{\"action\":\"history\"}", NULL);
        TEST("history without name returns error", has_error(res));
        free(res);
    }

    /* Test 30: History with name */
    {
        char *res = cronjob_handler("{\"action\":\"history\",\"name\":\"myjob\"}", NULL);
        TEST("history with name returns non-NULL", res != NULL);
        if (res) {
            TEST("history has history array", json_contains(res, "\"history\""));
            TEST("history has total_runs", json_contains(res, "\"total_runs\""));
        }
        free(res);
    }

    /* Test 31: History with limit param */
    {
        char *res = cronjob_handler(
            "{\"action\":\"history\",\"name\":\"myjob\",\"limit\":5}", NULL);
        TEST("history with limit returns non-NULL", res != NULL);
        free(res);
    }

    /* Test 32: Unknown @-schedule */
    {
        char *res = cronjob_handler(
            "{\"action\":\"add\",\"name\":\"bad_sched\",\"schedule\":\"@fortnightly\",\"command\":\"x\"}", NULL);
        TEST("unknown @-schedule returns error", has_error(res));
        free(res);
    }

    /* Test 33: Add with timezone */
    {
        char *res = cronjob_handler(
            "{\"action\":\"add\",\"name\":\"tz_test\",\"schedule\":\"@daily\","
            "\"command\":\"echo tz\",\"timezone\":\"America/New_York\"}", NULL);
        TEST("add with timezone returns non-NULL", res != NULL);
        if (res) {
            TEST("add with timezone has status", json_contains(res, "\"status\""));
        }
        free(res);
    }

    /* Test 34: Config with timezone */
    {
        char *res = cronjob_handler(
            "{\"action\":\"config\",\"name\":\"tz_job\",\"timezone\":\"UTC\"}", NULL);
        TEST("config with timezone returns non-NULL", res != NULL);
        if (res) {
            TEST("config with timezone returns status configured",
                 json_contains(res, "\"configured\""));
        }
        free(res);
    }

    /* Test 35: Add with empty command */
    {
        char *res = cronjob_handler(
            "{\"action\":\"add\",\"name\":\"empty_cmd\",\"schedule\":\"@hourly\",\"command\":\"\"}", NULL);
        TEST("add with empty command returns non-NULL", res != NULL);
        if (res) {
            TEST("add empty command succeeds", json_contains(res, "\"status\""));
        }
        free(res);
    }

    /* Test 36: List with name filter */
    {
        char *res = cronjob_handler(
            "{\"action\":\"list\",\"name\":\"myjob\"}", NULL);
        TEST("list with name filter returns non-NULL", res != NULL);
        if (res) {
            TEST("list filtered has jobs", json_contains(res, "\"jobs\""));
        }
        free(res);
    }

    /* Test 37: Config without any settings (just name) */
    {
        char *res = cronjob_handler("{\"action\":\"config\",\"name\":\"test\"}", NULL);
        TEST("config with just name returns non-NULL", res != NULL);
        if (res) {
            TEST("config bare returns status configured",
                 json_contains(res, "\"configured\""));
        }
        free(res);
    }

    /* Test 38: Update with empty schedule (no change requested, should error "No changes") */
    {
        char *res = cronjob_handler(
            "{\"action\":\"update\",\"name\":\"myjob\",\"schedule\":\"\"}", NULL);
        TEST("update with empty schedule returns error", has_error(res));
        free(res);
    }

    /* Test 39: Add with empty name should succeed (not error on name alone) */
    {
        char *res = cronjob_handler(
            "{\"action\":\"add\",\"name\":\"\",\"schedule\":\"@daily\",\"command\":\"x\"}", NULL);
        TEST("add with empty name returns non-NULL", res != NULL);
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All cron tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    cron_store_cleanup();
    return failed ? 1 : 0;
}
