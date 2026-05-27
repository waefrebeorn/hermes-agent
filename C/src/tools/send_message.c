/*
 * send_message.c — Send message tool for Hermes C.
 * Sends messages to platforms. Currently supports: local (stdout/file), telegram.
 */

#include "hermes.h"
#include "hermes_json.h"
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
      "\"media_path\":{\"type\":\"string\",\"description\":\"F43: Optional file path to attach as media (image, audio, video, document)\"},"
      "\"platform\":{\"type\":\"string\",\"description\":\"F42: Platform name override (e.g., 'telegram', 'discord') — overrides target parsing\"},"
      "\"thread_id\":{\"type\":\"string\",\"description\":\"Thread/topic ID for platforms that support threaded conversations (e.g., Telegram topic ID)\"}"
    "},"
    "\"required\":[\"message\"]"
"}";

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
            /* Route to Telegram platform */
            char cmd[16384];
            const char *tg_msg = actual_message ? actual_message : "";
            char escaped_msg[8192];
            int ei = 0;
            for (int i = 0; tg_msg[i] && ei < (int)sizeof(escaped_msg) - 4; i++) {
                if (tg_msg[i] == '\'') {
                    if (ei + 4 < (int)sizeof(escaped_msg)) {
                        escaped_msg[ei++] = '\'';
                        escaped_msg[ei++] = '\\';
                        escaped_msg[ei++] = '\'';
                        escaped_msg[ei++] = '\'';
                    }
                } else {
                    escaped_msg[ei++] = tg_msg[i];
                }
            }
            escaped_msg[ei] = '\0';

            if (actual_media) {
                char escaped_path[2048];
                snprintf(escaped_path, sizeof(escaped_path), "%s", actual_media);
                /* Choose send method based on extension */
                const char *ext = strrchr(actual_media, '.');
                if (ext) {
                    ext++;
                    if (strcmp(ext, "ogg") == 0 || strcmp(ext, "oga") == 0 || strcmp(ext, "wav") == 0 || strcmp(ext, "mp3") == 0) {
                        snprintf(cmd, sizeof(cmd),
                                 "hermes gateway telegram sendVoice --chat_id '%s' --voice '%s' 2>/dev/null",
                                 chat_id ? chat_id : "", actual_media);
                    } else if (strcmp(ext, "mp4") == 0 || strcmp(ext, "mov") == 0 || strcmp(ext, "avi") == 0) {
                        snprintf(cmd, sizeof(cmd),
                                 "hermes gateway telegram sendVideo --chat_id '%s' --video '%s' 2>/dev/null",
                                 chat_id ? chat_id : "", actual_media);
                    } else if (strcmp(ext, "gif") == 0 || strcmp(ext, "webp") == 0) {
                        snprintf(cmd, sizeof(cmd),
                                 "hermes gateway telegram sendAnimation --chat_id '%s' --animation '%s' 2>/dev/null",
                                 chat_id ? chat_id : "", actual_media);
                    } else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0 || strcmp(ext, "png") == 0 || strcmp(ext, "webp") == 0) {
                        snprintf(cmd, sizeof(cmd),
                                 "hermes gateway telegram sendPhoto --chat_id '%s' --photo '%s' --caption '%s' 2>/dev/null",
                                 chat_id ? chat_id : "", actual_media, escaped_msg);
                    } else {
                        snprintf(cmd, sizeof(cmd),
                                 "hermes gateway telegram sendDocument --chat_id '%s' --document '%s' --caption '%s' 2>/dev/null",
                                 chat_id ? chat_id : "", actual_media, escaped_msg);
                    }
                } else {
                    snprintf(cmd, sizeof(cmd),
                             "hermes gateway telegram sendDocument --chat_id '%s' --document '%s' --caption '%s' 2>/dev/null",
                             chat_id ? chat_id : "", actual_media, escaped_msg);
                }
            } else {
                if (reply_to) {
                    snprintf(cmd, sizeof(cmd),
                             "hermes gateway telegram sendMessage --chat_id '%s' --text '%s' --reply_to '%s' 2>/dev/null",
                             chat_id ? chat_id : "", escaped_msg, reply_to);
                } else {
                    snprintf(cmd, sizeof(cmd),
                             "hermes gateway telegram sendMessage --chat_id '%s' --text '%s' 2>/dev/null",
                             chat_id ? chat_id : "", escaped_msg);
                }
            }

            int rc = system(cmd);
            if (rc == 0) {
                json_object_set(result, "status", json_new_string("sent"));
                json_object_set(result, "platform", json_new_string("telegram"));
                if (chat_id) json_object_set(result, "chat_id", json_new_string(chat_id));
            } else {
                json_object_set(result, "status", json_new_string("error"));
                json_object_set(result, "error", json_new_string("Telegram send failed"));
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
        "Optional 'thread_id' for threaded conversations (e.g., Telegram topic IDs).",
        SCHEMA, send_message_handler);
}
