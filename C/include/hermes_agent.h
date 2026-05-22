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
                                  const char *tool_call_id, const char *reasoning);
message_t *message_new_assistant_with_toolcalls(const char *content,
                                                  const tool_call_t *tcalls,
                                                  int tcalls_count,
                                                  const char *reasoning);
void message_free(message_t *msg);

/* === Context operations (context.c) === */
void context_init(agent_state_t *state);
bool context_push(agent_state_t *state, message_t *msg);
message_t *context_pop(agent_state_t *state);
void context_clear(agent_state_t *state);
bool context_set_system(agent_state_t *state, const char *content);
void context_truncate(agent_state_t *state, size_t max_messages);
const message_t *context_get(const agent_state_t *state, size_t index);

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

/* === Agent Loop (agent_loop.c) === */
void agent_init(agent_state_t *state);
void agent_generate_session_id(agent_state_t *state);
void agent_free(agent_state_t *state);
char *agent_run_conversation(agent_state_t *state,
                              const char *user_message,
                              const char *system_message);
char *agent_chat(agent_state_t *state, const char *message);

/* === Session persistence (agent_loop.c) === */
bool agent_open_db(agent_state_t *state);
bool agent_save_session(agent_state_t *state);
bool agent_load_session(agent_state_t *state, const char *session_id);
void agent_close_db(agent_state_t *state);

/* P28: Undo snapshot — capture/restore message state */
void agent_snapshot_take(agent_state_t *state);
bool agent_snapshot_restore(agent_state_t *state);

/* === Title (title.c) === */
char *agent_generate_title(llm_config_t *cfg, const char *first_message);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_AGENT_H */
