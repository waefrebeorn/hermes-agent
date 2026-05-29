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
#include <ctype.h>

/* Parsed target info: platform, chat_id, thread_id */
typedef struct {
    char platform[64];
    char chat_id[256];
    char thread_id[64];
} send_target_t;

/* Parse target string in "platform:chat_id[:thread_id]" format.
 * If target is "stdout" or "local" without a colon and no override,
 * sets platform to empty string (caller interprets bare target).
 * If platform_override is set, uses it instead of parsing from target.
 * Returns true on success, false on invalid format. */
bool parse_send_target(const char *target, const char *platform_override, send_target_t *out) {
    memset(out, 0, sizeof(*out));
    if (!target || !target[0]) return false;

    if (platform_override && platform_override[0]) {
        snprintf(out->platform, sizeof(out->platform), "%s", platform_override);
        /* If target contains ':', parse chat_id from target even with override */
        const char *colon = strchr(target, ':');
        if (colon && colon != target) {
            const char *second_colon = strchr(colon + 1, ':');
            if (second_colon) {
                size_t cid_len = (size_t)(second_colon - colon - 1);
                if (cid_len < sizeof(out->chat_id)) {
                    memcpy(out->chat_id, colon + 1, cid_len);
                    out->chat_id[cid_len] = '\0';
                }
                snprintf(out->thread_id, sizeof(out->thread_id), "%s", second_colon + 1);
            } else {
                snprintf(out->chat_id, sizeof(out->chat_id), "%s", colon + 1);
            }
        }
        return true;
    }

    /* No override: parse platform from colon-separated target */
    const char *colon = strchr(target, ':');
    if (colon && colon != target) {
        size_t plen = (size_t)(colon - target);
        if (plen < sizeof(out->platform)) {
            memcpy(out->platform, target, plen);
            /* Check for second colon (chat_id:thread_id) */
            const char *second_colon = strchr(colon + 1, ':');
            if (second_colon) {
                size_t cid_len = (size_t)(second_colon - colon - 1);
                if (cid_len < sizeof(out->chat_id)) {
                    memcpy(out->chat_id, colon + 1, cid_len);
                    out->chat_id[cid_len] = '\0';
                }
                snprintf(out->thread_id, sizeof(out->thread_id), "%s", second_colon + 1);
            } else {
                snprintf(out->chat_id, sizeof(out->chat_id), "%s", colon + 1);
            }
            return true;
        }
    }

    /* Bare target like "stdout" or "local" — leave platform empty, caller uses target directly */
    out->platform[0] = '\0'; /* Signal: use target directly */
    return true;
}

/* Redact secrets from error text (port of Python send_message_tool._sanitize_error_text) */
char *sanitize_error_text(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    /* Copy and redact in-place */
    strcpy(buf, text);

    /* URL secret query param redact: [?&]access_token=xxx -> [?&]access_token=*** */
    const char *secret_params[] = {"access_token", "api_key", "api-key",
        "auth_token", "auth-token", "token", "signature", "sig", NULL};
    for (int pi = 0; secret_params[pi]; pi++) {
        size_t plen = strlen(secret_params[pi]);
        char *p = buf;
        while ((p = strstr(p, secret_params[pi])) != NULL) {
            /* Check preceded by ? or & or whitespace or start */
            if (p > buf && p[-1] != '?' && p[-1] != '&' && p[-1] != ' ' &&
                p[-1] != '\n' && p[-1] != '=') {
                p++;
                continue;
            }
            /* Check followed by = */
            if (p[plen] == '=') {
                char *val_start = p + plen + 1;
                char *val_end = val_start;
                while (*val_end && *val_end != '&' && *val_end != ' ' &&
                       *val_end != '\n' && *val_end != '\r' && *val_end != ';' &&
                       *val_end != ',') val_end++;
                if (val_end > val_start) {
                    memset(val_start, '*', (size_t)(val_end - val_start));
                }
                p = val_end;
            } else {
                p++;
            }
        }
    }

    /* Generic secret assign redact: access_token = xxx -> access_token=*** */
    for (int pi = 0; secret_params[pi]; pi++) {
        size_t plen = strlen(secret_params[pi]);
        char *p = buf;
        while ((p = strstr(p, secret_params[pi])) != NULL) {
            /* Check preceded by word boundary (non-alnum, not first char of word) */
            if (p > buf && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) {
                p++;
                continue;
            }
            char *eq = p + plen;
            /* Skip whitespace before = */
            while (*eq == ' ' || *eq == '\t') eq++;
            if (*eq == '=') {
                eq++;
                /* Skip whitespace after = */
                while (*eq == ' ' || *eq == '\t') eq++;
                char *val_end = eq;
                while (*val_end && *val_end != ' ' && *val_end != '\t' &&
                       *val_end != '\n' && *val_end != '\r' && *val_end != ';' &&
                       *val_end != ',') val_end++;
                if (val_end > eq) {
                    memset(eq, '*', (size_t)(val_end - eq));
                }
                p = val_end;
            } else {
                p++;
            }
        }
    }
    return buf;
}

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"Action: 'send' (default) or 'list' (show available platforms)\"},"
      "\"target\":{\"type\":\"string\",\"description\":\"'local' (save), 'stdout' (print), or 'platform:target' (e.g., telegram:-100123456)\"},"
      "\"message\":{\"type\":\"string\",\"description\":\"Message text to send\"},"
      "\"media_path\":{\"type\":\"string\",\"description\":\"Optional file path to attach as media (image, audio, video, document)\"},"
      "\"platform\":{\"type\":\"string\",\"description\":\"Platform name override (e.g., 'telegram', 'discord') — overrides target parsing\"},"
      "\"thread_id\":{\"type\":\"string\",\"description\":\"Thread/topic ID for platforms that support threaded conversations (e.g., Telegram topic ID)\"},"
      "\"reply_to_message_id\":{\"type\":\"string\",\"description\":\"Message ID to reply to\"},"
      "\"inline_buttons\":{\"type\":\"array\",\"description\":\"D06: Array of inline button objects [{text, url?, callback_data?, row?}] for inline keyboards\"},"
      "\"media_group\":{\"type\":\"array\",\"description\":\"B08: Array of file paths for Telegram media group\",\"items\":{\"type\":\"string\"}},"
      "\"disable_notification\":{\"type\":\"boolean\",\"description\":\"Send message silently (no notification sound). Telegram only. Default: false\"},"      "\"disable_link_previews\":{\"type\":\"boolean\",\"description\":\"Disable link previews in sent message (Telegram). Default: false\"},"
      "\"parse_mode\":{\"type\":\"string\",\"description\":\"Telegram parse mode: Markdown, MarkdownV2, HTML, or empty for plain text. Default: Markdown\"}"
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

    const char *action = json_object_get_string(args, "action", "send");
    const char *target = json_object_get_string(args, "target", "stdout");
    const char *message = json_object_get_string(args, "message", NULL);
    const char *media_path = json_object_get_string(args, "media_path", NULL);
    const char *platform_override = json_object_get_string(args, "platform", NULL);
    const char *thread_id = json_object_get_string(args, "thread_id", NULL);
    const char *reply_to = json_object_get_string(args, "reply_to_message_id", NULL);
    json_node_t *inline_buttons_node = json_object_get(args, "inline_buttons");
    json_node_t *media_group = json_object_get(args, "media_group");

    /* Parse disable_link_previews and parse_mode from args before json_free */
    bool disable_preview = json_object_get_bool(args, "disable_link_previews", false);
    bool disable_notification = json_object_get_bool(args, "disable_notification", false);
    const char *parse_mode = json_object_get_string(args, "parse_mode", NULL);
    if (!parse_mode || !parse_mode[0]) parse_mode = "Markdown";
    /* Validate parse_mode against known Telegram modes */
    if (strcmp(parse_mode, "Markdown") != 0 &&
        strcmp(parse_mode, "MarkdownV2") != 0 &&
        strcmp(parse_mode, "HTML") != 0) {
        json_node_t *result = json_new_object();
        json_object_set(result, "error", json_new_string("Invalid parse_mode. Must be 'Markdown', 'MarkdownV2', or 'HTML'."));
        json_object_set(result, "parse_mode", json_new_string(parse_mode));
        char *json_out = json_serialize(result);
        json_free(result);
        json_free(args);
        return json_out;
    }

    json_node_t *result = json_new_object();

    /* action=list: return available platforms */
    if (strcmp(action, "list") == 0) {
        json_free(args);
        json_object_set(result, "action", json_new_string("list"));
        json_node_t *platforms = json_new_array();
        json_array_append(platforms, json_new_string("stdout"));
        json_array_append(platforms, json_new_string("local"));
        json_array_append(platforms, json_new_string("telegram"));
        json_array_append(platforms, json_new_string("discord"));
        json_array_append(platforms, json_new_string("slack"));
        json_array_append(platforms, json_new_string("matrix"));
        json_array_append(platforms, json_new_string("signal"));
        json_object_set(result, "platforms", platforms);
        json_object_set(result, "format", json_new_string("platform:chat_id[:thread_id]"));
        char *json_out = json_serialize(result);
        json_free(result);
        return json_out;
    }

    if (!message) {
        json_object_set(result, "error", json_new_string("Missing message"));
    } else {
        /* F42: Parse target for platform:chat_id format */
        send_target_t parsed_target;
        if (!parse_send_target(target, platform_override, &parsed_target)) {
            json_object_set(result, "error", json_new_string("Invalid target format"));
            char *json_out = json_serialize(result);
            json_free(result);
            json_free(args);
            return json_out;
        }
        const char *platform = parsed_target.platform[0] ? parsed_target.platform : NULL;
        const char *chat_id = parsed_target.chat_id[0] ? parsed_target.chat_id : NULL;
        const char *target_thread_id = parsed_target.thread_id[0] ? parsed_target.thread_id : NULL;

        /* Use thread_id from args if provided, fallback to target parsing */
        if (!thread_id || !thread_id[0]) {
            thread_id = target_thread_id;
        }

        /* F43: Check for MEDIA: prefix in message (backward compat) */
        const char *actual_message = message;
        const char *actual_media = media_path;
        bool force_document = false;

        /* B08: [[as_document]] directive — strip from message, force document delivery */
        char *cleaned_msg = NULL;
        if (actual_message) {
            const char *as_doc = strstr(actual_message, "[[as_document]]");
            if (as_doc) {
                force_document = true;
                /* Remove [[as_document]] from message */
                size_t prefix_len = (size_t)(as_doc - actual_message);
                size_t remaining = strlen(actual_message) - prefix_len - 14; /* len of "[[as_document]]" */
                cleaned_msg = (char *)malloc(prefix_len + remaining + 2);
                if (cleaned_msg) {
                    memcpy(cleaned_msg, actual_message, prefix_len);
                    const char *after = as_doc + 14;
                    while (*after == ' ' || *after == '\n') after++;
                    memcpy(cleaned_msg + prefix_len, after, remaining + 1);
                    actual_message = cleaned_msg;
                }
            }
        }

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

#ifndef TEST_BUILD
            /* Get bot token from config or env */
            hermes_config_t cfg;
            memset(&cfg, 0, sizeof(cfg));
            hermes_config_load(&cfg, NULL);
            const char *bot_token = cfg.telegram.bot_token[0] ? cfg.telegram.bot_token : getenv("TELEGRAM_BOT_TOKEN");

            if (bot_token && bot_token[0]) {
                telegram_set_token(bot_token);
                http_client_t *http = http_client_new(30);
                if (http) {
                    /* B08: Media group — send multiple files as a single Telegram media group */
                    if (media_group && media_group->type == JSON_ARRAY) {
                        size_t mg_count = json_len(media_group);
                        if (mg_count >= 2) {
                            json_node_t *input_media = json_new_array();
                            size_t added = 0;
                            for (size_t mi = 0; mi < mg_count; mi++) {
                                json_node_t *item = json_array_get(media_group, mi);
                                if (!item || item->type != JSON_STRING) continue;
                                const char *path = item->str_val;
                                if (!path || !path[0]) continue;
                                const char *ext = strrchr(path, '.');
                                const char *media_type = "document";
                                if (ext && !force_document) {
                                    if (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 ||
                                        strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".webp") == 0)
                                        media_type = "photo";
                                    else if (strcasecmp(ext, ".mp4") == 0 || strcasecmp(ext, ".mov") == 0)
                                        media_type = "video";
                                    else if (strcasecmp(ext, ".gif") == 0)
                                        media_type = "animation";
                                }
                                json_node_t *mi2 = json_new_object();
                                json_object_set(mi2, "type", json_new_string(media_type));
                                json_object_set(mi2, "media", json_new_string(path));
                                json_array_append(input_media, mi2);
                                added++;
                            }
                            if (added >= 2) {
                                sent = telegram_send_media_group(http, chat_id ? chat_id : "", input_media);
                            } else if (added == 1) {
                                json_node_t *first = json_array_get(media_group, 0);
                                if (first && first->type == JSON_STRING)
                                    actual_media = first->str_val;
                            }
                            json_free(input_media);
                            if (sent) goto send_done;
                        }
                    }
                    if (actual_media && actual_media[0]) {
                        /* MEDIA: prefix — send file via appropriate API */
                        const char *ext = strrchr(actual_media, '.');
                        if (ext && !force_document) {
                            if (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 ||
                                strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".webp") == 0)
                                sent = telegram_send_photo(http, chat_id ? chat_id : "", actual_media, NULL, NULL);
                            else if (strcasecmp(ext, ".ogg") == 0 || strcasecmp(ext, ".opus") == 0)
                                sent = telegram_send_voice(http, chat_id ? chat_id : "", actual_media, NULL, NULL);
                            else if (strcasecmp(ext, ".mp4") == 0 || strcasecmp(ext, ".mov") == 0)
                                sent = telegram_send_video(http, chat_id ? chat_id : "", actual_media, NULL, NULL);
                            else if (strcasecmp(ext, ".gif") == 0)
                                sent = telegram_send_animation(http, chat_id ? chat_id : "", actual_media, NULL, NULL);
                            else
                                sent = telegram_send_document(http, chat_id ? chat_id : "", actual_media, NULL, NULL);
                        } else {
                            sent = telegram_send_document(http, chat_id ? chat_id : "", actual_media, NULL, NULL);
                        }
                    } else if (inline_buttons_node && inline_buttons_node->type == JSON_ARRAY) {
                        /* Build reply_markup with inline keyboard */
                        json_node_t *reply_markup = build_inline_keyboard(inline_buttons_node);
                        if (reply_markup) {
                            sent = telegram_send_message_with_keyboard(http, chat_id ? chat_id : "",
                                                                       tg_msg, parse_mode, thread_id, reply_markup, disable_notification, disable_preview);
                            json_free(reply_markup);
                        } else {
                            /* Fallback: send without keyboard */
                            sent = telegram_send_message(http, chat_id ? chat_id : "",
                                                         tg_msg, parse_mode, thread_id, disable_notification, disable_preview);
                        }
                    } else {
                        sent = telegram_send_message(http, chat_id ? chat_id : "",
                                                     tg_msg, parse_mode, thread_id, disable_notification, disable_preview);
                    }
send_done:
                    http_client_free(http);
                }
            }
#endif

            if (sent) {
                json_object_set(result, "status", json_new_string("sent"));
                json_object_set(result, "platform", json_new_string("telegram"));
                if (chat_id) json_object_set(result, "chat_id", json_new_string(chat_id));
            } else {
                json_object_set(result, "status", json_new_string("error"));
#ifndef TEST_BUILD
                const char *reason = !bot_token ? "TELEGRAM_BOT_TOKEN not set" : "Telegram send failed";
#else
                const char *reason = "Telegram send skipped (test build)";
#endif
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
                char *sanitized = sanitize_error_text(errbuf);
                json_object_set(result, "error", json_new_string(sanitized ? sanitized : errbuf));
                free(sanitized);
            }

        } else {
            json_object_set(result, "error", json_new_string("Unsupported target"));
        }

        /* Clean up strdup'd media path if created from MEDIA: prefix */
        if (actual_media && actual_media != media_path) {
            free((void *)actual_media);
        }
        /* Clean up malloc'd cleaned message if [[as_document]] was stripped */
        if (cleaned_msg) {
            free(cleaned_msg);
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
