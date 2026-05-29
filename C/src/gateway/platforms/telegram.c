/*
 * telegram.c — Telegram Bot API platform adapter for Hermes C.
 * Handles message formatting, sending, and receiving.
 * Uses the Telegram Bot API via HTTP.
 *
 * P104: Full feature parity — inline queries, callback queries,
 * polls, forum topics, media groups, message editing.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ================================================================
 *  Bot identity
 * ================================================================ */

static char bot_username[128] = "";

void telegram_set_username(const char *username) {
    if (username) snprintf(bot_username, sizeof(bot_username), "%s", username);
}

const char *telegram_get_username(void) {
    return bot_username;
}

/* L08: Check if message has @mention of the bot in entities or text.
 * Returns true if the bot is directly mentioned. */
bool telegram_is_mentioned(json_node_t *update) {
    if (!update || !bot_username[0]) return true; /* No username = assume mentioned */

    json_node_t *msg = json_obj_get(update, "message");
    if (!msg) {
        msg = json_obj_get(update, "edited_message");
        if (!msg) return true; /* Non-message updates always processed */
    }

    /* Check entities for mention type */
    json_node_t *entities = json_obj_get(msg, "entities");
    if (entities) {
        size_t n = json_len(entities);
        for (size_t i = 0; i < n; i++) {
            json_node_t *ent = json_get(entities, i);
            const char *type = json_get_str(ent, "type", "");
            if (strcmp(type, "mention") == 0) {
                const char *txt = json_get_str(msg, "text", "");
                if (!txt) continue;
                long offset = (long)json_get_num(ent, "offset", -1);
                long length = (long)json_get_num(ent, "length", 0);
                if (offset >= 0 && length > 0) {
                    char mention[128] = "";
                    size_t slen = strlen(txt);
                    if ((size_t)offset < slen) {
                        size_t mlen = (size_t)length;
                        if (mlen > sizeof(mention) - 1) mlen = sizeof(mention) - 1;
                        memcpy(mention, txt + offset, mlen);
                        mention[mlen] = '\0';
                        /* Check if mention matches @bot_username */
                        if (mention[0] == '@' &&
                            strcasecmp(mention + 1, bot_username) == 0)
                            return true;
                    }
                }
            }
        }
    }

    /* Also check text directly for @bot_username */
    const char *text = json_get_str(msg, "text", NULL);
    if (text) {
        char at_mention[256];
        snprintf(at_mention, sizeof(at_mention), "@%s", bot_username);
        if (strstr(text, at_mention))
            return true;
    }

    return false;
}

/* L08: Check if the message is from a group chat.
 * Returns true if chat type is group, supergroup, or channel. */
bool telegram_is_group(json_node_t *update) {
    if (!update) return false;
    json_node_t *msg = json_obj_get(update, "message");
    if (!msg) {
        msg = json_obj_get(update, "edited_message");
        if (!msg) return false;
    }
    json_node_t *chat = json_obj_get(msg, "chat");
    if (!chat) return false;
    const char *type = json_get_str(chat, "type", "private");
    return (strcmp(type, "group") == 0 || strcmp(type, "supergroup") == 0);
}

/* ================================================================
 *  Telegram API URLs
 * ================================================================ */

static char api_base[512] = "https://api.telegram.org/bot";

void telegram_set_token(const char *token) {
    snprintf(api_base, sizeof(api_base), "https://api.telegram.org/bot%s", token);
}

/* Fetch bot info from /getMe. Returns true on success, sets bot_username. */
bool telegram_get_me(http_client_t *http) {
    if (!http) return false;
    char url[512];
    snprintf(url, sizeof(url), "%s/getMe", api_base);
    http_response_t *resp = http_request(http, HTTP_GET, url, NULL, NULL, 0);
    if (!resp) return false;
    char *err = NULL;
    json_node_t *j = json_parse(resp->body, &err);
    http_response_free(resp);
    if (!j) { free(err); return false; }
    bool ok = json_get_bool(j, "ok", false);
    if (ok) {
        json_node_t *user = json_obj_get(j, "result");
        if (user) {
            const char *uname = json_get_str(user, "username", NULL);
            if (uname) telegram_set_username(uname);
        }
    }
    json_free(j); free(err);
    return ok;
}

/* ================================================================
 *  P104: Helper — POST JSON to Telegram API
 * ================================================================ */

static http_response_t *tg_post(http_client_t *http, const char *method,
                                 json_node_t *body) {
    char url[512];
    snprintf(url, sizeof(url), "%s/%s", api_base, method);
    char *payload = json_serialize(body);
    json_free(body);
    http_response_t *resp = http_request_json(http, HTTP_POST, url, payload);
    free(payload);
    return resp;
}

/* ================================================================
 *  Send message
 * ================================================================ */

/* D06: Telegram sendMessage wrapper. Returns true on HTTP 200.
 * Supports parse_mode (HTML/Markdown) and thread_id (topic threads).
 * Optional disable_preview disables link previews in the message. */
bool telegram_send_message(http_client_t *http, const char *chat_id,
                            const char *text, const char *parse_mode,
                            const char *thread_id, bool disable_preview)
{
    if (!http || !chat_id || !text) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "text", json_new_string(text));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));
    if (thread_id)
        json_object_set(body, "message_thread_id", json_new_string(thread_id));
    if (disable_preview)
        json_object_set(body, "disable_web_page_preview", json_new_bool(true));

    http_response_t *resp = tg_post(http, "sendMessage", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P104: Send message with reply markup (inline keyboards)
 * ================================================================ */

bool telegram_send_message_with_keyboard(http_client_t *http,
                                          const char *chat_id,
                                          const char *text,
                                          const char *parse_mode,
                                          const char *thread_id,
                                          json_node_t *reply_markup,
                                          bool disable_preview)
{
    if (!http || !chat_id || !text) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "text", json_new_string(text));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));
    if (thread_id)
        json_object_set(body, "message_thread_id", json_new_string(thread_id));
    if (reply_markup)
        json_object_set(body, "reply_markup", json_copy(reply_markup));
    if (disable_preview)
        json_object_set(body, "disable_web_page_preview", json_new_bool(true));

    http_response_t *resp = tg_post(http, "sendMessage", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P104: Edit message text
 * ================================================================ */

bool telegram_edit_message_text(http_client_t *http, const char *chat_id,
                                 const char *message_id, const char *text,
                                 const char *parse_mode)
{
    if (!http || !chat_id || !message_id || !text) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "message_id", json_new_string(message_id));
    json_object_set(body, "text", json_new_string(text));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));

    http_response_t *resp = tg_post(http, "editMessageText", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P104: Delete message
 * ================================================================ */

bool telegram_delete_message(http_client_t *http, const char *chat_id,
                              const char *message_id)
{
    if (!http || !chat_id || !message_id) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "message_id", json_new_string(message_id));

    http_response_t *resp = tg_post(http, "deleteMessage", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Send typing indicator
 * ================================================================ */

bool telegram_send_chat_action(http_client_t *http, const char *chat_id,
                                const char *action)
{
    if (!http || !chat_id || !action) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "action", json_new_string(action));

    http_response_t *resp = tg_post(http, "sendChatAction", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* E13: Async typing indicator — sends typing in background thread */
#include <pthread.h>

typedef struct {
    http_client_t *http;
    char chat_id[64];
    volatile bool running;
    pthread_t thread;
} typing_state_t;

static void *typing_thread_func(void *arg) {
    typing_state_t *state = (typing_state_t *)arg;
    while (state->running) {
        telegram_send_chat_action(state->http, state->chat_id, "typing");
        /* Sleep 5 seconds (Telegram's typing indicator lasts ~5-6s) */
        for (int i = 0; i < 50 && state->running; i++)
            usleep(100000); /* 100ms × 50 = 5s */
    }
    return NULL;
}

typing_state_t *telegram_start_typing(http_client_t *http, const char *chat_id) {
    if (!http || !chat_id) return NULL;
    typing_state_t *state = calloc(1, sizeof(typing_state_t));
    if (!state) return NULL;
    state->http = http;
    snprintf(state->chat_id, sizeof(state->chat_id), "%s", chat_id);
    state->running = true;
    if (pthread_create(&state->thread, NULL, typing_thread_func, state) != 0) {
        free(state);
        return NULL;
    }
    pthread_detach(state->thread);
    return state;
}

void telegram_stop_typing(typing_state_t *state) {
    if (!state) return;
    state->running = false;
    /* Thread is detached, will exit on next interval check */
    /* Don't free immediately — thread may still be running */
    /* The thread owns this memory; on next loop it sees running=false and exits */
}

/* ================================================================
 *  P104: Answer inline query
 * ================================================================ */

bool telegram_answer_inline_query(http_client_t *http, const char *inline_query_id,
                                   json_node_t *results)
{
    if (!http || !inline_query_id) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "inline_query_id", json_new_string(inline_query_id));
    if (results)
        json_object_set(body, "results", json_copy(results));
    else
        json_object_set(body, "results", json_new_array());

    http_response_t *resp = tg_post(http, "answerInlineQuery", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P104: Answer callback query
 * ================================================================ */

bool telegram_answer_callback_query(http_client_t *http, const char *callback_query_id,
                                     const char *text, bool show_alert)
{
    if (!http || !callback_query_id) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "callback_query_id", json_new_string(callback_query_id));
    if (text)
        json_object_set(body, "text", json_new_string(text));
    json_object_set(body, "show_alert", json_new_bool(show_alert));

    http_response_t *resp = tg_post(http, "answerCallbackQuery", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P104: Send poll
 * ================================================================ */

bool telegram_send_poll(http_client_t *http, const char *chat_id,
                         const char *question, json_node_t *options,
                         bool is_anonymous, const char *poll_type,
                         bool is_closed)
{
    if (!http || !chat_id || !question) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "question", json_new_string(question));
    if (options)
        json_object_set(body, "options", json_copy(options));
    else
        json_object_set(body, "options", json_new_array());
    json_object_set(body, "is_anonymous", json_new_bool(is_anonymous));
    if (poll_type)
        json_object_set(body, "type", json_new_string(poll_type));
    json_object_set(body, "is_closed", json_new_bool(is_closed));

    http_response_t *resp = tg_post(http, "sendPoll", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P104: Send media group (photos with captions)
 * ================================================================ */

bool telegram_send_media_group(http_client_t *http, const char *chat_id,
                                json_node_t *media)
{
    if (!http || !chat_id) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    if (media)
        json_object_set(body, "media", json_copy(media));
    else
        json_object_set(body, "media", json_new_array());

    http_response_t *resp = tg_post(http, "sendMediaGroup", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  E01-E05: Media send methods (sendPhoto, sendDocument, etc.)
 * ================================================================ */

bool telegram_send_photo(http_client_t *http, const char *chat_id,
                          const char *photo, const char *caption,
                          const char *parse_mode)
{
    if (!http || !chat_id || !photo) return false;
    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "photo", json_new_string(photo));
    if (caption)
        json_object_set(body, "caption", json_new_string(caption));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));
    http_response_t *resp = tg_post(http, "sendPhoto", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

bool telegram_send_document(http_client_t *http, const char *chat_id,
                             const char *document, const char *caption,
                             const char *parse_mode)
{
    if (!http || !chat_id || !document) return false;
    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "document", json_new_string(document));
    if (caption)
        json_object_set(body, "caption", json_new_string(caption));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));
    http_response_t *resp = tg_post(http, "sendDocument", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

bool telegram_send_voice(http_client_t *http, const char *chat_id,
                          const char *voice, const char *caption,
                          const char *parse_mode)
{
    if (!http || !chat_id || !voice) return false;
    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "voice", json_new_string(voice));
    if (caption)
        json_object_set(body, "caption", json_new_string(caption));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));
    http_response_t *resp = tg_post(http, "sendVoice", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

bool telegram_send_video(http_client_t *http, const char *chat_id,
                          const char *video, const char *caption,
                          const char *parse_mode)
{
    if (!http || !chat_id || !video) return false;
    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "video", json_new_string(video));
    if (caption)
        json_object_set(body, "caption", json_new_string(caption));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));
    http_response_t *resp = tg_post(http, "sendVideo", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

bool telegram_send_animation(http_client_t *http, const char *chat_id,
                              const char *animation, const char *caption,
                              const char *parse_mode)
{
    if (!http || !chat_id || !animation) return false;
    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "animation", json_new_string(animation));
    if (caption)
        json_object_set(body, "caption", json_new_string(caption));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));
    http_response_t *resp = tg_post(http, "sendAnimation", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  E14: Forward message
 * ================================================================ */

bool telegram_forward_message(http_client_t *http, const char *chat_id,
                               const char *from_chat_id,
                               const char *message_id)
{
    if (!http || !chat_id || !from_chat_id || !message_id) return false;
    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "from_chat_id", json_new_string(from_chat_id));
    json_object_set(body, "message_id", json_new_string(message_id));
    http_response_t *resp = tg_post(http, "forwardMessage", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  E15: Pin/unpin message
 * ================================================================ */

bool telegram_pin_chat_message(http_client_t *http, const char *chat_id,
                                const char *message_id)
{
    if (!http || !chat_id || !message_id) return false;
    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "message_id", json_new_string(message_id));
    http_response_t *resp = tg_post(http, "pinChatMessage", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

bool telegram_unpin_chat_message(http_client_t *http, const char *chat_id,
                                  const char *message_id)
{
    if (!http || !chat_id || !message_id) return false;
    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "message_id", json_new_string(message_id));
    http_response_t *resp = tg_post(http, "unpinChatMessage", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  E16: Message reactions
 * ================================================================ */

bool telegram_set_message_reaction(http_client_t *http, const char *chat_id,
                                    const char *message_id, const char *emoji)
{
    if (!http || !chat_id || !message_id) return false;
    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "message_id", json_new_string(message_id));

    if (emoji && *emoji) {
        json_node_t *reaction = json_new_array();
        json_node_t *r = json_new_object();
        json_object_set(r, "type", json_new_string("emoji"));
        json_object_set(r, "emoji", json_new_string(emoji));
        json_array_append(reaction, r);
        json_object_set(body, "reaction", reaction);
    }

    http_response_t *resp = tg_post(http, "setMessageReaction", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

bool telegram_create_forum_topic(http_client_t *http, const char *chat_id,
                                  const char *name)
{
    if (!http || !chat_id || !name) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "name", json_new_string(name));

    http_response_t *resp = tg_post(http, "createForumTopic", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

bool telegram_edit_forum_topic(http_client_t *http, const char *chat_id,
                                const char *message_thread_id, const char *name)
{
    if (!http || !chat_id || !message_thread_id) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "message_thread_id", json_new_string(message_thread_id));
    if (name)
        json_object_set(body, "name", json_new_string(name));

    http_response_t *resp = tg_post(http, "editForumTopic", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

bool telegram_close_forum_topic(http_client_t *http, const char *chat_id,
                                 const char *message_thread_id)
{
    if (!http || !chat_id || !message_thread_id) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "message_thread_id", json_new_string(message_thread_id));

    http_response_t *resp = tg_post(http, "closeForumTopic", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

bool telegram_reopen_forum_topic(http_client_t *http, const char *chat_id,
                                  const char *message_thread_id)
{
    if (!http || !chat_id || !message_thread_id) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "message_thread_id", json_new_string(message_thread_id));

    http_response_t *resp = tg_post(http, "reopenForumTopic", body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Get updates (used by poll loop in server.c)
 * ================================================================ */

json_node_t *telegram_get_updates(http_client_t *http, int offset, int timeout)
{
    if (!http) return NULL;

    json_node_t *body = json_new_object();
    json_object_set(body, "offset", json_new_number((double)offset));
    json_object_set(body, "timeout", json_new_number((double)timeout));
    /* P104: Allow all update types */
    json_node_t *allowed = json_new_array();
    json_array_append(allowed, json_new_string("message"));
    json_array_append(allowed, json_new_string("inline_query"));
    json_array_append(allowed, json_new_string("callback_query"));
    json_array_append(allowed, json_new_string("poll"));
    json_array_append(allowed, json_new_string("poll_answer"));
    json_array_append(allowed, json_new_string("my_chat_member"));
    json_object_set(body, "allowed_updates", allowed);

    http_response_t *resp = tg_post(http, "getUpdates", body);
    if (!resp || !resp->body) return NULL;

    json_node_t *root = json_parse(resp->body, NULL);
    http_resp_free(resp);
    return root;
}

/* ================================================================
 *  Extract message info from update
 *  P104: Handle all update types
 * ================================================================ */

/* Get message text from any update type */
const char *telegram_get_text(json_node_t *update) {
    if (!update) return NULL;

    /* Inline query */
    json_node_t *inline_q = json_object_get(update, "inline_query");
    if (inline_q)
        return json_object_get_string(inline_q, "query", NULL);

    /* Callback query */
    json_node_t *cb = json_object_get(update, "callback_query");
    if (cb) {
        /* Return the data from the callback */
        const char *data = json_object_get_string(cb, "data", NULL);
        if (data) return data;
        /* Fallback: return the message text */
        json_node_t *cb_msg = json_object_get(cb, "message");
        if (cb_msg)
            return json_object_get_string(cb_msg, "text", NULL);
        return NULL;
    }

    /* Regular message */
    json_node_t *msg = json_object_get(update, "message");
    if (!msg) {
        /* Edited message */
        msg = json_object_get(update, "edited_message");
        if (!msg) return NULL;
    }

    /* Try text first */
    const char *text = json_object_get_string(msg, "text", NULL);
    if (text) return text;

    /* E17: Sticker */
    json_node_t *sticker = json_object_get(msg, "sticker");
    if (sticker) {
        static char sticker_buf[256];
        const char *emoji = json_object_get_string(sticker, "emoji", "");
        const char *set_name = json_object_get_string(sticker, "set_name", "");
        snprintf(sticker_buf, sizeof(sticker_buf), "Sticker%s%s%s%s",
                 emoji[0] ? " " : "", emoji,
                 set_name[0] ? " [" : "", set_name[0] ? set_name : "",
                 set_name[0] ? "]" : "");
        /* If neither emoji nor set name, use file_id */
        if (!emoji[0] && !set_name[0]) {
            const char *fid = json_object_get_string(sticker, "file_id", "");
            snprintf(sticker_buf, sizeof(sticker_buf), "Sticker: %s", fid);
        }
        return sticker_buf;
    }

    /* E18: Animation (GIF) */
    json_node_t *animation = json_object_get(msg, "animation");
    if (animation) {
        static char anim_buf[128];
        const char *fname = json_object_get_string(animation, "file_name", "");
        const char *mime = json_object_get_string(animation, "mime_type", "");
        snprintf(anim_buf, sizeof(anim_buf), "Animation%s%s%s%s",
                 fname[0] ? " " : "", fname,
                 mime[0] ? " (" : "", mime[0] ? mime : "",
                 mime[0] ? ")" : "");
        return anim_buf;
    }

    /* E19: Voice message */
    json_node_t *voice = json_object_get(msg, "voice");
    if (voice) {
        double duration = json_object_get_number(voice, "duration", 0);
        static char voice_buf[64];
        snprintf(voice_buf, sizeof(voice_buf), "Voice message (%.0f sec)", duration);
        return voice_buf;
    }

    /* E20: Video */
    json_node_t *video = json_object_get(msg, "video");
    if (video) {
        const char *caption = json_object_get_string(msg, "caption", NULL);
        static char vid_buf[256];
        if (caption && caption[0])
            snprintf(vid_buf, sizeof(vid_buf), "Video: %s", caption);
        else
            snprintf(vid_buf, sizeof(vid_buf), "Video message");
        return vid_buf;
    }

    /* E21: Audio */
    json_node_t *audio = json_object_get(msg, "audio");
    if (audio) {
        const char *title = json_object_get_string(audio, "title", "");
        const char *performer = json_object_get_string(audio, "performer", "");
        static char audio_buf[256];
        if (title[0] && performer[0])
            snprintf(audio_buf, sizeof(audio_buf), "Audio: %s - %s", performer, title);
        else if (title[0])
            snprintf(audio_buf, sizeof(audio_buf), "Audio: %s", title);
        else
            snprintf(audio_buf, sizeof(audio_buf), "Audio file");
        return audio_buf;
    }

    /* E22: Photo */
    json_node_t *photos = json_object_get(msg, "photo");
    if (photos && json_len(photos) > 0) {
        const char *caption = json_object_get_string(msg, "caption", NULL);
        static char photo_buf[256];
        if (caption && caption[0])
            snprintf(photo_buf, sizeof(photo_buf), "Photo: %s", caption);
        else
            snprintf(photo_buf, sizeof(photo_buf), "Photo");
        return photo_buf;
    }

    /* E23: Location */
    json_node_t *location = json_object_get(msg, "location");
    if (location) {
        double lat = json_object_get_number(location, "latitude", 0);
        double lon = json_object_get_number(location, "longitude", 0);
        static char loc_buf[64];
        snprintf(loc_buf, sizeof(loc_buf), "Location: %.6f, %.6f", lat, lon);
        return loc_buf;
    }

    /* E24: Venue */
    json_node_t *venue = json_object_get(msg, "venue");
    if (venue) {
        const char *title = json_object_get_string(venue, "title", "");
        const char *address = json_object_get_string(venue, "address", "");
        static char venue_buf[256];
        snprintf(venue_buf, sizeof(venue_buf), "Venue: %s, %s", title, address);
        return venue_buf;
    }

    /* E25: Contact */
    json_node_t *contact = json_object_get(msg, "contact");
    if (contact) {
        const char *fn = json_object_get_string(contact, "first_name", "");
        const char *ln = json_object_get_string(contact, "last_name", "");
        const char *phone = json_object_get_string(contact, "phone_number", "");
        static char contact_buf[256];
        if (ln[0])
            snprintf(contact_buf, sizeof(contact_buf), "Contact: %s %s, %s", fn, ln, phone);
        else
            snprintf(contact_buf, sizeof(contact_buf), "Contact: %s, %s", fn, phone);
        return contact_buf;
    }

    /* E26: Poll/Quiz */
    json_node_t *poll = json_object_get(msg, "poll");
    if (poll) {
        const char *question = json_object_get_string(poll, "question", "");
        static char poll_buf[256];
        /* Check if it's a quiz by looking for correct_option_id */
        double corr_opt = json_object_get_number(poll, "correct_option_id", -1);
        if (corr_opt >= 0)
            snprintf(poll_buf, sizeof(poll_buf), "Quiz: %s", question);
        else
            snprintf(poll_buf, sizeof(poll_buf), "Poll: %s", question);
        return poll_buf;
    }

    return NULL;
}

const char *telegram_get_chat_id(json_node_t *update) {
    static char buf[32];
    if (!update) return NULL;

    /* Inline query */
    json_node_t *inline_q = json_object_get(update, "inline_query");
    if (inline_q) {
        json_node_t *from = json_object_get(inline_q, "from");
        if (from) {
            double id = json_object_get_number(from, "id", 0);
            snprintf(buf, sizeof(buf), "%.0f", id);
            return buf;
        }
    }

    /* Callback query */
    json_node_t *cb = json_object_get(update, "callback_query");
    if (cb) {
        /* Return the inline message ID or the original message chat ID */
        json_node_t *cb_msg = json_object_get(cb, "message");
        if (cb_msg) {
            json_node_t *chat = json_object_get(cb_msg, "chat");
            if (chat) {
                double id = json_object_get_number(chat, "id", 0);
                snprintf(buf, sizeof(buf), "%.0f", id);
                return buf;
            }
        }
        json_node_t *from = json_object_get(cb, "from");
        if (from) {
            double id = json_object_get_number(from, "id", 0);
            snprintf(buf, sizeof(buf), "%.0f", id);
            return buf;
        }
    }

    /* Poll answer */
    json_node_t *poll = json_object_get(update, "poll_answer");
    if (poll) {
        json_node_t *user = json_object_get(poll, "user");
        if (user) {
            double id = json_object_get_number(user, "id", 0);
            snprintf(buf, sizeof(buf), "%.0f", id);
            return buf;
        }
    }

    /* Regular message */
    json_node_t *msg = json_object_get(update, "message");
    if (!msg)
        msg = json_object_get(update, "edited_message");
    if (!msg) return NULL;

    json_node_t *chat = json_object_get(msg, "chat");
    if (!chat) return NULL;

    double id = json_object_get_number(chat, "id", 0);
    snprintf(buf, sizeof(buf), "%.0f", id);
    return buf;
}

/* E17-E26: Get message type identifier — returns static string
 * like "text", "sticker", "animation", "voice", "video", "audio",
 * "photo", "location", "venue", "contact", "poll", "quiz". */
const char *telegram_get_update_type(json_node_t *update) {
    if (!update) return NULL;

    /* Check each update type in priority order */
    if (json_object_get(update, "inline_query")) return "inline_query";
    if (json_object_get(update, "callback_query")) return "callback_query";
    if (json_object_get(update, "poll_answer")) return "poll_answer";

    json_node_t *msg = json_object_get(update, "message");
    if (!msg) msg = json_object_get(update, "edited_message");
    if (!msg) return NULL;

    if (json_object_get_string(msg, "text", NULL))               return "text";
    if (json_object_get(msg, "sticker"))                         return "sticker";
    if (json_object_get(msg, "animation"))                       return "animation";
    if (json_object_get(msg, "voice"))                           return "voice";
    if (json_object_get(msg, "video"))                           return "video";
    if (json_object_get(msg, "audio"))                           return "audio";
    if (json_object_get(msg, "photo") && json_len(json_object_get(msg, "photo")) > 0)
                                                                 return "photo";
    if (json_object_get(msg, "location"))                        return "location";
    if (json_object_get(msg, "venue"))                           return "venue";
    if (json_object_get(msg, "contact"))                         return "contact";
    if (json_object_get(msg, "poll"))                            return "poll";
    return "unknown";
}

/* P104: Get callback query ID for answering */
const char *telegram_get_callback_query_id(json_node_t *update) {
    static char buf[64];
    if (!update) return NULL;
    json_node_t *cb = json_object_get(update, "callback_query");
    if (!cb) return NULL;
    const char *id = json_object_get_string(cb, "id", NULL);
    if (!id) return NULL;
    snprintf(buf, sizeof(buf), "%s", id);
    return buf;
}

/* P104: Get inline query ID */
const char *telegram_get_inline_query_id(json_node_t *update) {
    static char buf[64];
    if (!update) return NULL;
    json_node_t *inline_q = json_object_get(update, "inline_query");
    if (!inline_q) return NULL;
    const char *id = json_object_get_string(inline_q, "id", NULL);
    if (!id) return NULL;
    snprintf(buf, sizeof(buf), "%s", id);
    return buf;
}

/* P104: Get message thread ID for forum topics */
const char *telegram_get_message_thread_id(json_node_t *update) {
    static char buf[32];
    if (!update) return NULL;
    json_node_t *msg = json_object_get(update, "message");
    if (!msg) return NULL;
    double id = json_object_get_number(msg, "message_thread_id", 0);
    if (id == 0) return NULL;
    snprintf(buf, sizeof(buf), "%.0f", id);
    return buf;
}

/* ================================================================
 *  E07-E12: Interactive Telegram send methods
 * ================================================================ */

/* E07: Send editable draft with placeholder text (no keyboard) */
bool telegram_send_draft(http_client_t *http, const char *chat_id,
                          const char *text, const char *parse_mode)
{
    return telegram_send_message_with_keyboard(http, chat_id, text, parse_mode, NULL, NULL, false);
}
/* E08: Send clarification prompt with inline Yes/No/Explain buttons */
bool telegram_send_clarify(http_client_t *http, const char *chat_id,
                            const char *question, const char **options, int n_options,
                            const char *parse_mode)
{
    if (!http || !chat_id || !question) return false;

    json_node_t *keyboard = json_new_object();
    json_node_t *rows = json_new_array();
    json_node_t *row = json_new_array();
    for (int i = 0; i < n_options && i < 4; i++) {
        json_node_t *btn = json_new_object();
        json_object_set(btn, "text", json_new_string(options[i]));
        json_object_set(btn, "callback_data", json_new_string(options[i]));
        json_array_append(row, btn);
    }
    json_array_append(rows, row);
    json_object_set(keyboard, "inline_keyboard", rows);

    bool ok = telegram_send_message_with_keyboard(http, chat_id, question, parse_mode, NULL, keyboard, false);
    json_free(keyboard);
    return ok;
}
/* E09: Send dangerous command approval prompt with Approve/Deny buttons */
bool telegram_send_approval_prompt(http_client_t *http, const char *chat_id,
                                    const char *command, const char *reason,
                                    const char *parse_mode)
{
    if (!http || !chat_id || !command) return false;

    char text[4096];
    if (reason && *reason)
        snprintf(text, sizeof(text), "⚠️ *Approve dangerous command?*\n\n`%s`\n\nReason: %s", command, reason);
    else
        snprintf(text, sizeof(text), "⚠️ *Approve dangerous command?*\n\n`%s`", command);

    json_node_t *keyboard = json_new_object();
    json_node_t *rows = json_new_array();
    json_node_t *row = json_new_array();

    json_node_t *approve = json_new_object();
    json_object_set(approve, "text", json_new_string("✅ Approve"));
    json_object_set(approve, "callback_data", json_new_string("approve"));
    json_array_append(row, approve);

    json_node_t *deny = json_new_object();
    json_object_set(deny, "text", json_new_string("❌ Deny"));
    json_object_set(deny, "callback_data", json_new_string("deny"));
    json_array_append(row, deny);

    json_array_append(rows, row);
    json_object_set(keyboard, "inline_keyboard", rows);

    bool ok = telegram_send_message_with_keyboard(http, chat_id, text, "Markdown", NULL, keyboard, false);
    json_free(keyboard);
    return ok;
}
/* E10: Send slash command confirmation with Confirm/Cancel buttons */
bool telegram_send_confirm_prompt(http_client_t *http, const char *chat_id,
                                   const char *action, const char *detail,
                                   const char *parse_mode)
{
    if (!http || !chat_id || !action) return false;

    char text[4096];
    if (detail && *detail)
        snprintf(text, sizeof(text), "Confirm: %s\n\n%s", action, detail);
    else
        snprintf(text, sizeof(text), "Confirm: %s", action);

    json_node_t *keyboard = json_new_object();
    json_node_t *rows = json_new_array();
    json_node_t *row = json_new_array();

    json_node_t *confirm = json_new_object();
    json_object_set(confirm, "text", json_new_string("✅ Confirm"));
    json_object_set(confirm, "callback_data", json_new_string("confirm"));
    json_array_append(row, confirm);

    json_node_t *cancel = json_new_object();
    json_object_set(cancel, "text", json_new_string("❌ Cancel"));
    json_object_set(cancel, "callback_data", json_new_string("cancel"));
    json_array_append(row, cancel);

    json_array_append(rows, row);
    json_object_set(keyboard, "inline_keyboard", rows);

    bool ok = telegram_send_message_with_keyboard(http, chat_id, text,
                parse_mode ? parse_mode : "Markdown", NULL, keyboard, false);
    json_free(keyboard);
    return ok;
}

/* E11: Send model picker with inline keyboard of models */
bool telegram_send_model_picker(http_client_t *http, const char *chat_id,
                                 const char **models, int n_models,
                                 const char *current_model)
{
    if (!http || !chat_id || !models || n_models <= 0) return false;

    char text[512];
    if (current_model && *current_model)
        snprintf(text, sizeof(text), "Select model (current: %s):", current_model);
    else
        snprintf(text, sizeof(text), "Select model:");

    json_node_t *keyboard = json_new_object();
    json_node_t *rows = json_new_array();
    json_node_t *row = json_new_array();
    int items_in_row = 0;

    for (int i = 0; i < n_models; i++) {
        json_node_t *btn = json_new_object();
        char label[128];
        if (current_model && strcmp(models[i], current_model) == 0)
            snprintf(label, sizeof(label), "✓ %s", models[i]);
        else
            snprintf(label, sizeof(label), "%s", models[i]);
        json_object_set(btn, "text", json_new_string(label));
        json_object_set(btn, "callback_data", json_new_string(models[i]));
        json_array_append(row, btn);
        items_in_row++;

        /* Max 2 per row for readability */
        if (items_in_row >= 2 || i == n_models - 1) {
            json_array_append(rows, row);
            row = json_new_array();
            items_in_row = 0;
        }
    }
    json_object_set(keyboard, "inline_keyboard", rows);

    bool ok = telegram_send_message_with_keyboard(http, chat_id, text, NULL, NULL, keyboard, false);
    json_free(keyboard);
    return ok;
}
/* E12: Send update prompt with diff + Apply/Dismiss buttons */
bool telegram_send_update_prompt(http_client_t *http, const char *chat_id,
                                  const char *diff_text, const char *summary,
                                  const char *parse_mode)
{
    if (!http || !chat_id || !diff_text) return false;

    char text[4096];
    if (summary && *summary)
        snprintf(text, sizeof(text), "*Update available:* %s\n\n```diff\n%s\n```", summary, diff_text);
    else
        snprintf(text, sizeof(text), "*Update:*\n```diff\n%s\n```", diff_text);

    json_node_t *keyboard = json_new_object();
    json_node_t *rows = json_new_array();
    json_node_t *row = json_new_array();

    json_node_t *apply = json_new_object();
    json_object_set(apply, "text", json_new_string("✅ Apply"));
    json_object_set(apply, "callback_data", json_new_string("apply"));
    json_array_append(row, apply);

    json_node_t *dismiss = json_new_object();
    json_object_set(dismiss, "text", json_new_string("❌ Dismiss"));
    json_object_set(dismiss, "callback_data", json_new_string("dismiss"));
    json_array_append(row, dismiss);

    json_array_append(rows, row);
    json_object_set(keyboard, "inline_keyboard", rows);

    bool ok = telegram_send_message_with_keyboard(http, chat_id, text,
                parse_mode ? parse_mode : "Markdown", NULL, keyboard, false);
    json_free(keyboard);
    return ok;
}
