/*
 * agent_loop.c — Core agent conversation loop for Hermes C.
 *
 * The loop:
 * 1. Read next message from input
 * 2. Send to LLM (with tools if enabled)
 * 3. If LLM returns text → output and done
 * 4. If LLM returns tool_calls → execute each, append results, loop
 * 5. Repeat until max_iterations or final response
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Agent initialization
 * ================================================================ */

void agent_init(agent_state_t *state) {
    memset(state, 0, sizeof(*state));
    state->max_iterations = HERMES_MAX_TOOL_CALLS;
    context_init(state);

    /* Generate a session ID */
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    snprintf(state->session_id, sizeof(state->session_id),
             "%04d%02d%02d_%02d%02d%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void agent_free(agent_state_t *state) {
    context_clear(state);
    /* Tool registry cleanup will be in Phase 3 */
}

/* ================================================================
 *  Core loop
 * ================================================================ */

/* Convert tool registry to JSON for tool_choice */
static json_node_t *tools_to_json(tool_registry_t *reg) {
    json_node_t *tools = json_new_array();
    for (size_t i = 0; i < reg->count; i++) {
        if (!reg->tools[i].available) continue;

        json_node_t *tool = json_new_object();
        json_object_set(tool, "type", json_new_string("function"));

        json_node_t *fn = json_new_object();
        json_object_set(fn, "name", json_new_string(reg->tools[i].name));
        json_object_set(fn, "description", json_new_string(reg->tools[i].description));

        /* Parse schema */
        if (reg->tools[i].schema_json[0]) {
            char *err = NULL;
            json_node_t *schema = json_parse(reg->tools[i].schema_json, &err);
            if (schema) {
                json_object_set(fn, "parameters", schema);
            } else {
                json_object_set(fn, "parameters", json_new_object());
                free(err);
            }
        } else {
            json_object_set(fn, "parameters", json_new_object());
        }

        json_object_set(tool, "function", fn);
        json_array_append(tools, tool);
    }
    return tools;
}

/* Main conversation run. Returns LLM final response. */
char *agent_run_conversation(agent_state_t *state,
                              const char *user_message,
                              const char *system_message)
{
    /* Set system message if provided */
    if (system_message && system_message[0])
        context_set_system(state, system_message);

    /* Add user message */
    message_t *user_msg = message_new(MSG_USER, user_message);
    if (!user_msg) return strdup("Error: OOM");
    context_push(state, user_msg);

    /* Build tools JSON from registry */
    json_node_t *tools_json = NULL;
    if (state->tools.count > 0)
        tools_json = tools_to_json(&state->tools);

    int iteration = 0;

    while (iteration < state->max_iterations && !state->interrupted) {
        state->iteration_count = iteration;

        /* Call LLM */
        llm_response_t *llm_resp = llm_chat_completion(
            &state->llm,
            (const message_t **)state->messages,
            state->message_count,
            tools_json);

        if (!llm_resp) {
            json_free(tools_json);
            return strdup("Error: LLM call returned NULL");
        }

        /* No content + no tool calls → error */
        if (!llm_resp->content && llm_resp->tool_calls_count == 0) {
            char *result = strdup("Error: Empty LLM response");
            llm_response_free(llm_resp);
            json_free(tools_json);
            return result;
        }

        iteration++;

        /* If no tool calls, we're done — return final content */
        if (llm_resp->tool_calls_count == 0) {
            char *result = llm_resp->content ? strdup(llm_resp->content) : strdup("");
            llm_response_free(llm_resp);
            json_free(tools_json);
            return result;
        }

        /* Has tool calls — create assistant message with tool_calls */
        message_t *assistant_msg = message_new_assistant_with_toolcalls(
            llm_resp->content, llm_resp->tool_calls, llm_resp->tool_calls_count,
            llm_resp->reasoning);
        context_push(state, assistant_msg);

        /* Execute each tool call */
        for (int i = 0; i < llm_resp->tool_calls_count; i++) {
            char *result = registry_dispatch(
                llm_resp->tool_calls[i].name,
                llm_resp->tool_calls[i].arguments,
                state->session_id);

            /* Create tool result message */
            message_t *tool_msg = message_new_tool(
                llm_resp->tool_calls[i].id,
                result ? result : "{\"error\":\"Tool returned NULL\"}");
            context_push(state, tool_msg);
            free(result);
        }

        llm_response_free(llm_resp);
        /* Loop back to LLM with tool results appended */
    }

    json_free(tools_json);
    if (state->interrupted)
        return strdup("Error: Interrupted");
    return strdup("Error: Max iterations reached");
}

/* Simple chat interface */
char *agent_chat(agent_state_t *state, const char *message) {
    return agent_run_conversation(state, message, NULL);
}
