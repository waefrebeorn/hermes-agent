/*
 * test_process.c — Process tool unit tests (M39).
 *
 * Tests process management: start, poll, wait, kill, log, signal.
 * Each test is self-contained and cleans up after itself.
 *
 * Build:
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_process.c src/tools/process.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_process -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_process
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

extern char *process_handler(const char *args_json, const char *task_id);

static json_node_t *parse_result(const char *json_str) {
    char *err = NULL;
    json_node_t *r = json_parse(json_str, &err);
    if (!r) fprintf(stderr, "  JSON parse error: %s\n", err ? err : "unknown");
    free(err);
    return r;
}

static const char *result_get_str(json_node_t *obj, const char *key, const char *def) {
    return json_object_get_string(obj, key, def);
}

int main(void) {
    printf("=== Process Tool Tests ===\n");

    json_node_t *r;
    char poll_args[256];
    char kill_args[256];
    char wait_args[256];

    /* 1. null args */
    r = parse_result(process_handler(NULL, NULL));
    TEST("process null args returns error",
         r && strstr(result_get_str(r, "error", ""), "No args") != NULL);
    json_free(r);

    /* 2. missing command on start */
    r = parse_result(process_handler("{\"action\":\"start\"}", NULL));
    TEST("process start missing command returns error",
         r && strstr(result_get_str(r, "error", ""), "Missing command") != NULL);
    json_free(r);

    /* 3. start echo and wait for completion */
    r = parse_result(process_handler("{\"action\":\"start\",\"command\":\"echo hello_process\",\"timeout\":5}", NULL));
    TEST("process start echo returns status started",
         r && strcmp(result_get_str(r, "status", ""), "started") == 0);
    double sid = json_object_get_number(r, "session_id", -1);
    TEST("process start has session_id", sid > 0);
    json_free(r);

    /* 4. wait for echo to complete */
    snprintf(wait_args, sizeof(wait_args),
             "{\"action\":\"wait\",\"session_id\":%.0f,\"timeout\":5}", sid);
    r = parse_result(process_handler(wait_args, NULL));
    TEST("process wait echo returns completed",
         r && strcmp(result_get_str(r, "status", ""), "completed") == 0);
    TEST("process wait echo has exit_code (0)",
         r && json_object_get_number(r, "exit_code", -1) == 0.0);
    json_free(r);

    /* 5. poll after completion — verify exit code */
    snprintf(poll_args, sizeof(poll_args),
             "{\"action\":\"poll\",\"session_id\":%.0f}", sid);
    r = parse_result(process_handler(poll_args, NULL));
    TEST("process poll echo shows exit_code 0",
         r && json_object_get_number(r, "exit_code", -1) == 0.0);
    json_free(r);

    /* 6. poll non-existent session */
    r = parse_result(process_handler("{\"action\":\"poll\",\"session_id\":99999}", NULL));
    TEST("process poll non-existent returns error",
         r && strstr(result_get_str(r, "error", ""), "Process not found") != NULL);
    json_free(r);

    /* 7. start sleep and kill it */
    r = parse_result(process_handler("{\"action\":\"start\",\"command\":\"sleep 60\",\"timeout\":10}", NULL));
    TEST("process start sleep returns status started",
         r && strcmp(result_get_str(r, "status", ""), "started") == 0);
    double sid2 = json_object_get_number(r, "session_id", -1);
    json_free(r);

    /* 8. kill the sleep process */
    snprintf(kill_args, sizeof(kill_args),
             "{\"action\":\"kill\",\"session_id\":%.0f}", sid2);
    r = parse_result(process_handler(kill_args, NULL));
    TEST("process kill returns status killed",
         r && strcmp(result_get_str(r, "status", ""), "killed") == 0);
    json_free(r);

    /* 9. verify killed process is not running */
    snprintf(poll_args, sizeof(poll_args),
             "{\"action\":\"poll\",\"session_id\":%.0f}", sid2);
    r = parse_result(process_handler(poll_args, NULL));
    TEST("process poll killed process shows not running",
         r && !json_object_get_bool(r, "running", true));
    json_free(r);

    /* 10. start a command with env override */
    r = parse_result(process_handler(
        "{\"action\":\"start\",\"command\":\"echo $TEST_VAR\",\"env\":\"TEST_VAR=envtest\",\"timeout\":5}", NULL));
    TEST("process start with env returns started",
         r && strcmp(result_get_str(r, "status", ""), "started") == 0);
    double sid3 = json_object_get_number(r, "session_id", -1);
    json_free(r);

    /* Wait then poll to verify exit */
    snprintf(wait_args, sizeof(wait_args),
             "{\"action\":\"wait\",\"session_id\":%.0f,\"timeout\":5}", sid3);
    r = parse_result(process_handler(wait_args, NULL));
    json_free(r);

    snprintf(poll_args, sizeof(poll_args),
             "{\"action\":\"poll\",\"session_id\":%.0f}", sid3);
    r = parse_result(process_handler(poll_args, NULL));
    TEST("process poll env shows exit_code 0",
         r && json_object_get_number(r, "exit_code", -1) == 0.0);
    json_free(r);

    /* 11. signal by name */
    r = parse_result(process_handler("{\"action\":\"start\",\"command\":\"sleep 30\",\"timeout\":5}", NULL));
    TEST("process start for signal test",
         r && strcmp(result_get_str(r, "status", ""), "started") == 0);
    double sid4 = json_object_get_number(r, "session_id", -1);
    json_free(r);

    snprintf(kill_args, sizeof(kill_args),
             "{\"action\":\"signal\",\"session_id\":%.0f,\"signal\":\"SIGKILL\"}", sid4);
    r = parse_result(process_handler(kill_args, NULL));
    TEST("process signal SIGKILL returns signal_sent",
         r && strcmp(result_get_str(r, "status", ""), "signal_sent") == 0);
    json_free(r);

    /* 12. unknown signal name */
    snprintf(kill_args, sizeof(kill_args),
             "{\"action\":\"signal\",\"session_id\":%.0f,\"signal\":\"SIGINVALID\"}", sid4);
    r = parse_result(process_handler(kill_args, NULL));
    TEST("process signal unknown returns error",
         r && strstr(result_get_str(r, "error", ""), "Unknown signal") != NULL);
    json_free(r);

    /* 13. JSON parse error */
    r = parse_result(process_handler("{invalid}", NULL));
    TEST("process JSON parse returns error",
         r && strstr(result_get_str(r, "error", ""), "JSON parse") != NULL);
    json_free(r);

    /* 14. list action — should have running/completed processes */
    {
        json_node_t *r2 = parse_result(process_handler("{\"action\":\"list\"}", NULL));
        TEST("process list returns non-null", r2 != NULL);
        if (r2) {
            double count = json_object_get_number(r2, "count", -1);
            TEST("process list has count > 0", count > 0);
        }
        json_free(r2);
    }

    /* 15. health action */
    {
        json_node_t *r2 = parse_result(process_handler("{\"action\":\"health\"}", NULL));
        TEST("process health returns non-null", r2 != NULL);
        if (r2) {
            const char *status = json_object_get_string(r2, "status", "");
            TEST("process health has status field", strlen(status) > 0);
            double total = json_object_get_number(r2, "total_slots", 0);
            TEST("process health total_slots > 0", total > 0);
        }
        json_free(r2);
    }

    /* 16. log action — get output from completed echo */
    {
        double sid_log = sid;  /* from test 3-5, the echo process */
        char log_args[256];
        snprintf(log_args, sizeof(log_args),
                 "{\"action\":\"log\",\"session_id\":%.0f}", sid_log);
        json_node_t *r2 = parse_result(process_handler(log_args, NULL));
        TEST("process log returns non-null", r2 != NULL);
        if (r2) {
            /* output field always present (may be empty for no-capture procs) */
            const char *out = json_object_get_string(r2, "output", NULL);
            TEST("process log has output field", out != NULL);
        }
        json_free(r2);
    }

    /* 17. Invalid action string */
    {
        json_node_t *r2 = parse_result(process_handler(
            "{\"action\":\"nonexistent_action\",\"session_id\":1}", NULL));
        TEST("process invalid action returns error",
             r2 && strstr(json_object_get_string(r2, "error", ""), "Unknown action") != NULL);
        json_free(r2);
    }

    /* 18. Process not found for kill */
    {
        json_node_t *r2 = parse_result(process_handler(
            "{\"action\":\"kill\",\"session_id\":99999}", NULL));
        TEST("process kill non-existent returns error",
             r2 && strstr(json_object_get_string(r2, "error", ""), "Process not found") != NULL);
        json_free(r2);
    }

    /* 19. Negative session_id (poll) */
    {
        json_node_t *r2 = parse_result(process_handler(
            "{\"action\":\"poll\",\"session_id\":-1}", NULL));
        TEST("process poll negative session_id returns error",
             r2 && strstr(json_object_get_string(r2, "error", ""), "not found") != NULL);
        json_free(r2);
    }

    /* 20. Empty command (just verify no crash) */
    {
        json_node_t *r2 = parse_result(process_handler(
            "{\"action\":\"start\",\"command\":\"\",\"timeout\":5}", NULL));
        TEST("process start empty command does not crash", r2 != NULL);
        json_free(r2);
    }

    /* 21. Write to a running process (cat to echo back) */
    {
        r = parse_result(process_handler(
            "{\"action\":\"start\",\"command\":\"cat\",\"timeout\":5}", NULL));
        TEST("process start for write test",
             r && strcmp(result_get_str(r, "status", ""), "started") == 0);
        double sid_write = json_object_get_number(r, "session_id", -1);
        json_free(r);

        char write_args[256];
        snprintf(write_args, sizeof(write_args),
                 "{\"action\":\"write\",\"session_id\":%.0f,\"data\":\"hello_write\"}", sid_write);
        r = parse_result(process_handler(write_args, NULL));
        TEST("process write returns status written",
             r && strcmp(result_get_str(r, "status", ""), "written") == 0);
        json_free(r);

        snprintf(write_args, sizeof(write_args),
                 "{\"action\":\"close\",\"session_id\":%.0f}", sid_write);
        r = parse_result(process_handler(write_args, NULL));
        /* Process may already have exited after cat echoed input — close may
         * return "closed", "completed", or a different terminal status */
        TEST("process close stdin does not crash",
             r && strstr(result_get_str(r, "status", ""), "ed") != NULL);
        json_free(r);
    }

    /* 22. Signal by number (SIGTERM=15) */
    {
        r = parse_result(process_handler(
            "{\"action\":\"start\",\"command\":\"sleep 30\",\"timeout\":5}", NULL));
        TEST("process start for signal number test",
             r && strcmp(result_get_str(r, "status", ""), "started") == 0);
        double sid_sig = json_object_get_number(r, "session_id", -1);
        json_free(r);

        char sig_args[256];
        snprintf(sig_args, sizeof(sig_args),
                 "{\"action\":\"signal\",\"session_id\":%.0f,\"signal\":15}", sid_sig);
        r = parse_result(process_handler(sig_args, NULL));
        TEST("process signal number 15 (SIGTERM) returns signal_sent",
             r && strcmp(result_get_str(r, "status", ""), "signal_sent") == 0);
        json_free(r);
    }

    /* 23. Log with non-existent session_id */
    {
        json_node_t *r2 = parse_result(process_handler(
            "{\"action\":\"log\",\"session_id\":99999}", NULL));
        TEST("process log non-existent returns error",
             r2 && strstr(json_object_get_string(r2, "error", ""), "not found") != NULL);
        json_free(r2);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
