/*
 * context.c — Message context management for Hermes C.
 * Manages the array of messages in agent_state_t.
 * Provides: push, pop, get, truncate, system prompt injection.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
