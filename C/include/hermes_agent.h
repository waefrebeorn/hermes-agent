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

/* === Message lifecycle (context.c) === */
message_t *message_new(message_role_t role, const char *content);
message_t *message_new_tool(const char *tool_call_id, const char *content);
message_t *message_new_assistant(const char *content, const char *tool_name,
                                  const char *tool_call_id, const char *reasoning);
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
void llm_response_free(llm_response_t *resp);

/* === Agent Loop (agent_loop.c) === */
void agent_init(agent_state_t *state);
void agent_free(agent_state_t *state);
char *agent_run_conversation(agent_state_t *state,
                              const char *user_message,
                              const char *system_message);
char *agent_chat(agent_state_t *state, const char *message);

/* === Title (title.c) === */
char *agent_generate_title(llm_config_t *cfg, const char *first_message);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_AGENT_H */
