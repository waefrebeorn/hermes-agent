/*
 * test_permissions.c — ACP permissions bridging test suite (P60).
 *
 * Tests: acp_build_permission_options, acp_build_permission_tool_call,
 *        acp_map_outcome_to_hermes.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_permissions.c src/acp/permissions.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_permissions -lm
 *
 * Run:
 *   /tmp/hermes_test_permissions
 */

#include "acp/permissions.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)
#define TEST_NULL(name, p) TEST(name, p == NULL)

/* Helper: get a string field from an object */
static const char *obj_str(json_node_t *obj, const char *key) {
    return json_object_get_string(obj, key, NULL);
}

/* ================================================================
 *  1. acp_build_permission_options
 * ================================================================ */
static void test_build_options(void) {
    printf("\n--- build_permission_options ---\n");

    /* Without allow_permanent: 4 options */
    json_node_t *opts = acp_build_permission_options(false);
    TEST_NOT_NULL("options without permanent returns non-NULL", opts);
    TEST_EQ("options without permanent = 4", json_len(opts), (size_t)4);
    json_free(opts);

    /* With allow_permanent: 5 options */
    opts = acp_build_permission_options(true);
    TEST_NOT_NULL("options with permanent returns non-NULL", opts);
    TEST_EQ("options with permanent = 5", json_len(opts), (size_t)5);
    json_free(opts);

    /* Verify option structure (without permanent) */
    opts = acp_build_permission_options(false);
    json_node_t *first = json_get(opts, 0);
    json_node_t *last  = json_get(opts, 3);
    TEST_NOT_NULL("first option exists", first);
    TEST_NOT_NULL("last option exists", last);
    TEST_STR_EQ("first: name = 'Allow once'",
        obj_str(first, "name"), "Allow once");
    TEST_STR_EQ("first: option_id = 'allow_once'",
        obj_str(first, "option_id"), "allow_once");
    TEST_STR_EQ("last: name = 'Deny always'",
        obj_str(last, "name"), "Deny always");
    TEST_STR_EQ("last: option_id = 'deny_always'",
        obj_str(last, "option_id"), "deny_always");
    json_free(opts);

    /* With permanent: option at index 2 should be 'Allow always' */
    opts = acp_build_permission_options(true);
    json_node_t *third = json_get(opts, 2);
    TEST_NOT_NULL("third option with permanent", third);
    TEST_STR_EQ("third: name = 'Allow always'",
        obj_str(third, "name"), "Allow always");
    json_free(opts);
}

/* ================================================================
 *  2. acp_build_permission_tool_call
 * ================================================================ */
static void test_build_tool_call(void) {
    printf("\n--- build_permission_tool_call ---\n");

    /* NULL command returns NULL */
    TEST_NULL("NULL command returns NULL",
        acp_build_permission_tool_call(NULL, "desc"));

    /* Basic tool call with command only */
    json_node_t *tc = acp_build_permission_tool_call("ls -la", NULL);
    TEST_NOT_NULL("tool call with command only", tc);
    TEST_STR_EQ("kind = 'execute'",
        obj_str(tc, "kind"), "execute");
    TEST_STR_EQ("status = 'pending'",
        obj_str(tc, "status"), "pending");
    TEST_STR_EQ("title = command when no desc",
        obj_str(tc, "title"), "ls -la");
    json_free(tc);

    /* Tool call with command and description */
    tc = acp_build_permission_tool_call("rm -rf /", "Dangerous delete");
    TEST_NOT_NULL("tool call with command+desc", tc);
    TEST_STR_EQ("title contains description",
        obj_str(tc, "title"), "Dangerous delete: rm -rf /");
    json_free(tc);

    /* Tool call IDs are sequential and unique */
    json_node_t *tc1 = acp_build_permission_tool_call("echo 1", NULL);
    json_node_t *tc2 = acp_build_permission_tool_call("echo 2", NULL);
    TEST_NOT_NULL("tc1", tc1);
    TEST_NOT_NULL("tc2", tc2);
    const char *id1 = obj_str(tc1, "tool_call_id");
    const char *id2 = obj_str(tc2, "tool_call_id");
    TEST_NOT_NULL("tc1 id", id1);
    TEST_NOT_NULL("tc2 id", id2);
    TEST("sequential IDs differ", strcmp(id1, id2) != 0);
    TEST("id1 contains 'perm-check'", strstr(id1, "perm-check") != NULL);
    json_free(tc1);
    json_free(tc2);

    /* Content and raw_input fields exist */
    tc = acp_build_permission_tool_call("test cmd", NULL);
    json_node_t *content = json_object_get(tc, "content");
    json_node_t *raw_input = json_object_get(tc, "raw_input");
    TEST_NOT_NULL("content field", content);
    TEST_NOT_NULL("raw_input field", raw_input);
    TEST_STR_EQ("raw_input.command = 'test cmd'",
        obj_str(raw_input, "command"), "test cmd");
    json_free(tc);
}

/* ================================================================
 *  3. acp_map_outcome_to_hermes
 * ================================================================ */
static void test_map_outcome(void) {
    printf("\n--- map_outcome_to_hermes ---\n");

    /* Basic mappings */
    TEST_STR_EQ("allow_once -> 'once'",
        acp_map_outcome_to_hermes("allow_once", "allow_once,allow_session,deny"),
        "once");
    TEST_STR_EQ("allow_session -> 'session'",
        acp_map_outcome_to_hermes("allow_session", "allow_once,allow_session,deny"),
        "session");
    TEST_STR_EQ("allow_always -> 'always'",
        acp_map_outcome_to_hermes("allow_always", "allow_once,allow_session,allow_always,deny"),
        "always");
    TEST_STR_EQ("deny -> 'deny'",
        acp_map_outcome_to_hermes("deny", "allow_once,deny"),
        "deny");
    TEST_STR_EQ("deny_always -> 'deny'",
        acp_map_outcome_to_hermes("deny_always", "allow_once,deny,deny_always"),
        "deny");

    /* NULL / edge cases */
    TEST_STR_EQ("NULL option_id -> 'deny'",
        acp_map_outcome_to_hermes(NULL, "allow_once,deny"),
        "deny");
    TEST_STR_EQ("NULL allowed_list -> 'deny'",
        acp_map_outcome_to_hermes("allow_once", NULL),
        "deny");

    /* Not in allowed list */
    TEST_STR_EQ("allow_always not in list -> 'deny'",
        acp_map_outcome_to_hermes("allow_always", "allow_once,deny"),
        "deny");

    /* Substring match prevention */
    TEST_STR_EQ("substring 'allow' not full match -> 'deny'",
        acp_map_outcome_to_hermes("allow", "allow_once,deny"),
        "deny");

    /* Comma boundary check */
    TEST_STR_EQ("allow_session in full list -> 'session'",
        acp_map_outcome_to_hermes("allow_session",
            "allow_once,allow_session,allow_always,deny,deny_always"),
        "session");
}

/* ================================================================
 *  4. Edge cases
 * ================================================================ */
static void test_edge_cases(void) {
    printf("\n--- edge cases ---\n");

    /* Empty description produces just command as title */
    json_node_t *tc = acp_build_permission_tool_call("cmd", "");
    TEST_NOT_NULL("empty desc returns non-NULL", tc);
    TEST_STR_EQ("empty desc title = 'cmd'", obj_str(tc, "title"), "cmd");
    json_free(tc);

    /* Very long command (4000 chars) */
    {
        char long_cmd[4096];
        memset(long_cmd, 'x', 4000);
        long_cmd[4000] = '\0';
        tc = acp_build_permission_tool_call(long_cmd, NULL);
        TEST_NOT_NULL("long command returns non-NULL", tc);
        json_free(tc);
    }

    /* Memory: free should never crash */
    json_node_t *opts = acp_build_permission_options(true);
    TEST_NOT_NULL("permanent options", opts);
    json_free(opts);
    json_free(NULL); /* should be safe */
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== ACP Permissions Test Suite ===\n");

    test_build_options();
    test_build_tool_call();
    test_map_outcome();
    test_edge_cases();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
