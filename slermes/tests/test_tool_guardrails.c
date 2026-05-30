/*
 * test_tool_guardrails.c — Tests for tool call loop guardrails (G28-G30).
 */

#include "hermes_tool_guardrails.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

static void test_init_reset(void)
{
    tool_guardrail_controller_t ctrl;
    tool_guardrail_init(&ctrl);

    TEST("init exact failure warn after", ctrl.exact_failure_warn_after == 2);
    TEST("init exact failure block after", ctrl.exact_failure_block_after == 5);
    TEST("init warnings enabled", ctrl.warnings_enabled == true);
    TEST("init hard stop disabled", ctrl.hard_stop_enabled == false);
    TEST("init count zero", ctrl.count == 0);
    TEST("init no halt", ctrl.halt_decision_active == false);

    tool_guardrail_reset(&ctrl);
    TEST("reset count zero", ctrl.count == 0);
    TEST("reset no halt", ctrl.halt_decision_active == false);
}

static void test_idempotent_check(void)
{
    TEST("read_file is idempotent", tool_guardrail_is_idempotent("read_file"));
    TEST("search_files is idempotent", tool_guardrail_is_idempotent("search_files"));
    TEST("web_search is idempotent", tool_guardrail_is_idempotent("web_search"));
    TEST("unknown not idempotent", !tool_guardrail_is_idempotent("unknown_tool"));
    TEST("NULL not idempotent", !tool_guardrail_is_idempotent(NULL));
}

static void test_mutating_check(void)
{
    TEST("terminal is mutating", tool_guardrail_is_mutating("terminal"));
    TEST("write_file is mutating", tool_guardrail_is_mutating("write_file"));
    TEST("patch is mutating", tool_guardrail_is_mutating("patch"));
    TEST("memory is mutating", tool_guardrail_is_mutating("memory"));
    TEST("read_file NOT mutating", !tool_guardrail_is_mutating("read_file"));
    TEST("NULL not mutating", !tool_guardrail_is_mutating(NULL));
}

static void test_before_call_no_hardstop(void)
{
    tool_guardrail_controller_t ctrl;
    tool_guardrail_init(&ctrl);
    ctrl.hard_stop_enabled = false;

    tool_guardrail_decision_t d = tool_guardrail_before_call(&ctrl, "read_file", "{}");
    TEST("no hard stop: allow", d.action == GUARDRAIL_ALLOW);
    TEST("no hard stop: tool name", strcmp(d.tool_name, "read_file") == 0);
}

static void test_before_call_with_hardstop(void)
{
    tool_guardrail_controller_t ctrl;
    tool_guardrail_init(&ctrl);
    ctrl.hard_stop_enabled = true;

    /* First call should always be allowed */
    tool_guardrail_decision_t d = tool_guardrail_before_call(&ctrl, "read_file", "{}");
    TEST("first call allow", d.action == GUARDRAIL_ALLOW);

    /* After halted, all calls should be skipped */
    ctrl.halt_decision_active = true;
    d = tool_guardrail_before_call(&ctrl, "read_file", "{}");
    TEST("after halt: allow without blocking msg",
        d.action == GUARDRAIL_ALLOW && d.message[0] != '\0');
}

static void test_after_call_failure_tracking(void)
{
    tool_guardrail_controller_t ctrl;
    tool_guardrail_init(&ctrl);
    ctrl.warnings_enabled = true;
    ctrl.hard_stop_enabled = true;

    /* First failure — should be allow */
    tool_guardrail_decision_t d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    TEST("first fail: allow", d.action == GUARDRAIL_ALLOW);

    /* Second failure with same args — exact warning threshold (warn_after=2) */
    d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    TEST("second fail exact: warn", d.action == GUARDRAIL_WARN);
    TEST("second fail exact: code contains repeated_exact",
        strstr(d.code, "repeated_exact") != NULL);

    /* Third, fourth, fifth, sixth, seventh — still warn */
    d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);

    /* Eighth failure — halt (same_tool_failure_halt_after=8) */
    d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    TEST("eighth fail: halt", d.action == GUARDRAIL_HALT);
    TEST("eighth fail: halt code", strstr(d.code, "same_tool_failure") != NULL);

    /* After halt, halt_decision should be active */
    TEST("halt decision active", ctrl.halt_decision_active == true);
}

static void test_after_call_success_clears(void)
{
    tool_guardrail_controller_t ctrl;
    tool_guardrail_init(&ctrl);
    ctrl.warnings_enabled = true;

    /* Two failures */
    tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    tool_guardrail_decision_t d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    TEST("two fails: warn", d.action == GUARDRAIL_WARN);

    /* Then success — should clear counter */
    d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "ok", false);
    TEST("after success: allow", d.action == GUARDRAIL_ALLOW);

    /* Next failure should start from 1 again */
    d = tool_guardrail_after_call(&ctrl, "terminal", "ls", "error", true);
    TEST("fail after success: allow (counter reset)", d.action == GUARDRAIL_ALLOW);
}

static void test_idempotent_no_progress(void)
{
    tool_guardrail_controller_t ctrl;
    tool_guardrail_init(&ctrl);
    ctrl.warnings_enabled = true;

    /* Idempotent tool returning same result */
    tool_guardrail_decision_t d = tool_guardrail_after_call(&ctrl, "read_file", "/etc/hostname", "localhost", false);
    TEST("idempotent first: allow", d.action == GUARDRAIL_ALLOW);

    d = tool_guardrail_after_call(&ctrl, "read_file", "/etc/hostname", "localhost", false);
    TEST("idempotent second: warn (no progress)", d.action == GUARDRAIL_WARN);
    TEST("idempotent second: no_progress code",
        strstr(d.code, "no_progress") != NULL);

    /* Different result resets */
    d = tool_guardrail_after_call(&ctrl, "read_file", "/etc/hostname", "myhost", false);
    TEST("idempotent different result: allow", d.action == GUARDRAIL_ALLOW);
}

static void test_hash(void)
{
    TEST("hash empty", tool_guardrail_hash("") == 5381);
    TEST("hash same", tool_guardrail_hash("hello") == tool_guardrail_hash("hello"));
    TEST("hash different", tool_guardrail_hash("hello") != tool_guardrail_hash("world"));
    TEST("hash NULL", tool_guardrail_hash(NULL) == 0);
}

static void test_synthetic_result(void)
{
    tool_guardrail_decision_t d;
    memset(&d, 0, sizeof(d));
    d.action = GUARDRAIL_BLOCK;
    snprintf(d.code, sizeof(d.code), "test_block");
    snprintf(d.tool_name, sizeof(d.tool_name), "read_file");
    d.count = 3;

    char *result = tool_guardrail_synthetic_result(&d);
    TEST("synthetic result not null", result != NULL);
    if (result) {
        TEST("synthetic contains guardrail", strstr(result, "guardrail") != NULL);
        TEST("synthetic contains tool name", strstr(result, "read_file") != NULL);
        free(result);
    }
}

static void test_append_guidance(void)
{
    tool_guardrail_decision_t d;
    memset(&d, 0, sizeof(d));
    d.action = GUARDRAIL_WARN;
    snprintf(d.code, sizeof(d.code), "test_warn");
    snprintf(d.message, sizeof(d.message), "test message here");
    d.count = 2;

    char *appended = tool_guardrail_append_guidance("original result", &d);
    TEST("appended not null", appended != NULL);
    if (appended) {
        TEST("appended contains original", strstr(appended, "original result") != NULL);
        TEST("appended contains warning label", strstr(appended, "Tool loop warning") != NULL);
        TEST("appended contains code", strstr(appended, "test_warn") != NULL);
        free(appended);
    }

    /* No guidance for allow */
    memset(&d, 0, sizeof(d));
    d.action = GUARDRAIL_ALLOW;
    appended = tool_guardrail_append_guidance("result", &d);
    TEST("allow: no append change", appended != NULL && strcmp(appended, "result") == 0);
    free(appended);
}

int main(void)
{
    printf("=== Tool Call Guardrails Library Tests (G28-G30) ===\n");

    test_init_reset();
    test_idempotent_check();
    test_mutating_check();
    test_hash();
    test_before_call_no_hardstop();
    test_before_call_with_hardstop();
    test_after_call_failure_tracking();
    test_after_call_success_clears();
    test_idempotent_no_progress();
    test_synthetic_result();
    test_append_guidance();

    printf("\nResults: %d passed, %d failed, %d total\n", passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
