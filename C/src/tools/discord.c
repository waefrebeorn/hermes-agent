/*
 * discord.c — P41: Discord server introspection and management tool.
 *
 * Port of Python hermes-agent tools/discord_tool.py.
 * Uses Discord REST API v10 with Bot token auth.
 * Single "discord" tool with action dispatch.
 *
 * Actions:
 *   list_guilds, guild_info, list_channels, channel_info,
 *   list_roles, member_info, send_message, fetch_messages, delete_message,
 *   create_channel, kick_member, ban_member, search_members, list_pins,
 *   pin_message, unpin_message, create_thread, add_role, remove_role
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_tool_helpers.h"
#include "hermes_tool_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define DISCORD_API_BASE "https://discord.com/api/v10"

/* ================================================================
 *  Error helpers
 * ================================================================ */

static char *json_error(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg));
    char *s = json_serialize(o);
    json_free(o);
    return s;
}

static char *json_errorf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return json_error(buf);
}

/* ================================================================
 *  Auth
 * ================================================================ */

static const char *get_token(void) {
    return tool_config_get_token("discord");
}

/* Build "Bot <token>" auth header */
static char *build_bot_header(void) {
    const char *token = get_token();
    if (!token) return NULL;
    size_t len = strlen(token) + 32;
    char *hdr = (char *)malloc(len);
    if (!hdr) return NULL;
    snprintf(hdr, len, "Authorization: Bot %s\nUser-Agent: Hermes-C (discord-tool)", token);
    return hdr;
}

/* ================================================================
 *  JSON response builders
 * ================================================================ */

static char *list_guilds(void) {
    char url[512];
    snprintf(url, sizeof(url), "%s/users/@me/guilds", DISCORD_API_BASE);
    tool_api_result_t *r = tool_api_call(url, NULL, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = strdup(r->error ? r->error : "Failed to list guilds");
        tool_api_result_free(r);
        return err;
    }
    /* Build simplified output */
    json_t *result = json_object();
    json_t *guilds_arr = json_array();
    if (r->json) {
        size_t len = json_len(r->json);
        for (size_t i = 0; i < len; i++) {
            json_t *g = json_get(r->json, i);
            if (!g) continue;
            json_t *entry = json_object();
            json_set(entry, "id", json_copy(json_obj_get(g, "id")));
            json_set(entry, "name", json_copy(json_obj_get(g, "name")));
            json_set(entry, "owner", json_copy(json_obj_get(g, "owner")));
            json_append(guilds_arr, entry);
        }
    }
    json_set(result, "guilds", guilds_arr);
    json_set(result, "count", json_number((double)json_len(guilds_arr)));
    char *s = json_serialize(result);
    json_free(result);
    tool_api_result_free(r);
    return s;
}

static char *guild_info(const char *guild_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s", DISCORD_API_BASE, guild_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Guild info failed: %s",
                        r->error ? r->error : "unknown");
        tool_api_result_free(r);
        return err;
    }
    /* Pass through relevant fields */
    json_t *result = json_object();
    if (r->json) {
        const char *keys[] = {"id", "name", "description", "owner_id",
            "approximate_member_count", "approximate_presence_count",
            "premium_tier", "verification_level", NULL};
        for (int i = 0; keys[i]; i++) {
            json_t *v = json_obj_get(r->json, keys[i]);
            if (v) json_set(result, keys[i], json_copy(v));
        }
    }
    char *s = json_serialize(result);
    json_free(result);
    tool_api_result_free(r);
    return s;
}

static char *list_channels(const char *guild_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/channels", DISCORD_API_BASE, guild_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("List channels failed: %s",
                        r->error ? r->error : "unknown");
        tool_api_result_free(r);
        return err;
    }
    /* Simplify: list id, name, type for each channel */
    json_t *result = json_object();
    json_t *channels = json_array();
    if (r->json) {
        size_t len = json_len(r->json);
        for (size_t i = 0; i < len; i++) {
            json_t *ch = json_get(r->json, i);
            if (!ch) continue;
            json_t *entry = json_object();
            json_set(entry, "id", json_copy(json_obj_get(ch, "id")));
            json_set(entry, "name", json_copy(json_obj_get(ch, "name")));
            json_set(entry, "type", json_copy(json_obj_get(ch, "type")));
            json_append(channels, entry);
        }
    }
    json_set(result, "channels", channels);
    json_set(result, "count", json_number((double)json_len(channels)));
    char *s = json_serialize(result);
    json_free(result);
    tool_api_result_free(r);
    return s;
}

static char *send_message(const char *channel_id, const char *content) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s/messages", DISCORD_API_BASE, channel_id);

    /* Build JSON body */
    json_t *body = json_object();
    json_set(body, "content", json_string(content ? content : ""));
    char *body_str = json_serialize(body);
    json_free(body);

    tool_api_result_t *r = tool_api_call(url, NULL, "POST", body_str, 15, 2);
    free(body_str);

    if (!tool_api_success(r)) {
        char *err = json_errorf("Send message failed: %s",
                        r->error ? r->error : "unknown");
        tool_api_result_free(r);
        return err;
    }

    /* Return message id + timestamp */
    json_t *result = json_object();
    if (r->json) {
        json_set(result, "id", json_copy(json_obj_get(r->json, "id")));
        json_set(result, "channel_id", json_string(channel_id));
        json_set(result, "timestamp", json_copy(json_obj_get(r->json, "timestamp")));
    }
    char *s = json_serialize(result);
    json_free(result);
    tool_api_result_free(r);
    return s;
}

static char *fetch_messages(const char *channel_id, int limit) {
    char url[512];
    char params[64] = "";
    if (limit > 0)
        snprintf(params, sizeof(params), "?limit=%d", limit > 100 ? 100 : limit);
    snprintf(url, sizeof(url), "%s/channels/%s/messages%s",
             DISCORD_API_BASE, channel_id, params);

    tool_api_result_t *r = tool_api_call(url, NULL, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Fetch messages failed: %s",
                        r->error ? r->error : "unknown");
        tool_api_result_free(r);
        return err;
    }

    json_t *result = json_object();
    json_t *msgs = json_array();
    if (r->json) {
        size_t len = json_len(r->json);
        for (size_t i = 0; i < len; i++) {
            json_t *m = json_get(r->json, i);
            if (!m) continue;
            json_t *entry = json_object();
            json_set(entry, "id", json_copy(json_obj_get(m, "id")));
            json_set(entry, "author", json_copy(json_obj_get(m, "author")));
            json_set(entry, "content", json_copy(json_obj_get(m, "content")));
            json_set(entry, "timestamp", json_copy(json_obj_get(m, "timestamp")));
            json_append(msgs, entry);
        }
    }
    json_set(result, "messages", msgs);
    json_set(result, "count", json_number((double)json_len(msgs)));
    char *s = json_serialize(result);
    json_free(result);
    tool_api_result_free(r);
    return s;
}

/* ================================================================
 *  Additional regular actions (P42)
 * ================================================================ */

static char *channel_info(const char *channel_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s", DISCORD_API_BASE, channel_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Channel info failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    if (r->json) {
        const char *keys[] = {"id","name","type","guild_id","topic","nsfw",
            "position","parent_id","rate_limit_per_user","last_message_id",NULL};
        for (int i = 0; keys[i]; i++) {
            json_t *v = json_obj_get(r->json, keys[i]);
            if (v) json_set(result, keys[i], json_copy(v));
        }
    }
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *list_roles(const char *guild_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/roles", DISCORD_API_BASE, guild_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("List roles failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_t *roles = json_array();
    if (r->json) {
        size_t len = json_len(r->json);
        for (size_t i = 0; i < len; i++) {
            json_t *rl = json_get(r->json, i);
            if (!rl) continue;
            json_t *e = json_object();
            json_set(e, "id", json_copy(json_obj_get(rl, "id")));
            json_set(e, "name", json_copy(json_obj_get(rl, "name")));
            json_set(e, "color", json_copy(json_obj_get(rl, "color")));
            json_set(e, "position", json_copy(json_obj_get(rl, "position")));
            json_set(e, "mentionable", json_copy(json_obj_get(rl, "mentionable")));
            json_append(roles, e);
        }
    }
    json_set(result, "roles", roles);
    json_set(result, "count", json_number((double)json_len(roles)));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *member_info(const char *guild_id, const char *user_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/members/%s", DISCORD_API_BASE, guild_id, user_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Member info failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    if (r->json) {
        json_set(result, "user_id", json_copy(json_obj_get(json_obj_get(r->json, "user"), "id")));
        json_set(result, "username", json_copy(json_obj_get(json_obj_get(r->json, "user"), "username")));
        json_set(result, "nick", json_copy(json_obj_get(r->json, "nick")));
        json_set(result, "roles", json_copy(json_obj_get(r->json, "roles")));
        json_set(result, "joined_at", json_copy(json_obj_get(r->json, "joined_at")));
    }
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *delete_message(const char *channel_id, const char *message_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s/messages/%s", DISCORD_API_BASE, channel_id, message_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "DELETE", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Delete failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[128];
    snprintf(msg, sizeof(msg), "Message %s deleted", message_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

/* ================================================================
 *  Admin actions (P42)
 * ================================================================ */

static char *create_channel(const char *guild_id, const char *name, int type) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/channels", DISCORD_API_BASE, guild_id);
    json_t *body = json_object();
    json_set(body, "name", json_string(name));
    json_set(body, "type", json_number((double)type));
    char *body_str = json_serialize(body); json_free(body);
    tool_api_result_t *r = tool_api_call(url, NULL, "POST", body_str, 15, 2);
    free(body_str);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Create channel failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    if (r->json) {
        json_set(result, "id", json_copy(json_obj_get(r->json, "id")));
        json_set(result, "name", json_copy(json_obj_get(r->json, "name")));
    }
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *kick_member(const char *guild_id, const char *user_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/members/%s", DISCORD_API_BASE, guild_id, user_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "DELETE", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Kick failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg1[128];
    snprintf(msg1, sizeof(msg1), "User %s kicked from guild %s", user_id, guild_id);
    json_set(result, "message", json_string(msg1));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *ban_member(const char *guild_id, const char *user_id, const char *reason) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/bans/%s", DISCORD_API_BASE, guild_id, user_id);
    json_t *body = json_object();
    if (reason) json_set(body, "reason", json_string(reason));
    char *body_str = json_serialize(body); json_free(body);
    tool_api_result_t *r = tool_api_call(url, NULL, "PUT", body_str, 15, 2);
    free(body_str);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Ban failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg2[128];
    snprintf(msg2, sizeof(msg2), "User %s banned from guild %s", user_id, guild_id);
    json_set(result, "message", json_string(msg2));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

/* ================================================================
 *  Message actions (P43 — depth expansion)
 * ================================================================ */

static char *search_members(const char *guild_id, const char *query, int limit) {
    char url[1024];
    if (limit < 1) limit = 20;
    if (limit > 1000) limit = 1000;
    snprintf(url, sizeof(url), "%s/guilds/%s/members/search?query=%s&limit=%d",
             DISCORD_API_BASE, guild_id, query, limit);
    tool_api_result_t *r = tool_api_call(url, NULL, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Search members failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_t *members = json_array();
    if (r->json) {
        size_t len = json_len(r->json);
        for (size_t i = 0; i < len; i++) {
            json_t *m = json_get(r->json, i);
            if (!m) continue;
            json_t *e = json_object();
            json_t *user = json_obj_get(m, "user");
            if (user) {
                json_set(e, "id", json_copy(json_obj_get(user, "id")));
                json_set(e, "username", json_copy(json_obj_get(user, "username")));
                json_set(e, "global_name", json_copy(json_obj_get(user, "global_name")));
            }
            json_set(e, "nick", json_copy(json_obj_get(m, "nick")));
            json_set(e, "roles", json_copy(json_obj_get(m, "roles")));
            json_set(e, "joined_at", json_copy(json_obj_get(m, "joined_at")));
            json_append(members, e);
        }
    }
    json_set(result, "members", members);
    json_set(result, "count", json_number((double)json_len(members)));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *list_pins(const char *channel_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s/pins", DISCORD_API_BASE, channel_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("List pins failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_t *pins = json_array();
    if (r->json) {
        size_t len = json_len(r->json);
        for (size_t i = 0; i < len; i++) {
            json_t *m = json_get(r->json, i);
            if (!m) continue;
            json_t *e = json_object();
            json_set(e, "id", json_copy(json_obj_get(m, "id")));
            json_set(e, "content", json_copy(json_obj_get(m, "content")));
            json_set(e, "author", json_copy(json_obj_get(m, "author")));
            json_set(e, "timestamp", json_copy(json_obj_get(m, "timestamp")));
            json_append(pins, e);
        }
    }
    json_set(result, "pins", pins);
    json_set(result, "count", json_number((double)json_len(pins)));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *pin_message(const char *channel_id, const char *message_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s/pins/%s", DISCORD_API_BASE, channel_id, message_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "PUT", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Pin failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[128];
    snprintf(msg, sizeof(msg), "Message %s pinned in channel %s", message_id, channel_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *unpin_message(const char *channel_id, const char *message_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s/pins/%s", DISCORD_API_BASE, channel_id, message_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "DELETE", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Unpin failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[128];
    snprintf(msg, sizeof(msg), "Message %s unpinned from channel %s", message_id, channel_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *create_thread(const char *channel_id, const char *name, const char *message_id) {
    char url[512];
    json_t *body = json_object();
    json_set(body, "name", json_string(name ? name : "new-thread"));
    if (message_id)
        snprintf(url, sizeof(url), "%s/channels/%s/messages/%s/threads", DISCORD_API_BASE, channel_id, message_id);
    else
        snprintf(url, sizeof(url), "%s/channels/%s/threads", DISCORD_API_BASE, channel_id);
    if (!message_id)
        json_set(body, "type", json_number(11));
    char *body_str = json_serialize(body); json_free(body);
    tool_api_result_t *r = tool_api_call(url, NULL, "POST", body_str, 15, 2);
    free(body_str);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Create thread failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    if (r->json) {
        json_set(result, "id", json_copy(json_obj_get(r->json, "id")));
        json_set(result, "name", json_copy(json_obj_get(r->json, "name")));
        json_set(result, "type", json_copy(json_obj_get(r->json, "type")));
    }
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

/* ================================================================
 *  Role management actions (P43 — depth expansion)
 * ================================================================ */

static char *add_role(const char *guild_id, const char *user_id, const char *role_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/members/%s/roles/%s",
             DISCORD_API_BASE, guild_id, user_id, role_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "PUT", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Add role failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[256];
    snprintf(msg, sizeof(msg), "Role %s added to user %s in guild %s", role_id, user_id, guild_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

static char *remove_role(const char *guild_id, const char *user_id, const char *role_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/members/%s/roles/%s",
             DISCORD_API_BASE, guild_id, user_id, role_id);
    tool_api_result_t *r = tool_api_call(url, NULL, "DELETE", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Remove role failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[256];
    snprintf(msg, sizeof(msg), "Role %s removed from user %s in guild %s", role_id, user_id, guild_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}

/* ================================================================
 *  Main handler — action dispatch
 * ================================================================ */

char *discord_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    /* Check token availability */
    if (!get_token()) {
        return json_error("DISCORD_BOT_TOKEN not set");
    }

    /* Parse JSON args */
    char *err = NULL;
    json_t *args = json_parse(args_json, &err);
    if (!args) {
        char *result = json_errorf("Invalid JSON: %s", err ? err : "parse failed");
        free(err);
        return result;
    }

    const char *action = json_get_str(args, "action", "");
    if (!action || !action[0]) {
        json_free(args);
        return json_error("Missing 'action' field");
    }

    const char *guild_id   = json_get_str(args, "guild_id", NULL);
    const char *channel_id = json_get_str(args, "channel_id", NULL);
    const char *content    = json_get_str(args, "content", NULL);
    const char *user_id    = json_get_str(args, "user_id", NULL);
    const char *reason     = json_get_str(args, "reason", NULL);
    const char *message_id = json_get_str(args, "message_id", NULL);
    const char *name       = json_get_str(args, "name", NULL);
    const char *role_id    = json_get_str(args, "role_id", NULL);
    const char *query      = json_get_str(args, "query", NULL);
    int    limit           = (int)json_get_num(args, "limit", 50);
    int    channel_type    = (int)json_get_num(args, "type", 0); /* 0=text */

    char *result = NULL;

    if (strcmp(action, "list_guilds") == 0) {
        result = list_guilds();
    }
    else if (strcmp(action, "guild_info") == 0) {
        if (!guild_id) result = json_error("guild_id required");
        else result = guild_info(guild_id);
    }
    else if (strcmp(action, "list_channels") == 0) {
        if (!guild_id) result = json_error("guild_id required");
        else result = list_channels(guild_id);
    }
    else if (strcmp(action, "channel_info") == 0) {
        if (!channel_id) result = json_error("channel_id required");
        else result = channel_info(channel_id);
    }
    else if (strcmp(action, "list_roles") == 0) {
        if (!guild_id) result = json_error("guild_id required");
        else result = list_roles(guild_id);
    }
    else if (strcmp(action, "member_info") == 0) {
        if (!guild_id || !user_id) result = json_error("guild_id and user_id required");
        else result = member_info(guild_id, user_id);
    }
    else if (strcmp(action, "send_message") == 0) {
        if (!channel_id) result = json_error("channel_id required");
        else if (!content) result = json_error("content required for send_message");
        else result = send_message(channel_id, content);
    }
    else if (strcmp(action, "fetch_messages") == 0) {
        if (!channel_id) result = json_error("channel_id required");
        else result = fetch_messages(channel_id, limit);
    }
    else if (strcmp(action, "delete_message") == 0) {
        if (!channel_id || !message_id) result = json_error("channel_id and message_id required");
        else result = delete_message(channel_id, message_id);
    }
    /* Admin actions */
    else if (strcmp(action, "create_channel") == 0) {
        if (!guild_id || !name) result = json_error("guild_id and name required");
        else result = create_channel(guild_id, name, channel_type);
    }
    else if (strcmp(action, "kick_member") == 0) {
        if (!guild_id || !user_id) result = json_error("guild_id and user_id required");
        else result = kick_member(guild_id, user_id);
    }
    else if (strcmp(action, "ban_member") == 0) {
        if (!guild_id || !user_id) result = json_error("guild_id and user_id required");
        else result = ban_member(guild_id, user_id, reason);
    }
    /* Message management */
    else if (strcmp(action, "search_members") == 0) {
        if (!guild_id || !query) result = json_error("guild_id and query required");
        else result = search_members(guild_id, query, limit);
    }
    else if (strcmp(action, "list_pins") == 0) {
        if (!channel_id) result = json_error("channel_id required");
        else result = list_pins(channel_id);
    }
    else if (strcmp(action, "pin_message") == 0) {
        if (!channel_id || !message_id) result = json_error("channel_id and message_id required");
        else result = pin_message(channel_id, message_id);
    }
    else if (strcmp(action, "unpin_message") == 0) {
        if (!channel_id || !message_id) result = json_error("channel_id and message_id required");
        else result = unpin_message(channel_id, message_id);
    }
    else if (strcmp(action, "create_thread") == 0) {
        if (!channel_id || !name) result = json_error("channel_id and name required");
        else result = create_thread(channel_id, name, message_id);
    }
    /* Role management */
    else if (strcmp(action, "add_role") == 0) {
        if (!guild_id || !user_id || !role_id) result = json_error("guild_id, user_id, and role_id required");
        else result = add_role(guild_id, user_id, role_id);
    }
    else if (strcmp(action, "remove_role") == 0) {
        if (!guild_id || !user_id || !role_id) result = json_error("guild_id, user_id, and role_id required");
        else result = remove_role(guild_id, user_id, role_id);
    }
    else {
        result = json_errorf("Unknown discord action: %s", action);
    }

    json_free(args);
    return result;
}

/* ================================================================
 *  Registry registration
 * ================================================================ */

void registry_init_discord(void) {
    registry_register(
        "discord",
        "Interact with Discord servers via the REST API. "
        "Actions: list_guilds, guild_info, list_channels, channel_info, "
        "list_roles, member_info, send_message, fetch_messages, "
        "delete_message, create_channel, kick_member, ban_member, "
        "search_members, list_pins, pin_message, unpin_message, "
        "create_thread, add_role, remove_role. "
        "Requires DISCORD_BOT_TOKEN env var.",
        "{\"type\":\"object\",\"properties\":{"
          "\"action\":{\"type\":\"string\",\"description\":\"Action to execute\","
            "\"enum\":[\"list_guilds\",\"guild_info\",\"list_channels\","
                     "\"channel_info\",\"list_roles\",\"member_info\","
                     "\"send_message\",\"fetch_messages\",\"delete_message\","
                     "\"create_channel\",\"kick_member\",\"ban_member\"]},"
          "\"guild_id\":{\"type\":\"string\",\"description\":\"Discord guild/server ID\"},"
          "\"channel_id\":{\"type\":\"string\",\"description\":\"Discord channel ID\"},"
          "\"user_id\":{\"type\":\"string\",\"description\":\"Discord user ID\"},"
          "\"content\":{\"type\":\"string\",\"description\":\"Message content\"},"
          "\"message_id\":{\"type\":\"string\",\"description\":\"Message ID for delete_message\"},"
          "\"name\":{\"type\":\"string\",\"description\":\"Channel name (create_channel)\"},"
          "\"reason\":{\"type\":\"string\",\"description\":\"Ban reason\"},"
          "\"limit\":{\"type\":\"integer\",\"description\":\"Max messages\",\"default\":50},"
          "\"type\":{\"type\":\"integer\",\"description\":\"Channel type 0=text 2=voice 4=category\",\"default\":0}"
        "},\"required\":[\"action\"]}",
        discord_handler
    );
    registry_set_timeout("discord", 30);

    /* Also register discord_admin for admin-specific actions */
    registry_register(
        "discord_admin",
        "Manage a Discord server: create_channel, kick_member, ban_member, add_role, remove_role, search_members, list_pins, pin_message, unpin_message, create_thread. "
        "Requires DISCORD_BOT_TOKEN and appropriate permissions.",
        NULL,
        discord_handler
    );
}
