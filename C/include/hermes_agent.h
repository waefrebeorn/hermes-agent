/**
 * @defgroup hermes_agent Agent Loop
 * @brief Core conversation loop and state management.
 *
 *
Defines agent_state_t, agent_run_conversation(), and the main
LLM interaction loop with tool calling, session persistence,
streaming, and interrupt handling.
 *
 * @{
 */
#ifndef HERMES_AGENT_H
#define HERMES_AGENT_H

/*
 * hermes_agent.h — Agent-related declarations for Hermes C.
 * Extends the types in hermes.h with agent-specific functions.
 */

#include "hermes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* === Tool Registry (tools/registry.c) === */
bool registry_register(const char *name, const char *description,
                        const char *schema_json,
                        char *(*handler)(const char *args_json, const char *task_id));
/* P150: Extended registration with toolset name for enabled/disabled filtering */
bool registry_register_ex(const char *name, const char *description,
                          const char *schema_json, const char *toolset,
                          char *(*handler)(const char *args_json, const char *task_id));
void registry_set_available(const char *name, bool available);
tool_t *registry_find(const char *name);
char *registry_dispatch(const char *name, const char *args_json,
                        const char *task_id);
tool_registry_t *registry_get(void);
size_t registry_count(void);
json_node_t *registry_to_json(void);

/* Tool initialization */
void tools_init_all(void);

/* === Message lifecycle (context.c) === */
message_t *message_new(message_role_t role, const char *content);
message_t *message_new_tool(const char *tool_call_id, const char *content);
message_t *message_new_assistant(const char *content, const char *tool_name,
                                  const char *tool_call_id, const char *reasoning,
                                  const char *encrypted_content);
message_t *message_new_assistant_with_toolcalls(const char *content,
                                                  const tool_call_t *tcalls,
                                                  int tcalls_count,
                                                  const char *reasoning,
                                                  const char *encrypted_content);
void message_free(message_t *msg);
/* Clone a message (deep copy) */
message_t *message_clone(const message_t *src);

/* === Context operations (context.c) === */
void context_init(agent_state_t *state);
bool context_push(agent_state_t *state, message_t *msg);
message_t *context_pop(agent_state_t *state);
void context_clear(agent_state_t *state);
bool context_set_system(agent_state_t *state, const char *content);
void context_truncate(agent_state_t *state, size_t max_messages);
const message_t *context_get(const agent_state_t *state, size_t index);

/* P90: Smart context eviction strategies */
typedef enum {
    EVICT_OLDEST_TOOL_FIRST,
    EVICT_OLDEST_USER,
    EVICT_KEEP_RECENT_N,
} eviction_strategy_t;

int  context_message_tokens(const message_t *msg);
int  context_total_tokens(const agent_state_t *state);
void context_evict_smart(agent_state_t *state, size_t max_messages,
                          eviction_strategy_t strategy);
const char *context_get_system(const agent_state_t *state);

/* P97: Compression feedback — user-rated quality and adaptive threshold */
void compression_feedback_init(compression_feedback_t *fb);
void compression_feedback_positive(compression_feedback_t *fb);
void compression_feedback_negative(compression_feedback_t *fb);
float compression_feedback_get_threshold(const compression_feedback_t *fb, float config_threshold);
void compression_feedback_status(const compression_feedback_t *fb, char *buf, size_t sz);

/* P98: Checkpoint manager — auto-save, named checkpoints, rollback */
void    checkpoint_init(checkpoint_manager_t *mgr);
void    checkpoint_free(checkpoint_manager_t *mgr);
void    checkpoint_set_limits(checkpoint_manager_t *mgr, int max_snapshots, int auto_save_interval);
bool    checkpoint_save(checkpoint_manager_t *mgr, agent_state_t *state, const char *label);
bool    checkpoint_restore(checkpoint_manager_t *mgr, agent_state_t *state, const char *checkpoint_id);
size_t  checkpoint_list(const checkpoint_manager_t *mgr, char (*ids)[64], char (*labels)[128], size_t max_count);
size_t  checkpoint_count(const checkpoint_manager_t *mgr);
bool    checkpoint_try_autosave(checkpoint_manager_t *mgr, agent_state_t *state);

/* G29: Generate diff between checkpoint and current state. Caller must free. */
char    *checkpoint_diff(const checkpoint_t *cp, const agent_state_t *state);

/* G30: Restore checkpoint and branch from that point. */
bool    checkpoint_branch_restore(checkpoint_manager_t *mgr, agent_state_t *state,
                                  const char *checkpoint_id, const char *new_session_id);

/* === LLM Client (llm_client.c) === */
llm_response_t *llm_chat_completion(llm_config_t *cfg,
                                     const message_t **messages,
                                     size_t message_count,
                                     json_node_t *tools_json);

/* Streaming variant. Calls token_cb with each content delta as it arrives.
 * Still returns full llm_response_t with accumulated content + tool_calls. */
llm_response_t *llm_chat_completion_stream(llm_config_t *cfg,
                                            const message_t **messages,
                                            size_t message_count,
                                            json_node_t *tools_json,
                                            llm_token_cb_t token_cb,
                                            void *userdata);
void llm_response_free(llm_response_t *resp);

/* Token estimation (approximate: 1 token ≈ 4 chars) */
static inline size_t llm_estimate_tokens(const char *text) {
    if (!text) return 0;
    return (strlen(text) + 3) / 4; /* ceil division */
}

/* Count approximate tokens in a message list */
size_t llm_count_context_tokens(const message_t **msgs, size_t count, size_t max_tokens);

/* Truncate context to fit within token budget. Keeps first (system) and last N. */
void llm_truncate_context(agent_state_t *state, size_t max_tokens);

/* Smart context compression: summarize old messages before dropping.
 * Returns summary string (caller inserts) or NULL (fall back to dropping).
 * Pass compression.enabled flag from config. */
char *llm_compress_context(agent_state_t *state, size_t max_tokens, bool enabled);

/* P100: Background review — lightweight AI review of tool results.
 * Returns malloc'd review text, caller must free. */
char *llm_background_review(llm_config_t *cfg,
                             const char *tool_name,
                             const char *tool_args,
                             const char *tool_result);

/* === Agent Loop (agent_loop.c) === */
void agent_init(agent_state_t *state);
void agent_generate_session_id(agent_state_t *state);
void agent_free(agent_state_t *state);
void agent_configure_from_config(agent_state_t *state, const hermes_config_t *cfg);
char *agent_run_conversation(agent_state_t *state,
                              const char *user_message,
                              const char *system_message);
char *agent_chat(agent_state_t *state, const char *message);

/* G18: Inject conversation history — preload messages from JSON array */
bool agent_inject_history(agent_state_t *state, const char *history_json);

/* === Session persistence (agent_loop.c) === */
bool agent_open_db(agent_state_t *state);
bool agent_save_session(agent_state_t *state);
bool agent_load_session(agent_state_t *state, const char *session_id);
void agent_close_db(agent_state_t *state);

/* P141-P150: Session DB depth */
bool agent_save_meta(agent_state_t *state);
bool agent_load_meta(agent_state_t *state, session_meta_t *meta);
bool agent_session_create(agent_state_t *state, const char *session_id);
session_entry_t *agent_session_list(size_t *count, const char *tag_filter, int limit);
bool agent_session_delete(agent_state_t *state, const char *session_id);
bool agent_auto_save(agent_state_t *state, int interval);
bool agent_crash_recover(agent_state_t *state);
int  agent_auto_prune(agent_state_t *state, int retention_days);
bool agent_session_add_tag(agent_state_t *state, const char *tag);
bool agent_session_remove_tag(agent_state_t *state, const char *tag);
char *agent_session_export_json(agent_state_t *state, const char *session_id);
char *agent_session_export_markdown(agent_state_t *state, const char *session_id);
bool agent_session_branch(agent_state_t *state, const char *new_id, int branch_point);
int  agent_session_migrate(agent_state_t *state);

/* P28: Undo snapshot — capture/restore message state */
void agent_snapshot_take(agent_state_t *state);
bool agent_snapshot_restore(agent_state_t *state);

/* === Title (title.c) === */
char *agent_generate_title(llm_config_t *cfg, const char *first_message);

/* === Message sequence repair (agent_message_repair.c) === */
int hermes_repair_message_sequence(message_t *messages, int *count);
int hermes_sanitize_tool_call_arguments(message_t *messages, int *count);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of hermes_agent group */
#endif /* HERMES_AGENT_H */
