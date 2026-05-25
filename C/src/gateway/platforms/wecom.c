/*
 * wecom.c — WeCom (WeChat Work) gateway for Hermes C.
 * P113: Full feature parity — markdown, at-mention, images, files, news, task cards.
 *
 * Env vars:
 *   WECOM_WEBHOOK_URL       — WeCom bot webhook URL
 *   WECOM_CORP_ID           — WeCom corp ID (for app API mode)
 *   WECOM_CORP_SECRET       — WeCom corp secret (for app API mode)
 *   WECOM_AGENT_ID          — WeCom agent ID (for app API mode)
 *
 * WeCom Bot API Docs:
 *   https://developer.work.weixin.qq.com/document/path/91770
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_wecom_webhook[1024] = "";
static char g_corp_id[256] = "";
static char g_corp_secret[512] = "";
static char g_agent_id[128] = "";

/* Cached access_token for app API mode */
static char g_access_token[1024] = "";
static double g_token_expires = 0.0;

void wecom_set_webhook(const char *url) {
    if (url) snprintf(g_wecom_webhook, sizeof(g_wecom_webhook), "%s", url);
}

void wecom_set_app_credentials(const char *corp_id, const char *corp_secret,
                                const char *agent_id) {
    if (corp_id) snprintf(g_corp_id, sizeof(g_corp_id), "%s", corp_id);
    if (corp_secret) snprintf(g_corp_secret, sizeof(g_corp_secret), "%s", corp_secret);
    if (agent_id) snprintf(g_agent_id, sizeof(g_agent_id), "%s", agent_id);
}

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Internal: POST JSON to webhook */
static bool post_webhook(http_client_t *http, json_node_t *root) {
    if (!g_wecom_webhook[0]) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char *body = json_serialize(root);
    json_free(root);
    if (!body) { if (local_http) http_client_free(local_http); return false; }

    char headers[256];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(http, HTTP_POST, g_wecom_webhook,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* Internal: get WeCom app access_token */
static const char *get_app_token(http_client_t *http) {
    if (!g_corp_id[0] || !g_corp_secret[0]) return NULL;

    double now = (double)time(NULL);
    if (g_token_expires > now + 60 && g_access_token[0])
        return g_access_token;

    /* Need fresh token */
    http_client_t *h = http ? http : http_client_new(10);
    http_client_t *local_http = NULL;
    if (!h) { local_http = http_client_new(10); h = local_http; }
    if (!h) return NULL;

    char url[1024];
    snprintf(url, sizeof(url),
             "https://qyapi.weixin.qq.com/cgi-bin/gettoken"
             "?corpid=%s&corpsecret=%s",
             g_corp_id, g_corp_secret);

    char headers[64];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(h, HTTP_GET, url, headers, NULL, 0);
    if (!resp || !resp->body) {
        if (local_http) http_client_free(local_http);
        return NULL;
    }

    json_node_t *j = json_parse(resp->body, NULL);
    int errcode = (int)json_get_num(j, "errcode", -1);
    if (errcode == 0) {
        const char *tok = json_get_str(j, "access_token", "");
        int expires = (int)json_get_num(j, "expires_in", 7200);
        if (*tok) {
            snprintf(g_access_token, sizeof(g_access_token), "%s", tok);
            g_token_expires = now + (double)expires;
        }
    }
    json_free(j);
    http_response_free(resp);
    if (local_http) http_client_free(local_http);

    return g_access_token[0] ? g_access_token : NULL;
}

/* ================================================================
 *  P113: Send plain text message (webhook)
 * ================================================================ */
bool wecom_send_message(http_client_t *http, const char *text) {
    if (!text) return false;
    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("text"));
    json_node_t *content = json_new_object();
    json_set(content, "content", json_string(text));
    json_set(root, "text", content);
    return post_webhook(http, root);
}

/* ================================================================
 *  P113: Send markdown message (webhook)
 *  WeCom markdown syntax: # header, **bold**, [link](url), @user
 * ================================================================ */
bool wecom_send_markdown(http_client_t *http, const char *text) {
    if (!text) return false;
    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("markdown"));
    json_node_t *content = json_new_object();
    json_set(content, "content", json_string(text));
    json_set(root, "markdown", content);
    return post_webhook(http, root);
}

/* ================================================================
 *  P113: Send text with @-mentions
 *  mentioned_list_json: JSON array of user IDs, e.g. "[\"wangqing\",\"@all\"]"
 *  mentioned_mobile_list_json: JSON array of phone numbers
 * ================================================================ */
bool wecom_send_text_with_at(http_client_t *http, const char *text,
                              const char *mentioned_list_json,
                              const char *mentioned_mobile_list_json) {
    if (!text) return false;

    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("text"));
    json_node_t *content = json_new_object();
    json_set(content, "content", json_string(text));
    json_set(root, "text", content);

    /* Add mentioned_list if present */
    if (mentioned_list_json && *mentioned_list_json) {
        json_node_t *ml = json_parse(mentioned_list_json, NULL);
        if (ml) json_set(content, "mentioned_list", ml);
    }

    /* Add mentioned_mobile_list if present */
    if (mentioned_mobile_list_json && *mentioned_mobile_list_json) {
        json_node_t *mml = json_parse(mentioned_mobile_list_json, NULL);
        if (mml) json_set(content, "mentioned_mobile_list", mml);
    }

    return post_webhook(http, root);
}

/* ================================================================
 *  P113: Send image message (app API mode)
 *  requries get_app_token to have succeeded
 * ================================================================ */
bool wecom_send_image(http_client_t *http, const char *base64_data,
                       const char *md5_hex) {
    if (!base64_data || !md5_hex) return false;

    /* App API mode: use the Send Message API */
    const char *tok = get_app_token(http);
    if (!tok) {
        /* Fallback to webhook text note about image */
        char note[2048];
        snprintf(note, sizeof(note),
                 "[Image: need app credentials to send image data]");
        return wecom_send_message(http, note);
    }

    http_client_t *h = http ? http : http_client_new(10);
    http_client_t *local_http = NULL;
    if (!h) { local_http = http_client_new(10); h = local_http; }
    if (!h) return false;

    char url[1024];
    snprintf(url, sizeof(url),
             "https://qyapi.weixin.qq.com/cgi-bin/message/send?access_token=%s",
             tok);

    json_node_t *root = json_new_object();
    json_set(root, "touser", json_string("@all"));
    json_set(root, "msgtype", json_string("image"));
    json_set(root, "agentid", json_number((int)strtol(g_agent_id, NULL, 10)));

    json_node_t *img = json_new_object();
    json_set(img, "base64", json_string(base64_data));
    json_set(img, "md5", json_string(md5_hex));
    json_set(root, "image", img);

    char *body = json_serialize(root);
    json_free(root);
    if (!body) { if (local_http) http_client_free(local_http); return false; }

    char headers[256];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(h, HTTP_POST, url,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* ================================================================
 *  P113: Send file by media_id (app API mode)
 * ================================================================ */
bool wecom_send_file(http_client_t *http, const char *media_id) {
    if (!media_id) return false;

    const char *tok = get_app_token(http);
    if (!tok) return false;

    http_client_t *h = http ? http : http_client_new(10);
    http_client_t *local_http = NULL;
    if (!h) { local_http = http_client_new(10); h = local_http; }
    if (!h) return false;

    char url[1024];
    snprintf(url, sizeof(url),
             "https://qyapi.weixin.qq.com/cgi-bin/message/send?access_token=%s",
             tok);

    json_node_t *root = json_new_object();
    json_set(root, "touser", json_string("@all"));
    json_set(root, "msgtype", json_string("file"));
    json_set(root, "agentid", json_number((int)strtol(g_agent_id, NULL, 10)));

    json_node_t *file = json_new_object();
    json_set(file, "media_id", json_string(media_id));
    json_set(root, "file", file);

    char *body = json_serialize(root);
    json_free(root);
    if (!body) { if (local_http) http_client_free(local_http); return false; }

    char headers[256];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(h, HTTP_POST, url,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* ================================================================
 *  P113: Send news/article message (webhook)
 *  articles_json: JSON array of {"title":"...","url":"...","picurl":"..."}
 * ================================================================ */
bool wecom_send_news(http_client_t *http, const char *articles_json) {
    if (!articles_json) return false;

    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("news"));

    json_node_t *news = json_new_object();
    json_node_t *articles = json_parse(articles_json, NULL);
    if (articles)
        json_set(news, "articles", articles);
    json_set(root, "news", news);

    return post_webhook(http, root);
}

/* ================================================================
 *  P113: Send interactive task card (buttons) — webhook
 *  btns_json: JSON array of {"key":"...","name":"...","is_permanant":false}
 * ================================================================ */
bool wecom_send_taskcard(http_client_t *http,
                          const char *title, const char *description,
                          const char *url, const char *task_id,
                          const char *btns_json) {
    if (!title) return false;

    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("template_card"));

    json_node_t *card = json_new_object();
    json_set(card, "card_type", json_string("button_interaction"));
    json_set(card, "title", json_string(title));
    if (description)
        json_set(card, "description", json_string(description));
    if (url)
        json_set(card, "url", json_string(url));
    if (task_id)
        json_set(card, "task_id", json_string(task_id));

    if (btns_json && *btns_json) {
        json_node_t *btns = json_parse(btns_json, NULL);
        if (btns) {
            json_node_t *task_card = json_new_object();
            json_set(task_card, "btns", btns);
            json_set(card, "task_card", task_card);
        }
    }

    json_set(root, "template_card", card);
    return post_webhook(http, root);
}

/* ================================================================
 *  Webhook message queue — stores messages received via HTTP callback
 * ================================================================ */

#define WECOM_QUEUE_MAX 64

typedef struct {
    char chat_id[128];
    char text[4096];
    char sender_id[128];
    time_t timestamp;
} wecom_msg_t;

static wecom_msg_t g_wx_queue[WECOM_QUEUE_MAX];
static int g_wx_head = 0;
static int g_wx_tail = 0;
static pthread_mutex_t g_wx_lock = PTHREAD_MUTEX_INITIALIZER;

/* Push a message into the ring buffer. Thread-safe. */
void wecom_queue_message(const char *chat_id, const char *text,
                          const char *sender_id) {
    pthread_mutex_lock(&g_wx_lock);
    int next = (g_wx_tail + 1) % WECOM_QUEUE_MAX;
    if (next != g_wx_head) {
        snprintf(g_wx_queue[g_wx_tail].chat_id, sizeof(g_wx_queue[g_wx_tail].chat_id), "%s", chat_id ? chat_id : "wecom");
        snprintf(g_wx_queue[g_wx_tail].text, sizeof(g_wx_queue[g_wx_tail].text), "%s", text ? text : "");
        snprintf(g_wx_queue[g_wx_tail].sender_id, sizeof(g_wx_queue[g_wx_tail].sender_id), "%s", sender_id ? sender_id : "");
        g_wx_queue[g_wx_tail].timestamp = time(NULL);
        g_wx_tail = next;
    }
    pthread_mutex_unlock(&g_wx_lock);
}

/* Handle incoming WeCom webhook callback.
 * WeCom callback format (XML body):
 *   <xml><ToUserName><![CDATA[...]]></ToUserName>
 *       <FromUserName><![CDATA[...]]></FromUserName>
 *       <CreateTime>...</CreateTime>
 *       <MsgType><![CDATA[text]]></MsgType>
 *       <Content><![CDATA[...]]></Content>
 *       <MsgId>...</MsgId>
 *     </xml>
 * For simplicity, accept JSON wrapper or pass raw body for agent processing. */
void wecom_handle_webhook(const char *body) {
    if (!body || !body[0]) return;

    /* Try JSON format first */
    char *err = NULL;
    json_t *root = json_parse(body, &err);
    if (root) {
        const char *chat_id = json_get_str(root, "FromUserName", NULL);
        const char *text = NULL;
        const char *msgtype = json_get_str(root, "MsgType", "");
        if (strcmp(msgtype, "text") == 0) {
            text = json_get_str(root, "Content", NULL);
        }
        if (chat_id && text) {
            wecom_queue_message(chat_id, text,
                json_get_str(root, "ToUserName", NULL));
        }
        json_free(root);
        return;
    }
    free(err);

    /* XML fallback: queue raw body as text */
    wecom_queue_message("wecom", body, NULL);
}

/* Poll for inbound WeCom messages.
 * Returns JSON array of messages from the webhook queue. */
json_node_t *wecom_poll_messages(http_client_t *http) {
    /* Refresh app token as side effect if credentials exist */
    if (g_corp_id[0] && g_corp_secret[0])
        get_app_token(http);

    pthread_mutex_lock(&g_wx_lock);
    if (g_wx_head == g_wx_tail) {
        pthread_mutex_unlock(&g_wx_lock);
        return NULL;
    }

    json_node_t *results = json_array();
    while (g_wx_head != g_wx_tail) {
        json_node_t *msg = json_new_object();
        json_set(msg, "chat_id", json_string(g_wx_queue[g_wx_head].chat_id));
        json_set(msg, "text", json_string(g_wx_queue[g_wx_head].text));
        json_set(msg, "sender_id", json_string(g_wx_queue[g_wx_head].sender_id));
        json_append(results, msg);
        g_wx_head = (g_wx_head + 1) % WECOM_QUEUE_MAX;
    }
    pthread_mutex_unlock(&g_wx_lock);
    return results;
}

const char *wecom_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "wecom");
}

const char *wecom_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
