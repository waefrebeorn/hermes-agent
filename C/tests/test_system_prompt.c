/*
 * test_system_prompt.c — Tests for hermes_system_prompt
 * Verifies that system_prompt_build produces expected output.
 */

#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("  TEST: %s\\n", name); \
} while(0)

#define PASS() do { \
    printf("    PASS\\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("    FAIL: %s\\n", msg); \
    tests_failed++; \
} while(0)

static void test_default_identity(void) {
    TEST("default identity string is non-empty");
    assert(SYSPRMPT_DEFAULT_IDENTITY != NULL);
    assert(strlen(SYSPRMPT_DEFAULT_IDENTITY) > 50);
    PASS();
}

static void test_build_minimal(void) {
    TEST("build minimal system prompt");
    system_prompt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_soul = false;

    char *result = system_prompt_build(&cfg);
    if (!result) { FAIL("NULL result"); return; }
    if (strlen(result) < 100) { FAIL("result too short"); free(result); return; }
    if (!strstr(result, "Hermes Agent")) { FAIL("missing identity"); free(result); return; }
    if (!strstr(result, "hermes-agent")) { FAIL("missing help guidance"); free(result); return; }
    free(result);
    PASS();
}

static void test_build_with_memory_tool(void) {
    TEST("build with memory guidance");
    system_prompt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_soul = false;
    cfg.has_memory = true;

    char *result = system_prompt_build(&cfg);
    if (!result) { FAIL("NULL result"); return; }
    if (!strstr(result, "persistent memory")) { FAIL("missing memory guidance"); free(result); return; }
    free(result);
    PASS();
}

static void test_build_with_tool_enforcement(void) {
    TEST("build with tool enforcement");
    system_prompt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_soul = false;
    cfg.enforce_tools = true;

    char *result = system_prompt_build(&cfg);
    if (!result) { FAIL("NULL result"); return; }
    if (!strstr(result, "Tool-use enforcement")) { FAIL("missing enforcement"); free(result); return; }
    free(result);
    PASS();
}

static void test_build_with_openai_exec(void) {
    TEST("build with OpenAI execution guidance");
    system_prompt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_soul = false;
    cfg.enforce_tools = true;
    cfg.is_openai_family = true;

    char *result = system_prompt_build(&cfg);
    if (!result) { FAIL("NULL result"); return; }
    if (!strstr(result, "Execution discipline")) { FAIL("missing execution discipline"); free(result); return; }
    free(result);
    PASS();
}

static void test_build_with_google_ops(void) {
    TEST("build with Google operational guidance");
    system_prompt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_soul = false;
    cfg.enforce_tools = true;
    cfg.is_google_family = true;

    char *result = system_prompt_build(&cfg);
    if (!result) { FAIL("NULL result"); return; }
    if (!strstr(result, "Google model operational")) { FAIL("missing Google ops"); free(result); return; }
    free(result);
    PASS();
}

static void test_build_with_volatile(void) {
    TEST("build with volatile tier (timestamp)");
    system_prompt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_soul = false;
    cfg.model_name = "deepseek-v4-flash";
    cfg.provider_name = "deepseek";

    char *result = system_prompt_build(&cfg);
    if (!result) { FAIL("NULL result"); return; }
    if (!strstr(result, "Conversation started")) { FAIL("missing timestamp"); free(result); return; }
    if (!strstr(result, "Model: deepseek-v4-flash")) { FAIL("missing model name"); free(result); return; }
    if (!strstr(result, "Provider: deepseek")) { FAIL("missing provider"); free(result); return; }
    free(result);
    PASS();
}

static void test_build_with_session_id(void) {
    TEST("build with session ID");
    system_prompt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_soul = false;
    cfg.pass_session_id = true;
    cfg.session_id = "test-session-123";
    cfg.model_name = "test-model";

    char *result = system_prompt_build(&cfg);
    if (!result) { FAIL("NULL result"); return; }
    if (!strstr(result, "Session ID: test-session-123")) { FAIL("missing session ID"); free(result); return; }
    free(result);
    PASS();
}

static void test_build_with_context(void) {
    TEST("build with context tier");
    system_prompt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_soul = false;
    cfg.system_message = "Custom system instructions.";
    cfg.context_files = "Project context from AGENTS.md";

    char *result = system_prompt_build(&cfg);
    if (!result) { FAIL("NULL result"); return; }
    if (!strstr(result, "Custom system instructions")) { FAIL("missing system_message"); free(result); return; }
    if (!strstr(result, "Project context")) { FAIL("missing context files"); free(result); return; }
    free(result);
    PASS();
}

static void test_platform_hint(void) {
    TEST("build with platform hint");
    system_prompt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_soul = false;
    cfg.platform_hint = "You are on Telegram. Markdown formatting is supported.";

    char *result = system_prompt_build(&cfg);
    if (!result) { FAIL("NULL result"); return; }
    if (!strstr(result, "Telegram")) { FAIL("missing platform hint"); free(result); return; }
    free(result);
    PASS();
}

int main(void) {
    printf("=== System Prompt Tests ===\n\n");

    test_default_identity();
    test_build_minimal();
    test_build_with_memory_tool();
    test_build_with_tool_enforcement();
    test_build_with_openai_exec();
    test_build_with_google_ops();
    test_build_with_volatile();
    test_build_with_session_id();
    test_build_with_context();
    test_platform_hint();

    printf("\nResults: %d passed, %d failed\\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
