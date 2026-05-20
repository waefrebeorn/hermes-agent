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

/* Forward declaration of tool handler (from registry in Phase 3) */
typedef struct {
    char *(*handler_fn)(const char *args_json, const char *task_id);
    json_node_t *schema;
} tool_handler_t;

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

/* Run tool and return result string */
static char *run_tool(agent_state_t *state, const char *tool_name,
                      const char *args_json)
{
    /* Search tool registry */
    for (size_t i = 0; i < state->tools.count; i++) {
        if (strcmp(state->tools.tools[i].name, tool_name) == 0) {
            if (state->tools.tools[i].handler) {
                return state->tools.tools[i].handler(args_json, state->session_id);
            }
        }
    }

    /* Tool not found */
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"error\": \"tool '%s' not found\", \"available_tools\": [",
             tool_name);
    size_t pos = strlen(buf);
    for (size_t i = 0; i < state->tools.count; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "%s\"%s\"", i > 0 ? ", " : "",
                        state->tools.tools[i].name);
    }
    snprintf(buf + pos, sizeof(buf) - pos, "]}");
    return strdup(buf);
}

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

        /* No content + no tool calls → final response */
        if (!llm_resp->content && llm_resp->tool_calls_count == 0) {
            char *result = strdup("Error: Empty LLM response");
            llm_response_free(llm_resp);
            json_free(tools_json);
            return result;
        }

        /* Add assistant message */
        message_t *assistant_msg = message_new_assistant(
            llm_resp->content,
            NULL,   /* No single tool — tool_calls JSON is in content */
            NULL,
            llm_resp->reasoning);
        context_push(state, assistant_msg);

        /* If no tool calls, we're done */
        if (llm_resp->tool_calls_count == 0) {
            char *result = llm_resp->content ? strdup(llm_resp->content) : strdup("");
            llm_response_free(llm_resp);
            json_free(tools_json);
            return result;
        }

        /* Parse tool calls from the LLM response content */
        /* The content includes tool_calls JSON embedded by some providers */
        /* For simplicity, we re-parse the LLM response JSON */
        /* Actually, the tool_calls come from the structured JSON response,
           but our simplified client puts them in the content as JSON.
           
           Real implementation: the llm_chat_completion should return
           extracted tool calls. For now, try to parse tool calls from
           the message content if it looks like tool call JSON. */

        /* Check if content contains tool calls (OpenAI format) */
        const char *content = llm_resp->content;
        if (content && strstr(content, "tool_calls") == NULL) {
            /* No tool calls in content either — final response */
            char *result = strdup(content ? content : "");
            llm_response_free(llm_resp);
            json_free(tools_json);
            return result;
        }

        /* Parse tool calls from content */
        /* This is a simplified parsing — real implementation would
           extract tool_calls from the structured response */
        /* For now: return the content as-is (it includes tool calls JSON) */
        char *result = strdup(content ? content : "");
        llm_response_free(llm_resp);
        json_free(tools_json);
        return result;

        /* TODO: Full tool call execution loop:
         * 1. Parse tool_calls array from response
         * 2. For each: call run_tool()
         * 3. Append tool result messages
         * 4. Loop back to LLM */
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
