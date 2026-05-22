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

/* ================================================================
 *  Telegram API URLs
 * ================================================================ */

static char api_base[512] = "https://api.telegram.org/bot";

void telegram_set_token(const char *token) {
    snprintf(api_base, sizeof(api_base), "https://api.telegram.org/bot%s", token);
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

bool telegram_send_message(http_client_t *http, const char *chat_id,
                            const char *text, const char *parse_mode)
{
    if (!http || !chat_id || !text) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "text", json_new_string(text));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));

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
                                          json_node_t *reply_markup)
{
    if (!http || !chat_id || !text) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "text", json_new_string(text));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));
    if (reply_markup)
        json_object_set(body, "reply_markup", json_copy(reply_markup));

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

    return tg_post(http, "getUpdates", body);
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
    return json_object_get_string(msg, "text", NULL);
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

/* P104: Get update type string for routing */
const char *telegram_get_update_type(json_node_t *update) {
    if (!update) return "unknown";
    if (json_object_get(update, "message")) return "message";
    if (json_object_get(update, "edited_message")) return "edited_message";
    if (json_object_get(update, "inline_query")) return "inline_query";
    if (json_object_get(update, "callback_query")) return "callback_query";
    if (json_object_get(update, "poll")) return "poll";
    if (json_object_get(update, "poll_answer")) return "poll_answer";
    if (json_object_get(update, "my_chat_member")) return "my_chat_member";
    return "unknown";
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
    return telegram_send_message_with_keyboard(http, chat_id, text, parse_mode, NULL);
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

    bool ok = telegram_send_message_with_keyboard(http, chat_id, question, parse_mode, keyboard);
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

    bool ok = telegram_send_message_with_keyboard(http, chat_id, text, "Markdown", keyboard);
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
                parse_mode ? parse_mode : "Markdown", keyboard);
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

    bool ok = telegram_send_message_with_keyboard(http, chat_id, text, NULL, keyboard);
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
                parse_mode ? parse_mode : "Markdown", keyboard);
    json_free(keyboard);
    return ok;
}
