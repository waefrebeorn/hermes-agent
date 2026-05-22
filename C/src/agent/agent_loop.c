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
#include "provider.h"
#include "plugin.h"
#include "budget_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <signal.h>
#ifndef _WIN32
#include <pthread.h>
#endif

/* ================================================================
 *  Agent initialization
 * ================================================================ */

/* P89: Global state pointer for SIGINT handler */
#ifndef _WIN32
static agent_state_t *g_signal_state = NULL;

static void sigint_handler(int sig) {
    (void)sig;
    if (g_signal_state) {
        g_signal_state->interrupted = true;
        /* Print message to stderr so it interrupts cleanly */
        fprintf(stderr, "\n! Interrupted (SIGINT). Use /stop to force quit.\n");
    }
}
#endif

void agent_init(agent_state_t *state) {
    memset(state, 0, sizeof(*state));
    state->message_capacity = 64;
    state->messages = (message_t **)calloc(state->message_capacity, sizeof(message_t *));
    state->max_iterations = HERMES_MAX_TOOL_CALLS;
    state->snapshot_capacity = 0;  /* lazy init on first snapshot_take */
    /* P86: Create budget tracker */
    state->budget = (budget_tracker_t *)calloc(1, sizeof(budget_tracker_t));
    if (state->budget) budget_tracker_init(state->budget);
    /* P89: Register SIGINT handler */
#ifndef _WIN32
    g_signal_state = state;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
#endif
    context_init(state);
    agent_generate_session_id(state);
    /* Register built-in LLM providers */
    provider_register_builtins();
}

void agent_free(agent_state_t *state) {
    context_clear(state);
    /* Free plugin registry if loaded */
    if (state->plugin_reg) {
        plugin_registry_free((plugin_registry_t *)state->plugin_reg);
        state->plugin_reg = NULL;
    }
    /* Free budget tracker */
    free(state->budget);
    state->budget = NULL;
    /* P89: Unregister SIGINT handler */
#ifndef _WIN32
    if (g_signal_state == state) {
        g_signal_state = NULL;
        signal(SIGINT, SIG_DFL);
    }
#endif
}

void agent_generate_session_id(agent_state_t *state) {
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

/* ================================================================
 *  Core loop
 * ================================================================ */

/* P87: Tool dispatch thread wrapper (for pthread_create) */
struct tool_dispatch_arg {
    const char *session_id;
    char *tool_name;
    char *tool_args;
    char **result_out;
};

static void *tool_dispatch_thread(void *arg) {
    struct tool_dispatch_arg *a = (struct tool_dispatch_arg *)arg;
    *a->result_out = registry_dispatch(a->tool_name, a->tool_args, a->session_id);
    return NULL;
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

    /* P92: Inject prefill assistant message (before user message) */
    if (state->prefill[0]) {
        message_t *prefill_msg = message_new(MSG_ASSISTANT, state->prefill);
        if (prefill_msg) context_push(state, prefill_msg);
    }

    /* Add user message */
    message_t *user_msg = message_new(MSG_USER, user_message);
    if (!user_msg) return strdup("Error: OOM");
    context_push(state, user_msg);

    /* Build tools JSON from registry */
    json_node_t *tools_json = NULL;
    if (state->tools.count > 0)
        tools_json = tools_to_json(&state->tools);

    int iteration = 0;
    bool grace_call = false; /* P88: one extra LLM call without tools after budget */

    while (iteration < state->max_iterations && !state->interrupted &&
           !(iteration > 0 && grace_call)) {
        state->iteration_count = iteration;

        /* P86: Check budget exceeded — trigger grace call */
        if (state->budget && budget_tracker_is_exceeded(state->budget) && !grace_call) {
            grace_call = true;
            /* Remove tools for grace call so LLM just summarizes */
            if (tools_json) { json_free(tools_json); tools_json = NULL; }
        }

        /* Smart context compression: summarize old messages before dropping */
        char *summary = llm_compress_context(state, HERMES_MAX_CTX_TOKENS,
                                              state->compress_enabled);
        if (summary) {
            /* Insert summary as a user message and truncate */
            message_t *summary_msg = message_new(MSG_USER, summary);
            if (summary_msg) {
                /* Insert after system message */
                size_t idx = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
                /* Remove old messages up to last 2 */
                size_t keep = idx;
                size_t remove_count = state->message_count - keep - 2;
                for (size_t i = 0; i < remove_count; i++)
                    message_free(state->messages[keep + i]);
                memmove(&state->messages[keep], &state->messages[keep + remove_count],
                        (state->message_count - keep - remove_count) * sizeof(message_t *));
                state->message_count -= remove_count;

                /* Insert summary message */
                context_push(state, summary_msg);
            }
            free(summary);
        }

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

        /* P91: Mark system prompt as cached after first successful call */
        if (!state->llm.system_cached) {
            state->llm.system_cached = true;
        }

        iteration++;

        /* P86: Report turn to budget tracker */
        if (state->budget) {
            double cost = budget_tracker_estimate_cost(
                state->llm.model,
                llm_resp->input_tokens,
                llm_resp->output_tokens);
            budget_tracker_report_turn(state->budget,
                                        llm_resp->input_tokens,
                                        llm_resp->output_tokens,
                                        cost,
                                        state->llm.model);
        }

        /* If no tool calls, we're done — return final content */
        if (llm_resp->tool_calls_count == 0) {
            /* Take snapshot for undo before finalizing */
            agent_snapshot_take(state);
            char *result = llm_resp->content ? strdup(llm_resp->content) : strdup("");
            llm_response_free(llm_resp);
            json_free(tools_json);
            /* Auto-save session */
            if (state->db) agent_save_session(state);
            return result;
        }

        /* Has tool calls — create assistant message with tool_calls */
        /* Snapshot before tool execution for undo */
        agent_snapshot_take(state);
        message_t *assistant_msg = message_new_assistant_with_toolcalls(
            llm_resp->content, llm_resp->tool_calls, llm_resp->tool_calls_count,
            llm_resp->reasoning);
        context_push(state, assistant_msg);

        /* P87: Parallel tool dispatch — execute independent tools concurrently */
        int n_calls = llm_resp->tool_calls_count;
        typedef struct {
            int    index;
            char  *tool_name;
            char  *tool_args;
            char  *tool_id;
            int    approved;
            char  *result;  /* output */
        } tool_work_t;

        tool_work_t *works = (tool_work_t *)calloc((size_t)n_calls, sizeof(tool_work_t));
        if (!works) {
            llm_response_free(llm_resp);
            json_free(tools_json);
            return strdup("Error: OOM for tool dispatch");
        }

        /* Phase 1: Security approval (sequential — approval_check may be stateful) */
        for (int i = 0; i < n_calls; i++) {
            works[i].index = i;
            works[i].tool_name = strdup(llm_resp->tool_calls[i].name);
            works[i].tool_args = strdup(llm_resp->tool_calls[i].arguments);
            works[i].tool_id = strdup(llm_resp->tool_calls[i].id);
            works[i].approved = approval_check(
                llm_resp->tool_calls[i].name,
                llm_resp->tool_calls[i].arguments);
            works[i].result = NULL;
        }

        /* Phase 2: Parallel execution via pthreads */
#if defined(_WIN32) || defined(WIN32)
        /* No pthreads on Windows — fallback to sequential */
        for (int i = 0; i < n_calls; i++) {
            if (works[i].approved) {
                works[i].result = registry_dispatch(
                    works[i].tool_name, works[i].tool_args, state->session_id);
            } else {
                char denied[1024];
                snprintf(denied, sizeof(denied),
                    "{\"error\":\"Operation denied by security approval: "
                    "tool '%s' was blocked. Use /approve in interactive mode "
                    "to allow it.\"}",
                    works[i].tool_name);
                works[i].result = strdup(denied);
            }
        }
#else
        /* POSIX — parallel dispatch */
        pthread_t *threads = (pthread_t *)calloc((size_t)n_calls, sizeof(pthread_t));
        struct tool_dispatch_arg *args = (struct tool_dispatch_arg *)calloc(
            (size_t)n_calls, sizeof(struct tool_dispatch_arg));

        for (int i = 0; i < n_calls; i++) {
            if (works[i].approved) {
                args[i].session_id = state->session_id;
                args[i].tool_name = works[i].tool_name;
                args[i].tool_args = works[i].tool_args;
                args[i].result_out = &works[i].result;
                pthread_create(&threads[i], NULL, tool_dispatch_thread, &args[i]);
            } else {
                char denied[1024];
                snprintf(denied, sizeof(denied),
                    "{\"error\":\"Operation denied by security approval: "
                    "tool '%s' was blocked.\"}",
                    works[i].tool_name);
                works[i].result = strdup(denied);
            }
        }

        /* Join all threads */
        for (int i = 0; i < n_calls; i++) {
            if (works[i].approved) {
                pthread_join(threads[i], NULL);
            }
        }

        free(threads);
        free(args);
#endif

        /* Phase 3: Create tool result messages (in original order) */
        for (int i = 0; i < n_calls; i++) {
            message_t *tool_msg = message_new_tool(
                works[i].tool_id,
                works[i].result ? works[i].result :
                    "{\"error\":\"Tool returned NULL\"}");
            context_push(state, tool_msg);
            free(works[i].tool_name);
            free(works[i].tool_args);
            free(works[i].tool_id);
            free(works[i].result);
        }
        free(works);

        llm_response_free(llm_resp);
        /* Loop back to LLM with tool results appended */
    }

    json_free(tools_json);
    if (state->interrupted)
        return strdup("Error: Interrupted");
    if (state->budget && budget_tracker_is_exceeded(state->budget))
        return strdup("Error: Budget exceeded (token/cost/turn limit reached)");
    return strdup("Error: Max iterations reached");
}

/* ================================================================
 *  P28: Undo snapshot
 * ================================================================ */

static message_t *message_clone(const message_t *src) {
    message_t *dst = (message_t *)calloc(1, sizeof(message_t));
    if (!dst) return NULL;
    dst->role = src->role;
    if (src->content) dst->content = strdup(src->content);
    if (src->tool_call_id) dst->tool_call_id = strdup(src->tool_call_id);
    if (src->tool_name) dst->tool_name = strdup(src->tool_name);
    if (src->reasoning) dst->reasoning = strdup(src->reasoning);
    dst->tool_calls_count = src->tool_calls_count;
    for (int i = 0; i < src->tool_calls_count && i < 64; i++) {
        snprintf(dst->tool_calls[i].id, sizeof(dst->tool_calls[i].id), "%s", src->tool_calls[i].id);
        snprintf(dst->tool_calls[i].name, sizeof(dst->tool_calls[i].name), "%s", src->tool_calls[i].name);
        snprintf(dst->tool_calls[i].arguments, sizeof(dst->tool_calls[i].arguments), "%s", src->tool_calls[i].arguments);
    }
    return dst;
}

void agent_snapshot_take(agent_state_t *state) {
    /* Free old snapshot if any */
    if (state->snapshot) {
        for (size_t i = 0; i < state->snapshot_count; i++)
            message_free(state->snapshot[i]);
        free(state->snapshot);
        state->snapshot = NULL;
        state->snapshot_count = 0;
    }

    /* Allocate snapshot array */
    state->snapshot_capacity = state->message_count + 16;
    state->snapshot = (message_t **)calloc(state->snapshot_capacity, sizeof(message_t *));
    if (!state->snapshot) return;

    /* Clone all messages */
    for (size_t i = 0; i < state->message_count; i++) {
        state->snapshot[i] = message_clone(state->messages[i]);
        if (state->snapshot[i])
            state->snapshot_count++;
    }

    snprintf(state->snapshot_id, sizeof(state->snapshot_id), "%s", state->session_id);
}

bool agent_snapshot_restore(agent_state_t *state) {
    if (!state->snapshot || state->snapshot_count == 0) return false;

    /* Free current messages */
    context_clear(state);

    /* Allocate space */
    if (state->message_capacity < state->snapshot_count + 16) {
        state->message_capacity = state->snapshot_count + 16;
        message_t **new_msgs = (message_t **)realloc(state->messages,
                                                      state->message_capacity * sizeof(message_t *));
        if (!new_msgs) return false;
        state->messages = new_msgs;
    }

    /* Clone snapshot back */
    for (size_t i = 0; i < state->snapshot_count; i++) {
        if (state->snapshot[i]) {
            state->messages[i] = message_clone(state->snapshot[i]);
            state->message_count++;
        }
    }

    /* Restore session ID from snapshot */
    if (state->snapshot_id[0])
        snprintf(state->session_id, sizeof(state->session_id), "%s", state->snapshot_id);

    return true;
}

/* Simple chat interface */
char *agent_chat(agent_state_t *state, const char *message) {
    return agent_run_conversation(state, message, NULL);
}
