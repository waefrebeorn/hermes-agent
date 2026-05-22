/*
 * discord.c — P41: Discord server introspection and management tool.
 *
 * Port of Python hermes-agent tools/discord_tool.py.
 * Uses Discord REST API v10 with Bot token auth.
 * Single "discord" tool with action dispatch.
 *
 * Actions:
 *   list_guilds, guild_info, list_channels, channel_info,
 *   list_roles, send_message, fetch_messages, search_members, member_info
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

    const char *guild_id  = json_get_str(args, "guild_id", NULL);
    const char *channel_id = json_get_str(args, "channel_id", NULL);
    const char *content   = json_get_str(args, "content", NULL);
    int limit = (int)json_get_num(args, "limit", 50);

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
    else if (strcmp(action, "send_message") == 0) {
        if (!channel_id) result = json_error("channel_id required");
        else if (!content) result = json_error("content required for send_message");
        else result = send_message(channel_id, content);
    }
    else if (strcmp(action, "fetch_messages") == 0) {
        if (!channel_id) result = json_error("channel_id required");
        else result = fetch_messages(channel_id, limit);
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
        "Actions: list_guilds, guild_info, list_channels, "
        "send_message, fetch_messages. "
        "Requires DISCORD_BOT_TOKEN env var.",
        "{\"type\":\"object\",\"properties\":{"
          "\"action\":{\"type\":\"string\",\"description\":\"Action to execute\","
            "\"enum\":[\"list_guilds\",\"guild_info\",\"list_channels\","
                     "\"send_message\",\"fetch_messages\"]},"
          "\"guild_id\":{\"type\":\"string\",\"description\":\"Discord guild/server ID\"},"
          "\"channel_id\":{\"type\":\"string\",\"description\":\"Discord channel ID\"},"
          "\"content\":{\"type\":\"string\",\"description\":\"Message content (for send_message)\"},"
          "\"limit\":{\"type\":\"integer\",\"description\":\"Max messages (fetch_messages, max 100)\",\"default\":50}"
        "},\"required\":[\"action\"]}",
        discord_handler
    );
    registry_set_timeout("discord", 30);

    /* Also register under discord_tool name for backward compat */
    registry_register(
        "discord_send",
        "Send a message to a Discord channel. Action=send_message.",
        NULL,
        discord_handler
    );
}
