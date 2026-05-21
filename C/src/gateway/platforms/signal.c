/*
 * signal.c — Signal gateway for Hermes C.
 * Uses signal-cli for sending and receiving messages.
 * Falls back gracefully if signal-cli is not installed.
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

bool signal_send_message(http_client_t *http, const char *to, const char *text) {
    (void)http;
    if (!to || !text || !g_signal_available) return false;

    char cmd[8192];
    int r = snprintf(cmd, sizeof(cmd),
        "%s -a %s send -m \"%s\" %s 2>/dev/null",
        g_signal_cli, g_signal_number, text, to);
    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;

    return system(cmd) == 0;
}

/* Poll for incoming Signal messages using signal-cli receive */
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

    /* Parse JSON output from signal-cli --json */
    /* signal-cli outputs one JSON object per message */
    while (fgets(line, sizeof(line), fp)) {
        /* Skip empty lines */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        char *err = NULL;
        json_node_t *msg = json_parse(line, &err);
        if (!msg) { free(err); continue; }

        const char *envelope = json_get_str(msg, "envelope", NULL);
        /* Simple flat format: {"envelope": {"source": "+1234", "dataMessage": {"message": "hello"}}} */
        json_node_t *env = json_obj_get(msg, "envelope");
        if (env) {
            const char *source = json_get_str(env, "source", "unknown");
            json_node_t *data = json_obj_get(env, "dataMessage");
            const char *text = "";
            if (data)
                text = json_get_str(data, "message", "");
            else {
                /* Check for syncMessage */
                json_node_t *sync = json_obj_get(env, "syncMessage");
                if (sync) {
                    json_node_t *sent = json_obj_get(sync, "sentMessage");
                    if (sent)
                        text = json_get_str(sent, "message", "");
                }
            }

            json_node_t *result = json_object();
            json_set(result, "chat_id", json_string(source));
            json_set(result, "text", json_string(text && text[0] ? text : "(no text)"));
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
