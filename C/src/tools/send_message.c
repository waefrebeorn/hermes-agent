/*
 * send_message.c — Send message tool for Hermes C.
 * Sends messages to platforms. Supports: local (stdout/file), telegram with inline buttons.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"target\":{\"type\":\"string\",\"description\":\"'local' (save), 'stdout' (print), or 'platform:target' (e.g., telegram:-100123456)\"},"
      "\"message\":{\"type\":\"string\",\"description\":\"Message text to send\"},"
      "\"media_path\":{\"type\":\"string\",\"description\":\"Optional file path to attach as media (image, audio, video, document)\"},"
      "\"platform\":{\"type\":\"string\",\"description\":\"Platform name override (e.g., 'telegram', 'discord') — overrides target parsing\"},"
      "\"thread_id\":{\"type\":\"string\",\"description\":\"Thread/topic ID for platforms that support threaded conversations (e.g., Telegram topic ID)\"},"
      "\"reply_to_message_id\":{\"type\":\"string\",\"description\":\"Message ID to reply to\"},"
      "\"inline_buttons\":{\"type\":\"array\",\"description\":\"D06: Array of inline button objects [{text, url?, callback_data?, row?}] for inline keyboards\"}"
    "},"
    "\"required\":[\"message\"]"
"}";

/*
 * D06: Build inline_keyboard reply_markup JSON from inline_buttons array.
 * Input: JSON array of {text, url?, callback_data?, row?} objects.
 * Output: JSON node with {inline_keyboard: [[{text, url/callback_data}], ...]}
 */
static json_node_t *build_inline_keyboard(json_node_t *buttons) {
    if (!buttons || buttons->type != JSON_ARRAY) return NULL;
    size_t count = json_len(buttons);
    if (count == 0) return NULL;

    /* First pass: determine max row number to size the row array */
    int max_row = 0;
    for (size_t i = 0; i < count; i++) {
        json_node_t *btn = json_array_get(buttons, i);
        if (btn) {
            int row = (int)json_object_get_number(btn, "row", 0);
            if (row > max_row) max_row = row;
        }
    }

    /* Allocate row arrays (max_row+1 rows, each starting empty) */
    size_t row_count = (size_t)(max_row + 1);
    json_node_t **rows = calloc(row_count, sizeof(json_node_t *));
    size_t *row_sizes = calloc(row_count, sizeof(size_t));
    if (!rows || !row_sizes) {
        free(rows);
        free(row_sizes);
        return NULL;
    }

    /* Second pass: group buttons into rows */
    for (size_t i = 0; i < count; i++) {
        json_node_t *btn = json_array_get(buttons, i);
        if (!btn) continue;

        const char *text = json_object_get_string(btn, "text", NULL);
        if (!text) continue;

        int row_idx = (int)json_object_get_number(btn, "row", 0);
        if (row_idx < 0) row_idx = 0;
        if (row_idx > max_row) row_idx = max_row;

        json_node_t *button = json_new_object();
        json_object_set(button, "text", json_new_string(text));

        const char *url = json_object_get_string(btn, "url", NULL);
        const char *callback_data = json_object_get_string(btn, "callback_data", NULL);
        if (url) {
            json_object_set(button, "url", json_new_string(url));
        } else if (callback_data) {
            json_object_set(button, "callback_data", json_new_string(callback_data));
        }

        /* Append to row array */
        if (!rows[(size_t)row_idx]) {
            rows[(size_t)row_idx] = json_new_array();
        }
        json_array_append(rows[(size_t)row_idx], button);
        row_sizes[(size_t)row_idx]++;
    }

    /* Build final keyboard: inline_keyboard: [[row1], [row2], ...] */
    json_node_t *keyboard = json_new_array();
    for (size_t r = 0; r < row_count; r++) {
        if (rows[r] && row_sizes[r] > 0) {
            json_array_append(keyboard, rows[r]);
        } else if (rows[r]) {
            json_free(rows[r]);
        }
    }

    json_node_t *reply_markup = json_new_object();
    json_object_set(reply_markup, "inline_keyboard", keyboard);
    /* keyboard is now owned by reply_markup, don't free separately */

    free(rows);
    free(row_sizes);
    return reply_markup;
}

char *send_message_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *target = json_object_get_string(args, "target", "stdout");
    const char *message = json_object_get_string(args, "message", NULL);
    const char *media_path = json_object_get_string(args, "media_path", NULL);
    const char *platform_override = json_object_get_string(args, "platform", NULL);
    const char *thread_id = json_object_get_string(args, "thread_id", NULL);
    const char *reply_to = json_object_get_string(args, "reply_to_message_id", NULL);
    json_node_t *inline_buttons_node = json_object_get(args, "inline_buttons");

    json_node_t *result = json_new_object();

    if (!message) {
        json_object_set(result, "error", json_new_string("Missing message"));
    } else {
        /* F42: Parse target for platform:chat_id format */
        char platform_buf[64] = {0};
        char chat_id_buf[256] = {0};
        const char *platform = platform_override;
        const char *chat_id = NULL;

        if (!platform) {
            const char *colon = strchr(target, ':');
            if (colon && colon != target) {
                /* platform:chat_id format */
                size_t plen = (size_t)(colon - target);
                if (plen < sizeof(platform_buf)) {
                    memcpy(platform_buf, target, plen);
                    platform_buf[plen] = '\0';
                    platform = platform_buf;
                    snprintf(chat_id_buf, sizeof(chat_id_buf), "%s", colon + 1);
                    chat_id = chat_id_buf;
                }
            }
        }

        /* F43: Check for MEDIA: prefix in message (backward compat) */
        const char *actual_message = message;
        const char *actual_media = media_path;
        if (!actual_media && strncmp(message, "MEDIA:", 6) == 0) {
            const char *space = strchr(message + 6, ' ');
            if (space) {
                /* Extract media path and remaining message */
                size_t mlen = (size_t)(space - message - 6);
                char path_buf[1024];
                snprintf(path_buf, sizeof(path_buf), "%.*s", (int)mlen, message + 6);
                actual_media = strdup(path_buf);
                actual_message = space + 1;
            } else {
                actual_media = strdup(message + 6);
                actual_message = "";
            }
        }

        if (platform && strcmp(platform, "stdout") == 0) {
            printf("%s\n", actual_message ? actual_message : "");
            json_object_set(result, "status", json_new_string("sent"));
            json_object_set(result, "target", json_new_string("stdout"));

        } else if (!platform || strcmp(platform, "local") == 0) {
            /* local: save to file */
            const char *home = getenv("HERMES_HOME");
            if (!home) home = getenv("HOME");
            if (!home) home = "/tmp";

            char dir[4096], path[4096];
            snprintf(dir, sizeof(dir), "%s/.hermes/output", home);
            mkdir(dir, 0755);

            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            snprintf(path, sizeof(path), "%s/message_%04d%02d%02d_%02d%02d%02d.txt",
                     dir, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
                     tm->tm_hour, tm->tm_min, tm->tm_sec);

            FILE *f = fopen(path, "w");
            if (f) {
                if (actual_media) {
                    /* Include media reference in saved message */
                    fprintf(f, "[Media: %s]\n\n", actual_media);
                }
                fputs(actual_message, f);
                fclose(f);
                json_object_set(result, "status", json_new_string("saved"));
                json_object_set(result, "path", json_new_string(path));
            } else {
                json_object_set(result, "error", json_new_string("Cannot write file"));
            }

        } else if (platform && strcmp(platform, "telegram") == 0) {
            /* D06: Direct Telegram send via libhttp (replaces broken system() call) */
            bool sent = false;
            const char *tg_msg = actual_message ? actual_message : "";

            /* Get bot token from config or env */
            hermes_config_t cfg;
            memset(&cfg, 0, sizeof(cfg));
            hermes_config_load(&cfg, NULL);
            const char *bot_token = cfg.telegram.bot_token[0] ? cfg.telegram.bot_token : getenv("TELEGRAM_BOT_TOKEN");

            if (bot_token && bot_token[0]) {
                telegram_set_token(bot_token);
                http_client_t *http = http_client_new(30);
                if (http) {
                    if (inline_buttons_node && inline_buttons_node->type == JSON_ARRAY) {
                        /* Build reply_markup with inline keyboard */
                        json_node_t *reply_markup = build_inline_keyboard(inline_buttons_node);
                        if (reply_markup) {
                            sent = telegram_send_message_with_keyboard(http, chat_id ? chat_id : "",
                                                                       tg_msg, "Markdown", reply_markup);
                            json_free(reply_markup);
                        } else {
                            /* Fallback: send without keyboard */
                            sent = telegram_send_message(http, chat_id ? chat_id : "",
                                                         tg_msg, "Markdown");
                        }
                    } else {
                        sent = telegram_send_message(http, chat_id ? chat_id : "",
                                                     tg_msg, "Markdown");
                    }
                    http_client_free(http);
                }
            }

            if (sent) {
                json_object_set(result, "status", json_new_string("sent"));
                json_object_set(result, "platform", json_new_string("telegram"));
                if (chat_id) json_object_set(result, "chat_id", json_new_string(chat_id));
            } else {
                json_object_set(result, "status", json_new_string("error"));
                const char *reason = !bot_token ? "TELEGRAM_BOT_TOKEN not set" : "Telegram send failed";
                json_object_set(result, "error", json_new_string(reason));
            }

        } else if (platform) {
            /* Generic platform routing via hermes gateway */
            char cmd[16384];
            char escaped_msg[8192];
            int ei = 0;
            const char *msg = actual_message ? actual_message : "";
            for (int i = 0; msg[i] && ei < (int)sizeof(escaped_msg) - 4; i++) {
                if (msg[i] == '\'') {
                    escaped_msg[ei++] = '\'';
                    escaped_msg[ei++] = '\\';
                    escaped_msg[ei++] = '\'';
                    escaped_msg[ei++] = '\'';
                } else {
                    escaped_msg[ei++] = msg[i];
                }
            }
            escaped_msg[ei] = '\0';

            if (chat_id) {
                snprintf(cmd, sizeof(cmd),
                         "hermes gateway %s sendMessage --chat_id '%s' --text '%s' 2>/dev/null",
                         platform, chat_id, escaped_msg);
                if (reply_to) {
                    int clen = strlen(cmd);
                    snprintf(cmd + clen, sizeof(cmd) - clen,
                             " --reply_to '%s'", reply_to);
                }
            } else if (target[0] == '#') {
                snprintf(cmd, sizeof(cmd),
                         "hermes gateway %s sendMessage --channel '%s' --text '%s' 2>/dev/null",
                         platform, target + 1, escaped_msg);
            } else {
                snprintf(cmd, sizeof(cmd),
                         "hermes gateway %s sendMessage --text '%s' 2>/dev/null",
                         platform, escaped_msg);
            }

            int rc = system(cmd);
            if (rc == 0) {
                json_object_set(result, "status", json_new_string("sent"));
                json_object_set(result, "platform", json_new_string(platform));
                if (thread_id) json_object_set(result, "thread_id", json_new_string(thread_id));
            } else {
                json_object_set(result, "status", json_new_string("error"));
                char errbuf[256];
                snprintf(errbuf, sizeof(errbuf), "Platform '%s' send failed", platform);
                json_object_set(result, "error", json_new_string(errbuf));
            }

        } else {
            json_object_set(result, "error", json_new_string("Unsupported target"));
        }

        /* Clean up strdup'd media path if created from MEDIA: prefix */
        if (actual_media && actual_media != media_path) {
            free((void *)actual_media);
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_send_message(void) {
    registry_register("send_message",
        "Send a message to a platform. Supports: 'stdout' (console), 'local' (file), "
        "or 'platform:target' for platform routing (e.g., 'telegram:-100123456', "
        "'discord:#channel'). "
        "Use 'media_path' to attach files (images, audio, video, documents). "
        "The MEDIA: prefix in message text is also supported for backward compat. "
        "Optional 'thread_id' for threaded conversations (e.g., Telegram topic IDs). "
        "D06: 'inline_buttons' array for inline keyboards [{text, url?, callback_data?, row?}].",
        SCHEMA, send_message_handler);
}
