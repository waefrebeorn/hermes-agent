/*
 * weixin.c — WeChat (iLink Bot API) gateway for Hermes C.
 * Long-poll getupdates for inbound messages + sendmessage for outbound.
 * Configuration via env vars: WEIXIN_TOKEN, WEIXIN_ACCOUNT_ID.
 * MIT License — WuBu Hermes Project
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

/* ================================================================
 *  Constants
 * ================================================================ */

#define ILINK_BASE_URL "https://ilinkai.weixin.qq.com"
#define EP_GET_UPDATES "ilink/bot/getupdates"
#define EP_SEND_MESSAGE "ilink/bot/sendmessage"
#define LONG_POLL_TIMEOUT_MS 35000
#define API_TIMEOUT_MS 15000
#define MAX_CONSECUTIVE_FAILURES 3
#define RETRY_DELAY_SEC 2
#define BACKOFF_DELAY_SEC 30
#define CONTEXT_TOKEN_DIR "weixin"
#define MAX_PATH 4096
#define MAX_BODY 16384

/* ================================================================
 *  State
 * ================================================================ */

typedef struct {
    char token[512];
    char account_id[128];
    char base_url[1024];
    bool running;
    http_t *http;
    char sync_buf[4096];
    int consecutive_failures;
} weixin_state_t;

static weixin_state_t g_wx;

/* ================================================================
 *  iLink API — POST with custom headers
 * ================================================================ */

/* Build iLink headers as a single newline-separated string */
static void build_headers(const char *body, char *hdr_buf, size_t hdr_sz) {
    snprintf(hdr_buf, hdr_sz,
        "Content-Type: application/json\n"
        "AuthorizationType: ilink_bot_token\n"
        "Authorization: Bearer %s\n"
        "Content-Length: %zu\n"
        "iLink-App-Id: bot\n"
        "iLink-App-ClientVersion: 131584",
        g_wx.token, strlen(body));
}

/* POST to iLink API with full payload */
static char *wx_api_post(const char *endpoint, const char *payload_json,
                          int timeout_ms, const char *override_token) {
    char url[2048];
    snprintf(url, sizeof(url), "%s/%s", g_wx.base_url, endpoint);

    /* Build full payload: add base_info */
    char full_body[MAX_BODY];
    snprintf(full_body, sizeof(full_body),
             "%s,\"base_info\":{\"channel_version\":\"2.2.0\"}}",
             payload_json);

    /* Build headers */
    char headers[1024];
    const char *tok = override_token ? override_token : g_wx.token;
    snprintf(headers, sizeof(headers),
        "Content-Type: application/json\n"
        "AuthorizationType: ilink_bot_token\n"
        "Authorization: Bearer %s\n"
        "Content-Length: %zu\n"
        "iLink-App-Id: bot\n"
        "iLink-App-ClientVersion: 131584",
        tok, strlen(full_body));

    /* Create temporary http client for each request */
    http_t *h = http_new(timeout_ms > 0 ? (timeout_ms / 1000) : 15);
    if (!h) return NULL;

    http_resp_t *resp = http_request(h, HTTP_POST, url, headers,
                                      full_body, strlen(full_body));

    char *result = NULL;
    if (resp && resp->body) {
        result = strdup(resp->body);
    }
    if (resp) http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Context token store (file-based)
 * ================================================================ */

static void ensure_weixin_dir(void) {
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (home) {
        char dir[MAX_PATH];
        snprintf(dir, sizeof(dir), "%s/%s", home, CONTEXT_TOKEN_DIR);
        mkdir(dir, 0755);
    }
}

static void ctx_token_path(char *buf, size_t sz) {
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/%s/%s.context-tokens.json", home, CONTEXT_TOKEN_DIR, g_wx.account_id);
}

static void ctx_token_save(const char *user_id, const char *token) {
    char path[MAX_PATH];
    ctx_token_path(path, sizeof(path));

    json_t *data = NULL;
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            char *buf = malloc((size_t)sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, (size_t)sz, f);
                buf[n] = '\0';
                data = json_parse(buf, NULL);
                free(buf);
            }
        }
        fclose(f);
    }

    if (!data) data = json_object();
    json_set(data, user_id, json_string(token));

    char *json_str = json_serialize(data);
    if (json_str) {
        FILE *out = fopen(path, "wb");
        if (out) { fwrite(json_str, 1, strlen(json_str), out); fclose(out); }
        free(json_str);
    }
    json_free(data);
}

static const char *ctx_token_get(const char *user_id) {
    static char cached_token[512];
    cached_token[0] = '\0';

    char path[MAX_PATH];
    ctx_token_path(path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    json_t *data = json_parse(buf, NULL);
    free(buf);
    if (!data) return NULL;

    const char *tok = json_get_str(data, user_id, "");
    if (*tok) {
        snprintf(cached_token, sizeof(cached_token), "%s", tok);
        json_free(data);
        return cached_token;
    }
    json_free(data);
    return NULL;
}

/* ================================================================
 *  Sync buffer persistence
 * ================================================================ */

static void sync_buf_path(char *buf, size_t sz) {
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/%s/%s.sync.json", home, CONTEXT_TOKEN_DIR, g_wx.account_id);
}

static void sync_buf_load(void) {
    char path[MAX_PATH];
    sync_buf_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    json_t *data = json_parse(buf, NULL);
    free(buf);
    if (data) {
        const char *sb = json_get_str(data, "get_updates_buf", "");
        if (*sb)
            snprintf(g_wx.sync_buf, sizeof(g_wx.sync_buf), "%s", sb);
        json_free(data);
    }
}

static void sync_buf_save(void) {
    char path[MAX_PATH];
    sync_buf_path(path, sizeof(path));
    char json[4128];
    snprintf(json, sizeof(json),
             "{\"get_updates_buf\":\"%s\"}", g_wx.sync_buf);
    ensure_weixin_dir();
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(json, 1, strlen(json), f); fclose(f); }
}

/* ================================================================
 *  iLink API wrappers
 * ================================================================ */

/* getUpdates — long-poll */
static char *get_updates(void) {
    char payload[MAX_BODY];
    snprintf(payload, sizeof(payload),
             "{\"get_updates_buf\":\"%s\"", g_wx.sync_buf);

    char *resp = wx_api_post(EP_GET_UPDATES, payload, LONG_POLL_TIMEOUT_MS, NULL);
    if (!resp) {
        /* Timeout is normal — return empty result */
        return strdup("{\"ret\":0,\"msgs\":[]}");
    }
    return resp;
}

/* sendMessage */
static int send_message(const char *to_user_id, const char *text,
                         const char *context_token) {
    char payload[MAX_BODY];
    if (context_token && *context_token) {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":1,\"content\":\"%s\","
                 "\"context_token\":\"%s\"",
                 to_user_id, text, context_token);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":1,\"content\":\"%s\"",
                 to_user_id, text);
    }

    char *resp = wx_api_post(EP_SEND_MESSAGE, payload, API_TIMEOUT_MS, NULL);
    if (!resp) return -1;

    json_t *j = json_parse(resp, NULL);
    free(resp);
    if (!j) return -1;

    int ret = (int)json_get_num(j, "ret", 0);
    int errcode = (int)json_get_num(j, "errcode", 0);
    json_free(j);

    if (ret != 0 || errcode != 0) return errcode ? errcode : ret;
    return 0;
}

/* ================================================================
 *  Process inbound message
 * ================================================================ */

static void process_message(json_t *msg) {
    const char *from_id = json_get_str(msg, "from_user_id", "");
    const char *room_id = json_get_str(msg, "room_id", "");

    if (!*from_id) return;
    const char *chat_id = *room_id ? room_id : from_id;

    /* Extract context token */
    const char *ctx_token = json_get_str(msg, "context_token", "");
    if (*ctx_token)
        ctx_token_save(from_id, ctx_token);

    /* Extract text from items */
    json_t *items = json_obj_get(msg, "items");
    char text[4096] = "";
    if (items && items->type == JSON_ARRAY) {
        size_t n = json_len(items);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(items, i);
            int type = (int)json_get_num(item, "type", 0);
            if (type == 1) { /* text */
                json_t *text_item = json_obj_get(item, "text_item");
                if (text_item) {
                    const char *t = json_get_str(text_item, "text", "");
                    if (*t) {
                        if (*text) strncat(text, " ", sizeof(text) - strlen(text) - 1);
                        strncat(text, t, sizeof(text) - strlen(text) - 1);
                    }
                }
            }
        }
    }

    if (!*text) return;

    printf("[gateway:weixin] Message from %s: %.200s\n", from_id, text);

    /* Forward to agent */
    extern gateway_state_t g_gw;
    char *resp = agent_chat(&g_gw.agent, text);
    if (resp) {
        const char *ctx = ctx_token_get(from_id);
        send_message(chat_id, resp, ctx);
        free(resp);
    }
}

/* ================================================================
 *  Poll loop
 * ================================================================ */

static void *thread_weixin_poll(void *arg) {
    (void)arg;
    printf("[gateway] Weixin poll loop started\n");

    sync_buf_load();

    while (g_wx.running) {
        char *resp = get_updates();
        if (!resp) {
            g_wx.consecutive_failures++;
            sleep(RETRY_DELAY_SEC);
            continue;
        }

        json_t *root = json_parse(resp, NULL);
        free(resp);

        if (!root) {
            g_wx.consecutive_failures++;
            sleep(RETRY_DELAY_SEC);
            continue;
        }

        int ret = (int)json_get_num(root, "ret", 0);
        int errcode = (int)json_get_num(root, "errcode", 0);

        if (ret != 0 || errcode != 0) {
            /* Session expired (-14) — pause 10 min */
            if (errcode == -14 || ret == -14) {
                printf("[gateway:weixin] Session expired; pausing 10m\n");
                json_free(root);
                for (int i = 0; i < 600 && g_wx.running; i++) sleep(1);
                g_wx.consecutive_failures = 0;
                continue;
            }

            g_wx.consecutive_failures++;
            if (g_wx.consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
                printf("[gateway:weixin] Too many failures; backing off %ds\n",
                       BACKOFF_DELAY_SEC);
                sleep(BACKOFF_DELAY_SEC);
                g_wx.consecutive_failures = 0;
            } else {
                sleep(RETRY_DELAY_SEC);
            }
            json_free(root);
            continue;
        }

        /* Success */
        g_wx.consecutive_failures = 0;

        /* Update sync_buf */
        const char *new_buf = json_get_str(root, "get_updates_buf", "");
        if (*new_buf) {
            snprintf(g_wx.sync_buf, sizeof(g_wx.sync_buf), "%s", new_buf);
            sync_buf_save();
        }

        /* Process messages */
        json_t *msgs = json_obj_get(root, "msgs");
        if (msgs && msgs->type == JSON_ARRAY) {
            size_t n = json_len(msgs);
            for (size_t i = 0; i < n; i++) {
                process_message(json_get(msgs, i));
            }
        }

        json_free(root);
    }

    printf("[gateway:weixin] Poll loop stopped\n");
    return NULL;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool weixin_init(const char *token, const char *account_id) {
    memset(&g_wx, 0, sizeof(g_wx));
    if (token) snprintf(g_wx.token, sizeof(g_wx.token), "%s", token);
    if (account_id) snprintf(g_wx.account_id, sizeof(g_wx.account_id), "%s", account_id);
    snprintf(g_wx.base_url, sizeof(g_wx.base_url), "%s", ILINK_BASE_URL);
    return *g_wx.token && *g_wx.account_id;
}

void weixin_start(void) {
    if (!*g_wx.token || !*g_wx.account_id) {
        fprintf(stderr, "[gateway:weixin] Token and account_id required\n");
        return;
    }
    g_wx.running = true;
    ensure_weixin_dir();
    printf("[gateway] Weixin platform started (account=%s)\n", g_wx.account_id);
    thread_weixin_poll(NULL);
}

void weixin_stop(void) {
    g_wx.running = false;
}
