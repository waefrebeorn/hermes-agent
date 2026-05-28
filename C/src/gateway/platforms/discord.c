/*
 * discord.c — Discord Bot API platform adapter for Hermes C.
 * REST-only polling (no WebSocket). Supports multiple channels,
 * slash commands, threads, interactions, and embeds.
 *
 * P105: Slash commands, thread management, voice channel text,
 * buttons/select menus.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISCORD_API "https://discord.com/api/v10"

static char bot_token[512] = "";
static char channel_id[128] = "";
static char application_id[128] = "";
static long long last_message_id = 0;

/* P105: Multi-channel poll tracking */
typedef struct {
    char    id[128];
    long long last_message_id;
} discord_channel_t;

#define DISCORD_MAX_CHANNELS 16
static discord_channel_t g_channels[DISCORD_MAX_CHANNELS];
static int g_channel_count = 0;

void discord_set_token(const char *token) {
    snprintf(bot_token, sizeof(bot_token), "%s", token);
}

void discord_set_channel(const char *id) {
    snprintf(channel_id, sizeof(channel_id), "%s", id);
}

/* P105: Set application ID (required for slash commands) */
void discord_set_application_id(const char *id) {
    if (id) snprintf(application_id, sizeof(application_id), "%s", id);
}

/* P105: Add a channel to poll */
void discord_add_channel(const char *id) {
    if (g_channel_count >= DISCORD_MAX_CHANNELS) return;
    snprintf(g_channels[g_channel_count].id, sizeof(g_channels[0].id), "%s", id);
    g_channels[g_channel_count].last_message_id = 0;
    g_channel_count++;
}

/* Build headers */
static void discord_build_headers(char *buf, size_t cap, bool with_body) {
    if (with_body) {
        snprintf(buf, cap,
                 "Authorization: Bot %s\r\n"
                 "Content-Type: application/json",
                 bot_token);
    } else {
        snprintf(buf, cap,
                 "Authorization: Bot %s",
                 bot_token);
    }
}

/* Helper: POST JSON to Discord API */
static http_response_t *discord_post(http_client_t *http, const char *url,
                                      json_node_t *body) {
    char payload[16384];
    memset(payload, 0, sizeof(payload));
    char *p = json_serialize(body);
    if (!p) return NULL;
    size_t len = strlen(p);
    if (len >= sizeof(payload) - 1) {
        /* Too large — use heap */
        char headers[2048];
        discord_build_headers(headers, sizeof(headers), true);
        http_response_t *resp = http_request(http, HTTP_POST, url,
                                              headers, p, len);
        free(p);
        return resp;
    }
    memcpy(payload, p, len);
    free(p);

    char headers[2048];
    discord_build_headers(headers, sizeof(headers), true);
    return http_request(http, HTTP_POST, url, headers, payload, len);
}

/* ================================================================
 *  Send message
 * ================================================================ */

/* Forward declaration */
bool discord_send_message_to(http_client_t *http, const char *channel,
                              const char *text);

bool discord_send_message(http_client_t *http, const char *text) {
    return discord_send_message_to(http, channel_id, text);
}

/* P105: Send to specific channel */
bool discord_send_message_to(http_client_t *http, const char *channel,
                              const char *text) {
    if (!http || !text || !channel) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/channels/%s/messages", DISCORD_API, channel);

    json_node_t *body = json_new_object();
    json_object_set(body, "content", json_new_string(text));

    http_response_t *resp = discord_post(http, url, body);
    json_free(body);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* P105: Send message with embed */
bool discord_send_embed(http_client_t *http, const char *channel,
                         const char *title, const char *description,
                         const char *color_hex)
{
    if (!http || !channel) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/channels/%s/messages", DISCORD_API, channel);

    json_node_t *embed = json_new_object();
    if (title) json_object_set(embed, "title", json_new_string(title));
    if (description) json_object_set(embed, "description", json_new_string(description));
    if (color_hex) {
        long color = strtol(color_hex, NULL, 16);
        json_object_set(embed, "color", json_new_number((double)color));
    }

    json_node_t *embeds = json_new_array();
    json_array_append(embeds, embed);

    json_node_t *body = json_new_object();
    json_object_set(body, "embeds", embeds);

    http_response_t *resp = discord_post(http, url, body);
    json_free(body);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P105: Thread management
 * ================================================================ */

/* Start a thread from a message */
bool discord_start_thread(http_client_t *http, const char *channel,
                           const char *message_id, const char *name,
                           int auto_archive_duration)
{
    if (!http || !channel || !name) return false;

    char url[1024];
    if (message_id && message_id[0]) {
        snprintf(url, sizeof(url), "%s/channels/%s/messages/%s/threads",
                 DISCORD_API, channel, message_id);
    } else {
        snprintf(url, sizeof(url), "%s/channels/%s/threads",
                 DISCORD_API, channel);
    }

    json_node_t *body = json_new_object();
    json_object_set(body, "name", json_new_string(name));
    if (auto_archive_duration > 0)
        json_object_set(body, "auto_archive_duration",
                        json_new_number((double)auto_archive_duration));

    http_response_t *resp = discord_post(http, url, body);
    json_free(body);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* Join a thread */
bool discord_join_thread(http_client_t *http, const char *thread_id) {
    if (!http || !thread_id) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/channels/%s/thread-members/@me",
             DISCORD_API, thread_id);

    char headers[1024];
    discord_build_headers(headers, sizeof(headers), false);

    http_response_t *resp = http_request(http, HTTP_PUT, url, headers, NULL, 0);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P105: Slash command registration
 * ================================================================ */

bool discord_register_slash_command(http_client_t *http,
                                     const char *name,
                                     const char *description,
                                     json_node_t *options)
{
    if (!http || !name || !description) return false;
    if (!application_id[0]) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/applications/%s/commands",
             DISCORD_API, application_id);

    json_node_t *body = json_new_object();
    json_object_set(body, "name", json_new_string(name));
    json_object_set(body, "description", json_new_string(description));
    json_object_set(body, "type", json_new_number(1.0)); /* CHAT_INPUT */
    if (options)
        json_object_set(body, "options", json_copy(options));

    http_response_t *resp = discord_post(http, url, body);
    json_free(body);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* P105: Bulk overwrite all slash commands */
bool discord_bulk_overwrite_commands(http_client_t *http,
                                      json_node_t *commands)
{
    if (!http || !commands || !application_id[0]) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/applications/%s/commands",
             DISCORD_API, application_id);

    char body[16384];
    memset(body, 0, sizeof(body));
    char *serialized = json_serialize(commands);
    if (!serialized) return false;
    memcpy(body, serialized, strlen(serialized) + 1);

    char headers[2048];
    discord_build_headers(headers, sizeof(headers), true);

    http_response_t *resp = http_request(http, HTTP_PUT, url,
                                          headers, body, strlen(body));
    free(serialized);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P105: Interaction response (buttons, select menus)
 * ================================================================ */

/* Send initial response to an interaction */
bool discord_create_interaction_response(http_client_t *http,
                                          const char *interaction_id,
                                          const char *interaction_token,
                                          json_node_t *response_data)
{
    if (!http || !interaction_id || !interaction_token) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/interactions/%s/%s/callback",
             DISCORD_API, interaction_id, interaction_token);

    json_node_t *body = json_new_object();
    json_object_set(body, "type", json_new_number(4.0)); /* CHANNEL_MESSAGE_WITH_SOURCE */
    if (response_data)
        json_object_set(body, "data", json_copy(response_data));
    else {
        json_node_t *data = json_new_object();
        json_object_set(data, "content", json_new_string(""));
        json_object_set(body, "data", data);
    }

    http_response_t *resp = discord_post(http, url, body);
    json_free(body);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* Edit original interaction response */
bool discord_edit_interaction_response(http_client_t *http,
                                        const char *application_id_str,
                                        const char *interaction_token,
                                        json_node_t *response_data)
{
    if (!http || !application_id_str || !interaction_token) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/webhooks/%s/%s/messages/@original",
             DISCORD_API, application_id_str, interaction_token);

    json_node_t *body = json_new_object();
    if (response_data) {
        const char *content = json_get_str(response_data, "content", "");
        json_object_set(body, "content", json_new_string(content));
    }

    char payload[16384];
    memset(payload, 0, sizeof(payload));
    char *p = json_serialize(body);
    if (!p) { json_free(body); return false; }
    memcpy(payload, p, strlen(p) + 1);
    free(p);

    char headers[2048];
    discord_build_headers(headers, sizeof(headers), true);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    json_free(body);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Send typing indicator
 * ================================================================ */

void discord_send_typing_to(http_client_t *http, const char *channel);

/* P105: Send typing indicator */
void discord_send_typing(http_client_t *http) {
    if (http && channel_id && channel_id[0])
        discord_send_typing_to(http, channel_id);
}

/* P105: Send typing to specific channel */
void discord_send_typing_to(http_client_t *http, const char *channel) {
    if (!http || !channel) return;

    char url[1024];
    snprintf(url, sizeof(url), "%s/channels/%s/typing", DISCORD_API, channel);

    char headers[1024];
    discord_build_headers(headers, sizeof(headers), false);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, NULL, 0);
    if (resp) http_response_free(resp);
}

/* ================================================================
 *  Poll for new messages (multi-channel)
 * ================================================================ */

json_node_t *discord_poll_messages(http_client_t *http) {
    if (!http) return NULL;

    json_node_t *all_updates = json_new_array();

    /* If specific channels are configured, poll each */
    int channels_to_poll = (g_channel_count > 0) ? g_channel_count : 1;
    for (int ci = 0; ci < channels_to_poll; ci++) {
        const char *poll_id = (g_channel_count > 0) ?
                               g_channels[ci].id : channel_id;
        if (!poll_id[0]) continue;

        long long *last_id = (g_channel_count > 0) ?
                              &g_channels[ci].last_message_id : &last_message_id;

        char url[1024];
        if (*last_id > 0) {
            snprintf(url, sizeof(url), "%s/channels/%s/messages?limit=5&after=%lld",
                     DISCORD_API, poll_id, *last_id);
        } else {
            snprintf(url, sizeof(url), "%s/channels/%s/messages?limit=1",
                     DISCORD_API, poll_id);
        }

        char headers[512];
        discord_build_headers(headers, sizeof(headers), false);

        http_response_t *resp = http_get_with_headers(http, url, headers);
        if (!resp || resp->status != 200) {
            if (resp) http_response_free(resp);
            continue;
        }

        char *err = NULL;
        json_node_t *msgs = json_parse(resp->body, &err);
        http_response_free(resp);
        free(err);

        if (!msgs || json_len(msgs) == 0) {
            json_free(msgs);
            continue;
        }

        size_t n = json_len(msgs);
        for (size_t i = 0; i < n; i++) {
            json_node_t *msg = json_get(msgs, n - 1 - i);
            long long mid = (long long)json_get_num(msg, "id", 0);

            if (mid <= *last_id) continue;
            if (mid > *last_id) *last_id = mid;

            /* Skip bot's own messages */
            json_node_t *author = json_obj_get(msg, "author");
            if (author && json_get_bool(author, "bot", false))
                continue;

            /* Build update wrapper */
            json_node_t *update = json_new_object();
            json_set(update, "update_id", json_number((double)mid));

            json_node_t *message = json_new_object();
            json_set(message, "message_id", json_number((double)mid));

            json_node_t *chat = json_new_object();
            json_set(chat, "id", json_number((double)mid));
            /* Store the actual channel ID for sending replies */
            json_set(chat, "channel_id", json_string(poll_id));
            json_set(message, "chat", chat);

            json_set(message, "text", json_string(json_get_str(msg, "content", "")));
            json_set(update, "message", message);

            json_append(all_updates, update);
        }

        json_free(msgs);
    }

    return all_updates;
}

/* ================================================================
 *  Extract info from update
 * ================================================================ */

const char *discord_get_chat_id(json_node_t *update) {
    static char buf[128];
    if (!update) return NULL;

    /* Try to get the actual channel_id we stored */
    json_node_t *msg = json_obj_get(update, "message");
    if (msg) {
        json_node_t *chat = json_obj_get(msg, "chat");
        if (chat) {
            const char *channel = json_get_str(chat, "channel_id", NULL);
            if (channel) {
                snprintf(buf, sizeof(buf), "%s", channel);
                return buf;
            }
        }
    }

    /* Fallback */
    snprintf(buf, sizeof(buf), "%s", channel_id);
    return buf;
}

const char *discord_get_text(json_node_t *update) {
    json_node_t *msg = json_obj_get(update, "message");
    if (!msg) return NULL;
    return json_get_str(msg, "text", NULL);
}

/* ================================================================
 *  E48: Advanced interaction handling (modal, deferred, components)
 * ================================================================ */

/* Discord interaction types */
#define DISCORD_INTERACTION_PING            1
#define DISCORD_INTERACTION_COMMAND         2
#define DISCORD_INTERACTION_COMPONENT       3
#define DISCORD_INTERACTION_AUTOCOMPLETE    4
#define DISCORD_INTERACTION_MODAL_SUBMIT    5

/* Discord interaction callback types */
#define DISCORD_CB_PONG                     1
#define DISCORD_CB_CHANNEL_WITH_SOURCE      4
#define DISCORD_CB_DEFERRED_UPDATE          6
#define DISCORD_CB_DEFERRED_CREATE          5
#define DISCORD_CB_UPDATE                   7
#define DISCORD_CB_MODAL                    9

/* Get interaction type from update. Returns 0 if not an interaction. */
int discord_get_interaction_type(json_node_t *update) {
    if (!update) return 0;
    json_node_t *interaction = json_obj_get(update, "interaction");
    if (!interaction) return 0;
    return (int)json_get_num(interaction, "type", 0);
}

/* Get interaction ID from update for responding. */
const char *discord_get_interaction_id(json_node_t *update) {
    static char buf[64];
    if (!update) return NULL;
    json_node_t *interaction = json_obj_get(update, "interaction");
    if (!interaction) return NULL;
    double id = json_get_num(interaction, "id", 0);
    if (id == 0) return NULL;
    snprintf(buf, sizeof(buf), "%.0f", id);
    return buf;
}

/* Get interaction token from update for responding. */
const char *discord_get_interaction_token(json_node_t *update) {
    static char buf[512];
    if (!update) return NULL;
    json_node_t *interaction = json_obj_get(update, "interaction");
    if (!interaction) return NULL;
    const char *token = json_get_str(interaction, "token", NULL);
    if (!token) return NULL;
    snprintf(buf, sizeof(buf), "%s", token);
    return buf;
}

/* Respond to an interaction with a deferred update (for component interactions).
 * This tells Discord we'll edit the message later. */
bool discord_defer_interaction(http_client_t *http,
                                const char *interaction_id,
                                const char *interaction_token) {
    if (!http || !interaction_id || !interaction_token) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/interactions/%s/%s/callback",
             DISCORD_API, interaction_id, interaction_token);

    json_node_t *body = json_new_object();
    json_object_set(body, "type", json_new_number(DISCORD_CB_DEFERRED_UPDATE));

    http_response_t *resp = discord_post(http, url, body);
    json_free(body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* Respond to an interaction with a modal. */
bool discord_show_modal(http_client_t *http,
                         const char *interaction_id,
                         const char *interaction_token,
                         const char *custom_id,
                         const char *title,
                         json_node_t *components) {
    if (!http || !interaction_id || !interaction_token) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/interactions/%s/%s/callback",
             DISCORD_API, interaction_id, interaction_token);

    json_node_t *body = json_new_object();
    json_object_set(body, "type", json_new_number(DISCORD_CB_MODAL));
    json_node_t *data = json_new_object();
    json_object_set(data, "custom_id", json_new_string(custom_id ? custom_id : "modal"));
    json_object_set(data, "title", json_new_string(title ? title : "Hermes"));
    if (components)
        json_object_set(data, "components", json_copy(components));
    else
        json_object_set(data, "components", json_new_array());
    json_object_set(body, "data", data);

    http_response_t *resp = discord_post(http, url, body);
    json_free(body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  E52: Typing with graceful 429 handling — rate-limit aware typing
 * ================================================================ */

/* Track last typing time per channel for rate limit avoidance */
#define DISCORD_TYPING_TRACKER_MAX 32
static struct {
    char   channel[128];
    double last_typing; /* monotonic time */
} g_typing_tracker[DISCORD_TYPING_TRACKER_MAX];
static int g_typing_tracker_count = 0;
static pthread_mutex_t g_typing_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Send typing indicator with 429 avoidance.
 * Discord rate-limits typing to once per ~8 seconds per channel.
 * Returns true if typing was sent, false if skipped (rate limited) or failed. */
bool discord_send_typing_graceful(http_client_t *http, const char *channel) {
    if (!http || !channel) return false;

    double now = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;

    pthread_mutex_lock(&g_typing_mutex);

    /* Check if we recently sent typing for this channel */
    bool should_send = true;
    for (int i = 0; i < g_typing_tracker_count; i++) {
        if (strcmp(g_typing_tracker[i].channel, channel) == 0) {
            if (now - g_typing_tracker[i].last_typing < 7.0) {
                should_send = false; /* Still within cooldown */
            }
            g_typing_tracker[i].last_typing = now;
            break;
        }
    }

    if (should_send && g_typing_tracker_count < DISCORD_TYPING_TRACKER_MAX) {
        snprintf(g_typing_tracker[g_typing_tracker_count].channel,
                 sizeof(g_typing_tracker[0].channel), "%s", channel);
        g_typing_tracker[g_typing_tracker_count].last_typing = now;
        g_typing_tracker_count++;
    }

    pthread_mutex_unlock(&g_typing_mutex);

    if (!should_send) return false; /* Rate limited — skip silently */

    char url[1024];
    snprintf(url, sizeof(url), "%s/channels/%s/typing", DISCORD_API, channel);

    char headers[1024];
    discord_build_headers(headers, sizeof(headers), false);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, NULL, 0);
    if (resp) {
        bool ok = resp->status == 200 || resp->status == 204;
        if (resp->status == 429) {
            /* Rate limited by Discord — reset tracker */
            pthread_mutex_lock(&g_typing_mutex);
            for (int i = 0; i < g_typing_tracker_count; i++) {
                if (strcmp(g_typing_tracker[i].channel, channel) == 0) {
                    g_typing_tracker[i].last_typing = now + 10.0; /* Extend cooldown */
                    break;
                }
            }
            pthread_mutex_unlock(&g_typing_mutex);
        }
        http_response_free(resp);
        return ok;
    }
    return false;
}
