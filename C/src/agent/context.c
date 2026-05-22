/*
 * context.c — Message context management for Hermes C.
 * Manages the array of messages in agent_state_t.
 * Provides: push, pop, get, truncate, eviction strategies, system prompt injection.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Token estimation (rough: ~4 chars per token)
 * ================================================================ */

static int estimate_tokens(const char *text) {
    if (!text) return 0;
    size_t len = strlen(text);
    return (int)(len / 4) + 1; /* rough estimate */
}

/* ================================================================
 *  Message lifecycle
 * ================================================================ */

message_t *message_new(message_role_t role, const char *content) {
    message_t *msg = (message_t *)calloc(1, sizeof(message_t));
    if (!msg) return NULL;
    msg->role = role;
    msg->content = content ? strdup(content) : NULL;
    msg->tool_call_id = NULL;
    msg->tool_name = NULL;
    msg->reasoning = NULL;
    return msg;
}

message_t *message_new_tool(const char *tool_call_id, const char *content) {
    message_t *msg = message_new(MSG_TOOL, content);
    if (msg && tool_call_id)
        msg->tool_call_id = strdup(tool_call_id);
    return msg;
}

message_t *message_new_assistant(const char *content, const char *tool_name,
                                  const char *tool_call_id, const char *reasoning)
{
    message_t *msg = message_new(MSG_ASSISTANT, content);
    if (msg) {
        if (tool_name) msg->tool_name = strdup(tool_name);
        if (tool_call_id) msg->tool_call_id = strdup(tool_call_id);
        if (reasoning) msg->reasoning = strdup(reasoning);
        msg->tool_calls_count = 0;
    }
    return msg;
}

/* Create assistant message with tool calls (from LLM response) */
message_t *message_new_assistant_with_toolcalls(const char *content,
                                                  const tool_call_t *tcalls,
                                                  int tcalls_count,
                                                  const char *reasoning)
{
    message_t *msg = message_new(MSG_ASSISTANT, content);
    if (!msg) return NULL;
    if (reasoning) msg->reasoning = strdup(reasoning);
    msg->tool_calls_count = tcalls_count > 64 ? 64 : tcalls_count;
    for (int i = 0; i < msg->tool_calls_count; i++) {
        memcpy(&msg->tool_calls[i], &tcalls[i], sizeof(tool_call_t));
    }
    return msg;
}

void message_free(message_t *msg) {
    if (!msg) return;
    free(msg->content);
    free(msg->tool_call_id);
    free(msg->tool_name);
    free(msg->reasoning);
    free(msg);
}

/* ================================================================
 *  Context operations
 * ================================================================ */

void context_init(agent_state_t *state) {
    state->messages = NULL;
    state->message_count = 0;
    state->message_capacity = 0;
}

bool context_push(agent_state_t *state, message_t *msg) {
    if (!state || !msg) return false;

    if (state->message_count >= state->message_capacity) {
        size_t new_cap = state->message_capacity == 0 ? 64 : state->message_capacity * 2;
        if (new_cap > HERMES_MAX_MESSAGES) new_cap = HERMES_MAX_MESSAGES;
        message_t **new_msgs = (message_t **)realloc(
            state->messages, new_cap * sizeof(message_t *));
        if (!new_msgs) return false;
        state->messages = new_msgs;
        state->message_capacity = new_cap;
    }

    state->messages[state->message_count++] = msg;
    return true;
}

message_t *context_pop(agent_state_t *state) {
    if (!state || state->message_count == 0) return NULL;
    return state->messages[--state->message_count];
}

void context_clear(agent_state_t *state) {
    if (!state || !state->messages) return;
    for (size_t i = 0; i < state->message_count; i++)
        message_free(state->messages[i]);
    free(state->messages);
    state->messages = NULL;
    state->message_count = 0;
    state->message_capacity = 0;
}

bool context_set_system(agent_state_t *state, const char *content) {
    /* Insert or update first system message */
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->role == MSG_SYSTEM) {
            free(state->messages[i]->content);
            state->messages[i]->content = content ? strdup(content) : NULL;
            return true;
        }
    }
    /* No system message — insert at front */
    message_t *sys = message_new(MSG_SYSTEM, content);
    if (!sys) return false;

    /* Shift all messages right by one */
    size_t new_count = state->message_count + 1;
    if (new_count > state->message_capacity) {
        size_t new_cap = state->message_capacity == 0 ? 64 : state->message_capacity * 2;
        message_t **new_msgs = (message_t **)realloc(
            state->messages, new_cap * sizeof(message_t *));
        if (!new_msgs) { message_free(sys); return false; }
        state->messages = new_msgs;
        state->message_capacity = new_cap;
    }

    memmove(&state->messages[1], &state->messages[0],
            state->message_count * sizeof(message_t *));
    state->messages[0] = sys;
    state->message_count++;
    return true;
}

void context_truncate(agent_state_t *state, size_t max_messages) {
    if (!state || state->message_count <= max_messages) return;
    /* Keep system message (index 0) + most recent max_messages-1 */
    size_t keep_system = (state->message_count > 0 &&
                          state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    size_t remove_count = state->message_count - max_messages;

    if (keep_system && remove_count > 0) {
        /* Don't remove system message */
        if (remove_count >= state->message_count - 1) {
            remove_count = state->message_count - 2;
        }
        if (remove_count > 0) {
            for (size_t i = 0; i < remove_count; i++)
                message_free(state->messages[1 + i]);
            memmove(&state->messages[1], &state->messages[1 + remove_count],
                    (state->message_count - 1 - remove_count) * sizeof(message_t *));
            state->message_count -= remove_count;
        }
    } else {
        /* Remove from front */
        for (size_t i = 0; i < remove_count; i++)
            message_free(state->messages[i]);
        memmove(&state->messages[0], &state->messages[remove_count],
                (state->message_count - remove_count) * sizeof(message_t *));
        state->message_count -= remove_count;
    }
}

const message_t *context_get(const agent_state_t *state, size_t index) {
    if (!state || index >= state->message_count) return NULL;
    return state->messages[index];
}

/* ================================================================
 *  P90: Smart context eviction
 * ================================================================ */

/* Count tokens in a message */
int context_message_tokens(const message_t *msg) {
    if (!msg) return 0;
    int t = estimate_tokens(msg->content);
    t += estimate_tokens(msg->reasoning);
    /* Tool calls add tokens */
    for (int i = 0; i < msg->tool_calls_count && i < 64; i++) {
        t += estimate_tokens(msg->tool_calls[i].name);
        t += estimate_tokens(msg->tool_calls[i].arguments);
    }
    return t;
}

/* Estimate total tokens in context */
int context_total_tokens(const agent_state_t *state) {
    if (!state) return 0;
    int total = 0;
    for (size_t i = 0; i < state->message_count; i++)
        total += context_message_tokens(state->messages[i]);
    return total;
}

/* Smart eviction: remove messages to stay within max messages, preferring
 * to drop tool results before conversation turns. Always preserves system. */
void context_evict_smart(agent_state_t *state, size_t max_messages,
                          eviction_strategy_t strategy) {
    if (!state || state->message_count <= max_messages) return;

    size_t keep_system = (state->message_count > 0 &&
                          state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    size_t target = max_messages > keep_system ? max_messages : keep_system + 4;
    size_t remove_count = state->message_count - target;

    if (remove_count == 0) return;

    size_t start_idx = keep_system; /* where to start removing from */

    switch (strategy) {
    case EVICT_OLDEST_TOOL_FIRST: {
        /* Count non-tool messages */
        size_t non_tool = 0;
        for (size_t i = start_idx; i < state->message_count; i++)
            if (state->messages[i]->role != MSG_TOOL)
                non_tool++;

        /* If removing would delete all non-tool messages beyond N, keep more */
        size_t max_tool_remove = state->message_count - start_idx - non_tool;
        if (remove_count > max_tool_remove + (non_tool > 2 ? non_tool - 2 : 0))
            remove_count = max_tool_remove + (non_tool > 2 ? non_tool - 2 : 0);
        if (remove_count == 0) return;

        /* Remove oldest messages (prefer tool results) */
        for (size_t i = 0; i < remove_count; i++) {
            /* Find oldest non-system message */
            size_t oldest = start_idx;
            for (size_t j = start_idx + 1; j < state->message_count; j++) {
                /* Prefer tools for removal */
                if (state->messages[j]->role == MSG_TOOL &&
                    state->messages[oldest]->role != MSG_TOOL) {
                    oldest = j;
                } else if (j < oldest) {
                    oldest = j;
                }
            }
            message_free(state->messages[oldest]);
            memmove(&state->messages[oldest], &state->messages[oldest + 1],
                    (state->message_count - oldest - 1) * sizeof(message_t *));
            state->message_count--;
        }
        break;
    }
    case EVICT_OLDEST_USER: {
        /* Count user+assistant pairs */
        size_t pairs = 0;
        for (size_t i = start_idx; i < state->message_count; i++)
            if (state->messages[i]->role == MSG_USER || state->messages[i]->role == MSG_ASSISTANT)
                pairs++;
        /* Remove oldest user/assistant messages, keep the rest */
        size_t removed = 0;
        size_t i = start_idx;
        while (i < state->message_count && removed < remove_count) {
            if (state->messages[i]->role == MSG_USER ||
                state->messages[i]->role == MSG_ASSISTANT) {
                message_free(state->messages[i]);
                memmove(&state->messages[i], &state->messages[i + 1],
                        (state->message_count - i - 1) * sizeof(message_t *));
                state->message_count--;
                removed++;
            } else {
                i++;
            }
        }
        break;
    }
    case EVICT_KEEP_RECENT_N: {
        /* Simple: remove oldest, keep N most recent + system */
        context_truncate(state, target);
        break;
    }
    }
}

/* Get system prompt text (first MSG_SYSTEM content, or NULL) */
const char *context_get_system(const agent_state_t *state) {
    if (!state) return NULL;
    for (size_t i = 0; i < state->message_count; i++)
        if (state->messages[i]->role == MSG_SYSTEM)
            return state->messages[i]->content;
    return NULL;
}
