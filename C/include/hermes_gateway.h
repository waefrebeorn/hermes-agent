#ifndef HERMES_GATEWAY_H
#define HERMES_GATEWAY_H

/*
 * hermes_gateway.h — Gateway platform adapter interface for Hermes C.
 * Each platform implements send, poll, and message extraction functions.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"

/* ================================================================
 *  Telegram platform
 * ================================================================ */

void telegram_set_token(const char *token);
bool telegram_send_message(http_client_t *http, const char *chat_id,
                            const char *text, const char *parse_mode);
bool telegram_send_chat_action(http_client_t *http, const char *chat_id,
                                const char *action);
json_node_t *telegram_get_updates(http_client_t *http, int offset, int timeout);
const char *telegram_get_chat_id(json_node_t *update);
const char *telegram_get_text(json_node_t *update);

/* ================================================================
 *  Discord platform
 * ================================================================ */

void discord_set_token(const char *token);
void discord_set_channel(const char *id);
bool discord_send_message(http_client_t *http, const char *text);
void discord_send_typing(http_client_t *http);
json_node_t *discord_poll_messages(http_client_t *http);
const char *discord_get_chat_id(json_node_t *update);
const char *discord_get_text(json_node_t *update);

#endif /* HERMES_GATEWAY_H */
