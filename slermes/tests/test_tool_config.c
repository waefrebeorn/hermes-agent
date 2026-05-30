/*
 * test_tool_config.c — Tool configuration dependency injection tests (P54).
 * Tests: runtime overrides, env var resolution, type coercion,
 *        NULL safety, lifecycle.
 */
#include "hermes_tool_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

/* ================================================================
 * 1. Runtime overrides — tool_config_set / tool_config_get
 * ================================================================ */

static void test_set_get_basic(void) {
    TEST("set + get basic string");
    tool_config_set("discord", "bot_token", "abc123");
    const char *v = tool_config_get("discord", "bot_token");
    assert(v != NULL);
    assert(strcmp(v, "abc123") == 0);
    PASS();
}

static void test_set_get_multiple_tools(void) {
    TEST("multiple tools, same key");
    tool_config_set("discord", "bot_token", "token_d");
    tool_config_set("telegram", "bot_token", "token_t");
    assert(strcmp(tool_config_get("discord", "bot_token"), "token_d") == 0);
    assert(strcmp(tool_config_get("telegram", "bot_token"), "token_t") == 0);
    PASS();
}

static void test_set_update_existing(void) {
    TEST("update existing override");
    tool_config_set("test", "key", "old");
    tool_config_set("test", "key", "new");
    assert(strcmp(tool_config_get("test", "key"), "new") == 0);
    PASS();
}

static void test_get_nonexistent(void) {
    TEST("get nonexistent key returns NULL");
    tool_config_clear();
    assert(tool_config_get("nope", "no_such_key") == NULL);
    PASS();
}

/* ================================================================
 * 2. tool_config_clear
 * ================================================================ */

static void test_clear_removes_all(void) {
    TEST("clear removes all overrides");
    tool_config_set("t1", "k1", "v1");
    tool_config_set("t2", "k2", "v2");
    tool_config_clear();
    assert(tool_config_get("t1", "k1") == NULL);
    assert(tool_config_get("t2", "k2") == NULL);
    PASS();
}

static void test_clear_empty(void) {
    TEST("clear when already empty, no crash");
    tool_config_clear();
    PASS();
}

/* ================================================================
 * 3. Convenience wrappers
 * ================================================================ */

static void test_get_api_key(void) {
    TEST("tool_config_get_api_key");
    tool_config_set("myapi", "api_key", "sk-xyz");
    assert(strcmp(tool_config_get_api_key("myapi"), "sk-xyz") == 0);
    PASS();
}

static void test_get_base_url(void) {
    TEST("tool_config_get_base_url");
    tool_config_set("myapi", "base_url", "https://api.example.com");
    assert(strcmp(tool_config_get_base_url("myapi"), "https://api.example.com") == 0);
    PASS();
}

static void test_get_token_bot_token(void) {
    TEST("tool_config_get_token uses bot_token first");
    tool_config_clear();
    tool_config_set("bot", "bot_token", "bot_abc");
    tool_config_set("bot", "token", "tok_xyz");
    assert(strcmp(tool_config_get_token("bot"), "bot_abc") == 0);
    PASS();
}

static void test_get_token_fallback_token(void) {
    TEST("tool_config_get_token falls back to token");
    tool_config_clear();
    tool_config_set("svc", "token", "tok_only");
    assert(strcmp(tool_config_get_token("svc"), "tok_only") == 0);
    PASS();
}

static void test_get_token_fallback_api_key(void) {
    TEST("tool_config_get_token falls back to api_key");
    tool_config_clear();
    tool_config_set("svc", "api_key", "key_only");
    assert(strcmp(tool_config_get_token("svc"), "key_only") == 0);
    PASS();
}

/* ================================================================
 * 4. tool_config_get_int
 * ================================================================ */

static void test_get_int_valid(void) {
    TEST("get_int valid integer");
    tool_config_set("int", "val", "42");
    assert(tool_config_get_int("int", "val", -1) == 42);
    PASS();
}

static void test_get_int_default(void) {
    TEST("get_int missing key returns default");
    assert(tool_config_get_int("int", "no_such", -1) == -1);
    PASS();
}

static void test_get_int_invalid(void) {
    TEST("get_int non-numeric returns default");
    tool_config_set("int", "bad", "not_a_number");
    assert(tool_config_get_int("int", "bad", 0) == 0);
    PASS();
}

/* ================================================================
 * 5. tool_config_get_bool
 * ================================================================ */

static void test_get_bool_true_strings(void) {
    TEST("get_bool 'true' -> true");
    tool_config_set("b", "v", "true");
    assert(tool_config_get_bool("b", "v", false) == true);
    PASS();
}

static void test_get_bool_true_1(void) {
    TEST("get_bool '1' -> true");
    tool_config_set("b", "v", "1");
    assert(tool_config_get_bool("b", "v", false) == true);
    PASS();
}

static void test_get_bool_true_yes(void) {
    TEST("get_bool 'yes' -> true");
    tool_config_set("b", "v", "yes");
    assert(tool_config_get_bool("b", "v", false) == true);
    PASS();
}

static void test_get_bool_false_strings(void) {
    TEST("get_bool 'false' -> false");
    tool_config_set("b", "v", "false");
    assert(tool_config_get_bool("b", "v", true) == false);
    PASS();
}

static void test_get_bool_default(void) {
    TEST("get_bool missing key returns default");
    assert(tool_config_get_bool("b", "no_such", true) == true);
    PASS();
}

/* ================================================================
 * 6. NULL safety
 * ================================================================ */

static void test_get_null_tool(void) {
    TEST("tool_config_get(NULL, key) returns NULL");
    assert(tool_config_get(NULL, "key") == NULL);
    PASS();
}

static void test_get_null_key(void) {
    TEST("tool_config_get(tool, NULL) returns NULL");
    assert(tool_config_get("tool", NULL) == NULL);
    PASS();
}

static void test_set_null_params(void) {
    TEST("tool_config_set(NULL, ...) no crash");
    tool_config_set(NULL, "key", "val");
    tool_config_set("tool", NULL, "val");
    tool_config_set("tool", "key", NULL);
    /* Verify none were set */
    assert(tool_config_get("tool", "key") == NULL);
    PASS();
}

/* ================================================================
 * 7. Env var resolution (per-tool + generic)
 * ================================================================ */

static void test_env_var_per_tool(void) {
    TEST("per-tool env var TESTTOOL_API_KEY");
    setenv("TESTTOOL_API_KEY", "env_key_abc", 1);
    const char *v = tool_config_get("testtool", "api_key");
    assert(v != NULL);
    assert(strcmp(v, "env_key_abc") == 0);
    unsetenv("TESTTOOL_API_KEY");
    PASS();
}

static void test_env_var_generic(void) {
    TEST("generic env var HERMES_BASE_URL");
    setenv("HERMES_BASE_URL", "https://generic.example.com", 1);
    const char *v = tool_config_get("anytool", "base_url");
    assert(v != NULL);
    assert(strcmp(v, "https://generic.example.com") == 0);
    unsetenv("HERMES_BASE_URL");
    PASS();
}

static void test_env_var_override_order(void) {
    TEST("runtime override beats env var");
    setenv("TOOL_TEST_KEY", "from_env", 1);
    tool_config_set("tool", "test_key", "from_override");
    const char *v = tool_config_get("tool", "test_key");
    assert(v != NULL);
    assert(strcmp(v, "from_override") == 0);
    unsetenv("TOOL_TEST_KEY");
    PASS();
}

/* ================================================================
 * 8. Vault integration
 * ================================================================ */

#include "hermes.h"

static void test_vault_integration(void) {
    TEST("vault credential resolved via tool_config_get");
    /* Set master key and store in vault */
    vault_set_master_key("test-vault-key");
    vault_set_path("/tmp/hermes_test_vault_tc.enc");
    vault_store("testvault", "api_key", "vault-secret-abc");
    vault_save();
    vault_lock();
    vault_set_master_key("test-vault-key");
    vault_load();

    const char *v = tool_config_get("testvault", "api_key");
    assert(v != NULL);
    assert(strcmp(v, "vault-secret-abc") == 0);
    PASS();
}

static void test_vault_override_priority(void) {
    TEST("runtime override beats vault");
    tool_config_set("testvault", "api_key", "override-value");
    const char *v = tool_config_get("testvault", "api_key");
    assert(v != NULL);
    assert(strcmp(v, "override-value") == 0);
    PASS();
}

static void test_vault_not_found(void) {
    TEST("vault missing key returns NULL (no override, no env)");
    tool_config_clear();
    const char *v = tool_config_get("nonexistent", "no_such_key");
    assert(v == NULL);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== Tool Config Tests (P54) ===\n");

    test_set_get_basic();
    test_set_get_multiple_tools();
    test_set_update_existing();
    test_get_nonexistent();
    test_clear_removes_all();
    test_clear_empty();
    test_get_api_key();
    test_get_base_url();
    test_get_token_bot_token();
    test_get_token_fallback_token();
    test_get_token_fallback_api_key();
    test_get_int_valid();
    test_get_int_default();
    test_get_int_invalid();
    test_get_bool_true_strings();
    test_get_bool_true_1();
    test_get_bool_true_yes();
    test_get_bool_false_strings();
    test_get_bool_default();
    test_get_null_tool();
    test_get_null_key();
    test_set_null_params();
    test_env_var_per_tool();
    test_env_var_generic();
    test_env_var_override_order();
    test_vault_integration();
    test_vault_override_priority();
    test_vault_not_found();

    printf("\n%d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
