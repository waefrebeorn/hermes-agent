/*
 * test_shell_hooks.c — Shell hooks unit tests.
 *
 * Tests JSON config parsing, spec_tool matching, register/unregister
 * lifecycle, and count tracking.
 *
 * Build (from C/):
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson \
 *       tests/test_shell_hooks.c src/agent/shell_hooks.c \
 *       src/agent/hook_registry.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_shell_hooks -lpthread
 *
 * Run:
 *   /tmp/hermes_test_shell_hooks
 */

#include "hermes_hooks.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

/* Public API we're testing */
extern int shell_hooks_parse_json(const json_t *hooks_json);
extern int shell_hooks_register_all(void);
extern void shell_hooks_shutdown(void);
extern int shell_hooks_count(void);

/* Internal functions made visible for testing */
extern int shell_hooks_parse_json(const json_t *hooks_json);

/* spec_matches_tool is static — test it via parsing + lifecycle instead */

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* Helper: parse a JSON string into a json_t node */
static json_t *parse_json(const char *str) {
    char *err = NULL;
    json_t *node = json_parse(str, &err);
    if (err) { free(err); return NULL; }
    return node;
}

/* Helper: build a hooks config JSON string from a simple description */
static char *make_hooks_config(const char *event, const char *cmd,
                                const char *matcher, int timeout) {
    char buf[4096];
    int n = snprintf(buf, sizeof(buf),
        "{\"%s\":[{\"command\":\"%s\"",
        event, cmd);
    if (matcher && matcher[0])
        n += snprintf(buf + n, sizeof(buf) - n,
            ",\"matcher\":\"%s\"", matcher);
    if (timeout > 0)
        n += snprintf(buf + n, sizeof(buf) - n,
            ",\"timeout\":%d", timeout);
    n += snprintf(buf + n, sizeof(buf) - n, "}]}");
    return strdup(buf);
}

int main(void)
{
    /* Clean slate for hook_registry */
    hook_reset_all();
    shell_hooks_shutdown();
    /* ================================================================== */
    /* 1. Parse empty / NULL config                                        */
    /* ================================================================== */

    TEST("parse: NULL returns 0", shell_hooks_parse_json(NULL) == 0);
    TEST("parse: count 0 after NULL", shell_hooks_count() == 0);
    shell_hooks_shutdown();

    /* Empty object */
    json_t *empty = parse_json("{}");
    TEST("parse: empty obj", empty != NULL && shell_hooks_parse_json(empty) == 0);
    json_free(empty);
    TEST("parse: count 0 after empty", shell_hooks_count() == 0);
    shell_hooks_shutdown();

    /* ================================================================== */
    /* 2. Parse single hook (basic)                                        */
    /* ================================================================== */

    json_t *hooks = parse_json(
        "{\"pre_tool_call\":[{\"command\":\"/bin/true\"}]}");
    int n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: single hook returns 1", n == 1);
    TEST("parse: count 1", shell_hooks_count() == 1);
    shell_hooks_shutdown();
    TEST("parse: count 0 after shutdown", shell_hooks_count() == 0);

    /* ================================================================== */
    /* 3. Parse multiple hooks, multiple events                            */
    /* ================================================================== */

    hooks = parse_json(
        "{"
        "\"pre_tool_call\":["
        "  {\"command\":\"/bin/true\"},"
        "  {\"command\":\"/bin/false\",\"matcher\":\"terminal\"}"
        "],"
        "\"post_tool_call\":["
        "  {\"command\":\"/usr/bin/logger\"}"
        "]"
        "}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: 3 hooks total", n == 3);
    TEST("parse: count 3", shell_hooks_count() == 3);
    shell_hooks_shutdown();

    /* ================================================================== */
    /* 4. Parse with matcher and timeout                                   */
    /* ================================================================== */

    hooks = parse_json(
        "{\"pre_tool_call\":["
        "  {\"command\":\"/bin/cat\",\"matcher\":\"terminal\",\"timeout\":15}"
        "]}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: matcher+timeout returns 1", n == 1);
    TEST("parse: count 1", shell_hooks_count() == 1);
    shell_hooks_shutdown();

    /* ================================================================== */
    /* 5. Parse with missing command field (should skip)                   */
    /* ================================================================== */

    hooks = parse_json(
        "{\"pre_tool_call\":["
        "  {\"command\":\"/bin/true\"},"
        "  {\"matcher\":\"terminal\"},"
        "  {\"command\":\"/bin/false\"}"
        "]}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: skip entry without command", n == 2);
    TEST("parse: count 2", shell_hooks_count() == 2);
    shell_hooks_shutdown();

    /* ================================================================== */
    /* 6. Parse with empty command (should skip)                           */
    /* ================================================================== */

    hooks = parse_json(
        "{\"pre_tool_call\":["
        "  {\"command\":\"\"},"
        "  {\"command\":\"/bin/true\"}"
        "]}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: skip empty command", n == 1);
    TEST("parse: count 1", shell_hooks_count() == 1);
    shell_hooks_shutdown();

    /* ================================================================== */
    /* 7. Parse with invalid timeout ranges (clamped)                      */
    /* ================================================================== */

    hooks = parse_json(
        "{\"pre_tool_call\":["
        "  {\"command\":\"/bin/true\",\"timeout\":0},"
        "  {\"command\":\"/bin/false\",\"timeout\":999},"
        "  {\"command\":\"/bin/other\",\"timeout\":30}"
        "]}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: 3 hooks with timeout clamping", n == 3);

    /* Register all and verify count via hook_registry */
    int registered = shell_hooks_register_all();
    TEST("register: 3 hooks registered", registered == 3);
    TEST("register: has_callbacks for pre_tool_call",
         hook_has_callbacks("pre_tool_call"));
    TEST("register: event_count 1 (all same event)",
         hook_event_count() == 1);

    shell_hooks_shutdown();
    TEST("shutdown: no callbacks after unregister",
         !hook_has_callbacks("pre_tool_call"));
    TEST("shutdown: count 0", shell_hooks_count() == 0);
    /* hook_event_count may still be >0 (events with 0 cbs remain in registry) */

    /* ================================================================== */
    /* 8. Multiple events: register with separate event names              */
    /* ================================================================== */

    hooks = parse_json(
        "{"
        "\"pre_tool_call\":[{\"command\":\"/bin/echo\"}],"
        "\"post_tool_call\":[{\"command\":\"/bin/logger\"}],"
        "\"pre_llm_call\":[{\"command\":\"/bin/cat\"}]"
        "}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: 3 events, 3 hooks", n == 3);

    registered = shell_hooks_register_all();
    TEST("register: 3 registered across 3 events", registered == 3);
    TEST("register: event_count 3", hook_event_count() == 3);
    TEST("register: pre_tool_call",
         hook_has_callbacks("pre_tool_call"));
    TEST("register: post_tool_call",
         hook_has_callbacks("post_tool_call"));
    TEST("register: pre_llm_call",
         hook_has_callbacks("pre_llm_call"));

    shell_hooks_shutdown();
    TEST("shutdown: no pre_tool_call after unreg",
         !hook_has_callbacks("pre_tool_call"));
    TEST("shutdown: no post_tool_call after unreg",
         !hook_has_callbacks("post_tool_call"));
    TEST("shutdown: no pre_llm_call after unreg",
         !hook_has_callbacks("pre_llm_call"));
    /* hook_has_callbacks confirms cleanup — event_count may retain event names */

    /* ================================================================== */
    /* 9. Register/unregister with same event name, different specs        */
    /* ================================================================== */

    hooks = parse_json(
        "{\"pre_tool_call\":["
        "  {\"command\":\"/bin/true\",\"matcher\":\"terminal\"},"
        "  {\"command\":\"/bin/false\",\"matcher\":\"web_search\"}"
        "]}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: 2 hooks same event", n == 2);

    registered = shell_hooks_register_all();
    TEST("register: 2 same event", registered == 2);
    TEST("register: has_callbacks for pre_tool_call",
         hook_has_callbacks("pre_tool_call"));

    shell_hooks_shutdown();
    TEST("shutdown: no callbacks for pre_tool_call",
         !hook_has_callbacks("pre_tool_call"));

    /* ================================================================== */
    /* 10. Idempotent shutdown                                             */
    /* ================================================================== */

    /* Shutdown with nothing registered should not crash */
    shell_hooks_shutdown();
    TEST("shutdown: empty no-op", 1);
    TEST("shutdown: count still 0", shell_hooks_count() == 0);

    /* ================================================================== */
    /* 11. Parse with regex matcher (valid regex)                          */
    /* ================================================================== */

    hooks = parse_json(
        "{\"pre_tool_call\":["
        "  {\"command\":\"/bin/true\",\"matcher\":\"^term\\\\.\"}"
        "]}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: regex matcher returns 1", n == 1);
    /* Can't directly test the regex matching since spec_matches_tool is
     * static, but we verify parse didn't crash with valid regex */
    shell_hooks_shutdown();

    /* ================================================================== */
    /* 12. Parse with invalid regex matcher (should compile but fall back) */
    /* ================================================================== */

    hooks = parse_json(
        "{\"pre_tool_call\":["
        "  {\"command\":\"/bin/true\",\"matcher\":\"[invalid\"}"
        "]}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: invalid regex doesn't crash", n == 1);
    /* Regex failed to compile but entry was still added */
    shell_hooks_shutdown();

    /* ================================================================== */
    /* 13. Large config: many hooks                                       */
    /* ================================================================== */

    hooks = parse_json(
        "{\"pre_tool_call\":["
        "  {\"command\":\"/bin/1\"},{\"command\":\"/bin/2\"},"
        "  {\"command\":\"/bin/3\"},{\"command\":\"/bin/4\"},"
        "  {\"command\":\"/bin/5\"},{\"command\":\"/bin/6\"}"
        "]}");
    n = shell_hooks_parse_json(hooks);
    json_free(hooks);
    TEST("parse: 6 hooks", n == 6);
    TEST("parse: count 6", shell_hooks_count() == 6);

    registered = shell_hooks_register_all();
    TEST("register: 6 registered", registered == 6);

    shell_hooks_shutdown();
    TEST("shutdown: all cleaned", shell_hooks_count() == 0);

    /* ================================================================== */
    /* 14. Parse non-object JSON (should return 0)                         */
    /* ================================================================== */

    json_t *arr = parse_json("[1,2,3]");
    TEST("parse: array returns 0", arr != NULL && shell_hooks_parse_json(arr) == 0);
    json_free(arr);

    json_t *str = parse_json("\"hello\"");
    TEST("parse: string returns 0", str != NULL && shell_hooks_parse_json(str) == 0);
    json_free(str);

    json_t *num = parse_json("42");
    TEST("parse: number returns 0", num != NULL && shell_hooks_parse_json(num) == 0);
    json_free(num);

    /* ================================================================== */
    /* 15. Cumulative parse (multiple calls add up)                        */
    /* ================================================================== */

    json_t *batch1 = parse_json("{\"ev1\":[{\"command\":\"/bin/a\"}]}");
    json_t *batch2 = parse_json("{\"ev2\":[{\"command\":\"/bin/b\"}]}");
    n = shell_hooks_parse_json(batch1);
    json_free(batch1);
    TEST("cumulative: batch1 returned 1", n == 1);
    TEST("cumulative: count 1", shell_hooks_count() == 1);

    n = shell_hooks_parse_json(batch2);
    json_free(batch2);
    TEST("cumulative: batch2 returned 1", n == 1);
    TEST("cumulative: count 2", shell_hooks_count() == 2);
    shell_hooks_shutdown();

    /* ================================================================== */
    /* Summary                                                             */
    /* ================================================================== */

    printf("\n");
    printf("  Results: %d passed, %d failed, 0 skipped\n", passed, tests - passed);

    return (passed == tests) ? 0 : 1;
}
