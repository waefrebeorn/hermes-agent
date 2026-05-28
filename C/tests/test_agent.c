/*
 * test_agent.c — Agent loop + context test suite (G166).
 *
 * Tests: message lifecycle, context operations, token estimation,
 * compression feedback, and checkpoint manager.
 *
 * NOTE: agent_init() tests are NOT included here because agent_loop.c
 * depends on db, llm_client, approval, and budget_tracker — too many deps
 * for a unit test. agent_init behavior is verified through the end-to-end
 * binary (integration tests).
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_agent.c src/agent/context.c src/agent/checkpoint.c \
 *       lib/libjson/json.c lib/libplugin/plugin.c \
 *       -o /tmp/hermes_test_agent -lm -lpthread
 *
 * Run:
 *   /tmp/hermes_test_agent
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_TRUE(name, expr) TEST(name, (expr))
#define TEST_FALSE(name, expr) TEST(name, !(expr))
#define TEST_EQ(name, a, b) do { \
    long long _av = (long long)(a); long long _bv = (long long)(b); \
    if (_av == _bv) { passed++; printf("  PASS: %s (%lld)\n", name, _av); } \
    else { failed++; printf("  FAIL: %s -- expected %lld, got %lld (line %d)\n", name, _bv, _av, __LINE__); } \
} while(0)
#define TEST_STR(name, a, b) do { \
    const char *_aa = (a) ? (a) : ""; const char *_bb = (b) ? (b) : ""; \
    if (strcmp(_aa, _bb) == 0) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s -- expected '%s', got '%s' (line %d)\n", name, _bb, _aa, __LINE__); } \
} while(0)

/* ================================================================
 *  1. Message lifecycle
 * ================================================================ */

static void test_message_new(void) {
    printf("\n--- Message: New & Free ---\n");
    message_t *m = message_new(MSG_USER, "hello world");
    TEST_TRUE("create user msg", m != NULL);
    TEST_EQ("role == MSG_USER", m->role, MSG_USER);
    TEST_STR("content correct", m->content, "hello world");
    TEST_TRUE("tool_call_id NULL", m->tool_call_id == NULL);
    TEST_TRUE("tool_name NULL", m->tool_name == NULL);
    TEST_TRUE("reasoning NULL", m->reasoning == NULL);
    TEST_EQ("no tool calls", m->tool_calls_count, 0);
    message_free(m);
    m = message_new(MSG_SYSTEM, "You are helpful.");
    TEST_TRUE("create system msg", m != NULL);
    TEST_EQ("role == MSG_SYSTEM", m->role, MSG_SYSTEM);
    message_free(m);
    m = message_new(MSG_USER, NULL);
    TEST_TRUE("create msg with NULL content", m != NULL);
    TEST_TRUE("content is NULL", m->content == NULL);
    message_free(m);
    message_free(NULL);
    TEST_TRUE("free NULL is safe", true);
}

static void test_message_tool(void) {
    printf("\n--- Message: Tool Results ---\n");
    message_t *m = message_new_tool("call_123", "result data");
    TEST_TRUE("create tool msg", m != NULL);
    TEST_EQ("role == MSG_TOOL", m->role, MSG_TOOL);
    TEST_STR("tool_call_id set", m->tool_call_id, "call_123");
    TEST_STR("content set", m->content, "result data");
    message_free(m);
    m = message_new_tool(NULL, "data");
    TEST_TRUE("tool msg NULL call_id", m != NULL);
    TEST_TRUE("tool_call_id NULL", m->tool_call_id == NULL);
    message_free(m);
    m = message_new_tool("call_456", NULL);
    TEST_TRUE("tool msg NULL content", m != NULL);
    TEST_TRUE("content NULL", m->content == NULL);
    message_free(m);
}

static void test_message_assistant(void) {
    printf("\n--- Message: Assistant ---\n");
    message_t *m = message_new_assistant("I think...", NULL, NULL, "reasoning", NULL);
    TEST_TRUE("create assistant msg", m != NULL);
    TEST_EQ("role == MSG_ASSISTANT", m->role, MSG_ASSISTANT);
    TEST_STR("content set", m->content, "I think...");
    TEST_STR("reasoning set", m->reasoning, "reasoning");
    TEST_TRUE("tool_name NULL", m->tool_name == NULL);
    message_free(m);
    m = message_new_assistant(NULL, "get_weather", "call_789", NULL, NULL);
    TEST_TRUE("assistant with tool", m != NULL);
    TEST_STR("tool_name set", m->tool_name, "get_weather");
    TEST_STR("tool_call_id set", m->tool_call_id, "call_789");
    TEST_TRUE("reasoning NULL", m->reasoning == NULL);
    message_free(m);
}

static void test_message_assistant_toolcalls(void) {
    printf("\n--- Message: Assistant With Tool Calls ---\n");
    tool_call_t tcalls[2];
    snprintf(tcalls[0].id, sizeof(tcalls[0].id), "call_1");
    snprintf(tcalls[0].name, sizeof(tcalls[0].name), "read_file");
    snprintf(tcalls[0].arguments, sizeof(tcalls[0].arguments), "{\"path\":\"/tmp\"}");
    snprintf(tcalls[1].id, sizeof(tcalls[1].id), "call_2");
    snprintf(tcalls[1].name, sizeof(tcalls[1].name), "write_file");
    snprintf(tcalls[1].arguments, sizeof(tcalls[1].arguments), "{\"path\":\"/tmp/x\",\"content\":\"hi\"}");
    message_t *m = message_new_assistant_with_toolcalls("Let me do that.", tcalls, 2, "reasoning text", NULL);
    TEST_TRUE("create assistant with tcalls", m != NULL);
    TEST_STR("content set", m->content, "Let me do that.");
    TEST_STR("reasoning set", m->reasoning, "reasoning text");
    TEST_EQ("2 tool calls", m->tool_calls_count, 2);
    TEST_STR("tc[0] name", m->tool_calls[0].name, "read_file");
    TEST_STR("tc[1] name", m->tool_calls[1].name, "write_file");
    TEST_STR("tc[0] id", m->tool_calls[0].id, "call_1");
    message_free(m);
    m = message_new_assistant_with_toolcalls("No tools.", NULL, 0, NULL, NULL);
    TEST_TRUE("assistant with 0 tcalls", m != NULL);
    TEST_EQ("0 tool calls", m->tool_calls_count, 0);
    TEST_TRUE("no reasoning", m->reasoning == NULL);
    message_free(m);
    m = message_new_assistant_with_toolcalls(NULL, tcalls, 1, NULL, NULL);
    TEST_TRUE("assistant with NULL content", m != NULL);
    TEST_TRUE("content NULL", m->content == NULL);
    message_free(m);
}

static void test_message_clone(void) {
    printf("\n--- Message: Clone ---\n");
    message_t *orig = message_new(MSG_USER, "hello");
    TEST_TRUE("create orig", orig != NULL);
    message_t *clone = message_clone(orig);
    TEST_TRUE("clone non-NULL", clone != NULL);
    TEST_EQ("same role", clone->role, orig->role);
    TEST_STR("same content", clone->content, orig->content);
    TEST_TRUE("different pointers", clone->content != orig->content);
    message_free(orig);
    message_free(clone);
    TEST_TRUE("clone NULL returns NULL", message_clone(NULL) == NULL);
    /* Clone assistant with tool calls */
    tool_call_t tc;
    snprintf(tc.id, sizeof(tc.id), "call_x");
    snprintf(tc.name, sizeof(tc.name), "tool_x");
    snprintf(tc.arguments, sizeof(tc.arguments), "{}");
    orig = message_new_assistant_with_toolcalls("do it", &tc, 1, "reason", NULL);
    clone = message_clone(orig);
    TEST_TRUE("clone with tcalls non-NULL", clone != NULL);
    TEST_EQ("clone has tcalls", clone->tool_calls_count, 1);
    TEST_STR("tcall id preserved", clone->tool_calls[0].id, "call_x");
    message_free(orig);
    message_free(clone);
}

/* ================================================================
 *  2. Token estimation
 *   formula: (strlen(text) + 3) / 4  (ceil division)
 * ================================================================ */

static void test_token_estimation(void) {
    printf("\n--- Token Estimation ---\n");
    /* llm_estimate_tokens inline function from hermes_agent.h */
    TEST_EQ("empty string", llm_estimate_tokens(""), (size_t)0);
    TEST_EQ("NULL safe", llm_estimate_tokens(NULL), (size_t)0);
    TEST_EQ("4 chars", llm_estimate_tokens("abcd"), (size_t)1);      /* (4+3)/4=1 */
    TEST_EQ("5 chars", llm_estimate_tokens("abcde"), (size_t)2);     /* (5+3)/4=2 */
    TEST_EQ("1 char", llm_estimate_tokens("A"), (size_t)1);          /* (1+3)/4=1 */
    TEST_EQ("0 chars", llm_estimate_tokens(""), (size_t)0);          /* (0+3)/4=0 */
    /* Test with a medium sentence — count chars then verify ceil division */
    const char *msg = "Hello, world! This is a test.";
    size_t len = strlen(msg);                                        /* 29 */
    size_t expected = (len + 3) / 4;                                 /* (29+3)/4 = 8 */
    TEST_EQ("medium text ceil div", llm_estimate_tokens(msg), expected);
}

/* ================================================================
 *  3. Context operations
 *   Note: context_init uses lazy allocation — messages=NULL, capacity=0
 *   until first push triggers realloc.
 * ================================================================ */

static void test_context_init(void) {
    printf("\n--- Context: Init (lazy allocation) ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    TEST_TRUE("messages NULL", state.messages == NULL);
    TEST_EQ("message_count 0", state.message_count, (size_t)0);
    TEST_EQ("message_capacity 0", state.message_capacity, (size_t)0);
}

static void test_context_push(void) {
    printf("\n--- Context: Push & Count ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    message_t *m1 = message_new(MSG_USER, "hello");
    message_t *m2 = message_new(MSG_USER, "world");
    message_t *m3 = message_new(MSG_ASSISTANT, "hi there");
    TEST_TRUE("push m1", context_push(&state, m1));
    TEST_EQ("count 1", state.message_count, (size_t)1);
    TEST_TRUE("capacity grew to 64", state.message_capacity == 64);
    TEST_TRUE("same pointer stored", state.messages[0] == m1);
    TEST_TRUE("push m2", context_push(&state, m2));
    TEST_EQ("count 2", state.message_count, (size_t)2);
    TEST_TRUE("push m3", context_push(&state, m3));
    TEST_EQ("count 3", state.message_count, (size_t)3);
    TEST_EQ("msg[0] role USER", state.messages[0]->role, MSG_USER);
    TEST_EQ("msg[2] role ASSISTANT", state.messages[2]->role, MSG_ASSISTANT);
    /* Test push NULL */
    TEST_FALSE("push NULL rejected", context_push(&state, NULL));
    context_clear(&state);
}

static void test_context_pop(void) {
    printf("\n--- Context: Pop ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    message_t *m1 = message_new(MSG_USER, "first");
    message_t *m2 = message_new(MSG_USER, "second");
    context_push(&state, m1);
    context_push(&state, m2);
    message_t *popped = context_pop(&state);
    TEST_TRUE("popped non-NULL", popped != NULL);
    TEST_STR("popped is second", popped->content, "second");
    TEST_EQ("count 1 after pop", state.message_count, (size_t)1);
    message_free(popped);
    popped = context_pop(&state);
    TEST_TRUE("popped first", popped != NULL);
    TEST_STR("popped is first", popped->content, "first");
    TEST_EQ("count 0", state.message_count, (size_t)0);
    message_free(popped);
    TEST_TRUE("pop empty returns NULL", context_pop(&state) == NULL);
    context_clear(&state);
}

static void test_context_clear(void) {
    printf("\n--- Context: Clear ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    context_push(&state, message_new(MSG_USER, "a"));
    context_push(&state, message_new(MSG_USER, "b"));
    context_push(&state, message_new(MSG_USER, "c"));
    TEST_EQ("count 3 before clear", state.message_count, (size_t)3);
    context_clear(&state);
    TEST_EQ("count 0 after clear", state.message_count, (size_t)0);
    TEST_TRUE("messages freed (NULL)", state.messages == NULL);
    TEST_EQ("capacity 0", state.message_capacity, (size_t)0);
    /* Clear empty is safe */
    context_clear(&state);
    TEST_TRUE("clear empty safe", true);
}

static void test_context_system(void) {
    printf("\n--- Context: System Message ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    const char *sys = context_get_system(&state);
    TEST_TRUE("no system msg initially", sys == NULL || sys[0] == '\0');
    TEST_TRUE("set system", context_set_system(&state, "You are a helpful assistant."));
    sys = context_get_system(&state);
    TEST_STR("system msg set", sys, "You are a helpful assistant.");
    TEST_TRUE("set system again", context_set_system(&state, "New system prompt."));
    sys = context_get_system(&state);
    TEST_STR("system msg updated", sys, "New system prompt.");
    TEST_TRUE("system message at index 0", state.message_count > 0);
    TEST_EQ("msg[0] is SYSTEM", state.messages[0]->role, MSG_SYSTEM);
    /* Set system with NULL clears it */
    TEST_TRUE("set system NULL", context_set_system(&state, NULL));
    sys = context_get_system(&state);
    TEST_TRUE("system msg cleared", sys == NULL || sys[0] == '\0');
    context_clear(&state);
}

static void test_context_get(void) {
    printf("\n--- Context: Get by Index ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    context_push(&state, message_new(MSG_USER, "a"));
    context_push(&state, message_new(MSG_USER, "b"));
    const message_t *m = context_get(&state, 0);
    TEST_TRUE("get[0] non-NULL", m != NULL);
    TEST_STR("get[0] content", m->content, "a");
    m = context_get(&state, 1);
    TEST_STR("get[1] content", m->content, "b");
    m = context_get(&state, 2);
    TEST_TRUE("get out-of-range NULL", m == NULL);
    m = context_get(NULL, 0);
    TEST_TRUE("get NULL state safe", m == NULL);
    context_clear(&state);
}

static void test_context_truncate(void) {
    printf("\n--- Context: Truncate ---\n");
    context_truncate(NULL, 5);
    TEST_TRUE("NULL state safe", true);
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    context_push(&state, message_new(MSG_USER, "a"));
    context_push(&state, message_new(MSG_USER, "b"));
    context_push(&state, message_new(MSG_USER, "c"));
    context_push(&state, message_new(MSG_USER, "d"));
    /* truncate removes OLDEST, keeps most recent N */
    context_truncate(&state, 2);
    TEST_EQ("count 2 after truncate", state.message_count, (size_t)2);
    TEST_STR("kept oldest of remaining (c)", state.messages[0]->content, "c");
    TEST_STR("kept most recent (d)", state.messages[1]->content, "d");
    /* Truncate beyond count is no-op */
    context_truncate(&state, 10);
    TEST_EQ("truncate > count no-op", state.message_count, (size_t)2);
    context_clear(&state);
}

static void test_context_eviction(void) {
    printf("\n--- Context: Smart Eviction ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    context_push(&state, message_new(MSG_USER, "user1"));
    context_push(&state, message_new(MSG_ASSISTANT, "asst1"));
    context_push(&state, message_new(MSG_USER, "user2"));
    context_push(&state, message_new(MSG_TOOL, "tool1"));
    context_push(&state, message_new(MSG_TOOL, "tool2"));
    context_push(&state, message_new(MSG_USER, "user3"));
    context_push(&state, message_new(MSG_ASSISTANT, "asst2"));
    context_evict_smart(&state, 5, EVICT_OLDEST_TOOL_FIRST);
    TEST_EQ("evicted to 5", state.message_count, (size_t)5);
    context_evict_smart(&state, 3, EVICT_OLDEST_USER);
    TEST_EQ("evicted to 3", state.message_count, (size_t)3);
    context_evict_smart(&state, 1, EVICT_KEEP_RECENT_N);
    TEST_EQ("keep 1 most recent", state.message_count, (size_t)1);
    TEST_STR("kept last", state.messages[0]->content, "asst2");
    /* Already <= max — no-op */
    context_evict_smart(&state, 5, EVICT_OLDEST_TOOL_FIRST);
    TEST_EQ("no-op when already under limit", state.message_count, (size_t)1);
    context_clear(&state);
}

/* ================================================================
 *  4. Compression feedback
 * ================================================================ */

static void test_compression_feedback(void) {
    printf("\n--- Compression Feedback ---\n");
    compression_feedback_t fb;
    memset(&fb, 0, sizeof(fb));
    compression_feedback_init(&fb);
    TEST_EQ("init total=0", fb.total_compressions, 0);
    TEST_EQ("init pos=0", fb.positive_feedback, 0);
    TEST_EQ("init neg=0", fb.negative_feedback, 0);
    TEST_TRUE("quality_score ~0.5", fabs(fb.quality_score - 0.5f) < 0.01f);

    compression_feedback_positive(&fb);
    TEST_EQ("1 positive", fb.positive_feedback, 1);
    TEST_EQ("total=1", fb.total_compressions, 1);
    TEST_TRUE("quality > 0.5", fb.quality_score > 0.5f);

    compression_feedback_negative(&fb);
    TEST_EQ("1 negative", fb.negative_feedback, 1);
    TEST_EQ("total=2", fb.total_compressions, 2);

    float thresh = compression_feedback_get_threshold(&fb, 0.5f);
    TEST_TRUE("threshold positive", thresh > 0.0f);
    TEST_TRUE("threshold <= 1.0", thresh <= 1.0f);
    /* With no feedback, threshold should degrade slightly */
    compression_feedback_t fb2;
    memset(&fb2, 0, sizeof(fb2));
    compression_feedback_init(&fb2);
    float thresh2 = compression_feedback_get_threshold(&fb2, 1.0f);
    TEST_TRUE("threshold with 1.0 config", thresh2 > 0.0f);

    char buf[256];
    compression_feedback_status(&fb, buf, sizeof(buf));
    TEST_TRUE("status non-empty", strlen(buf) > 0);
    /* Test with small buffer */
    compression_feedback_status(&fb, buf, 4);
    TEST_TRUE("truncated status safe", strlen(buf) >= 0);
}

/* ================================================================
 *  5. Checkpoint manager
 *   checkpoint_init uses lazy allocation — no malloc until first save
 * ================================================================ */

static void test_checkpoint_init(void) {
    printf("\n--- Checkpoint: Init (lazy) ---\n");
    checkpoint_manager_t mgr;
    memset(&mgr, 0, sizeof(mgr));
    checkpoint_init(&mgr);
    TEST_EQ("count 0", mgr.count, (size_t)0);
    TEST_TRUE("checkpoints NULL (lazy)", mgr.checkpoints == NULL);
    TEST_EQ("default max_snapshots", mgr.max_snapshots, 10);
    TEST_EQ("default auto_save_interval", mgr.auto_save_interval, 5);
    checkpoint_free(&mgr);
    TEST_EQ("count 0 after free", mgr.count, (size_t)0);
    /* Double free safe */
    checkpoint_free(&mgr);
    TEST_TRUE("double free safe", true);
}

static void test_checkpoint_limits(void) {
    printf("\n--- Checkpoint: Set Limits ---\n");
    checkpoint_manager_t mgr;
    memset(&mgr, 0, sizeof(mgr));
    checkpoint_init(&mgr);
    checkpoint_set_limits(&mgr, 10, 5);
    TEST_EQ("max_snapshots 10", mgr.max_snapshots, 10);
    TEST_EQ("auto_save_interval 5", mgr.auto_save_interval, 5);
    /* Zero values don't override */
    checkpoint_set_limits(&mgr, 0, 0);
    TEST_EQ("zero doesn't change max", mgr.max_snapshots, 10);
    TEST_EQ("zero doesn't change interval", mgr.auto_save_interval, 5);
    checkpoint_free(&mgr);
}

static void test_checkpoint_save_restore(void) {
    printf("\n--- Checkpoint: Save & Restore ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    state.max_iterations = 42;

    context_push(&state, message_new(MSG_USER, "hello"));
    context_push(&state, message_new(MSG_ASSISTANT, "world"));

    checkpoint_init(&state.checkpoints);
    checkpoint_set_limits(&state.checkpoints, 5, 0);

    TEST_TRUE("save checkpoint", checkpoint_save(&state.checkpoints, &state, "before_test"));
    TEST_EQ("1 checkpoint", checkpoint_count(&state.checkpoints), (size_t)1);

    /* List checkpoints */
    char ids[5][64], labels[5][128];
    size_t n = checkpoint_list(&state.checkpoints, ids, labels, 5);
    TEST_EQ("list returns 1", n, (size_t)1);
    TEST_STR("label matches", labels[0], "before_test");
    TEST_TRUE("id non-empty", strlen(ids[0]) > 0);

    /* Add more messages */
    context_push(&state, message_new(MSG_USER, "more data"));
    TEST_EQ("3 messages", state.message_count, (size_t)3);

    /* Restore checkpoint */
    TEST_TRUE("restore checkpoint", checkpoint_restore(&state.checkpoints, &state, ids[0]));
    TEST_EQ("restored to 2 messages", state.message_count, (size_t)2);
    TEST_STR("restored content", state.messages[1]->content, "world");
    TEST_EQ("max_iterations preserved", state.max_iterations, 42);

    /* Restore from non-existent ID */
    TEST_FALSE("restore unknown ID", checkpoint_restore(&state.checkpoints, &state, "nonexistent"));

    checkpoint_free(&state.checkpoints);
    context_clear(&state);
}

static void test_checkpoint_multiple(void) {
    printf("\n--- Checkpoint: Multiple Saves ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    context_push(&state, message_new(MSG_USER, "m1"));
    context_push(&state, message_new(MSG_USER, "m2"));
    checkpoint_init(&state.checkpoints);
    checkpoint_set_limits(&state.checkpoints, 5, 0);

    TEST_TRUE("save cp1", checkpoint_save(&state.checkpoints, &state, "cp1"));
    context_push(&state, message_new(MSG_USER, "m3"));
    TEST_TRUE("save cp2", checkpoint_save(&state.checkpoints, &state, "cp2"));
    /* Enforce max */
    checkpoint_set_limits(&state.checkpoints, 5, 0);
    context_push(&state, message_new(MSG_USER, "m4"));
    TEST_TRUE("save cp3", checkpoint_save(&state.checkpoints, &state, "cp3"));
    TEST_EQ("3 checkpoints", checkpoint_count(&state.checkpoints), (size_t)3);

    /* List all */
    char ids[5][64], labels[5][128];
    size_t n = checkpoint_list(&state.checkpoints, ids, labels, 5);
    TEST_EQ("list 3", n, (size_t)3);

    /* Restore each in sequence */
    TEST_TRUE("restore cp1", checkpoint_restore(&state.checkpoints, &state, ids[0]));
    TEST_EQ("cp1: 2 messages", state.message_count, (size_t)2);
    TEST_TRUE("restore cp2", checkpoint_restore(&state.checkpoints, &state, ids[1]));
    TEST_EQ("cp2: 3 messages", state.message_count, (size_t)3);
    TEST_TRUE("restore cp3", checkpoint_restore(&state.checkpoints, &state, ids[2]));
    TEST_EQ("cp3: 4 messages", state.message_count, (size_t)4);

    checkpoint_free(&state.checkpoints);
    context_clear(&state);
}

static void test_checkpoint_autosave(void) {
    printf("\n--- Checkpoint: Auto-save ---\n");
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);
    context_push(&state, message_new(MSG_USER, "user q"));
    context_push(&state, message_new(MSG_ASSISTANT, "response"));
    checkpoint_init(&state.checkpoints);
    checkpoint_set_limits(&state.checkpoints, 5, 3);
    TEST_EQ("turn counter start 0", state.checkpoints.turn_counter, 0);

    /* turn_counter=2 → becomes 3 → 3>=3 → save & reset */
    state.checkpoints.turn_counter = 2;
    TEST_TRUE("autosave fires", checkpoint_try_autosave(&state.checkpoints, &state));
    TEST_EQ("1 checkpoint", checkpoint_count(&state.checkpoints), (size_t)1);
    TEST_EQ("counter reset", state.checkpoints.turn_counter, 0);

    /* Not due yet */
    state.checkpoints.turn_counter = 1;
    TEST_FALSE("not due", checkpoint_try_autosave(&state.checkpoints, &state));
    TEST_EQ("still 1", checkpoint_count(&state.checkpoints), (size_t)1);
    TEST_EQ("counter 2", state.checkpoints.turn_counter, 2);

    /* Empty state doesn't save */
    agent_state_t empty_state;
    memset(&empty_state, 0, sizeof(empty_state));
    context_init(&empty_state);
    checkpoint_manager_t mgr;
    memset(&mgr, 0, sizeof(mgr));
    checkpoint_init(&mgr);
    TEST_FALSE("empty state no autosave", checkpoint_try_autosave(&mgr, &empty_state));

    checkpoint_free(&state.checkpoints);
    context_clear(&state);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== Agent Loop / Context Test Suite (G166) ===\n");

    test_message_new();
    test_message_tool();
    test_message_assistant();
    test_message_assistant_toolcalls();
    test_message_clone();
    test_token_estimation();
    test_context_init();
    test_context_push();
    test_context_pop();
    test_context_clear();
    test_context_system();
    test_context_get();
    test_context_truncate();
    test_context_eviction();
    test_compression_feedback();
    test_checkpoint_init();
    test_checkpoint_limits();
    test_checkpoint_save_restore();
    test_checkpoint_multiple();
    test_checkpoint_autosave();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
