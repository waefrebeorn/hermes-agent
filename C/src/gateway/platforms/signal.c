/*
 * signal.c — Signal gateway for Hermes C.
 * Uses signal-cli for sending and receiving messages.
 * Falls back gracefully if signal-cli is not installed.
 *
 * P108: Extended with group messages, reactions, quote replies,
 *       and file/image attachment support.
 *
 * Env vars:
 *   SIGNAL_CLI_PATH    — Path to signal-cli (default: "signal-cli")
 *   SIGNAL_NUMBER      — Your Signal phone number (required)
 *   SIGNAL_POLL_INTERVAL — Seconds between polls (default: 10)
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_signal_cli[256] = "signal-cli";
static char g_signal_number[64] = "";
static int g_signal_poll_interval = 10;
static bool g_signal_available = false;

void signal_set_number(const char *number) {
    if (number) snprintf(g_signal_number, sizeof(g_signal_number), "%s", number);
}

void signal_set_cli_path(const char *path) {
    if (path) snprintf(g_signal_cli, sizeof(g_signal_cli), "%s", path);
}

/* Check if signal-cli is available */
bool signal_check_available(void) {
    if (g_signal_number[0] == '\0') return false;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", g_signal_cli);
    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * Shell-escape a string for use inside double quotes in a shell command.
 * Replaces " with \", backticks with \`, and $ with \$.
 * Returns a pointer to a static buffer (not thread-safe but sufficient
 * for the serialised CLI calls in this module).
 * ------------------------------------------------------------------ */
static const char *shell_escape(const char *s) {
    static char buf[4096];
    size_t j = 0;
    for (size_t i = 0; s[i] && j < sizeof(buf) - 4; i++) {
        char c = s[i];
        if (c == '"' || c == '`' || c == '$' || c == '\\') {
            if (j < sizeof(buf) - 4) buf[j++] = '\\';
        }
        buf[j++] = c;
    }
    buf[j] = '\0';
    return buf;
}

/* ------------------------------------------------------------------
 * Basic single-message send (original API)
 * ------------------------------------------------------------------ */
bool signal_send_message(http_client_t *http, const char *to, const char *text) {
    (void)http;
    if (!to || !text || !g_signal_available) return false;

    char cmd[8192];
    const char *safe_text = shell_escape(text);
    int r = snprintf(cmd, sizeof(cmd),
        "%s -a %s send -m \"%s\" %s 2>/dev/null",
        g_signal_cli, g_signal_number, safe_text, to);
    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;

    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * P108: Send a message to a Signal group
 * Uses signal-cli's -g <group_id> flag instead of a recipient number.
 * ------------------------------------------------------------------ */
bool signal_send_group_message(http_client_t *http,
                                const char *group_id,
                                const char *text) {
    (void)http;
    if (!group_id || !text || !g_signal_available) return false;

    char cmd[8192];
    const char *safe_text = shell_escape(text);
    int r = snprintf(cmd, sizeof(cmd),
        "%s -a %s send -g %s -m \"%s\" 2>/dev/null",
        g_signal_cli, g_signal_number, group_id, safe_text);
    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;

    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * P108: Send an emoji reaction to a specific message
 * Uses signal-cli's send-reaction subcommand:
 *   signal-cli -a <number> send-reaction -e <emoji> \
 *     --target-author <author> --target-timestamp <ts> <recipient>
 * ------------------------------------------------------------------ */
bool signal_send_reaction(http_client_t *http,
                           const char *recipient,
                           const char *target_author,
                           const char *target_timestamp,
                           const char *emoji) {
    (void)http;
    if (!recipient || !target_author || !target_timestamp || !emoji || !g_signal_available)
        return false;

    char cmd[8192];
    const char *safe_emoji = shell_escape(emoji);
    int r = snprintf(cmd, sizeof(cmd),
        "%s -a %s send-reaction -e \"%s\" "
        "--target-author %s --target-timestamp %s %s 2>/dev/null",
        g_signal_cli, g_signal_number, safe_emoji,
        target_author, target_timestamp, recipient);
    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;

    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * P108: Send a quoted reply to a specific message
 * Uses signal-cli's --quote-timestamp and --quote-author flags:
 *   signal-cli -a <number> send --quote-timestamp <ts> \
 *     --quote-author <author> -m "<text>" <recipient>
 * ------------------------------------------------------------------ */
bool signal_send_quote_reply(http_client_t *http,
                              const char *recipient,
                              const char *text,
                              const char *quote_author,
                              const char *quote_timestamp) {
    (void)http;
    if (!recipient || !text || !quote_author || !quote_timestamp || !g_signal_available)
        return false;

    char cmd[8192];
    const char *safe_text = shell_escape(text);
    int r = snprintf(cmd, sizeof(cmd),
        "%s -a %s send --quote-timestamp %s --quote-author %s "
        "-m \"%s\" %s 2>/dev/null",
        g_signal_cli, g_signal_number,
        quote_timestamp, quote_author, safe_text, recipient);
    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;

    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * P108: Send a message with a file/image attachment
 * Uses signal-cli's -a <file_path> flag:
 *   signal-cli -a <number> send -a <file_path> -m "<text>" <recipient>
 * If text is NULL or empty, sends the attachment without a text caption.
 * ------------------------------------------------------------------ */
bool signal_send_attachment(http_client_t *http,
                             const char *recipient,
                             const char *text,
                             const char *file_path) {
    (void)http;
    if (!recipient || !file_path || !g_signal_available) return false;

    char cmd[8192];
    if (text && text[0]) {
        const char *safe_text = shell_escape(text);
        int r = snprintf(cmd, sizeof(cmd),
            "%s -a %s send -a \"%s\" -m \"%s\" %s 2>/dev/null",
            g_signal_cli, g_signal_number, file_path, safe_text, recipient);
        if (r < 0 || (size_t)r >= sizeof(cmd)) return false;
    } else {
        int r = snprintf(cmd, sizeof(cmd),
            "%s -a %s send -a \"%s\" %s 2>/dev/null",
            g_signal_cli, g_signal_number, file_path, recipient);
        if (r < 0 || (size_t)r >= sizeof(cmd)) return false;
    }

    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * Helper: extract a string field from a nested JSON path.
 * Keys is a NULL-terminated array of field names to traverse.
 * Returns the string value at the leaf, or def on failure.
 * ------------------------------------------------------------------ */
static const char *json_nested_str(const json_t *root, const char * const *keys,
                                    const char *def) {
    const json_t *cur = root;
    for (int i = 0; keys[i]; i++) {
        cur = json_obj_get(cur, keys[i]);
        if (!cur) return def;
    }
    return json_get_str(cur, "", def);
}

/* ------------------------------------------------------------------
 * Helper: extract a nested JSON object.
 * Returns NULL if any key in the path is missing.
 * ------------------------------------------------------------------ */
static json_t *json_nested_obj(json_t *root, const char * const *keys) {
    json_t *cur = root;
    for (int i = 0; keys[i]; i++) {
        cur = json_obj_get(cur, keys[i]);
        if (!cur) return NULL;
    }
    return cur;
}

/* ------------------------------------------------------------------
 * Poll for incoming Signal messages using signal-cli receive
 * P108: Extended to extract group_id, reaction info, and attachment
 * paths from incoming messages.
 * ------------------------------------------------------------------ */
json_node_t *signal_poll_messages(http_client_t *http) {
    (void)http;
    if (!g_signal_available) return NULL;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "%s -a %s receive --json 2>/dev/null | head -50",
        g_signal_cli, g_signal_number);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    json_node_t *results = json_array();
    char line[8192];

    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        char *err = NULL;
        json_node_t *msg = json_parse(line, &err);
        if (!msg) { free(err); continue; }

        json_node_t *env = json_obj_get(msg, "envelope");
        if (!env) { json_free(msg); continue; }

        const char *source = json_get_str(env, "source", "unknown");

        /* ---- dataMessage (normal text / attachment / quote) ---- */
        json_node_t *data = json_obj_get(env, "dataMessage");
        if (data) {
            const char *text = json_get_str(data, "message", "");

            json_node_t *result = json_object();
            json_set(result, "chat_id", json_string(source));
            json_set(result, "text", json_string(text && text[0] ? text : "(no text)"));

            /* Extract group_id if present */
            json_node_t *group = json_obj_get(data, "groupInfo");
            if (group) {
                const char *gid = json_get_str(group, "groupId", NULL);
                if (gid)
                    json_set(result, "group_id", json_string(gid));
            }

            /* Extract quote info (reply context) */
            json_node_t *quote = json_obj_get(data, "quote");
            if (quote) {
                const char *q_auth = json_get_str(quote, "author", NULL);
                const char *q_id   = json_get_str(quote, "id", NULL);
                if (q_auth) json_set(result, "quote_author", json_string(q_auth));
                if (q_id)   json_set(result, "quote_id", json_string(q_id));
            }

            /* Extract attachment paths */
            json_node_t *atts = json_obj_get(data, "attachments");
            if (atts && json_len(atts) > 0) {
                json_node_t *first_att = json_get(atts, 0);
                if (first_att) {
                    const char *att_path = json_get_str(first_att, "path", NULL);
                    if (att_path)
                        json_set(result, "attachment", json_string(att_path));
                }
            }

            json_append(results, result);
        }

        /* ---- syncMessage (sent from our own linked device) ---- */
        json_node_t *sync = json_obj_get(env, "syncMessage");
        if (sync) {
            json_node_t *sent = json_obj_get(sync, "sentMessage");
            if (sent) {
                const char *text = json_get_str(sent, "message", "");
                const char *dest = json_get_str(sent, "destination", source);

                json_node_t *result = json_object();
                json_set(result, "chat_id", json_string(dest));
                json_set(result, "text", json_string(text && text[0] ? text : "(no text)"));

                /* Extract group_id from syncMessage.sentMessage */
                json_node_t *group = json_obj_get(sent, "groupInfo");
                if (group) {
                    const char *gid = json_get_str(group, "groupId", NULL);
                    if (gid)
                        json_set(result, "group_id", json_string(gid));
                }

                /* Extract attachment from syncMessage.sentMessage */
                json_node_t *atts = json_obj_get(sent, "attachments");
                if (atts && json_len(atts) > 0) {
                    json_node_t *first_att = json_get(atts, 0);
                    if (first_att) {
                        const char *att_path = json_get_str(first_att, "path", NULL);
                        if (att_path)
                            json_set(result, "attachment", json_string(att_path));
                    }
                }

                json_append(results, result);
            }
        }

        /* ---- reaction (incoming emoji reaction) ---- */
        json_node_t *react = json_obj_get(env, "reaction");
        if (react) {
            const char *emoji = json_get_str(react, "emoji", "");
            const char *target_auth = json_get_str(react, "targetAuthor", source);
            json_node_t *target_sent = json_obj_get(react, "targetSentTimestamp");
            char ts_buf[64] = "0";
            if (target_sent && target_sent->type == JSON_NUMBER) {
                snprintf(ts_buf, sizeof(ts_buf), "%.0f", target_sent->num_val);
            }

            json_node_t *result = json_object();
            json_set(result, "chat_id", json_string(source));
            json_set(result, "text", json_string("(reaction)"));
            json_set(result, "reaction", json_string(emoji));
            json_set(result, "reaction_target_author", json_string(target_auth));
            json_set(result, "reaction_target_timestamp", json_string(ts_buf));

            json_append(results, result);
        }

        json_free(msg);
    }
    pclose(fp);

    return json_len(results) > 0 ? results : NULL;
}

const char *signal_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "");
}

const char *signal_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}

/* P108: Get group_id from a polled update (NULL if not a group message) */
const char *signal_get_group_id(json_node_t *update) {
    return json_get_str(update, "group_id", NULL);
}

/* P108: Get reaction emoji from a polled update (NULL if not a reaction) */
const char *signal_get_reaction(json_node_t *update) {
    return json_get_str(update, "reaction", NULL);
}

/* P108: Get attachment file path from a polled update (NULL if none) */
const char *signal_get_attachment(json_node_t *update) {
    return json_get_str(update, "attachment", NULL);
}
