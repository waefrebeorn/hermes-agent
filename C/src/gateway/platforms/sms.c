/*
 * sms.c — SMS gateway for Hermes C.
 * Uses Twilio REST API for sending SMS.
 * Expects curl on PATH for HTTP requests.
 *
 * Env vars:
 *   TWILIO_ACCOUNT_SID  — Twilio Account SID
 *   TWILIO_AUTH_TOKEN   — Twilio Auth Token
 *   TWILIO_FROM_NUMBER  — Twilio phone number to send from
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_twilio_sid[128] = "";
static char g_twilio_token[128] = "";
static char g_twilio_from[32] = "";

void sms_set_twilio(const char *sid, const char *token, const char *from) {
    if (sid) snprintf(g_twilio_sid, sizeof(g_twilio_sid), "%s", sid);
    if (token) snprintf(g_twilio_token, sizeof(g_twilio_token), "%s", token);
    if (from) snprintf(g_twilio_from, sizeof(g_twilio_from), "%s", from);
}

bool sms_send_message(http_client_t *http, const char *to, const char *text) {
    (void)http;
    if (!g_twilio_sid[0] || !g_twilio_from[0] || !to || !text) return false;

    /* Use curl to send via Twilio API */
    char cmd[8192];
    char *escaped_to = strdup(to);
    char *escaped_text = strdup(text);
    /* Simple escaping — replace special chars */
    for (char *p = escaped_text; *p; p++) {
        if (*p == '"') *p = '\'';
    }

    int r = snprintf(cmd, sizeof(cmd),
        "curl -s -X POST 'https://api.twilio.com/2010-04-01/Accounts/%s/Messages.json' "
        "--data-urlencode 'To=%s' "
        "--data-urlencode 'From=%s' "
        "--data-urlencode 'Body=%s' "
        "-u '%s:%s' >/dev/null 2>&1",
        g_twilio_sid, escaped_to, g_twilio_from,
        escaped_text, g_twilio_sid, g_twilio_token);
    free(escaped_to);
    free(escaped_text);

    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;
    return system(cmd) == 0;
}

/* SMS polling: no incoming (Twilio requires webhook setup).
 * Messages are received via the webhook platform. */
json_node_t *sms_poll_messages(http_client_t *http) {
    (void)http;
    return NULL;
}

const char *sms_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "sms");
}

const char *sms_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
