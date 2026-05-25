/**
 * @defgroup hermes_gateway Gateway
 * @brief Multi-platform messaging gateway.
 *
 *
Routes messages between 19+ platforms and the agent loop.
Handles platform adapters, message queues, rate limiting,
markdown formatting, and media attachments.
 *
 * @{
 */
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

    /* Per-platform HTTP keepalive config (seconds, 0=default) */
    double platform_keepalive_sec[GW_MAX_PLATFORMS];

    /* E28: Message deduplication — ring buffer of recent message IDs */
    char   dedup_ids[64][128];     /* recent message IDs (update_id or message_id) */
    double dedup_timestamps[64];   /* when each ID was seen (monotonic) */
    int    dedup_head;
    int    dedup_count;
    double dedup_ttl;              /* seconds to keep dedup entries (default 5.0) */

    /* E29: Batch aggregation — coalesce fragmented messages */
    char   batch_buf[4096];        /* accumulated text */
    char   batch_platform[32];
    char   batch_chat_id[128];
    double batch_start_time;       /* when batch accumulation started */
    bool   batch_active;

    /* E31: Per-platform cooldown (seconds to wait between actions) */
    double platform_cooldown_sec[GW_MAX_PLATFORMS];
    double platform_last_action[GW_MAX_PLATFORMS];

    /* E32: Reconnect backoff — exponential backoff per platform */
    int    reconnect_attempt[GW_MAX_PLATFORMS];
    double reconnect_delay_sec[GW_MAX_PLATFORMS];
#define GW_RECONNECT_BASE_SEC   1.0
#define GW_RECONNECT_MAX_SEC    60.0
#define GW_RECONNECT_JITTER     0.1

    /* E33: Proxy per-platform */
    char   platform_proxy[GW_MAX_PLATFORMS][512];
    bool   proxy_enabled[GW_MAX_PLATFORMS];

    /* E34: Group observe — observe unmentioned messages */
    char   group_observe_prefix[64];  /* prefix to strip from group names */
    bool   group_observe_enabled;
    /* L08: Group observe buffer — accumulated unmentioned messages */
    char   observe_buffer[65536];     /* rolling buffer of observed messages */
    pthread_mutex_t observe_mutex;
} gateway_state_t;

/* Global gateway state — defined in server.c */
extern gateway_state_t g_gw;

/* Thread-safe agent_chat wrapper — acquires mutex, calls agent_chat, releases */
char *gateway_agent_chat(const char *message);

/* E28: Check if message ID was already processed (dedup). Returns true if duplicate. */
bool gw_dedup_check(const char *message_id);

/* E28: Record a message ID as processed. */
void gw_dedup_add(const char *message_id);

/* E29: Accumulate a batch of text fragments. Call gw_batch_flush() to process. */
void gw_batch_accumulate(const char *platform, const char *chat_id, const char *fragment);

/* E29: Flush the current batch (process as single message). */
void gw_batch_flush(void);

/* E31: Check if platform is in cooldown. Returns seconds remaining (>0 = in cooldown). */
double gw_cooldown_remaining(int plat_idx);

/* E31: Mark an action as performed on platform (resets cooldown timer). */
void gw_cooldown_mark(int plat_idx);

/* E32: Calculate reconnect delay with exponential backoff. Returns seconds to sleep. */
double gw_reconnect_delay(int plat_idx);

/* E32: Reset reconnect backoff on successful connection. */
void gw_reconnect_reset(int plat_idx);

/* E34: Set group observe prefix. Messages from groups with this prefix are observed. */
void gw_set_group_observe(const char *prefix, bool enabled);
/* L08: Append a message to the observe buffer (thread-safe). */
void gw_observe_append(const char *platform, const char *chat_id, const char *text);
/* L08: Consume and clear the observe buffer for a given platform+chat (thread-safe).
 * Returns a strdup'd string, caller must free(). Returns NULL if empty. */
char *gw_observe_consume(const char *platform, const char *chat_id);

/* E35-E38: Gateway hooks system */
typedef json_node_t *(*gw_hook_t)(json_node_t *data, void *userdata);
void gw_register_pre_send(gw_hook_t hook, void *userdata);
void gw_register_post_receive(gw_hook_t hook, void *userdata);
void gw_register_interceptor(gw_hook_t hook, void *userdata);

/* E38: Gateway event bus */
typedef void (*gw_event_listener_t)(const char *event_type, json_node_t *data, void *userdata);
void gw_event_register(gw_event_listener_t listener, void *userdata);
void gw_event_emit(const char *event_type, json_node_t *data);

/* E40-E43: Gateway formatting utilities */
char *gw_markdown_to_html(const char *text);
char *gw_markdown_v2_escape(const char *text);
char *gw_truncate_message(const char *text, size_t max_len);

/* E44-E47: Gateway error handling */
bool gw_retry_with_backoff(bool (*api_call)(void *ctx), void *ctx, int max_retries, int base_delay_ms);
bool gw_refresh_token(int plat_idx);

/* ================================================================
 *  Telegram platform
 * ================================================================ */

void telegram_set_token(const char *token);
void telegram_set_username(const char *username);
const char *telegram_get_username(void);
bool telegram_get_me(http_client_t *http);
bool telegram_is_mentioned(json_node_t *update);
bool telegram_is_group(json_node_t *update);
bool telegram_send_message(http_client_t *http, const char *chat_id,
                            const char *text, const char *parse_mode);
bool telegram_send_chat_action(http_client_t *http, const char *chat_id,
                                const char *action);
json_node_t *telegram_get_updates(http_client_t *http, int offset, int timeout);
const char *telegram_get_chat_id(json_node_t *update);
const char *telegram_get_text(json_node_t *update);

/* ================================================================
 *  P104: Extended Telegram API
 * ================================================================ */

/* Keyboard/reply markup messages */
bool telegram_send_message_with_keyboard(http_client_t *http,
                                          const char *chat_id,
                                          const char *text,
                                          const char *parse_mode,
                                          json_node_t *reply_markup);

/* Edit/delete */
bool telegram_edit_message_text(http_client_t *http, const char *chat_id,
                                 const char *message_id, const char *text,
                                 const char *parse_mode);
bool telegram_delete_message(http_client_t *http, const char *chat_id,
                              const char *message_id);

/* Interactive queries */
bool telegram_answer_inline_query(http_client_t *http,
                                   const char *inline_query_id,
                                   json_node_t *results);
bool telegram_answer_callback_query(http_client_t *http,
                                     const char *callback_query_id,
                                     const char *text, bool show_alert);

/* Polls */
bool telegram_send_poll(http_client_t *http, const char *chat_id,
                         const char *question, json_node_t *options,
                         bool is_anonymous, const char *poll_type,
                         bool is_closed);

/* Media */
bool telegram_send_media_group(http_client_t *http, const char *chat_id,
                                json_node_t *media);

/* E01-E05: Media send methods */
bool telegram_send_photo(http_client_t *http, const char *chat_id,
                          const char *photo, const char *caption,
                          const char *parse_mode);
bool telegram_send_document(http_client_t *http, const char *chat_id,
                             const char *document, const char *caption,
                             const char *parse_mode);
bool telegram_send_voice(http_client_t *http, const char *chat_id,
                          const char *voice, const char *caption,
                          const char *parse_mode);
bool telegram_send_video(http_client_t *http, const char *chat_id,
                          const char *video, const char *caption,
                          const char *parse_mode);
bool telegram_send_animation(http_client_t *http, const char *chat_id,
                              const char *animation, const char *caption,
                              const char *parse_mode);

/* E14: Forward message */
bool telegram_forward_message(http_client_t *http, const char *chat_id,
                               const char *from_chat_id,
                               const char *message_id);

/* E15: Pin/unpin message */
bool telegram_pin_chat_message(http_client_t *http, const char *chat_id,
                                const char *message_id);
bool telegram_unpin_chat_message(http_client_t *http, const char *chat_id,
                                  const char *message_id);

/* E16: Message reactions */
bool telegram_set_message_reaction(http_client_t *http, const char *chat_id,
                                    const char *message_id, const char *emoji);

/* Forum topics */
bool telegram_create_forum_topic(http_client_t *http, const char *chat_id,
                                  const char *name);
bool telegram_edit_forum_topic(http_client_t *http, const char *chat_id,
                                const char *message_thread_id, const char *name);
bool telegram_close_forum_topic(http_client_t *http, const char *chat_id,
                                 const char *message_thread_id);
bool telegram_reopen_forum_topic(http_client_t *http, const char *chat_id,
                                  const char *message_thread_id);

/* Update parsers */
const char *telegram_get_update_type(json_node_t *update);
const char *telegram_get_callback_query_id(json_node_t *update);
const char *telegram_get_inline_query_id(json_node_t *update);
const char *telegram_get_message_thread_id(json_node_t *update);

/* E07-E12: Interactive Telegram send methods with inline keyboards */
bool telegram_send_draft(http_client_t *http, const char *chat_id,
                          const char *text, const char *parse_mode);
bool telegram_send_clarify(http_client_t *http, const char *chat_id,
                            const char *question, const char **options, int n_options,
                            const char *parse_mode);
bool telegram_send_approval_prompt(http_client_t *http, const char *chat_id,
                                    const char *command, const char *reason,
                                    const char *parse_mode);
bool telegram_send_confirm_prompt(http_client_t *http, const char *chat_id,
                                   const char *action, const char *detail,
                                   const char *parse_mode);
bool telegram_send_model_picker(http_client_t *http, const char *chat_id,
                                 const char **models, int n_models,
                                 const char *current_model);
bool telegram_send_update_prompt(http_client_t *http, const char *chat_id,
                                  const char *diff_text, const char *summary,
                                  const char *parse_mode);

/* ================================================================
 *  Discord platform
 * ================================================================ */

void discord_set_token(const char *token);
void discord_set_channel(const char *id);
void discord_set_application_id(const char *id);
void discord_add_channel(const char *id);
bool discord_send_message(http_client_t *http, const char *text);
bool discord_send_message_to(http_client_t *http, const char *channel,
                              const char *text);
bool discord_send_embed(http_client_t *http, const char *channel,
                         const char *title, const char *description,
                         const char *color_hex);
void discord_send_typing(http_client_t *http);
void discord_send_typing_to(http_client_t *http, const char *channel);
bool discord_start_thread(http_client_t *http, const char *channel,
                           const char *message_id, const char *name,
                           int auto_archive_duration);
bool discord_join_thread(http_client_t *http, const char *thread_id);
bool discord_register_slash_command(http_client_t *http,
                                     const char *name,
                                     const char *description,
                                     json_node_t *options);
bool discord_bulk_overwrite_commands(http_client_t *http,
                                      json_node_t *commands);
bool discord_create_interaction_response(http_client_t *http,
                                          const char *interaction_id,
                                          const char *interaction_token,
                                          json_node_t *response_data);
bool discord_edit_interaction_response(http_client_t *http,
                                        const char *application_id_str,
                                        const char *interaction_token,
                                        json_node_t *response_data);
json_node_t *discord_poll_messages(http_client_t *http);
const char *discord_get_chat_id(json_node_t *update);
const char *discord_get_text(json_node_t *update);

/* E48: Interaction handling — get interaction metadata from update */
int discord_get_interaction_type(json_node_t *update);
const char *discord_get_interaction_id(json_node_t *update);
const char *discord_get_interaction_token(json_node_t *update);
bool discord_defer_interaction(http_client_t *http,
                                const char *interaction_id,
                                const char *interaction_token);
bool discord_show_modal(http_client_t *http,
                         const char *interaction_id,
                         const char *interaction_token,
                         const char *custom_id,
                         const char *title,
                         json_node_t *components);

/* E52: Typing with graceful 429 handling */
bool discord_send_typing_graceful(http_client_t *http, const char *channel);

/* ================================================================
 *  Webhook HTTP API platform
 * ================================================================ */

/* Start HTTP API server on the given port. Blocks until g_gw.running
 * is set to false (SIGINT/SIGTERM). Runs in a thread. */
void webhook_server_run(int port);

/* ================================================================
 *  P112: Webhook subscription + HMAC verification + retry + rate limit
 * ================================================================ */

/* Max webhook subscriptions */
#define WEBHOOK_SUBS_MAX 32

/* Max custom headers per subscription */
#define WEBHOOK_HEADERS_MAX 16

/* Custom header entry */
typedef struct {
    char key[128];
    char value[512];
} webhook_header_t;

/* Webhook subscription entry */
typedef struct {
    char             endpoint[512];        /* Outbound URL */
    char             hmac_secret[128];     /* HMAC secret for signature verification */
    int              max_retries;          /* Max retry attempts (0 = no retry) */
    int              backoff_ms;           /* Initial backoff in ms (doubles each retry) */
    webhook_header_t headers[WEBHOOK_HEADERS_MAX]; /* Custom headers for outgoing calls */
    int              header_count;         /* Number of custom headers */
    gw_rate_limiter_t rate_limiter;        /* Per-subscription rate limiter */
    bool             in_use;              /* Slot occupied */
} webhook_subscription_t;

/* Set HMAC secret for verifying incoming webhook request signatures.
 * Calls with NULL or empty string disable verification. */
void webhook_set_verify_secret(const char *secret);

/* Verify an HMAC-SHA256 signature in "sha256=<hex>" format against body.
 * The signature value typically comes from X-Hub-Signature-256 header.
 * Returns true if signature matches, false otherwise. */
bool webhook_verify_hmac(const char *signature, const unsigned char *body, size_t body_len);

/* Register an outbound webhook subscription.
 * Returns subscription index on success, -1 on failure (table full or NULL args).
 * max_retries=3 and backoff_ms=1000 are reasonable defaults. */
int webhook_subscription_add(const char *endpoint, const char *secret,
                              int max_retries, int backoff_ms);

/* Remove a subscription by index. Returns true if removed. */
bool webhook_subscription_remove(int idx);

/* Add a custom header to an existing subscription.
 * Returns true on success. */
bool webhook_subscription_add_header(int idx, const char *key, const char *value);

/* Deliver a JSON payload to a subscription endpoint.
 * Uses exponential backoff retry with jitter, subject to per-endpoint
 * rate limiting. Returns true if at least one attempt succeeded. */
bool webhook_subscription_deliver(int idx, const char *payload);

/* Deliver payload to endpoint (one-shot, no subscription needed).
 * Useful for simple outgoing webhooks without registering a subscription.
 * No rate limiting or HMAC signing is applied in this mode. */
bool webhook_send_payload(const char *endpoint, const char *payload,
                           const char *custom_headers, int timeout_sec,
                           int max_retries, int backoff_ms);

/* Get count of active subscriptions */
int webhook_subscription_count(void);

/* List active subscriptions into provided array; returns count written. */
int webhook_subscription_list(webhook_subscription_t *out, int max_out);

/* ================================================================
 *  Slack platform
 * ================================================================ */

void slack_set_token(const char *token);
void slack_set_channel(const char *id);
void slack_set_signing_secret(const char *secret);
bool slack_send_message(http_client_t *http, const char *text);
bool slack_send_blocks(http_client_t *http, const char *channel,
                        const char *text, json_node_t *blocks);
bool slack_update_message(http_client_t *http, const char *channel,
                           const char *ts, const char *text);
bool slack_join_channel(http_client_t *http, const char *channel);
bool slack_leave_channel(http_client_t *http, const char *channel);
bool slack_upload_file(http_client_t *http, const char *file_path,
                       const char *filename, const char *channel);
json_node_t *slack_poll_messages(http_client_t *http);
const char *slack_get_chat_id(json_node_t *update);
const char *slack_get_text(json_node_t *update);

/* ================================================================
 *  Matrix platform
 * ================================================================ */

void matrix_set_homeserver(const char *hs);
void matrix_set_token(const char *token);
void matrix_set_room(const char *id);
void matrix_set_user_id(const char *uid);
void matrix_set_event_filter(const char *types);
bool matrix_send_message(http_client_t *http, const char *text);
json_node_t *matrix_poll_messages(http_client_t *http);
const char *matrix_get_chat_id(json_node_t *update);
const char *matrix_get_text(json_node_t *update);
json_node_t *matrix_list_rooms(http_client_t *http);
bool matrix_mark_read(http_client_t *http, const char *room_id, const char *event_id);
bool matrix_send_typing(http_client_t *http, const char *room_id, int timeout_ms);
/* E55: Room management */
const char *matrix_create_room(http_client_t *http, const char *name,
                                const char *alias, bool is_public);
bool matrix_join_room(http_client_t *http, const char *room_id_or_alias);
bool matrix_leave_room(http_client_t *http, const char *r_id);

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
bool whatsapp_send_template(http_client_t *http, const char *to,
                             const char *template_name,
                             const char *language_code,
                             json_node_t *components);
bool whatsapp_send_interactive_buttons(http_client_t *http, const char *to,
                                        const char *header_text,
                                        const char *body_text,
                                        const char *footer_text,
                                        json_node_t *buttons);
bool whatsapp_send_interactive_list(http_client_t *http, const char *to,
                                     const char *body_text,
                                     const char *button_text,
                                     const char *section_title,
                                     json_node_t *rows);
bool whatsapp_mark_read(http_client_t *http, const char *message_id);
const char *whatsapp_verify_webhook(const char *query_string);
json_node_t *whatsapp_parse_webhook(const char *body);
const char *whatsapp_get_chat_id(json_node_t *update);
const char *whatsapp_get_text(json_node_t *update);

/* ================================================================
 *  P110: Email platform — IMAP IDLE, attachments, HTML, threading
 * ================================================================ */

void email_set_from(const char *from);

/* Basic send (backward compat) */
bool email_send_message(http_client_t *http, const char *to,
                        const char *subject, const char *body);

/* Extended send with HTML, attachments, and threading.
 * attachments: json array of {path, filename, mime_type} or NULL.
 * in_reply_to: Message-ID this email replies to, for threading (or NULL). */
bool email_send_message_ext(const char *to, const char *subject,
                             const char *text_body, const char *html_body,
                             json_node_t *attachments,
                             const char *in_reply_to);

/* IMAP IDLE push-based inbox monitoring.
 * Configure via env vars: EMAIL_IMAP_SERVER, EMAIL_IMAP_PORT,
 * EMAIL_IMAP_USER, EMAIL_IMAP_PASS, EMAIL_IMAP_MAILBOX. */
bool email_imap_init(void);    /* Load config from env vars */
void email_imap_start(void);   /* Start IDLE loop (blocks in thread) */
void email_imap_stop(void);    /* Signal IDLE loop to stop */

/* Legacy maildir polling (optional) */
json_node_t *email_poll_messages(http_client_t *http);

/* Update extractors */
const char *email_get_chat_id(json_node_t *update);
const char *email_get_text(json_node_t *update);

/* P110: Extended update extractors */
const char *email_get_html(json_node_t *update);        /* HTML body if present */
const char *email_get_subject(json_node_t *update);     /* Email subject */
json_node_t *email_get_attachments(json_node_t *update); /* Array of {filename, mime_type, path, size} */
const char *email_get_thread_id(json_node_t *update);   /* In-Reply-To (thread parent) */
const char *email_get_message_id(json_node_t *update);  /* This email's Message-ID */
const char *email_get_references(json_node_t *update);  /* References header for threading */

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
 *  P108: Extended Signal API — groups, reactions, quotes, attachments
 * ================================================================ */

/* Send a message to a Signal group (uses -g group_id flag) */
bool signal_send_group_message(http_client_t *http,
                                const char *group_id,
                                const char *text);

/* Send an emoji reaction to a specific message.
 * target_author: the phone number of the original message sender.
 * target_timestamp: the server timestamp of the original message (string). */
bool signal_send_reaction(http_client_t *http,
                           const char *recipient,
                           const char *target_author,
                           const char *target_timestamp,
                           const char *emoji);

/* Send a quoted reply to a specific message.
 * quote_author: the phone number of the original message sender.
 * quote_timestamp: the server timestamp of the original message (string). */
bool signal_send_quote_reply(http_client_t *http,
                              const char *recipient,
                              const char *text,
                              const char *quote_author,
                              const char *quote_timestamp);

/* Send a message with a file/image attachment.
 * file_path: absolute path to the file on disk. */
bool signal_send_attachment(http_client_t *http,
                             const char *recipient,
                             const char *text,
                             const char *file_path);

/* Extended poll: also populates "group_id" on group messages,
 * "reaction" on reaction updates, and "attachment" paths. */
const char *signal_get_group_id(json_node_t *update);
const char *signal_get_reaction(json_node_t *update);
const char *signal_get_attachment(json_node_t *update);

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
 *  P111: SMS/MMS platform (Twilio) — carrier lookup, MMS, webhooks
 * ================================================================ */

/* Setup */
void sms_set_twilio(const char *sid, const char *token, const char *from);
void sms_set_status_callback(const char *url);
void sms_set_webhook_url(const char *url);

/* Send SMS (basic) */
bool sms_send_message(http_client_t *http, const char *to, const char *text);

/* Send MMS with media URL (image, video, audio) */
bool sms_send_mms(http_client_t *http, const char *to, const char *text,
                  const char *media_url);

/* Carrier lookup via Twilio Lookup API v2.
 * Returns JSON node with carrier info, or NULL on error.
 * Caller must json_free() the result. */
json_node_t *sms_lookup_carrier(http_client_t *http, const char *phone_number);

/* Webhook verification — called from the webhook server on GET /sms-webhook.
 * Twilio uses a simple GET request for number confirmation.
 * Returns the challenge response string, or NULL on failure. */
const char *sms_verify_webhook(const char *query_string);

/* Parse Twilio webhook POST body (form-urlencoded) into json_node_t updates.
 * Handles inbound SMS, MMS, and delivery status callbacks.
 * Returns a JSON array of update objects, or NULL.
 * Each update has: "from", "to", "text", "message_sid", "num_media",
 * "media_urls" (array of strings for MMS), "status" (delivery status),
 * "error_code", "error_message". */
json_node_t *sms_parse_webhook(const char *body);

/* Poll (no-op for SMS — inbound is webhook-driven) */
json_node_t *sms_poll_messages(http_client_t *http);

/* Update extractors */
const char *sms_get_chat_id(json_node_t *update);
const char *sms_get_text(json_node_t *update);
const char *sms_get_message_sid(json_node_t *update);
const char *sms_get_status(json_node_t *update);
const char *sms_get_media_url(json_node_t *update, size_t index);
size_t      sms_get_num_media(json_node_t *update);

/* ================================================================
 *  API Server platform (aliased as webhook)
 * ================================================================ */

/* API server uses the same webhook_server_run function but configured
 * with different endpoint paths. Declared as separate type for
 * config clarity. */

/* ================================================================
 *  P114: Feishu (Lark) platform — card messages, buttons, docs, media
 * ================================================================ */

/* ---- Basic config ---- */
void feishu_set_webhook(const char *url);
bool feishu_send_message(http_client_t *http, const char *text);

/* ---- App API mode (Open API with tenant access token) ---- */
void feishu_set_app_credentials(const char *app_id, const char *app_secret);
void feishu_set_default_receive_id(const char *receive_id);

/* ---- Card messages ---- */

/* Send interactive card via webhook (no app credentials needed).
 * card: JSON node with Feishu card structure (config/header/elements). */
bool feishu_send_interactive(http_client_t *http, json_node_t *card);

/* Send interactive card to a specific receive_id via Open API.
 * card: JSON node with Feishu card structure. receive_id is auto-escaped as JSON string. */
bool feishu_send_card(http_client_t *http, const char *receive_id,
                       json_node_t *card);

/* Convenience: send a card with interactive buttons.
 * buttons: JSON array of {"text":{...},"type":"default","value":{...}} elements.
 * template: card header color template ("blue","green","red","purple","yellow", etc.) */
bool feishu_send_card_with_buttons(http_client_t *http,
                                    const char *receive_id,
                                    const char *title,
                                    const char *body_text,
                                    json_node_t *buttons,
                                    const char *template);

/* ---- Doc integration (Open API) ---- */

/* Create a new doc in the given folder (pass NULL for root).
 * Returns malloc'd JSON string with document info (caller free()s), or NULL on error.
 * Result JSON contains: document_id, title, url. */
char *feishu_doc_create(http_client_t *http, const char *folder_token,
                         const char *title);

/* Get doc raw content as plain text / markdown.
 * Returns malloc'd string (caller free()s), or NULL on error. */
char *feishu_doc_get_raw_content(http_client_t *http, const char *doc_id);

/* ---- Image/File messages (Open API) ---- */

/* Upload an image. Returns malloc'd image_key string (caller free()s), or NULL.
 * image_path: absolute path to the image file on disk. */
char *feishu_upload_image(http_client_t *http, const char *image_path);

/* Send an image by image_key to a specific receive_id. */
bool feishu_send_image(http_client_t *http, const char *receive_id,
                        const char *image_key);

/* Upload a file. Returns malloc'd file_key string (caller free()s), or NULL.
 * file_path: absolute path to the file. file_name: display name in chat. */
char *feishu_upload_file(http_client_t *http, const char *file_path,
                          const char *file_name);

/* Send a file by file_key to a specific receive_id. */
bool feishu_send_file(http_client_t *http, const char *receive_id,
                       const char *file_key);

/* ---- Poll / update extractors (unchanged) ---- */
json_node_t *feishu_poll_messages(http_client_t *http);
const char *feishu_get_chat_id(json_node_t *update);
const char *feishu_get_text(json_node_t *update);

/* ================================================================
 *  P113: WeCom (WeChat Work) platform — full feature parity
 * ================================================================ */
void wecom_set_webhook(const char *url);
void wecom_set_app_credentials(const char *corp_id, const char *corp_secret,
                                const char *agent_id);
bool wecom_send_message(http_client_t *http, const char *text);
bool wecom_send_markdown(http_client_t *http, const char *text);
bool wecom_send_text_with_at(http_client_t *http, const char *text,
                              const char *mentioned_list_json,
                              const char *mentioned_mobile_list_json);
bool wecom_send_image(http_client_t *http, const char *base64_data,
                       const char *md5_hex);
bool wecom_send_file(http_client_t *http, const char *media_id);
bool wecom_send_news(http_client_t *http, const char *articles_json);
bool wecom_send_taskcard(http_client_t *http,
                          const char *title, const char *description,
                          const char *url, const char *task_id,
                          const char *btns_json);
json_node_t *wecom_poll_messages(http_client_t *http);
const char *wecom_get_chat_id(json_node_t *update);
const char *wecom_get_text(json_node_t *update);

/* ================================================================
 *  P113: DingTalk platform — full feature parity
 * ================================================================ */
void dingtalk_set_webhook(const char *url);
void dingtalk_set_app_credentials(const char *app_id, const char *app_secret);
bool dingtalk_send_message(http_client_t *http, const char *text);
bool dingtalk_send_markdown(http_client_t *http, const char *title, const char *text);
bool dingtalk_send_text_with_at(http_client_t *http, const char *text,
                                 const char *at_mobiles_json,
                                 const char *at_user_ids_json,
                                 bool is_at_all);
bool dingtalk_send_markdown_with_at(http_client_t *http, const char *title,
                                     const char *text,
                                     const char *at_mobiles_json,
                                     const char *at_user_ids_json,
                                     bool is_at_all);
bool dingtalk_send_action_card(http_client_t *http,
                                const char *title, const char *text,
                                const char *btns_json,
                                const char *btn_orientation);
bool dingtalk_send_link(http_client_t *http,
                         const char *title, const char *text,
                         const char *message_url, const char *pic_url);
bool dingtalk_send_image_by_url(http_client_t *http, const char *image_url);
json_node_t *dingtalk_poll_messages(http_client_t *http);
const char *dingtalk_get_chat_id(json_node_t *update);
const char *dingtalk_get_text(json_node_t *update);

/* ================================================================
 *  P113: QQ Bot platform — full feature parity
 * ================================================================ */
void qqbot_set_webhook(const char *url);
void qqbot_set_token(const char *token);
bool qqbot_send_message(http_client_t *http, const char *text);
bool qqbot_send_markdown(http_client_t *http, const char *text);
bool qqbot_send_image(http_client_t *http, const char *image_url);
bool qqbot_send_with_keyboard(http_client_t *http, const char *text,
                               const char *keyboard_json);
bool qqbot_send_with_at(http_client_t *http, const char *text,
                         const char *at_user_id);
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

/* ================================================================
 *  P115: Extended BlueBubbles API — tapbacks, attachments, group chat
 * ================================================================ */

/* Tapback reaction codes (associatedMessageType values) */
#define BLUEBUBBLES_TAPBACK_LOVE      2000
#define BLUEBUBBLES_TAPBACK_LIKE      2001
#define BLUEBUBBLES_TAPBACK_DISLIKE   2002
#define BLUEBUBBLES_TAPBACK_LAUGH     2003
#define BLUEBUBBLES_TAPBACK_EMPHASIZE 2004
#define BLUEBUBBLES_TAPBACK_QUESTION  2005

/* Send a tapback reaction to a specific message.
 * chat_id: BlueBubbles chat GUID.
 * message_guid: the GUID of the message to react to.
 * tapback_type: one of the BLUEBUBBLES_TAPBACK_* constants. */
bool bluebubbles_send_tapback(http_client_t *http,
                               const char *chat_id,
                               const char *message_guid,
                               int tapback_type);

/* Send a file attachment (image, video, document) to a chat.
 * Uses curl subprocess (libhttp does not support multipart).
 * file_path: absolute path to the file on disk.
 * filename: display name for the attachment (or NULL to use basename). */
bool bluebubbles_send_attachment(http_client_t *http,
                                  const char *chat_id,
                                  const char *file_path,
                                  const char *filename);

/* Group chat detection */
bool bluebubbles_is_group(json_node_t *update);
const char *bluebubbles_get_group_id(json_node_t *update);

/* Typing indicator */
bool bluebubbles_send_typing(http_client_t *http, const char *chat_id);
bool bluebubbles_stop_typing(http_client_t *http, const char *chat_id);

/* P115: Extended update parsers */
const char *bluebubbles_get_attachment_path(json_node_t *update);
int bluebubbles_get_tapback_type(json_node_t *update);
const char *bluebubbles_get_message_guid(json_node_t *update);

/* Set a specific chat GUID to poll for recent messages.
 * Without this, bluebubbles_poll_messages() returns NULL (webhook-driven). */
void bluebubbles_set_poll_guid(const char *guid);

/* msgraph_webhook — raw socket HTTP server for Microsoft Graph notifications */
void msgraph_webhook_init(const char *webhook_path, const char *health_path, int port);
void msgraph_webhook_run(void);

/* weixin — iLink Bot API for WeChat */
bool weixin_init(const char *token, const char *account_id);
void weixin_start(void);
void weixin_stop(void);

/* P113: Weixin extended send API */
void weixin_send_text(const char *chat_id, const char *text,
                       const char *context_token);
void weixin_send_markdown(const char *chat_id, const char *text,
                           const char *context_token);
void weixin_send_image(const char *chat_id, const char *image_data,
                        int image_type, const char *context_token);

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

/* Shutdown all registered platforms — calls each platform's shutdown callback. */
void gw_platform_shutdown_all(void);

/** @} */ /* end of hermes_gateway group */
#endif /* HERMES_GATEWAY_H */
