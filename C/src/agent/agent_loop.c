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
#include <dirent.h>
#include <unistd.h>
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
    /* P97: Initialize compression feedback tracker */
    compression_feedback_init(&state->compression_fb);
    /* P98: Initialize checkpoint manager */
    checkpoint_init(&state->checkpoints);
}

/* P99: Initialize agent infrastructure from configuration.
 * Called after agent_init() to apply config settings. */
void agent_configure_from_config(agent_state_t *state, const hermes_config_t *cfg) {
    if (!state || !cfg) return;

    /* Set LLM config from provider config */
    if (cfg->provider_cfg.model[0])
        snprintf(state->llm.model, sizeof(state->llm.model), "%s", cfg->provider_cfg.model);
    if (cfg->provider_cfg.provider[0])
        snprintf(state->llm.provider, sizeof(state->llm.provider), "%s", cfg->provider_cfg.provider);
    if (cfg->provider_cfg.base_url[0])
        snprintf(state->llm.base_url, sizeof(state->llm.base_url), "%s", cfg->provider_cfg.base_url);
    if (cfg->provider_cfg.api_key[0])
        snprintf(state->llm.api_key, sizeof(state->llm.api_key), "%s", cfg->provider_cfg.api_key);

    /* Max iterations from agent config */
    if (cfg->agent.max_iterations > 0)
        state->max_iterations = cfg->agent.max_iterations;

    /* Compress enabled */
    state->compress_enabled = cfg->compress_enabled;

    /* Budget tracker limits from config */
    if (state->budget) {
        budget_tracker_set_limits(state->budget,
            cfg->agent.max_output_tokens > 0 ? (long long)cfg->agent.max_output_tokens * 10 : 0,
            0,  /* output: no limit from config yet */
            0.0, /* cost: no limit from config yet */
            state->max_iterations > 0 ? state->max_iterations : 0);
    }
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
    /* P98: Free checkpoint manager */
    checkpoint_free(&state->checkpoints);
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
        /* Save session + metadata before closing */
        agent_save_session(state);
        agent_save_meta(state);
        db_close(state->db);
        state->db = NULL;
    }
}

/* ================================================================
 *  P141: Session metadata
 * ================================================================ */

/* Save session metadata (title, model, token counts, timestamps) */
bool agent_save_meta(agent_state_t *state) {
    if (!state->db) return false;

    session_meta_t meta;
    db_load_meta(state->db, state->session_id, &meta);

    /* Update fields from current state */
    snprintf(meta.title, sizeof(meta.title), "%s", state->session_id);
    if (state->llm.model[0])
        snprintf(meta.model, sizeof(meta.model), "%s", state->llm.model);
    meta.message_count = (int)state->message_count;
    meta.updated_at = time(NULL);
    if (meta.created_at == 0) meta.created_at = meta.updated_at;
    if (meta.schema_version == 0) meta.schema_version = 1;

    return db_save_meta(state->db, state->session_id, &meta);
}

/* Load session metadata into provided struct */
bool agent_load_meta(agent_state_t *state, session_meta_t *meta) {
    if (!state->db || !meta) return false;
    return db_load_meta(state->db, state->session_id, meta);
}

/* ================================================================
 *  P143: Session CRUD
 * ================================================================ */

/* Create a new session with generated ID and default metadata */
bool agent_session_create(agent_state_t *state, const char *session_id) {
    if (!state->db) return false;
    if (!session_id || !*session_id) {
        /* Generate new session ID */
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char new_id[64];
        snprintf(new_id, sizeof(new_id), "%04d%02d%02d_%02d%02d%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
        session_id = new_id;
    }

    /* Save empty session data */
    if (!db_save(state->db, session_id, "[]")) return false;

    /* Create and save default metadata */
    session_meta_t meta;
    db_meta_init(&meta);
    if (state->llm.model[0])
        snprintf(meta.model, sizeof(meta.model), "%s", state->llm.model);
    snprintf(meta.title, sizeof(meta.title), "%s", session_id);
    return db_save_meta(state->db, session_id, &meta);
}

/* List sessions with optional tag filtering. Returns malloc'd array, caller must free. */
session_entry_t *agent_session_list(size_t *count, const char *tag_filter, int limit) {
    if (!count) return NULL;
    *count = 0;

    /* Get sessions directory */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    char db_dir[4096];
    snprintf(db_dir, sizeof(db_dir), "%s/.hermes/sessions", home);

    db_t *db = db_open(db_dir, NULL);
    if (!db) return NULL;

    size_t total = 0;
    db_session_entry_t *entries = db_list_with_meta(db, &total);
    if (!entries) { db_close(db); return NULL; }

    /* Build result with optional tag filtering and limit */
    size_t result_idx = 0;
    size_t result_cap = total > 0 ? total : 1;
    session_entry_t *result = (session_entry_t *)calloc(result_cap, sizeof(session_entry_t));
    if (!result) {
        for (size_t i = 0; i < total; i++) free(entries[i].id);
        free(entries);
        db_close(db);
        return NULL;
    }

    for (size_t i = 0; i < total && result_idx < (size_t)limit; i++) {
        bool include = true;

        /* Apply tag filter */
        if (tag_filter && *tag_filter) {
            include = false;
            for (int t = 0; t < entries[i].meta.tag_count; t++) {
                if (strstr(entries[i].meta.tags[t], tag_filter) ||
                    strstr(tag_filter, entries[i].meta.tags[t])) {
                    include = true;
                    break;
                }
            }
        }

        if (include) {
            snprintf(result[result_idx].id, sizeof(result[result_idx].id), "%s",
                     entries[i].id);
            memcpy(&result[result_idx].meta, &entries[i].meta, sizeof(session_meta_t));
            result_idx++;
        }
    }

    *count = result_idx;
    for (size_t i = 0; i < total; i++) free(entries[i].id);
    free(entries);
    db_close(db);
    return result;
}

/* Delete a session by ID */
bool agent_session_delete(agent_state_t *state, const char *session_id) {
    if (!state->db || !session_id) return false;
    return db_delete(state->db, session_id);
}

/* ================================================================
 *  P144: Auto-save and crash recovery
 * ================================================================ */

/* Auto-save current session if interval turns have passed.
 * Returns true if save was performed. */
bool agent_auto_save(agent_state_t *state, int interval) {
    if (!state->db) return false;
    if (interval <= 0) return false; /* auto-save disabled */

    /* Use a static/state-based turn counter */
    static int auto_save_counter = 0;
    auto_save_counter++;

    if (auto_save_counter >= interval) {
        auto_save_counter = 0;
        agent_save_session(state);
        agent_save_meta(state);
        return true;
    }
    return false;
}

/* Crash recovery: check for .tmp files from interrupted saves.
 * Returns true if recovery was performed. */
bool agent_crash_recover(agent_state_t *state) {
    if (!state->db) return false;

    /* Determine sessions directory */
    char dir_path[4096];
    snprintf(dir_path, sizeof(dir_path), "%s/sessions",
             state->hermes_home[0] ? state->hermes_home :
             (getenv("HOME") ? getenv("HOME") : "/tmp"));

    /* The db_save function already uses atomic write (write to .tmp, rename to .json).
     * Check for orphaned .tmp files from interrupted writes. */
    DIR *dir = opendir(dir_path);
    if (!dir) return false;

    struct dirent *entry;
    bool recovered = false;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 4 && strcmp(name + len - 4, ".tmp") == 0) {
            /* Clean up orphaned .tmp files */
            char tmp_path[4096];
            snprintf(tmp_path, sizeof(tmp_path), "%s/%s", dir_path, name);
            unlink(tmp_path);
            recovered = true;
        }
    }
    closedir(dir);
    return recovered;
}

/* Add auto-save call after each LLM iteration in the main loop.
 * This is used internally by agent_run_conversation. */
static void agent_check_auto_save(agent_state_t *state) {
    if (!state->db) return;
    /* Default auto-save interval: save every 3 turns */
    agent_auto_save(state, 3);
}

/* ================================================================
 *  P145: Auto-prune
 * ================================================================ */

/* Remove sessions older than retention_days. Returns number removed. */
int agent_auto_prune(agent_state_t *state, int retention_days) {
    if (!state->db || retention_days <= 0) return 0;
    return db_prune_by_age(state->db, retention_days);
}

/* ================================================================
 *  P146: Session tags
 * ================================================================ */

/* Add a tag to the current session's metadata */
bool agent_session_add_tag(agent_state_t *state, const char *tag) {
    if (!state->db || !tag || !*tag) return false;

    session_meta_t meta;
    db_load_meta(state->db, state->session_id, &meta);

    /* Check if tag already exists */
    for (int i = 0; i < meta.tag_count; i++) {
        if (strcmp(meta.tags[i], tag) == 0)
            return true; /* already present */
    }

    /* Add tag */
    if (meta.tag_count < 32) {
        snprintf(meta.tags[meta.tag_count], sizeof(meta.tags[0]), "%s", tag);
        meta.tag_count++;
        meta.updated_at = time(NULL);
        return db_save_meta(state->db, state->session_id, &meta);
    }
    return false;
}

/* Remove a tag from the current session's metadata */
bool agent_session_remove_tag(agent_state_t *state, const char *tag) {
    if (!state->db || !tag || !*tag) return false;

    session_meta_t meta;
    if (!db_load_meta(state->db, state->session_id, &meta))
        return false;

    int found = -1;
    for (int i = 0; i < meta.tag_count; i++) {
        if (strcmp(meta.tags[i], tag) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) return false;

    /* Shift remaining tags */
    for (int i = found; i < meta.tag_count - 1; i++)
        snprintf(meta.tags[i], sizeof(meta.tags[0]), "%s", meta.tags[i + 1]);
    meta.tag_count--;
    meta.updated_at = time(NULL);
    return db_save_meta(state->db, state->session_id, &meta);
}

/* ================================================================
 *  P148: Session export
 * ================================================================ */

/* Export session as JSON. Caller must free. */
char *agent_session_export_json(agent_state_t *state, const char *session_id) {
    if (!state->db || !session_id) return NULL;
    return db_export_json(state->db, session_id);
}

/* Export session as Markdown. Caller must free. */
char *agent_session_export_markdown(agent_state_t *state, const char *session_id) {
    if (!state->db || !session_id) return NULL;
    return db_export_markdown(state->db, session_id);
}

/* ================================================================
 *  P149: Session branch
 * ================================================================ */

/* Branch session at message index N. Creates new session with shared prefix. */
bool agent_session_branch(agent_state_t *state, const char *new_id, int branch_point) {
    if (!state->db || !new_id || branch_point < 0) return false;

    /* Generate new session ID if not provided */
    char generated_id[64];
    if (!*new_id) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        snprintf(generated_id, sizeof(generated_id), "%04d%02d%02d_%02d%02d%02d_br",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
        new_id = generated_id;
    }

    char *branched_data = db_branch(state->db, state->session_id, new_id, branch_point);
    if (!branched_data) return false;
    free(branched_data);
    return true;
}

/* ================================================================
 *  P150: Session migration
 * ================================================================ */

/* Migrate all sessions to latest schema. Returns number migrated. */
int agent_session_migrate(agent_state_t *state) {
    if (!state->db) return 0;
    return db_migrate(state->db);
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

/* P93: Tool result classification */
typedef enum {
    TOOL_RESULT_SUCCESS = 0,
    TOOL_RESULT_ERROR,   /* Non-fatal error — loop continues */
    TOOL_RESULT_FATAL,   /* Fatal error — abort loop */
} tool_result_class_t;

/* Classify a tool result string. Returns classification level. */
static tool_result_class_t classify_tool_result(const char *result, const char *tool_name) {
    (void)tool_name;
    if (!result || !*result) return TOOL_RESULT_ERROR;

    /* Check for error patterns in JSON result */
    if (strstr(result, "\"error\"")) {
        /* Check for fatal patterns */
        if (strstr(result, "fatal") || strstr(result, "FATAL") ||
            strstr(result, "not found") || strstr(result, "permission denied") ||
            strstr(result, "access denied") || strstr(result, "unauthorized"))
            return TOOL_RESULT_FATAL;
        return TOOL_RESULT_ERROR;
    }

    return TOOL_RESULT_SUCCESS;
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
            /* Auto-save session + metadata */
            if (state->db) {
                agent_save_session(state);
                agent_save_meta(state);
            }
            return result;
        }

        /* Has tool calls — create assistant message with tool_calls */
        /* Snapshot before tool execution for undo */
        agent_snapshot_take(state);
        /* P98: Try auto-save checkpoint */
        checkpoint_try_autosave(&state->checkpoints, state);
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
            /* P93: Classify tool result — abort on fatal */
            if (classify_tool_result(works[i].result, works[i].tool_name) == TOOL_RESULT_FATAL) {
                state->interrupted = true;
            }
            free(works[i].tool_name);
            free(works[i].tool_args);
            free(works[i].tool_id);
            free(works[i].result);
        }
        free(works);

        /* P144: Auto-save after each iteration with tool calls */
        agent_check_auto_save(state);

        llm_response_free(llm_resp);
        /* P93: Break on fatal tool result */
        if (!state->interrupted) {
            /* Loop back to LLM with tool results appended */
        }
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

/* Note: message_clone() is now in context.c (exported from hermes_agent.h) */

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
