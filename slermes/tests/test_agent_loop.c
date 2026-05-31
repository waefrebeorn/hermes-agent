/*
 * test_agent_loop.c — Agent loop core function tests (S7 X06).
 *
 * Tests self-contained agent_loop.c functions that don't require
 * network access, SQLite, or live LLM calls:
 * - agent_generate_session_id() format verification
 * - agent_free() on zeroed/partial state
 * - agent_configure_from_config() field mapping
 * - agent_inject_history() JSON message injection
 * - agent_snapshot_take() / agent_snapshot_restore() lifecycle
 *
 * Compile (from project root):
 *   gcc -O0 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_agent_loop.c \
 *       src/agent/agent_loop.c src/agent/context.c src/agent/checkpoint.c \
 *       lib/libjson/json.c lib/libplugin/plugin.c \
 *       -o /tmp/test_agent_loop -lm -lpthread \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/test_agent_loop
 */

#include "hermes.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

static int g_pass = 0, g_fail = 0;

#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        g_fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        g_pass++; \
    } \
} while(0)

#define TEST_STR(name, a, b) do { \
    const char *_aa = (a) ? (a) : ""; \
    const char *_bb = (b) ? (b) : ""; \
    TEST(name, strcmp(_aa, _bb) == 0); \
} while(0)

/* ================================================================
 *  1. agent_generate_session_id — format verification
 * ================================================================ */
static void test_session_id_format(void) {
    printf("\n--- agent_generate_session_id: Format ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    agent_generate_session_id(&state);

    /* Format: YYYYMMDD_HHMMSS (15 chars + null = 16) */
    TEST("session_id non-empty", state.session_id[0] != '\0');
    TEST("session_id length 15", strlen(state.session_id) == 15);

    /* Check format with regex: 8 digits, underscore, 6 digits */
    regex_t regex;
    int rc = regcomp(&regex, "^[0-9]{8}_[0-9]{6}$", REG_EXTENDED);
    TEST("regex compiled", rc == 0);
    if (rc == 0) {
        rc = regexec(&regex, state.session_id, 0, NULL, 0);
        TEST("session_id matches YYYYMMDD_HHMMSS", rc == 0);
        regfree(&regex);
    }

    /* Verify date components are in valid ranges */
    char year_str[5] = {0}, month_str[3] = {0}, day_str[3] = {0};
    char hour_str[3] = {0}, min_str[3] = {0}, sec_str[3] = {0};
    memcpy(year_str, state.session_id, 4);
    memcpy(month_str, state.session_id + 4, 2);
    memcpy(day_str, state.session_id + 6, 2);
    memcpy(hour_str, state.session_id + 9, 2);
    memcpy(min_str, state.session_id + 11, 2);
    memcpy(sec_str, state.session_id + 13, 2);

    int year = atoi(year_str);
    int month = atoi(month_str);
    int day = atoi(day_str);
    int hour = atoi(hour_str);
    int min = atoi(min_str);
    int sec = atoi(sec_str);

    TEST("year in reasonable range", year >= 2024 && year <= 2099);
    TEST("month in 1-12", month >= 1 && month <= 12);
    TEST("day in 1-31", day >= 1 && day <= 31);
    TEST("hour in 0-23", hour >= 0 && hour <= 23);
    TEST("minute in 0-59", min >= 0 && min <= 59);
    TEST("second in 0-59", sec >= 0 && sec <= 59);
}

/* ================================================================
 *  2. agent_free — cleanup safety
 * ================================================================ */
static void test_free_zeroed_state(void) {
    printf("\n--- agent_free: Zeroed State ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    /* Free should not crash on properly zeroed state */
    agent_free(&state);
    TEST("agent_free on zeroed state completes", true);

    /* All fields should be zeroed/freed */
    TEST("messages NULL after free", state.messages == NULL);
    TEST("message_count 0 after free", state.message_count == 0);
    TEST("message_capacity 0 after free", state.message_capacity == 0);
    TEST("budget NULL after free", state.budget == NULL);
}

static void test_free_with_messages(void) {
    printf("\n--- agent_free: State With Messages ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    /* Manually set up minimal state with messages (no agent_init to avoid unresolved deps) */
    message_t *m1 = message_new(MSG_USER, "hello");
    message_t *m2 = message_new(MSG_USER, "world");
    message_t *m3 = message_new(MSG_SYSTEM, "be helpful");

    state.messages = (message_t **)calloc(4, sizeof(message_t *));
    state.messages[0] = m1;
    state.messages[1] = m2;
    state.messages[2] = m3;
    state.message_count = 3;
    state.message_capacity = 4;

    agent_free(&state);
    TEST("agent_free with messages completes", true);
    TEST("messages NULL after free", state.messages == NULL);
    TEST("message_count 0", state.message_count == 0);
    TEST("message_capacity 0", state.message_capacity == 0);
}

/* ================================================================
 *  3. agent_configure_from_config — field mapping
 * ================================================================ */
static void test_configure_from_config(void) {
    printf("\n--- agent_configure_from_config: Field Mapping ---\n");

    agent_state_t state;
    memset(&state, 0, sizeof(state));

    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Set config fields */
    snprintf(cfg.provider_cfg.model, sizeof(cfg.provider_cfg.model),
             "gpt-4o");
    snprintf(cfg.provider_cfg.provider, sizeof(cfg.provider_cfg.provider),
             "openai");
    snprintf(cfg.provider_cfg.base_url, sizeof(cfg.provider_cfg.base_url),
             "https://api.openai.com/v1");
    snprintf(cfg.provider_cfg.api_key, sizeof(cfg.provider_cfg.api_key),
             "sk-test-key-12345");
    cfg.provider_cfg.max_tokens = 4096;
    cfg.provider_cfg.temperature = 0.7f;
    cfg.provider_cfg.top_p = 0.95f;
    cfg.agent.max_iterations = 50;
    cfg.agent.tool_delay = 0.5f;
    cfg.compress_enabled = true;
    cfg.compression.cooldown_secs = 60;

    /* Call the function */
    agent_configure_from_config(&state, &cfg);

    /* Verify field mapping */
    TEST_STR("model mapped", state.llm.model, "gpt-4o");
    TEST_STR("provider mapped", state.llm.provider, "openai");
    TEST_STR("base_url mapped", state.llm.base_url, "https://api.openai.com/v1");
    TEST_STR("api_key mapped", state.llm.api_key, "sk-test-key-12345");
    TEST("max_tokens mapped", state.llm.max_tokens == 4096);
    TEST("temperature mapped", state.llm.temperature == 0.7f);
    TEST("top_p mapped", state.llm.top_p == 0.95f);
    TEST("max_iterations from config", state.max_iterations == 50);
    TEST("tool_delay from config", state.tool_delay == 0.5f);
    TEST("compress_enabled set", state.compress_enabled == true);
    TEST("compress_cooldown from config", state.compress_cooldown_secs == 60);

    agent_free(&state);
}

static void test_configure_partial_config(void) {
    printf("\n--- agent_configure_from_config: Partial / Defaults ---\n");

    agent_state_t state;
    memset(&state, 0, sizeof(state));

    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Set only a few fields — rest should remain zeroed */
    snprintf(cfg.provider_cfg.model, sizeof(cfg.provider_cfg.model), "claude-sonnet-4");
    cfg.agent.max_iterations = 0;  /* should NOT override state default */

    agent_configure_from_config(&state, &cfg);

    TEST_STR("model set", state.llm.model, "claude-sonnet-4");
    TEST("provider empty", state.llm.provider[0] == '\0');
    TEST("base_url empty", state.llm.base_url[0] == '\0');
    TEST("api_key empty", state.llm.api_key[0] == '\0');
    TEST("max_tokens default 0", state.llm.max_tokens == 0);
    TEST("temperature default 0", state.llm.temperature == 0.0f);
    TEST("max_iterations not overridden (cfg had 0)", state.max_iterations == 0);
    TEST("compress not enabled by default", state.compress_enabled == false);

    agent_free(&state);
}

static void test_configure_null_config(void) {
    printf("\n--- agent_configure_from_config: NULL Safety ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    /* Should not crash */
    agent_configure_from_config(NULL, NULL);
    TEST("NULL state no crash", true);

    agent_configure_from_config(&state, NULL);
    TEST("NULL config no crash", true);
    TEST("state unchanged after NULL config", state.max_iterations == 0);

    agent_configure_from_config(NULL, (hermes_config_t*)1);
    TEST("NULL state non-NULL config no crash", true);
}

/* ================================================================
 *  4. agent_inject_history — message injection from JSON
 * ================================================================ */
static void test_inject_history_valid(void) {
    printf("\n--- agent_inject_history: Valid JSON ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    const char *json = "["
        "{\"role\":\"system\",\"content\":\"You are a helpful assistant.\"},"
        "{\"role\":\"user\",\"content\":\"Hello!\"},"
        "{\"role\":\"assistant\",\"content\":\"Hi! How can I help?\"}"
    "]";

    bool ok = agent_inject_history(&state, json);
    TEST("inject returns true", ok == true);
    TEST("3 messages injected", state.message_count == 3);
    TEST("msg[0] is system", state.messages[0]->role == MSG_SYSTEM);
    TEST_STR("msg[0] content", state.messages[0]->content, "You are a helpful assistant.");
    TEST("msg[1] is user", state.messages[1]->role == MSG_USER);
    TEST_STR("msg[1] content", state.messages[1]->content, "Hello!");
    TEST("msg[2] is assistant", state.messages[2]->role == MSG_ASSISTANT);
    TEST_STR("msg[2] content", state.messages[2]->content, "Hi! How can I help?");

    /* Clean up manually since we didn't use agent_init to set up budget etc. */
    context_clear(&state);
}

static void test_inject_history_null_empty(void) {
    printf("\n--- agent_inject_history: NULL / Empty Safety ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    TEST("NULL json returns false", agent_inject_history(&state, NULL) == false);
    TEST("NULL json doesn't change count", state.message_count == 0);
    TEST("empty string returns false", agent_inject_history(&state, "") == false);
    TEST("empty json array returns false",
         agent_inject_history(&state, "[]") == false);
    TEST("NULL state returns false", agent_inject_history(NULL, "[{}]") == false);
}

static void test_inject_history_invalid(void) {
    printf("\n--- agent_inject_history: Invalid JSON ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    /* Not an array */
    TEST("object returns false",
         agent_inject_history(&state, "{\"role\":\"user\"}") == false);
    TEST("string returns false",
         agent_inject_history(&state, "\"hello\"") == false);
    TEST("number returns false",
         agent_inject_history(&state, "42") == false);
    /* Malformed JSON */
    TEST("truncated returns false",
         agent_inject_history(&state, "[{\"role\":\"user\"") == false);
    TEST("no change after invalid", state.message_count == 0);
}

static void test_inject_history_tool_role(void) {
    printf("\n--- agent_inject_history: Tool Role ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    const char *json = "["
        "{\"role\":\"tool\",\"content\":\"result data\"},"
        "{\"role\":\"assistant\",\"content\":\"Done!\"}"
    "]";

    bool ok = agent_inject_history(&state, json);
    TEST("inject tool messages returns true", ok == true);
    TEST("2 messages", state.message_count == 2);
    TEST("msg[0] is tool", state.messages[0]->role == MSG_TOOL);
    TEST_STR("msg[0] content", state.messages[0]->content, "result data");

    context_clear(&state);
}

static void test_inject_history_append(void) {
    printf("\n--- agent_inject_history: Append to Existing ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    /* Inject first batch */
    bool ok = agent_inject_history(&state,
        "[{\"role\":\"user\",\"content\":\"first\"}]");
    TEST("first inject ok", ok == true);
    TEST("1 message after first", state.message_count == 1);

    /* Inject second batch — should append */
    ok = agent_inject_history(&state,
        "[{\"role\":\"user\",\"content\":\"second\"}]");
    TEST("second inject ok", ok == true);
    TEST("2 messages after append", state.message_count == 2);
    TEST_STR("msg[0] preserved", state.messages[0]->content, "first");
    TEST_STR("msg[1] appended", state.messages[1]->content, "second");

    context_clear(&state);
}

/* ================================================================
 *  5. agent_snapshot_take / agent_snapshot_restore
 * ================================================================ */
static void test_snapshot_empty_state(void) {
    printf("\n--- agent_snapshot: Empty State ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    /* Take snapshot of empty state */
    agent_snapshot_take(&state);
    TEST("snapshot taken on empty state completes", true);
    TEST("snapshot_count 0", state.snapshot_count == 0);
    TEST("snapshot non-NULL (capacity allocated)", state.snapshot != NULL);

    /* Restore from empty snapshot should fail */
    bool restored = agent_snapshot_restore(&state);
    TEST("restore from empty snapshot returns false", restored == false);

    /* Clean up snapshot memory manually */
    if (state.snapshot) {
        for (size_t i = 0; i < state.snapshot_count; i++)
            message_free(state.snapshot[i]);
        free(state.snapshot);
        state.snapshot = NULL;
    }
}

static void test_snapshot_with_messages(void) {
    printf("\n--- agent_snapshot: With Messages ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    /* Add messages directly */
    state.messages = (message_t **)calloc(8, sizeof(message_t *));
    state.messages[0] = message_new(MSG_USER, "snapshot test");
    state.messages[1] = message_new(MSG_ASSISTANT, "snapshot response");
    state.message_count = 2;
    state.message_capacity = 8;

    /* Take snapshot */
    agent_snapshot_take(&state);
    TEST("snapshot_count 2", state.snapshot_count == 2);
    TEST("snapshot non-NULL", state.snapshot != NULL);
    TEST_STR("snapshot[0] content preserved",
             state.snapshot[0]->content, "snapshot test");
    TEST_STR("snapshot[1] content preserved",
             state.snapshot[1]->content, "snapshot response");
    TEST("snapshot pointers differ from originals",
         state.snapshot[0] != state.messages[0]);

    /* Now modify messages */
    message_free(state.messages[0]);
    state.messages[0] = message_new(MSG_USER, "modified");
    TEST_STR("current msg[0] modified", state.messages[0]->content, "modified");

    /* Restore from snapshot */
    bool restored = agent_snapshot_restore(&state);
    TEST("restore returns true", restored == true);
    TEST("2 messages after restore", state.message_count == 2);
    TEST_STR("msg[0] restored", state.messages[0]->content, "snapshot test");
    TEST_STR("msg[1] restored", state.messages[1]->content, "snapshot response");

    context_clear(&state);
    /* Also free snapshot */
    if (state.snapshot) {
        for (size_t i = 0; i < state.snapshot_count; i++)
            message_free(state.snapshot[i]);
        free(state.snapshot);
        state.snapshot = NULL;
    }
}

static void test_snapshot_double_take(void) {
    printf("\n--- agent_snapshot: Double Take ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    state.messages = (message_t **)calloc(4, sizeof(message_t *));
    state.messages[0] = message_new(MSG_USER, "version 1");
    state.message_count = 1;
    state.message_capacity = 4;

    /* First snapshot */
    agent_snapshot_take(&state);
    TEST("snapshot_count 1", state.snapshot_count == 1);

    /* Modify and take second snapshot */
    message_free(state.messages[0]);
    state.messages[0] = message_new(MSG_USER, "version 2");
    agent_snapshot_take(&state);
    TEST("snapshot_count still 1 (old freed)", state.snapshot_count == 1);
    TEST_STR("snapshot[0] now version 2",
             state.snapshot[0]->content, "version 2");

    context_clear(&state);
    if (state.snapshot) {
        for (size_t i = 0; i < state.snapshot_count; i++)
            message_free(state.snapshot[i]);
        free(state.snapshot);
        state.snapshot = NULL;
    }
}

/* ================================================================
 *  6. agent_free with snapshot allocated
 * ================================================================ */
static void test_free_with_snapshot(void) {
    printf("\n--- agent_free: State With Snapshot ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));

    /* Set up minimal state with snapshot but no budget */
    state.messages = (message_t **)calloc(2, sizeof(message_t *));
    state.messages[0] = message_new(MSG_USER, "with snapshot");
    state.message_count = 1;
    state.message_capacity = 2;

    /* Also set up snapshot */
    state.snapshot = (message_t **)calloc(2, sizeof(message_t *));
    state.snapshot[0] = message_clone(state.messages[0]);
    state.snapshot_count = 1;
    state.snapshot_capacity = 2;

    /* Free — cleans up messages but NOT snapshot (pre-existing behaviour) */
    agent_free(&state);
    TEST("agent_free with snapshot + messages completes", true);
    TEST("messages NULL after free", state.messages == NULL);
    /* agent_free does NOT free snapshot — known memory leak / caller
       responsibility. Verify snapshot pointers are still valid so the
       test doesn't crash during cleanup below. */
    TEST("snapshot still allocated after free", state.snapshot != NULL);
    /* Clean up snapshot ourselves */
    for (size_t i = 0; i < state.snapshot_count; i++)
        message_free(state.snapshot[i]);
    free(state.snapshot);
    state.snapshot = NULL;
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Agent Loop Core Function Tests (S7 X06) ===\n");

    test_session_id_format();
    test_free_zeroed_state();
    test_free_with_messages();
    test_configure_from_config();
    test_configure_partial_config();
    test_configure_null_config();
    test_inject_history_valid();
    test_inject_history_null_empty();
    test_inject_history_invalid();
    test_inject_history_tool_role();
    test_inject_history_append();
    test_snapshot_empty_state();
    test_snapshot_with_messages();
    test_snapshot_double_take();
    test_free_with_snapshot();

    printf("\n=== Results: %d passed, %d failed, %d total ===\n",
           g_pass, g_fail, g_pass + g_fail);
    return g_fail > 0 ? 1 : 0;
}
