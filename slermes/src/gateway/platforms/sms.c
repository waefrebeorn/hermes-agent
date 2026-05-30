/*
 * sms.c — SMS/MMS gateway for Hermes C (Twilio).
 * P111: Carrier lookup, MMS support, inbound webhooks, delivery callbacks.
 *
 * Uses Twilio REST API for sending SMS/MMS via curl.
 * Uses libhttp for carrier lookup (GET with Basic Auth).
 * Inbound messages and delivery status arrive via a shared webhook server.
 *
 * Env vars:
 *   TWILIO_ACCOUNT_SID       — Twilio Account SID
 *   TWILIO_AUTH_TOKEN        — Twilio Auth Token
 *   TWILIO_FROM_NUMBER       — Twilio phone number to send from
 *   TWILIO_STATUS_CALLBACK   — URL for delivery status webhook callbacks
 *   TWILIO_WEBHOOK_PATH      — Path for inbound SMS webhook (default: /sms-webhook)
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ──────────────────────────────────────────────
 *  Static state
 * ────────────────────────────────────────────── */

static char g_twilio_sid[128] = "";
static char g_twilio_token[128] = "";
static char g_twilio_from[32] = "";
static char g_status_callback_url[2048] = "";
static char g_webhook_path[256] = "";

/* ──────────────────────────────────────────────
 *  Setup
 * ────────────────────────────────────────────── */

void sms_set_twilio(const char *sid, const char *token, const char *from) {
    if (sid)   snprintf(g_twilio_sid,   sizeof(g_twilio_sid),   "%s", sid);
    if (token) snprintf(g_twilio_token, sizeof(g_twilio_token), "%s", token);
    if (from)  snprintf(g_twilio_from,  sizeof(g_twilio_from),  "%s", from);
}

void sms_set_status_callback(const char *url) {
    if (url)
        snprintf(g_status_callback_url, sizeof(g_status_callback_url), "%s", url);
}

void sms_set_webhook_url(const char *url) {
    if (url)
        snprintf(g_webhook_path, sizeof(g_webhook_path), "%s", url);
}

/* ──────────────────────────────────────────────
 *  Base64 encoding (for HTTP Basic Auth)
 * ────────────────────────────────────────────── */

static const char b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const unsigned char *in, size_t in_len,
                           char *out, size_t out_cap) {
    size_t i = 0, o = 0;
    while (i < in_len && o + 4 < out_cap) {
        unsigned char a = in[i++];
        unsigned char b = i < in_len ? in[i++] : 0;
        unsigned char c = i < in_len ? in[i++] : 0;
        unsigned int triple = ((unsigned int)a << 16) | ((unsigned int)b << 8) | c;
        out[o++] = b64_chars[(triple >> 18) & 0x3F];
        out[o++] = b64_chars[(triple >> 12) & 0x3F];
        out[o++] = (i - 1 < in_len) ? b64_chars[(triple >> 6) & 0x3F] : '=';
        out[o++] = i < in_len ? b64_chars[triple & 0x3F] : '=';
    }
    out[o] = '\0';
}

/* ──────────────────────────────────────────────
 *  Send SMS via curl
 * ────────────────────────────────────────────── */

static bool sms_send_via_curl(const char *to, const char *text,
                               const char *media_url, const char *status_callback) {
    if (!g_twilio_sid[0] || !g_twilio_from[0] || !to || !text) return false;

    /* Build curl command */
    char cmd[16384];
    int pos = 0;

    pos += snprintf(cmd + pos, sizeof(cmd) - (size_t)pos,
        "curl -s -X POST 'https://api.twilio.com/2010-04-01/Accounts/%s/Messages.json' "
        "--data-urlencode 'To=%s' "
        "--data-urlencode 'From=%s' "
        "--data-urlencode 'Body=%s'",
        g_twilio_sid, to, g_twilio_from, text);

    if (pos < 0 || (size_t)pos >= sizeof(cmd)) return false;

    /* Optional: MediaUrl for MMS */
    if (media_url && media_url[0]) {
        int n = snprintf(cmd + pos, sizeof(cmd) - (size_t)pos,
                         " --data-urlencode 'MediaUrl=%s'", media_url);
        if (n > 0) pos += n;
        if (pos < 0 || (size_t)pos >= sizeof(cmd)) return false;
    }

    /* Optional: StatusCallback for delivery status */
    if (status_callback && status_callback[0]) {
        int n = snprintf(cmd + pos, sizeof(cmd) - (size_t)pos,
                         " --data-urlencode 'StatusCallback=%s'", status_callback);
        if (n > 0) pos += n;
        if (pos < 0 || (size_t)pos >= sizeof(cmd)) return false;
    }

    /* Auth */
    {
        int n = snprintf(cmd + pos, sizeof(cmd) - (size_t)pos,
                         " -u '%s:%s' >/dev/null 2>&1",
                         g_twilio_sid, g_twilio_token);
        if (n > 0) pos += n;
        if (pos < 0 || (size_t)pos >= sizeof(cmd)) return false;
    }

    return system(cmd) == 0;
}

bool sms_send_message(http_client_t *http, const char *to, const char *text) {
    (void)http;
    /* Include status callback URL if configured */
    const char *cb = g_status_callback_url[0] ? g_status_callback_url : NULL;
    return sms_send_via_curl(to, text, NULL, cb);
}

/* ──────────────────────────────────────────────
 *  P111: Send MMS (text + media)
 * ────────────────────────────────────────────── */

bool sms_send_mms(http_client_t *http, const char *to, const char *text,
                   const char *media_url) {
    (void)http;
    if (!media_url || !media_url[0]) {
        /* No media URL — fall back to plain SMS */
        return sms_send_message(http, to, text);
    }
    const char *cb = g_status_callback_url[0] ? g_status_callback_url : NULL;
    return sms_send_via_curl(to, text, media_url, cb);
}

/* ──────────────────────────────────────────────
 *  P111: Carrier lookup via Twilio Lookup API v2
 * ──────────────────────────────────────────────
 * GET https://lookups.twilio.com/v2/PhoneNumbers/{phone}
 * With Basic Auth (AccountSID:AuthToken)
 */

json_node_t *sms_lookup_carrier(http_client_t *http, const char *phone_number) {
    if (!http || !phone_number || !phone_number[0] ||
        !g_twilio_sid[0] || !g_twilio_token[0]) {
        return NULL;
    }

    /* Build URL */
    char url[2048];
    snprintf(url, sizeof(url),
             "https://lookups.twilio.com/v2/PhoneNumbers/%s?Fields=carrier",
             phone_number);

    /* Build Basic auth header */
    char auth_buf[512];
    snprintf(auth_buf, sizeof(auth_buf), "%s:%s", g_twilio_sid, g_twilio_token);
    char b64_buf[1024];
    base64_encode((const unsigned char *)auth_buf, strlen(auth_buf),
                   b64_buf, sizeof(b64_buf));

    char headers[2048];
    snprintf(headers, sizeof(headers),
             "Authorization: Basic %s\r\n"
             "Accept: application/json",
             b64_buf);

    http_response_t *resp = http_get(http, url, headers);
    if (!resp || resp->status != 200) {
        if (resp) http_response_free(resp);
        return NULL;
    }

    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    free(err);
    http_response_free(resp);

    if (!root) return NULL;

    /* Build a simplified result with carrier info */
    const char *carrier_name = "";
    const char *carrier_type = "";
    const char *country_code = "";
    const char *national_format = "";

    json_node_t *carrier = json_obj_get(root, "carrier");
    if (carrier) {
        carrier_name = json_get_str(carrier, "name", "");
        carrier_type = json_get_str(carrier, "type", "");
    }
    country_code = json_get_str(root, "country_code", "");
    national_format = json_get_str(root, "national_format", "");

    json_node_t *result = json_new_object();
    json_set(result, "phone_number",   json_string(phone_number));
    json_set(result, "country_code",   json_string(country_code));
    json_set(result, "national_format",json_string(national_format));
    json_set(result, "carrier_name",   json_string(carrier_name));
    json_set(result, "carrier_type",   json_string(carrier_type));

    json_free(root);
    return result;
}

/* ──────────────────────────────────────────────
 *  URL-decoding helper
 * ────────────────────────────────────────────── */

static char *url_decode(const char *src) {
    if (!src) return strdup("");
    size_t len = strlen(src);
    char *dst = malloc(len + 1);
    if (!dst) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len && src[i]; i++) {
        if (src[i] == '%' && i + 2 < len) {
            char hex[3] = {src[i+1], src[i+2], '\0'};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
    return dst;
}

/* ──────────────────────────────────────────────
 *  P111: Parse form-urlencoded Twilio webhook body
 * ──────────────────────────────────────────────
 *
 * Twilio POSTs form-urlencoded data to our webhook endpoint
 * on both inbound messages and delivery status callbacks.
 *
 * Inbound SMS keys:
 *   From, To, Body, MessageSid, NumMedia,
 *   MediaUrl0, MediaContentType0, ...
 *
 * Status callback keys:
 *   MessageSid, MessageStatus, ErrorCode, ErrorMessage,
 *   To, From, SmsStatus, AccountSid
 */

json_node_t *sms_parse_webhook(const char *body) {
    if (!body || !body[0]) return NULL;

    json_node_t *update = json_new_object();
    if (!update) return NULL;

    /* Parse key=value pairs separated by & */
    char buf[16384];
    snprintf(buf, sizeof(buf), "%s", body);

    char *saveptr;
    char *pair = strtok_r(buf, "&", &saveptr);
    while (pair) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';
            char *key = pair;
            char *raw_val = eq + 1;
            char *val = url_decode(raw_val);

            /* Map Twilio keys to our update fields */
            if (strcmp(key, "From") == 0) {
                json_set(update, "from", json_string(val ? val : ""));
            } else if (strcmp(key, "To") == 0) {
                json_set(update, "to", json_string(val ? val : ""));
            } else if (strcmp(key, "Body") == 0) {
                json_set(update, "text", json_string(val ? val : ""));
            } else if (strcmp(key, "MessageSid") == 0) {
                json_set(update, "message_sid", json_string(val ? val : ""));
            } else if (strcmp(key, "NumMedia") == 0) {
                long nm = val ? strtol(val, NULL, 10) : 0;
                json_set(update, "num_media", json_number((double)nm));
            } else if (strcmp(key, "SmsStatus") == 0 || strcmp(key, "MessageStatus") == 0) {
                json_set(update, "status", json_string(val ? val : ""));
            } else if (strcmp(key, "ErrorCode") == 0) {
                json_set(update, "error_code", json_string(val ? val : ""));
            } else if (strcmp(key, "ErrorMessage") == 0) {
                json_set(update, "error_message", json_string(val ? val : ""));
            } else if (strncmp(key, "MediaUrl", 8) == 0) {
                /* MediaUrl0, MediaUrl1, ... — collect into array */
                json_node_t *urls = json_obj_get(update, "media_urls");
                if (!urls) {
                    urls = json_new_array();
                    json_set(update, "media_urls", urls);
                }
                json_append(urls, json_string(val ? val : ""));
            }

            if (val) free(val);
        }
        pair = strtok_r(NULL, "&", &saveptr);
    }

    /* Ensure 'from' is set as chat_id for compatibility */
    if (!json_obj_get(update, "from")) {
        json_set(update, "from", json_string("unknown"));
    }
    /* Default text to empty string */
    if (!json_obj_get(update, "text")) {
        json_set(update, "text", json_string(""));
    }
    /* Default num_media */
    if (!json_obj_get(update, "num_media")) {
        json_set(update, "num_media", json_number(0));
    }

    /* Wrap in array for compatibility with poll API */
    json_node_t *result = json_new_array();
    json_append(result, update);
    return result;
}

/* ──────────────────────────────────────────────
 *  P111: Webhook verification (for Twilio number confirmation)
 * ──────────────────────────────────────────────
 *
 * Twilio sends GET /sms-webhook?From=...&To=... when verifying the webhook URL.
 * We accept any request to the configured webhook path.
 * Returns the challenge string (currently just the path), or NULL.
 */

const char *sms_verify_webhook(const char *query_string) {
    (void)query_string;
    /* We accept all requests — return a simple confirmation */
    return "OK";
}

/* ──────────────────────────────────────────────
 *  Webhook message queue — stores inbound SMS/MMS
 * ────────────────────────────────────────────── */

#define SMS_QUEUE_MAX 64

typedef struct {
    char chat_id[128];
    char text[4096];
    char sender_id[128];
    time_t timestamp;
} sms_msg_t;

static sms_msg_t g_sms_queue[SMS_QUEUE_MAX];
static int g_sms_head = 0;
static int g_sms_tail = 0;
static pthread_mutex_t g_sms_lock = PTHREAD_MUTEX_INITIALIZER;

void sms_queue_message(const char *chat_id, const char *text,
                        const char *sender_id) {
    pthread_mutex_lock(&g_sms_lock);
    int next = (g_sms_tail + 1) % SMS_QUEUE_MAX;
    if (next != g_sms_head) {
        snprintf(g_sms_queue[g_sms_tail].chat_id, sizeof(g_sms_queue[g_sms_tail].chat_id), "%s", chat_id ? chat_id : "sms");
        snprintf(g_sms_queue[g_sms_tail].text, sizeof(g_sms_queue[g_sms_tail].text), "%s", text ? text : "");
        snprintf(g_sms_queue[g_sms_tail].sender_id, sizeof(g_sms_queue[g_sms_tail].sender_id), "%s", sender_id ? sender_id : "");
        g_sms_queue[g_sms_tail].timestamp = time(NULL);
        g_sms_tail = next;
    }
    pthread_mutex_unlock(&g_sms_lock);
}

/* Handle incoming Twilio webhook POST. Parses body and queues message. */
void sms_handle_webhook(const char *body) {
    if (!body || !body[0]) return;

    json_node_t *parsed = sms_parse_webhook(body);
    if (!parsed) return;

    size_t n = json_len(parsed);
    for (size_t i = 0; i < n; i++) {
        json_node_t *update = json_get(parsed, i);
        const char *from = json_get_str(update, "from", "");
        const char *text = json_get_str(update, "text", "");
        const char *to = json_get_str(update, "to", "");
        if (from[0] && text[0]) {
            sms_queue_message(from, text, to);
        }
    }
    json_free(parsed);
}

/* ──────────────────────────────────────────────
 *  Poll messages — reads from webhook message queue
 * ────────────────────────────────────────────── */

json_node_t *sms_poll_messages(http_client_t *http) {
    (void)http;

    pthread_mutex_lock(&g_sms_lock);
    if (g_sms_head == g_sms_tail) {
        pthread_mutex_unlock(&g_sms_lock);
        return NULL;
    }

    json_node_t *results = json_array();
    while (g_sms_head != g_sms_tail) {
        json_node_t *msg = json_new_object();
        json_set(msg, "chat_id", json_string(g_sms_queue[g_sms_head].chat_id));
        json_set(msg, "text", json_string(g_sms_queue[g_sms_head].text));
        json_set(msg, "sender_id", json_string(g_sms_queue[g_sms_head].sender_id));
        json_append(results, msg);
        g_sms_head = (g_sms_head + 1) % SMS_QUEUE_MAX;
    }
    pthread_mutex_unlock(&g_sms_lock);
    return results;
}

/* ──────────────────────────────────────────────
 *  P111: Update extractors
 * ────────────────────────────────────────────── */

const char *sms_get_chat_id(json_node_t *update) {
    return json_get_str(update, "from", "sms");
}

const char *sms_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}

const char *sms_get_message_sid(json_node_t *update) {
    return json_get_str(update, "message_sid", "");
}

const char *sms_get_status(json_node_t *update) {
    return json_get_str(update, "status", "");
}

const char *sms_get_media_url(json_node_t *update, size_t index) {
    json_node_t *urls = json_obj_get(update, "media_urls");
    if (!urls || index >= (size_t)json_len(urls)) return NULL;
    json_node_t *item = json_get(urls, index);
    if (!item || item->type != JSON_STRING) return NULL;
    return item->str_val;
}

size_t sms_get_num_media(json_node_t *update) {
    json_node_t *nm = json_obj_get(update, "num_media");
    if (!nm) return 0;
    return (size_t)json_get_num(update, "num_media", 0.0);
}
