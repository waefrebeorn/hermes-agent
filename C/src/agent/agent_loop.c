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
#include "hermes_agent.h"
#include "provider.h"
#include "plugin.h"
#include "hermes_system_prompt.h"
#include "budget_tracker.h"
#include "hermes_subdir_hints.h"
#include "hermes_onboarding.h"
#include "hermes_logger.h"
#include "hermes_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <strings.h>
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
        /* G35: Force interrupt via SIGINT */
        g_signal_state->interrupt_type = INTERRUPT_FORCE;
        /* Print message to stderr so it interrupts cleanly */
        fprintf(stderr, "\n! Interrupted (SIGINT). Use /stop to force quit.\n");
    }
}
#endif

void agent_init(agent_state_t *state) {
    memset(state, 0, sizeof(*state));
    hermes_log_init();
    hermes_log(LOG_INFO, "agent_loop", "Agent initialized");
    state->compress_tail_messages = 2;  /* P99b: default tail keep */
    state->compress_cooldown_secs = 30;         /* L02: default cooldown */
    state->compress_failure_cooldown_secs = 600; /* L02: default failure cooldown */
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

    /* G21: Default compression strategy */
    state->compression_strategy = COMPRESS_OLDEST_TOOL_FIRST;
    /* G31: Default prefill as assistant message */
    state->prefill_role = MSG_ASSISTANT;
    /* G35: No interrupt */
    state->interrupt_type = INTERRUPT_NONE;
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

    /* Forward LLM request params from config */
    state->llm.max_tokens = cfg->provider_cfg.max_tokens;
    state->llm.temperature = cfg->provider_cfg.temperature;
    state->llm.top_p = cfg->provider_cfg.top_p;
    state->llm.stop_count = cfg->provider_cfg.stop_count;
    memcpy(state->llm.stop_sequences, cfg->provider_cfg.stop_sequences,
           sizeof(state->llm.stop_sequences));
    state->llm.presence_penalty = cfg->provider_cfg.presence_penalty;
    state->llm.frequency_penalty = cfg->provider_cfg.frequency_penalty;
    state->llm.seed = cfg->provider_cfg.seed;
    state->llm.logprobs = cfg->provider_cfg.logprobs;
    state->llm.top_logprobs = cfg->provider_cfg.top_logprobs;
    memcpy(state->llm.user, cfg->provider_cfg.user, sizeof(state->llm.user));
    memcpy(state->llm.service_tier, cfg->provider_cfg.service_tier,
           sizeof(state->llm.service_tier));
    memcpy(state->llm.reasoning_effort, cfg->provider_cfg.reasoning_effort,
           sizeof(state->llm.reasoning_effort));
    memcpy(state->llm.response_format, cfg->provider_cfg.response_format,
           sizeof(state->llm.response_format));
    memcpy(state->llm.metadata, cfg->provider_cfg.metadata,
           sizeof(state->llm.metadata));
    memcpy(state->llm.tool_choice, cfg->provider_cfg.tool_choice,
           sizeof(state->llm.tool_choice));
    state->llm.parallel_tool_calls = cfg->provider_cfg.parallel_tool_calls;
    state->llm.max_tool_calls = cfg->provider_cfg.max_tool_calls;
    state->llm.n = cfg->provider_cfg.n;
    state->llm.top_k = cfg->provider_cfg.top_k;
    state->llm.candidate_count = cfg->provider_cfg.candidate_count;
    state->llm.json_mode = cfg->provider_cfg.json_mode;
    state->llm.response_format_strict = cfg->provider_cfg.response_format_strict;
    memcpy(state->llm.safety_settings, cfg->provider_cfg.safety_settings,
           sizeof(state->llm.safety_settings));
    memcpy(state->llm.extra_body, cfg->provider_cfg.extra_body,
           sizeof(state->llm.extra_body));

    /* Phase 113: Forward retry and fallback config */
    state->llm.max_retries = cfg->agent.api_max_retries;
    memcpy(state->llm.fallback_model, cfg->provider_cfg.fallback_model,
           sizeof(state->llm.fallback_model));
    memcpy(state->llm.fallback_providers, cfg->provider_cfg.fallback_providers,
           sizeof(state->llm.fallback_providers));

    /* Max iterations from agent config */
    if (cfg->agent.max_iterations > 0)
        state->max_iterations = cfg->agent.max_iterations;

    /* Compress enabled */
    state->compress_enabled = cfg->compress_enabled;
    /* P99b: Tail messages from config (0 = use state default) */
    if (cfg->agent.compress_tail_messages >= 2)
        state->compress_tail_messages = cfg->agent.compress_tail_messages;

    /* L02: Compression cooldowns from compression config */
    state->compress_cooldown_secs = cfg->compression.cooldown_secs > 0
        ? cfg->compression.cooldown_secs : 30;
    state->compress_failure_cooldown_secs = cfg->compression.failure_cooldown_secs > 0
        ? cfg->compression.failure_cooldown_secs : 600;

    /* P150: Forward enabled/disabled toolsets */
    if (cfg->tools.enabled_toolsets[0])
        snprintf(state->enabled_toolsets, sizeof(state->enabled_toolsets), "%s", cfg->tools.enabled_toolsets);
    if (cfg->tools.disabled_toolsets[0])
        snprintf(state->disabled_toolsets, sizeof(state->disabled_toolsets), "%s", cfg->tools.disabled_toolsets);

    /* G21: Compression strategy from config */
    if (strcasecmp(cfg->compression.strategy, "oldest_tool_first") == 0)
        state->compression_strategy = EVICT_OLDEST_TOOL_FIRST;
    else if (strcasecmp(cfg->compression.strategy, "oldest_user") == 0)
        state->compression_strategy = EVICT_OLDEST_USER;
    else if (strcasecmp(cfg->compression.strategy, "keep_recent_n") == 0)
        state->compression_strategy = EVICT_KEEP_RECENT_N;

    /* G23: Preserve attachment metadata during compression */
    state->preserve_attachments = cfg->compression.preserve_system; /* reuse preserve_system flag */

    /* G27: Wire checkpoint auto-save interval from config */
    checkpoint_set_limits(&state->checkpoints,
        cfg->checkpoints.max_checkpoints > 0 ? cfg->checkpoints.max_checkpoints : 10,
        cfg->checkpoints.interval > 0 ? cfg->checkpoints.interval : 5);

    /* G24: Per-turn tool call limit from guardrails config or agent config */
    int per_turn = cfg->guardrails.max_tool_calls_per_turn;
    if (per_turn <= 0) per_turn = cfg->agent.max_tool_calls_round;
    if (state->budget)
        budget_tracker_set_per_turn_limit(state->budget, per_turn);

    /* G28-G30: Initialize tool call loop guardrails from config */
    tool_guardrail_init(&state->guardrails_ctrl);
    if (cfg->guardrails.max_consecutive_failures > 0) {
        state->guardrails_ctrl.exact_failure_block_after = cfg->guardrails.max_consecutive_failures;
        state->guardrails_ctrl.same_tool_failure_halt_after = cfg->guardrails.max_consecutive_failures + 3;
    }
    state->guardrails_ctrl.hard_stop_enabled = cfg->guardrails.abort_on_safety_violation;

    /* G26: Budget hard limit mode */
    if (state->budget)
        budget_tracker_set_hard_limit(state->budget, false); /* soft by default */

    /* G20: Derive model family from model name */
    if (cfg->provider_cfg.model[0]) {
        const char *m = cfg->provider_cfg.model;
        if (strstr(m, "claude"))      snprintf(state->model_family, sizeof(state->model_family), "claude");
        else if (strstr(m, "gpt"))    snprintf(state->model_family, sizeof(state->model_family), "gpt");
        else if (strstr(m, "gemini")) snprintf(state->model_family, sizeof(state->model_family), "gemini");
        else if (strstr(m, "deepseek")) snprintf(state->model_family, sizeof(state->model_family), "deepseek");
        else if (strstr(m, "grok"))   snprintf(state->model_family, sizeof(state->model_family), "grok");
        else                          snprintf(state->model_family, sizeof(state->model_family), "unknown");
    }

    /* Budget tracker limits from config */
    if (state->budget) {
        budget_tracker_set_limits(state->budget,
            cfg->agent.max_output_tokens > 0 ? (long long)cfg->agent.max_output_tokens * 10 : 0,
            0,  /* output: no limit from config yet */
            0.0, /* cost: no limit from config yet */
            state->max_iterations > 0 ? state->max_iterations : 0);
    }

    /* B07: Wire shell hooks from config hooks_json */
    if (cfg->hooks_json[0]) {
        json_t *hooks_root = json_parse(cfg->hooks_json, NULL);
        if (hooks_root) {
            shell_hooks_parse_json(hooks_root);
            shell_hooks_register_all();
            json_free(hooks_root);
        }
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
    /* Free message pointer array */
    free(state->messages);
    state->messages = NULL;
    state->message_capacity = 0;
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
    char db_dir[HERMES_PATH_MAX + 64];
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
    if (state->user_title[0])
        snprintf(meta.title, sizeof(meta.title), "%s", state->user_title);
    else
        snprintf(meta.title, sizeof(meta.title), "%s", state->session_id);
    if (state->llm.model[0])
        snprintf(meta.model, sizeof(meta.model), "%s", state->llm.model);
    meta.message_count = (int)state->message_count;
    /* Populate token tracking from agent state */
    meta.token_count = state->session_total_tokens;
    meta.input_tokens = state->session_input_tokens;
    meta.output_tokens = state->session_output_tokens;
    meta.cache_read_tokens = state->session_cache_read_tokens;
    meta.cache_write_tokens = state->session_cache_write_tokens;
    /* P141b: Save reasoning tokens and estimated cost */
    meta.reasoning_tokens = state->session_reasoning_tokens;
    meta.estimated_cost = state->session_estimated_cost_usd;
    meta.tool_call_count = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i])
            meta.tool_call_count += state->messages[i]->tool_calls_count;
    }
    meta.schema_version = SESSION_SCHEMA_VERSION;
    meta.updated_at = time(NULL);
    if (meta.created_at == 0) meta.created_at = meta.updated_at;

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
    char new_id[64];
    if (!session_id || !*session_id) {
        /* Generate new session ID */
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
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
    char dir_path[HERMES_PATH_MAX + 64];
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
            char tmp_path[HERMES_PATH_MAX * 2];
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

        /* Parse schema — ensure valid JSON Schema even when none provided */
        if (reg->tools[i].schema_json[0]) {
            char *err = NULL;
            json_node_t *schema = json_parse(reg->tools[i].schema_json, &err);
            if (schema) {
                /* Ensure schema has "type": "object" — required by strict providers */
                json_object_set(schema, "type", json_new_string("object"));
                json_object_set(fn, "parameters", schema);
            } else {
                json_node_t *params = json_new_object();
                json_object_set(params, "type", json_new_string("object"));
                json_object_set(params, "properties", json_new_object());
                json_object_set(fn, "parameters", params);
                free(err);
            }
        } else {
            json_node_t *params = json_new_object();
            json_object_set(params, "type", json_new_string("object"));
            json_object_set(params, "properties", json_new_object());
            json_object_set(fn, "parameters", params);
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

    hermes_log(LOG_INFO, "agent_loop", "Starting conversation (user msg: %.80s)", user_message ? user_message : "(null)");

    /* P92: Inject prefill message (before user message) */
    if (state->prefill[0]) {
        message_t *prefill_msg = message_new(state->prefill_role, state->prefill);
        if (prefill_msg) context_push(state, prefill_msg);
    }

    /* Onboarding: check for OpenClaw residue on first conversation */
    {
        char *opath = onboarding_default_path();
        if (opath && !onboarding_is_seen(opath, ONBOARDING_OPENCLAW_RESIDUE_FLAG)) {
            if (onboarding_detect_openclaw_residue()) {
                printf("%s\n", onboarding_openclaw_residue_hint_cli());
                onboarding_mark_seen(opath, ONBOARDING_OPENCLAW_RESIDUE_FLAG);
            }
        }
        free(opath);
    }

    /* Initialize subdirectory hint tracker */
    subdir_hints_init(NULL);

    /* Add user message */
    message_t *user_msg = message_new(MSG_USER, user_message);
    if (!user_msg) return strdup("Error: OOM");
    context_push(state, user_msg);
    /* G09: Count this as a user turn */
    state->user_turn_count++;

    /* G10: Set last activity on user message */
    state->last_activity_ts = time(NULL);

    /* G33-G34: Process steer queue — inject all queued steers in priority order */
    if (state->steer_count > 0) {
        for (int si = 0; si < state->steer_count && si < HERMES_MAX_STEERS; si++) {
            if (state->steer_queue[si][0]) {
                message_role_t role = state->steer_roles[si];
                /* Map invalid/unknown roles to system message (default steer type) */
                if (role != MSG_SYSTEM && role != MSG_USER && role != MSG_ASSISTANT)
                    role = MSG_SYSTEM;
                message_t *steer_msg = message_new(role, state->steer_queue[si]);
                if (steer_msg) context_push(state, steer_msg);
            }
        }
        /* Clear queue after processing */
        for (int si = 0; si < state->steer_count && si < HERMES_MAX_STEERS; si++)
            state->steer_queue[si][0] = '\0';
        state->steer_count = 0;
    }
    /* Backward compat: process single pending_steer if queue is empty */
    else if (state->pending_steer[0]) {
        message_t *steer_msg = message_new(MSG_SYSTEM, state->pending_steer);
        if (steer_msg) context_push(state, steer_msg);
        state->pending_steer[0] = '\0';
    }

    /* Build tools JSON from registry */
    json_node_t *tools_json = NULL;
    if (state->tools.count > 0) {
        /* P150: Apply enabled/disabled toolsets filter before building tool list */
        if (state->enabled_toolsets[0] || state->disabled_toolsets[0]) {
            tool_registry_t filtered = state->tools;
            registry_filter_by_toolset(&filtered,
                state->enabled_toolsets[0] ? state->enabled_toolsets : NULL,
                state->disabled_toolsets[0] ? state->disabled_toolsets : NULL);
            tools_json = tools_to_json(&filtered);
        } else {
            tools_json = tools_to_json(&state->tools);
        }
    }

    /* P150: Apply system_message override if set */
    /* Load context files (SOUL.md, .hermes.md, AGENTS.md, etc.) and prepend */
    if (state->system_message[0]) {
        /* Try to load context files from cwd, skip SOUL.md if already in identity */
        const char *cwd = getenv("PWD");
        if (!cwd || !cwd[0]) cwd = ".";
        char *context_block = context_build_files_prompt(cwd, true);
        if (context_block && context_block[0]) {
            /* Prepend context block to system message */
            size_t ctx_len = strlen(context_block);
            size_t sys_len = strlen(state->system_message);
            char *combined = (char *)malloc(ctx_len + 3 + sys_len + 1);
            if (combined) {
                memcpy(combined, context_block, ctx_len);
                combined[ctx_len] = '\n';
                combined[ctx_len + 1] = '\n';
                memcpy(combined + ctx_len + 2, state->system_message, sys_len + 1);
                context_set_system(state, combined);
                free(combined);
            } else {
                context_set_system(state, state->system_message);
            }
            free(context_block);
        } else {
            context_set_system(state, state->system_message);
            free(context_block);
        }
    }

    /* G13-G14: Apply runtime tool_choice/parallel_tool_calls overrides */
    if (state->tool_choice[0])
        snprintf(state->llm.tool_choice, sizeof(state->llm.tool_choice), "%s", state->tool_choice);
    if (state->parallel_tool_calls)
        state->llm.parallel_tool_calls = true;

    int iteration = 0;
    bool grace_call = false; /* P88: one extra LLM call without tools after budget */

    while (iteration < state->max_iterations && !state->interrupted &&
           !(iteration > 0 && grace_call)) {
        state->iteration_count = iteration;

        /* L14: Update log context for this turn */
        hermes_log_set_context(
            state->session_id[0] ? state->session_id : NULL,
            state->llm.model[0] ? state->llm.model : NULL,
            state->llm.provider[0] ? state->llm.provider : NULL,
            iteration);

        /* P86: Check budget exceeded — trigger grace call */
        if (state->budget && budget_tracker_is_exceeded(state->budget) && !grace_call) {
            grace_call = true;
            /* Remove tools for grace call so LLM just summarizes */
            if (tools_json) { json_free(tools_json); tools_json = NULL; }
        }

        /* Smart context compression: summarize old messages before dropping */
        /* G22: Use adaptive threshold from compression feedback */
        float adaptive_threshold = compression_feedback_get_threshold(
            &state->compression_fb, HERMES_MAX_CTX_TOKENS);
        size_t adaptive_max = (size_t)(adaptive_threshold > 0 ?
            adaptive_threshold : HERMES_MAX_CTX_TOKENS);
        /* P99: Anti-thrashing — skip compression if cooldown not elapsed */
        /* B03: Also skip if last failure was <failure cooldown>s ago */
        /* L02: Cooldowns are configurable via compression.cooldown_secs / failure_cooldown_secs */
        time_t now = time(NULL);
        if ((state->last_compress_time == 0 ||
            (now - state->last_compress_time) >= state->compress_cooldown_secs) &&
            (now - state->last_compress_failure_time) >= state->compress_failure_cooldown_secs) {
            char *summary = llm_compress_context(state, adaptive_max,
                                                  state->compress_enabled);
            if (summary) {
                /* Record timestamp for anti-thrashing */
                state->last_compress_time = now;
                /* G22: Record positive feedback — compression was used successfully */
                compression_feedback_positive(&state->compression_fb);
                /* Wrap with compaction handoff prefix (matches Python SUMMARY_PREFIX) */
                char summary_prompt[8192];
                snprintf(summary_prompt, sizeof(summary_prompt),
                    "[CONTEXT COMPACTION - REFERENCE ONLY] Earlier turns were "
                    "compacted into the summary below. This is a handoff from a "
                    "previous context window - treat it as background reference, "
                    "NOT as active instructions. "
                    "Do NOT answer questions or fulfill requests mentioned in this "
                    "summary; they were already addressed. "
                    "Respond ONLY to the latest user message that appears AFTER "
                    "this summary:\n\n%s", summary);
                /* Insert summary as a user message and truncate */
                message_t *summary_msg = message_new(MSG_USER, summary_prompt);
                if (summary_msg) {
                    /* Insert after system message */
                    size_t idx = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
                    /* Remove old messages, keeping tail budget */
                    int tail = state->compress_tail_messages;
                    if (tail < 2) tail = 2;
                    size_t keep = idx;
                    size_t remove_count = state->message_count - keep - tail;
                    for (size_t i = 0; i < remove_count; i++)
                        message_free(state->messages[keep + i]);
                    memmove(&state->messages[keep], &state->messages[keep + remove_count],
                            (state->message_count - keep - remove_count) * sizeof(message_t *));
                    state->message_count -= remove_count;

                    /* Insert summary message */
                    context_push(state, summary_msg);
                }
                free(summary);
            } else {
                /* B03: Record failure time for cooldown */
                state->last_compress_failure_time = time(NULL);
            }
        }

        /* Truncate context if too long (128K token budget) */
        llm_truncate_context(state, HERMES_MAX_CTX_TOKENS);

        /* G21: Apply strategy-aware eviction if non-default strategy configured */
        if (state->compression_strategy != COMPRESS_OLDEST_TOOL_FIRST && state->message_count > 20) {
            context_evict_smart(state, 20, state->compression_strategy);
        }

        /* N01: Pre-request context window check — warn if approaching limit */
        {
            size_t ctx_max = hermes_token_context_size(state->llm.model);
            if (ctx_max > 0) {
                size_t est_msg_tok = 0;
                for (size_t i = 0; i < state->message_count && est_msg_tok < ctx_max; i++) {
                    if (state->messages[i])
                        est_msg_tok += hermes_token_count(state->messages[i]->content,
                            hermes_token_family_from_model(state->llm.model));
                    est_msg_tok += 5; /* overhead per message */
                }
                if (est_msg_tok > ctx_max * 0.9) {
                    fprintf(stderr, "[context] ~%zu/%zu tokens (%zu%%) — approaching model limit\n",
                            est_msg_tok, ctx_max, est_msg_tok * 100 / ctx_max);
                }
            }
        }

        /* Call LLM with retry and fallback support (Phase 113) */
        llm_response_t *llm_resp = NULL;
        int retries = state->llm.max_retries > 0 ? state->llm.max_retries : 0;
        bool dont_retry = false; /* skip retries for certain errors */

        /* P186: Invoke pre_llm_call hook */
        {
            char payload[2048];
            snprintf(payload, sizeof(payload),
                "{\"event\":\"pre_llm_call\",\"model\":\"%s\",\"provider\":\"%s\","
                "\"iteration\":%d,\"messages\":%zu,\"tools\":%d}",
                state->llm.model, state->llm.provider,
                iteration, state->message_count,
                tools_json ? (int)json_len(tools_json) : 0);
            char *hook_resp = hook_invoke("pre_llm_call", payload);
            free(hook_resp);
        }

        /* Retry loop */
        for (int attempt = 0; attempt <= retries && !dont_retry; attempt++) {
            /* Backoff between retries: 1s, 2s, 4s... cap at 16s */
            if (attempt > 0) {
                int delay = 1 << (attempt - 1);
                if (delay > 16) delay = 16;
                fprintf(stderr, "[retry] attempt %d/%d after %ds...\n", attempt, retries, delay);
                struct timespec ts = {delay, 0};
                nanosleep(&ts, NULL);
            }

            /* Free previous failed response before retrying */
            if (llm_resp) { llm_response_free(llm_resp); llm_resp = NULL; }

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
                fprintf(stderr, "[retry] LLM call returned NULL (attempt %d/%d)\n",
                        attempt + 1, retries);
                continue; /* retry */
            }

            /* Check for empty response */
            if (!llm_resp->content && llm_resp->tool_calls_count == 0) {
                fprintf(stderr, "[retry] Empty LLM response (attempt %d/%d)\n",
                        attempt + 1, retries);
                continue; /* retry */
            }

            /* Success — break out of retry loop */
            break;
        }

        /* If all retries failed, try fallback providers */
        if (!llm_resp || (!llm_resp->content && llm_resp->tool_calls_count == 0)) {
            char saved_provider[64] = {0};
            char saved_model[128] = {0};
            memcpy(saved_provider, state->llm.provider, sizeof(saved_provider));
            memcpy(saved_model, state->llm.model, sizeof(saved_model));

            /* Try fallback model first if set */
            if (state->llm.fallback_model[0]) {
                fprintf(stderr, "[fallback] Trying fallback model: %s\n",
                        state->llm.fallback_model);
                memcpy(state->llm.model, state->llm.fallback_model,
                       sizeof(state->llm.model));

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

                if (llm_resp && (llm_resp->content || llm_resp->tool_calls_count > 0)) {
                    /* Restore original provider but keep using fallback model */
                    memcpy(state->llm.provider, saved_provider, sizeof(state->llm.provider));
                    goto retry_done;
                }
                if (llm_resp) { llm_response_free(llm_resp); llm_resp = NULL; }
                /* Restore original model */
                memcpy(state->llm.model, saved_model, sizeof(state->llm.model));
            }

            /* Try fallback providers */
            if (state->llm.fallback_providers[0]) {
                char fb_copy[1024];
                memcpy(fb_copy, state->llm.fallback_providers, sizeof(fb_copy));
                char *provider = fb_copy;
                char *next;

                while (provider && *provider) {
                    /* Skip whitespace */
                    while (*provider == ' ' || *provider == ',') provider++;
                    if (!*provider) break;

                    next = strchr(provider, ',');
                    if (next) *next++ = '\0';

                    /* Trim trailing spaces */
                    char *end = provider + strlen(provider) - 1;
                    while (end > provider && *end == ' ') end--;
                    *(end + 1) = '\0';

                    if (!*provider) { provider = next; continue; }

                    fprintf(stderr, "[fallback] Trying provider: %s\n", provider);
                    memcpy(state->llm.provider, provider, sizeof(state->llm.provider));

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

                    if (llm_resp && (llm_resp->content || llm_resp->tool_calls_count > 0)) {
                        memcpy(state->llm.model, saved_model, sizeof(state->llm.model));
                        goto retry_done;
                    }
                    if (llm_resp) { llm_response_free(llm_resp); llm_resp = NULL; }

                    provider = next;
                }
                /* All fallback providers failed — restore original provider */
                memcpy(state->llm.provider, saved_provider, sizeof(state->llm.provider));
                memcpy(state->llm.model, saved_model, sizeof(state->llm.model));
            }

            /* All retries and fallbacks exhausted */
            json_free(tools_json);
            if (llm_resp) llm_response_free(llm_resp);
            return strdup("{\"error\":\"LLM call failed after retries and fallbacks. "
                          "Check provider/network connectivity and config.\"}");
        }

retry_done:
        /* P91: Mark system prompt as cached after first successful call */
        if (!state->llm.system_cached) {
            state->llm.system_cached = true;
        }

        /* P186: Invoke post_llm_call hook */
        {
            char payload[2048];
            snprintf(payload, sizeof(payload),
                "{\"event\":\"post_llm_call\",\"model\":\"%s\",\"provider\":\"%s\","
                "\"iteration\":%d,\"input_tokens\":%d,\"output_tokens\":%d,"
                "\"tool_calls\":%d,\"finish_reason\":\"%s\"}",
                state->llm.model, state->llm.provider,
                iteration, llm_resp ? llm_resp->input_tokens : 0,
                llm_resp ? llm_resp->output_tokens : 0,
                llm_resp ? llm_resp->tool_calls_count : 0,
                llm_resp ? llm_resp->finish_reason : "error");
            char *hook_resp = hook_invoke("post_llm_call", payload);
            free(hook_resp);
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

        /* G01-G03: Track session token totals */
        state->session_input_tokens += llm_resp->input_tokens;
        state->session_output_tokens += llm_resp->output_tokens;
        state->session_total_tokens = state->session_input_tokens + state->session_output_tokens;

        /* G04-G06: Deep token tracking */
        state->session_reasoning_tokens += llm_resp->reasoning_tokens;
        state->session_cache_read_tokens += llm_resp->cache_read_tokens;
        state->session_cache_write_tokens += llm_resp->cache_write_tokens;

        /* G07-G08: Track cumulative cost from budget tracker */
        if (state->budget) {
            state->session_estimated_cost_usd = (double)state->budget->total_cost_usd;
            snprintf(state->session_cost_source, sizeof(state->session_cost_source), "budget_tracker");
        }

        /* G09: Count tool turns */
        if (llm_resp->tool_calls_count > 0) {
            state->tool_turn_count++;
        }

        /* G10: Update last activity timestamp */
        state->last_activity_ts = time(NULL);

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
            /* G25: Reset budget on successful completion */
            if (state->budget) budget_tracker_reset(state->budget);
            return result;
        }

        /* Has tool calls — create assistant message with tool_calls */
        /* Snapshot before tool execution for undo */
        agent_snapshot_take(state);
        /* P98: Try auto-save checkpoint */
        checkpoint_try_autosave(&state->checkpoints, state);
        message_t *assistant_msg = message_new_assistant_with_toolcalls(
            llm_resp->content, llm_resp->tool_calls, llm_resp->tool_calls_count,
            llm_resp->reasoning, llm_resp->encrypted_content);
        context_push(state, assistant_msg);

        /* P87: Parallel tool dispatch — execute independent tools concurrently */
        int n_calls = llm_resp->tool_calls_count;

        /* G24: Reset per-turn tool call counter and check per-turn limit */
        if (state->budget) budget_tracker_reset_turn_tools(state->budget);
        if (state->budget && state->budget->max_tool_calls_per_turn > 0 &&
            n_calls > state->budget->max_tool_calls_per_turn) {
            n_calls = state->budget->max_tool_calls_per_turn;
        }

        /* G28: Reset guardrail controller for this turn */
        tool_guardrail_reset(&state->guardrails_ctrl);

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

        /* Phase 1: Security approval + guardrail check (sequential) */

        /* P186: Invoke pre_tool_call hook */
        {
            char payload[2048];
            snprintf(payload, sizeof(payload),
                "{\"event\":\"pre_tool_call\",\"count\":%d,\"iteration\":%d}",
                n_calls, iteration);
            char *hook_resp = hook_invoke("pre_tool_call", payload);
            free(hook_resp);
        }

        for (int i = 0; i < n_calls; i++) {
            works[i].index = i;
            works[i].tool_name = strdup(llm_resp->tool_calls[i].name);
            works[i].tool_args = strdup(llm_resp->tool_calls[i].arguments);
            works[i].tool_id = strdup(llm_resp->tool_calls[i].id);
            works[i].approved = approval_check(
                llm_resp->tool_calls[i].name,
                llm_resp->tool_calls[i].arguments);

            /* G29: Guardrail before_call — block tools that are in a loop */
            if (works[i].approved) {
                tool_guardrail_decision_t gd = tool_guardrail_before_call(
                    &state->guardrails_ctrl,
                    llm_resp->tool_calls[i].name,
                    llm_resp->tool_calls[i].arguments);
                if (gd.action == GUARDRAIL_BLOCK || gd.action == GUARDRAIL_HALT) {
                    works[i].approved = 0;
                    works[i].result = tool_guardrail_synthetic_result(&gd);
                    fprintf(stderr, "[guardrail] Blocked %s: %s\n",
                            gd.tool_name, gd.code);
                }
            }

            if (!works[i].result)
                works[i].result = NULL;

            /* Fire tool.started event for approved tools */
            if (works[i].approved && state->tool_event_cb) {
                state->tool_event_cb("tool.started", works[i].tool_name,
                                     works[i].tool_args, state->tool_event_data);
            }
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

        /* P186: Invoke post_tool_call hook */
        {
            char payload[2048];
            snprintf(payload, sizeof(payload),
                "{\"event\":\"post_tool_call\",\"count\":%d,\"iteration\":%d}",
                n_calls, iteration);
            char *hook_resp = hook_invoke("post_tool_call", payload);
            free(hook_resp);
        }

        /* Phase 3: Create tool result messages (in original order) */
        for (int i = 0; i < n_calls; i++) {
            /* G24: Track per-turn tool call count */
            if (state->budget) budget_tracker_increment_tool_call(state->budget);

            /* P177: Subdirectory hint discovery — append context from new dirs */
            if (works[i].result) {
                char *hints = subdir_hints_check(works[i].tool_name, works[i].tool_args);
                if (hints) {
                    /* Append hints to tool result */
                    size_t old_len = strlen(works[i].result);
                    size_t hints_len = strlen(hints);
                    char *updated = (char *)realloc(works[i].result, old_len + hints_len + 1);
                    if (updated) {
                        memcpy(updated + old_len, hints, hints_len + 1);
                        works[i].result = updated;
                    }
                    free(hints);
                }
            }

            message_t *tool_msg = message_new_tool(
                works[i].tool_id,
                works[i].result ? works[i].result :
                    "{\"error\":\"Tool returned NULL\"}");
            context_push(state, tool_msg);
            /* P93: Classify tool result — abort on fatal */
            /* G35: Use typed interrupt for graceful vs force distinction */
            bool tool_failed = (classify_tool_result(works[i].result, works[i].tool_name) == TOOL_RESULT_FATAL);
            if (tool_failed) {
                state->interrupted = true;
                state->interrupt_type = INTERRUPT_GRACEFUL;
                /* G12: Set interrupt message with context */
                snprintf(state->interrupt_message, sizeof(state->interrupt_message),
                         "Fatal tool result from '%s': %s",
                         works[i].tool_name,
                         works[i].result ? works[i].result : "unknown error");
            }

            /* G30: Guardrail after_call — track tool results for loop detection */
            {
                tool_guardrail_decision_t gd = tool_guardrail_after_call(
                    &state->guardrails_ctrl,
                    works[i].tool_name,
                    works[i].tool_args,
                    works[i].result,
                    tool_failed);
                if (gd.action == GUARDRAIL_HALT) {
                    state->interrupted = true;
                    state->interrupt_type = INTERRUPT_GRACEFUL;
                    snprintf(state->interrupt_message, sizeof(state->interrupt_message),
                             "Guardrail halt: %s", gd.message);
                    fprintf(stderr, "[guardrail] Halt: %s\n", gd.code);
                } else if (gd.action == GUARDRAIL_WARN) {
                    /* Append warning to tool result */
                    char *updated = tool_guardrail_append_guidance(
                        works[i].result ? works[i].result : "", &gd);
                    if (updated) {
                        free(works[i].result);
                        works[i].result = updated;
                    }
                    fprintf(stderr, "[guardrail] Warn: %s\n", gd.code);
                }
            }

            /* Fire tool.completed event */
            if (state->tool_event_cb) {
                const char *completed_type = tool_failed ? "tool.failed" : "tool.completed";
                state->tool_event_cb(completed_type, works[i].tool_name,
                                     works[i].result, state->tool_event_data);
            }

            /* Optional background review for sensitive tools. Activate by setting
             * state->enable_background_review = true in agent init. */
            if (state->enable_background_review && !tool_failed) {
                char *review = llm_background_review(&state->llm,
                    works[i].tool_name, works[i].tool_args, works[i].result);
                if (review) {
                    /* Append review to tool result as guidance */
                    size_t rlen = strlen(works[i].result ? works[i].result : "");
                    size_t vlen = strlen(review);
                    char *updated = (char *)realloc(works[i].result, rlen + vlen + 64);
                    if (updated) {
                        snprintf(updated + rlen, vlen + 64, "\n\n[Background Review]\n%s", review);
                        works[i].result = updated;
                    }
                    free(review);
                }
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

    /* G35-G36: Typed interrupt handling */
    if (state->interrupt_type != INTERRUPT_NONE) {
        state->interrupted = true;
        if (state->interrupt_type == INTERRUPT_GRACEFUL && state->message_count > 0) {
            /* G36: Partial results are preserved in context */
            state->partial_results_saved = true;
        }
        hermes_log(LOG_WARNING, "agent_loop", "Conversation interrupted (type=%d)", state->interrupt_type);
        return strdup("Error: Interrupted");
    }
    /* G26: Hard budget limit — immediate stop, no grace call */
    if (state->budget && budget_tracker_is_hard_exceeded(state->budget)) {
        hermes_log(LOG_WARNING, "agent_loop", "Hard budget limit exceeded");
        return strdup("Error: Hard budget limit exceeded (immediate stop)");
    }
    /* Soft budget exceeded */
    if (state->budget && budget_tracker_is_exceeded(state->budget)) {
        hermes_log(LOG_WARNING, "agent_loop", "Budget exceeded (token/cost/turn limit)");
        return strdup("Error: Budget exceeded (token/cost/turn limit reached)");
    }
    hermes_log(LOG_INFO, "agent_loop", "Max iterations reached (%d)", state->max_iterations);
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

/* G18: Inject conversation history — preload messages from JSON array.
 * Each element: {"role":"user|assistant|tool","content":"..."}
 * Existing messages are preserved; new ones appended. */
bool agent_inject_history(agent_state_t *state, const char *history_json) {
    if (!state || !history_json || !*history_json) return false;
    char *err = NULL;
    json_node_t *arr = json_parse(history_json, &err);
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
    return n > 0;
}
