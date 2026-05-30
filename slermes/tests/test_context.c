/*
 * test_context.c -- Tests for message/context management.
 *
 * Tests: message_new/free lifecycle, message_clone deep copy, message_new_tool,
 * message_new_assistant, context_init/push/pop/clear/truncate,
 * context_set_system/get, token estimation, NULL safety.
 */

#include "hermes_agent.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int passed = 0, failed = 0;

#define TEST(name) do { printf("  %s: ", name); } while(0)
#define PASS do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

int main(void) {
    printf("=== Message/Context Tests ===\n");

    /* --- message_new / free --- */
    printf("\n--- Message Lifecycle ---\n");
    {
        TEST("message_new creates user message");
        message_t *m = message_new(MSG_USER, "hello");
        if (m && m->role == MSG_USER && m->content && strcmp(m->content, "hello") == 0) { PASS; }
        else { FAIL("wrong role or content"); }
        message_free(m);
    }
    {
        TEST("message_new with NULL content");
        message_t *m = message_new(MSG_SYSTEM, NULL);
        if (m && m->content == NULL) { PASS; }
        else { FAIL("expected NULL content"); }
        message_free(m);
    }
    {
        TEST("message_free(NULL) no crash");
        message_free(NULL);
        PASS;
    }
    {
        TEST("message_new_tool creates tool message");
        message_t *m = message_new_tool("call_123", "result data");
        if (m && m->role == MSG_TOOL && m->tool_call_id && strcmp(m->tool_call_id, "call_123") == 0) { PASS; }
        else { FAIL("wrong role or missing tool_call_id"); }
        message_free(m);
    }
    {
        TEST("message_new_tool with NULL tool_call_id");
        message_t *m = message_new_tool(NULL, "data");
        if (m && m->tool_call_id == NULL) { PASS; }
        else { FAIL("expected NULL tool_call_id"); }
        message_free(m);
    }
    {
        TEST("message_new_assistant with reasoning + encrypted");
        message_t *m = message_new_assistant("answer", "search", "call_456", "thinking...", "enc_data");
        if (m && m->role == MSG_ASSISTANT &&
            strcmp(m->content, "answer") == 0 &&
            strcmp(m->tool_name, "search") == 0 &&
            strcmp(m->tool_call_id, "call_456") == 0 &&
            strcmp(m->reasoning, "thinking...") == 0 &&
            strcmp(m->encrypted_content, "enc_data") == 0) { PASS; }
        else { FAIL("field mismatch"); }
        message_free(m);
    }
    {
        TEST("message_new_assistant with NULL optional fields");
        message_t *m = message_new_assistant("answer", NULL, NULL, NULL, NULL);
        if (m && m->content && m->tool_name == NULL && m->reasoning == NULL) { PASS; }
        else { FAIL("expected NULL fields"); }
        message_free(m);
    }

    /* --- message_clone --- */
    printf("\n--- Message Clone ---\n");
    {
        TEST("message_clone deep copy");
        message_t *orig = message_new_assistant("hello", "tool1", "call1", "reason", "enc");
        message_t *clone = message_clone(orig);
        if (clone && clone->role == orig->role &&
            strcmp(clone->content, orig->content) == 0 &&
            strcmp(clone->tool_name, orig->tool_name) == 0 &&
            strcmp(clone->reasoning, orig->reasoning) == 0 &&
            strcmp(clone->encrypted_content, orig->encrypted_content) == 0) { PASS; }
        else { FAIL("clone mismatch"); }
        /* Verify deep copy: modifying one shouldn't affect the other */
        if (clone && clone->content != orig->content) { PASS; }
        else { FAIL("shallow copy (same pointer)"); }
        message_free(orig);
        message_free(clone);
    }
    {
        TEST("message_clone(NULL) returns NULL");
        message_t *c = message_clone(NULL);
        if (c == NULL) { PASS; } else { FAIL("expected NULL"); message_free(c); }
    }

    /* --- Token estimation --- */
    printf("\n--- Token Estimation ---\n");
    {
        /* context_message_tokens uses estimate_tokens: strlen/4 + 1 */
        int tokens = context_message_tokens(NULL);
        if (tokens >= 0) { PASS; } else { FAIL("negative tokens"); }
    }
    {
        TEST("empty message token estimate");
        message_t *m = message_new(MSG_USER, "");
        int tokens = context_message_tokens(m);
        if (tokens == 1) { PASS; } else { char b[64]; snprintf(b, sizeof(b), "got %d", tokens); FAIL(b); }
        message_free(m);
    }
    {
        TEST("message with 20 chars ~= 6 tokens");
        message_t *m = message_new(MSG_USER, "abcdefghijklmnopqrst");
        int tokens = context_message_tokens(m);
        if (tokens == 6) { PASS; } else { char b[64]; snprintf(b, sizeof(b), "got %d", tokens); FAIL(b); }
        message_free(m);
    }

    /* --- Context management --- */
    printf("\n--- Context Operations ---\n");
    {
        TEST("context_init initializes empty state");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        if (state.message_count == 0 && state.messages == NULL) { PASS; }
        else { FAIL("not empty"); }
    }
    {
        TEST("context_push adds messages");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        message_t *m1 = message_new(MSG_USER, "first");
        message_t *m2 = message_new(MSG_ASSISTANT, "second");
        bool p1 = context_push(&state, m1);
        bool p2 = context_push(&state, m2);
        if (p1 && p2 && state.message_count == 2) { PASS; }
        else { FAIL("push failed"); }
        /* Clean up without freeing messages (context owns them) */
        context_clear(&state);
    }
    {
        TEST("context_push(NULL) returns false");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        bool ok = context_push(&state, NULL);
        if (!ok) { PASS; } else { FAIL("expected false"); }
    }
    {
        TEST("context_pop retrieves last message");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        message_t *m1 = message_new(MSG_USER, "first");
        message_t *m2 = message_new(MSG_ASSISTANT, "last");
        context_push(&state, m1);
        context_push(&state, m2);
        message_t *popped = context_pop(&state);
        if (popped == m2 && state.message_count == 1) {
            PASS;
            message_free(popped);
        } else { FAIL("wrong message"); }
        context_clear(&state);
    }
    {
        TEST("context_pop on empty returns NULL");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        message_t *m = context_pop(&state);
        if (m == NULL) { PASS; }
        else { FAIL("expected NULL"); message_free(m); }
    }
    {
        TEST("context_get retrieves by index");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        message_t *m1 = message_new(MSG_USER, "alpha");
        message_t *m2 = message_new(MSG_USER, "beta");
        context_push(&state, m1);
        context_push(&state, m2);
        const message_t *g1 = context_get(&state, 0);
        const message_t *g2 = context_get(&state, 1);
        if (g1 == m1 && g2 == m2) { PASS; }
        else { FAIL("wrong index"); }
        context_clear(&state);
    }
    {
        TEST("context_get out of bounds returns NULL");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        const message_t *g = context_get(&state, 999);
        if (g == NULL) { PASS; } else { FAIL("expected NULL"); }
    }

    /* --- System prompt --- */
    printf("\n--- System Prompt ---\n");
    {
        TEST("context_set_system sets system message");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        bool ok = context_set_system(&state, "You are a helpful bot.");
        const char *sys = context_get_system(&state);
        if (ok && sys && strcmp(sys, "You are a helpful bot.") == 0) { PASS; }
        else { FAIL("system prompt not set"); }
        context_clear(&state);
    }
    {
        TEST("context_set_system(NULL) clears system message");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        context_set_system(&state, "prompt");
        context_set_system(&state, NULL);
        const char *sys = context_get_system(&state);
        if (sys == NULL || sys[0] == '\0') { PASS; }
        else { FAIL("system not cleared"); }
        context_clear(&state);
    }

    /* --- Truncation --- */
    printf("\n--- Truncation ---\n");
    {
        TEST("context_truncate reduces to N messages");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        for (int i = 0; i < 10; i++)
            context_push(&state, message_new(MSG_USER, "msg"));
        context_truncate(&state, 3);
        if (state.message_count == 3) { PASS; }
        else { char b[64]; snprintf(b, sizeof(b), "count=%zu", state.message_count); FAIL(b); }
        context_clear(&state);
    }
    {
        TEST("context_truncate to larger count keeps all");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        for (int i = 0; i < 5; i++)
            context_push(&state, message_new(MSG_USER, "msg"));
        context_truncate(&state, 100);
        if (state.message_count == 5) { PASS; }
        else { FAIL("messages lost"); }
        context_clear(&state);
    }

    /* --- Smart eviction --- */
    printf("\n--- Smart Eviction ---\n");
    {
        TEST("context_evict_smart reduces count");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        for (int i = 0; i < 20; i++)
            context_push(&state, message_new(MSG_USER, "Hello world, this is a test message!"));
        context_evict_smart(&state, 10, EVICT_KEEP_RECENT_N);
        if (state.message_count <= 10) { PASS; }
        else { char b[64]; snprintf(b, sizeof(b), "count=%zu", state.message_count); FAIL(b); }
        context_clear(&state);
    }
    {
        TEST("context_total_tokens > 0 with messages");
        agent_state_t state;
        memset(&state, 0, sizeof(state));
        context_init(&state);
        context_push(&state, message_new(MSG_USER, "This is twelve words in a test context message!"));
        int tokens = context_total_tokens(&state);
        if (tokens > 0) { PASS; }
        else { char b[64]; snprintf(b, sizeof(b), "tokens=%d", tokens); FAIL(b); }
        context_clear(&state);
    }

    /* Summary */
    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n", passed, failed, 0);
    printf("==============================================\n");
    return failed > 0 ? 1 : 0;
}
