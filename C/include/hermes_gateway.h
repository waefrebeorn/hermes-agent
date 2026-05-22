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

/* Max pending messages in queue */
#define GW_QUEUE_MAX 256

/* Max path length */
#define GW_PATH_MAX 4096

/* Max HTTP clients in pool */
#define GW_POOL_MAX 16

/* P102: Max gateway sessions (one per unique chat_id) */
#define GW_SESSIONS_MAX 64

/* ================================================================
 *  P101: Gateway message queue entry
 * ================================================================ */

typedef struct {
    char platform[32];
    char chat_id[128];
    char text[4096];
    char thread_id[64];     /* Telegram topic / Discord thread */
    double timestamp;        /* monotonic time when queued */
} gateway_msg_t;

/* ================================================================
 *  P101: Per-platform rate limiter (token bucket)
 * ================================================================ */

typedef struct {
    double tokens_per_sec;  /* refill rate */
    double max_tokens;      /* burst capacity */
    double tokens;          /* current tokens */
    double last_refill;     /* monotonic time of last refill */
} gw_rate_limiter_t;

/* ================================================================
 *  P101: HTTP connection pool entry
 * ================================================================ */

typedef struct {
    http_client_t *client;
    bool           in_use;
    double         last_used;  /* monotonic time */
    char           endpoint[256]; /* API base URL this client is configured for */
} gw_http_pool_entry_t;

/* ================================================================
 *  P102: Gateway session entry
 * ================================================================ */

/* Each unique platform:chat_id pair gets its own agent session.
 * Sessions are auto-saved to DB and persisted across restarts. */
typedef struct {
    char            key[192];       /* "platform:chat_id" */
    agent_state_t   agent;
    db_t           *db;             /* session DB handle */
    double          last_active;    /* monotonic time */
    bool            in_use;
    char            session_id[64]; /* current session ID */
} gw_session_entry_t;

/* ================================================================
 *  P103: Unified platform interface
 * ================================================================ */

/* Common platform vtable. Each platform module fills this in.
 * Platforms that only need send (webhook-only) leave poll as NULL.
 * Platforms that are push-based (weixin, yuanbao) leave poll as NULL,
 * provide start/stop instead. */
typedef struct {
    const char *name;         /* "telegram", "discord", etc. */

    /* Initialize platform from global config/env vars. Returns true on success. */
    bool (*init)(void);

    /* Send text to a chat_id. Returns true on success. */
    bool (*send)(const char *chat_id, const char *text);

    /* Send typing indicator (optional — can be NULL). */
    void (*send_typing)(const char *chat_id);

    /* Poll for new messages. Returns a JSON array of updates, or NULL.
     * Each update must have at least "chat_id" and "text" fields.
     * The caller frees the returned JSON. NULL = no updates or error. */
    json_node_t *(*poll)(void);

    /* Start the platform's own event loop (for push-based platforms).
     * Called in a thread. Returns only when the platform stops. */
    void (*start)(void);

    /* Stop the platform's event loop (for push-based platforms). */
    void (*stop)(void);

    /* Shutdown and free resources */
    void (*shutdown)(void);

    /* Platform-specific state data (opaque) */
    void *data;
} gw_platform_t;

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

    /* P101: Message queue (circular buffer) */
    gateway_msg_t   msg_queue[GW_QUEUE_MAX];
    volatile int    msg_queue_head; /* producer writes here */
    volatile int    msg_queue_tail; /* consumer reads from here */
    pthread_mutex_t queue_mutex;
    pthread_cond_t  queue_cond;     /* signal when new message queued */

    /* P101: Per-platform rate limiters */
    gw_rate_limiter_t rate_limiters[GW_MAX_PLATFORMS];

    /* P101: HTTP connection pool */
    gw_http_pool_entry_t http_pool[GW_POOL_MAX];
    int                  pool_count;
    pthread_mutex_t      pool_mutex;

    /* P102: Per-chat session pool */
    gw_session_entry_t   sessions[GW_SESSIONS_MAX];
    int                  session_count;
    pthread_mutex_t      session_mutex;
    char                 session_db_path[GW_PATH_MAX];  /* where sessions are stored */

    /* P103: Platform registry */
    gw_platform_t        platform_defs[GW_MAX_PLATFORMS];
    int                  platform_def_count;

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

/* ================================================================
 *  Email platform
 * ================================================================ */

void email_set_from(const char *from);
bool email_send_message(http_client_t *http, const char *to,
                        const char *subject, const char *body);
json_node_t *email_poll_messages(http_client_t *http);
const char *email_get_chat_id(json_node_t *update);
const char *email_get_text(json_node_t *update);

/* ================================================================
 *  Signal platform
 * ================================================================ */

void signal_set_number(const char *number);
void signal_set_cli_path(const char *path);
bool signal_check_available(void);
bool signal_send_message(http_client_t *http, const char *to,
                          const char *text);
json_node_t *signal_poll_messages(http_client_t *http);
const char *signal_get_chat_id(json_node_t *update);
const char *signal_get_text(json_node_t *update);

/* ================================================================
 *  HomeAssistant platform
 * ================================================================ */

void ha_set_url(const char *url);
void ha_set_token(const char *token);
void ha_set_notify_entity(const char *entity);
bool ha_send_notification(const char *title, const char *message);
json_node_t *ha_poll_messages(http_client_t *http);
const char *ha_get_chat_id(json_node_t *update);
const char *ha_get_text(json_node_t *update);

/* ================================================================
 *  SMS platform (Twilio)
 * ================================================================ */

void sms_set_twilio(const char *sid, const char *token, const char *from);
bool sms_send_message(http_client_t *http, const char *to, const char *text);
json_node_t *sms_poll_messages(http_client_t *http);
const char *sms_get_chat_id(json_node_t *update);
const char *sms_get_text(json_node_t *update);

/* ================================================================
 *  API Server platform (aliased as webhook)
 * ================================================================ */

/* API server uses the same webhook_server_run function but configured
 * with different endpoint paths. Declared as separate type for
 * config clarity. */

/* ================================================================
 *  Feishu (Lark) platform
 * ================================================================ */
void feishu_set_webhook(const char *url);
bool feishu_send_message(http_client_t *http, const char *text);
json_node_t *feishu_poll_messages(http_client_t *http);
const char *feishu_get_chat_id(json_node_t *update);
const char *feishu_get_text(json_node_t *update);

/* ================================================================
 *  WeCom (WeChat Work) platform
 * ================================================================ */
void wecom_set_webhook(const char *url);
bool wecom_send_message(http_client_t *http, const char *text);
json_node_t *wecom_poll_messages(http_client_t *http);
const char *wecom_get_chat_id(json_node_t *update);
const char *wecom_get_text(json_node_t *update);

/* ================================================================
 *  DingTalk platform
 * ================================================================ */
void dingtalk_set_webhook(const char *url);
bool dingtalk_send_message(http_client_t *http, const char *text);
json_node_t *dingtalk_poll_messages(http_client_t *http);
const char *dingtalk_get_chat_id(json_node_t *update);
const char *dingtalk_get_text(json_node_t *update);

/* ================================================================
 *  QQ Bot platform
 * ================================================================ */
void qqbot_set_webhook(const char *url);
void qqbot_set_token(const char *token);
bool qqbot_send_message(http_client_t *http, const char *text);
json_node_t *qqbot_poll_messages(http_client_t *http);
const char *qqbot_get_chat_id(json_node_t *update);
const char *qqbot_get_text(json_node_t *update);

/* ================================================================
 *  BlueBubbles (iMessage) platform
 * ================================================================ */
void bluebubbles_set_url(const char *url);
void bluebubbles_set_password(const char *password);
bool bluebubbles_send_message(http_client_t *http, const char *to,
                              const char *text);
json_node_t *bluebubbles_poll_messages(http_client_t *http);
const char *bluebubbles_get_chat_id(json_node_t *update);
const char *bluebubbles_get_text(json_node_t *update);

/* msgraph_webhook — raw socket HTTP server for Microsoft Graph notifications */
void msgraph_webhook_init(const char *webhook_path, const char *health_path, int port);
void msgraph_webhook_run(void);

/* weixin — iLink Bot API for WeChat */
bool weixin_init(const char *token, const char *account_id);
void weixin_start(void);
void weixin_stop(void);

/* yuanbao — Yuanbao WebSocket gateway */
bool yuanbao_init(const char *app_id, const char *app_secret,
                  const char *bot_id, const char *ws_url,
                  const char *api_domain);
void yuanbao_start(void);
void yuanbao_stop(void);

/* ================================================================
 *  P101: Gateway queue API
 * ================================================================ */

/* Initialize message queue */
void gw_queue_init(void);

/* Push a message onto the queue. Blocks if full (up to 1s). Returns true on success. */
bool gw_queue_push(const char *platform, const char *chat_id,
                    const char *text, const char *thread_id);

/* Pop next message from queue. Non-blocking — returns false if empty. */
bool gw_queue_pop(gateway_msg_t *msg);

/* Get current queue depth */
int gw_queue_depth(void);

/* ================================================================
 *  P101: Rate limiter API
 * ================================================================ */

/* Initialize rate limiter for platform index.
 * tokens_per_sec: messages allowed per second (defaults)
 * max_burst: maximum burst size (default ~3x rate) */
void gw_rate_limit_init(int idx, double tokens_per_sec, double max_burst);

/* Check and consume a token. Returns true if allowed, false if rate-limited. */
bool gw_rate_limit_check(int idx);

/* ================================================================
 *  P101: HTTP connection pool API
 * ================================================================ */

/* Get an HTTP client from the pool (or create one).
 * endpoint: the API base URL for connection affinity.
 * Returns NULL on failure. */
http_client_t *gw_pool_get_client(const char *endpoint);

/* Return an HTTP client to the pool for reuse.
 * Pass NULL to free without returning. */
void gw_pool_return_client(http_client_t *client, const char *endpoint);

/* Close all idle connections in the pool */
void gw_pool_cleanup(void);

/* ================================================================
 *  P103: Platform interface API
 * ================================================================ */

/* Register a platform implementation. Called at startup per platform type. */
void gw_platform_register(const gw_platform_t *plat);

/* Send a message on a registered platform. Returns platform's send result. */
bool gw_platform_send(const char *platform_name, const char *chat_id, const char *text);

/* Send typing indicator on a registered platform (no-op if not supported). */
void gw_platform_send_typing(const char *platform_name, const char *chat_id);

#endif /* HERMES_GATEWAY_H */

