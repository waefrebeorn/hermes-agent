#ifndef HERMES_GATEWAY_H
#define HERMES_GATEWAY_H

/*
 * hermes_gateway.h — Gateway platform adapter interface for Hermes C.
 * Supports multiple platforms running concurrently via pthread.
 * Each platform type: telegram, discord, slack, matrix, mattermost, webhook, whatsapp.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <pthread.h>

/* Max platforms running simultaneously */
#define GW_MAX_PLATFORMS 8

/* ================================================================
 *  Gateway state (shared across platform modules)
 * ================================================================ */

typedef struct {
    agent_state_t   agent;
    hermes_config_t config;
    http_client_t  *http;           /* Default HTTP client (for gateway_send) */
    bool            running;
    int             poll_interval;  /* seconds between polls */
    int             platform_count;
    char            platforms[GW_MAX_PLATFORMS][32]; /* Active platform names */
    pthread_t       threads[GW_MAX_PLATFORMS];
    pthread_mutex_t agent_mutex;    /* Mutex for thread-safe agent_chat */

    /* Platform-specific state */
    int             tg_offset;      /* Telegram: last update_id + 1 */

    /* Per-platform HTTP clients (owned by thread functions) */
    http_client_t  *platform_http[GW_MAX_PLATFORMS];
} gateway_state_t;

/* Global gateway state — defined in server.c */
extern gateway_state_t g_gw;

/* Thread-safe agent_chat wrapper — acquires mutex, calls agent_chat, releases */
char *gateway_agent_chat(const char *message);

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

/* ================================================================
 *  Webhook HTTP API platform
 * ================================================================ */

/* Start HTTP API server on the given port. Blocks until g_gw.running
 * is set to false (SIGINT/SIGTERM). Runs in a thread. */
void webhook_server_run(int port);

/* ================================================================
 *  Slack platform
 * ================================================================ */

void slack_set_token(const char *token);
void slack_set_channel(const char *id);
bool slack_send_message(http_client_t *http, const char *text);
json_node_t *slack_poll_messages(http_client_t *http);
const char *slack_get_chat_id(json_node_t *update);
const char *slack_get_text(json_node_t *update);

/* ================================================================
 *  Matrix platform
 * ================================================================ */

void matrix_set_homeserver(const char *hs);
void matrix_set_token(const char *token);
void matrix_set_room(const char *id);
bool matrix_send_message(http_client_t *http, const char *text);
json_node_t *matrix_poll_messages(http_client_t *http);
const char *matrix_get_chat_id(json_node_t *update);
const char *matrix_get_text(json_node_t *update);

/* ================================================================
 *  Mattermost platform
 * ================================================================ */

void mattermost_set_url(const char *url);
void mattermost_set_token(const char *token);
void mattermost_set_channel(const char *id);
bool mattermost_send_message(http_client_t *http, const char *text);
json_node_t *mattermost_poll_messages(http_client_t *http);
const char *mattermost_get_chat_id(json_node_t *update);
const char *mattermost_get_text(json_node_t *update);

/* ================================================================
 *  WhatsApp Cloud API platform
 * ================================================================ */

void whatsapp_set_token(const char *token);
void whatsapp_set_phone_id(const char *id);
void whatsapp_set_verify_token(const char *token);
bool whatsapp_send_message(http_client_t *http, const char *to,
                            const char *text);
const char *whatsapp_verify_webhook(const char *query_string);
json_node_t *whatsapp_parse_webhook(const char *body);
const char *whatsapp_get_chat_id(json_node_t *update);
const char *whatsapp_get_text(json_node_t *update);

#endif /* HERMES_GATEWAY_H */
