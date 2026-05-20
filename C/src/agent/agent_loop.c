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
#include <sys/stat.h>

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

/* Open session database. Returns true on success. */
bool agent_open_db(agent_state_t *state) {
    if (state->db) return true;
    char db_dir[4096];
    snprintf(db_dir, sizeof(db_dir), "%s/sessions", state->hermes_home[0] ?
             state->hermes_home : (getenv("HOME") ? getenv("HOME") : "/tmp"));
    /* Ensure directory exists */
    mkdir(db_dir, 0755);
    state->db = db_open(db_dir, NULL);
    return state->db != NULL;
}

/* Serialize all messages to JSON for DB storage */
static char *serialize_messages(const agent_state_t *state) {
    json_node_t *arr = json_new_array();
    for (size_t i = 0; i < state->message_count; i++) {
        json_node_t *msg = json_new_object();
        const char *role_str;
        switch (state->messages[i]->role) {
            case MSG_SYSTEM:    role_str = "system";    break;
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool";      break;
            default:            role_str = "user";      break;
        }
        json_object_set(msg, "role", json_new_string(role_str));
        if (state->messages[i]->content)
            json_object_set(msg, "content", json_new_string(state->messages[i]->content));
        json_array_append(arr, msg);
    }
    char *json = json_serialize(arr);
    json_free(arr);
    return json;
}

/* Deserialize messages from JSON into state */
static bool deserialize_messages(agent_state_t *state, const char *json_str) {
    if (!json_str || !*json_str) return false;
    char *err = NULL;
    json_node_t *arr = json_parse(json_str, &err);
    if (!arr || arr->type != JSON_ARRAY) { free(err); json_free(arr); return false; }
    free(err);

    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_node_t *item = json_get(arr, i);
        const char *role = json_get_str(item, "role", "user");
        const char *content = json_get_str(item, "content", "");

        message_role_t r = MSG_USER;
        if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
        else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
        else if (strcmp(role, "tool") == 0) r = MSG_TOOL;

        message_t *msg = message_new(r, content);
        if (msg) context_push(state, msg);
    }
    json_free(arr);
    return state->message_count > 0;
}

/* Save current session to database */
bool agent_save_session(agent_state_t *state) {
    if (!state->db) return false;
    char *json = serialize_messages(state);
    if (!json) return false;
    bool ok = db_save(state->db, state->session_id, json);
    free(json);
    return ok;
}

/* Load a session from database into state */
bool agent_load_session(agent_state_t *state, const char *session_id) {
    if (!state->db) return false;
    context_clear(state);
    if (session_id && *session_id)
        snprintf(state->session_id, sizeof(state->session_id), "%s", session_id);

    char *json = db_load(state->db, state->session_id, NULL);
    if (!json) return false;
    bool ok = deserialize_messages(state, json);
    free(json);
    return ok;
}

/* Close session database */
void agent_close_db(agent_state_t *state) {
    if (state->db) {
        agent_save_session(state);
        db_close(state->db);
        state->db = NULL;
    }
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

        /* Truncate context if too long (128K token budget) */
        llm_truncate_context(state, HERMES_MAX_CTX_TOKENS);

        /* Call LLM — use streaming if callback set */
        llm_response_t *llm_resp;
        if (state->stream_cb) {
            llm_resp = llm_chat_completion_stream(
                &state->llm,
                (const message_t **)state->messages,
                state->message_count,
                tools_json,
                state->stream_cb,
                state->stream_data);
        } else {
            llm_resp = llm_chat_completion(
                &state->llm,
                (const message_t **)state->messages,
                state->message_count,
                tools_json);
        }

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
            /* Auto-save session */
            if (state->db) agent_save_session(state);
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
