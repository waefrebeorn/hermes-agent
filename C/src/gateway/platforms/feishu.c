/*
 * feishu.c — Feishu (Lark) gateway for Hermes C.
 * P114: Card messages, interactive buttons, doc integration, image/file sending.
 *
 * Two modes:
 *   1. Webhook mode (FEISHU_WEBHOOK_URL env) — text + interactive cards
 *   2. Open API mode (FEISHU_APP_ID + FEISHU_APP_SECRET env) — full API: cards,
 *      images, files, doc create/read, targeted messaging
 *
 * Env vars:
 *   FEISHU_WEBHOOK_URL       — Feishu bot webhook URL
 *   FEISHU_APP_ID            — Feishu app ID (for Open API mode)
 *   FEISHU_APP_SECRET        — Feishu app secret (for Open API mode)
 *   FEISHU_DEFAULT_RECEIVE_ID — Default receive_id (chat_id or open_id) for
 *                             Open API message sending
 *
 * Feishu Open API Docs:
 *   https://open.feishu.cn/document/server-docs/im-v1/message/create
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static char g_feishu_webhook[1024] = "";
static char g_app_id[256] = "";
static char g_app_secret[512] = "";
static char g_default_receive_id[128] = "";

/* Cached tenant access token for Open API mode */
static char g_tenant_token[1024] = "";
static double g_token_expires = 0.0;

/* Feishu Open API base */
#define FEISHU_API_BASE "https://open.feishu.cn/open-apis"

/* ================================================================
 *  Basic config
 * ================================================================ */

void feishu_set_webhook(const char *url) {
    if (url) snprintf(g_feishu_webhook, sizeof(g_feishu_webhook), "%s", url);
}

void feishu_set_app_credentials(const char *app_id, const char *app_secret) {
    if (app_id) snprintf(g_app_id, sizeof(g_app_id), "%s", app_id);
    if (app_secret) snprintf(g_app_secret, sizeof(g_app_secret), "%s", app_secret);
}

void feishu_set_default_receive_id(const char *receive_id) {
    if (receive_id)
        snprintf(g_default_receive_id, sizeof(g_default_receive_id), "%s", receive_id);
}

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* POST JSON to webhook (same as before) */
static bool post_webhook(http_client_t *http, json_node_t *root) {
    if (!g_feishu_webhook[0]) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char *body = json_serialize(root);
    json_free(root);
    if (!body) { if (local_http) http_client_free(local_http); return false; }

    char headers[256];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(http, HTTP_POST, g_feishu_webhook,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* Get tenant access token for Open API mode.
 * Caches token until ~60s before expiry. */
static const char *get_tenant_token(http_client_t *http) {
    if (!g_app_id[0] || !g_app_secret[0]) return NULL;

    double now = (double)time(NULL);
    if (g_token_expires > now + 60 && g_tenant_token[0])
        return g_tenant_token;

    /* Need fresh token */
    http_client_t *h = http ? http : http_client_new(10);
    http_client_t *local_http = NULL;
    if (!h) { local_http = http_client_new(10); h = local_http; }
    if (!h) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s/auth/v3/tenant_access_token/internal",
             FEISHU_API_BASE);

    json_node_t *body = json_new_object();
    json_set(body, "app_id", json_string(g_app_id));
    json_set(body, "app_secret", json_string(g_app_secret));
    char *json_body = json_serialize(body);
    json_free(body);
    if (!json_body) { if (local_http) http_client_free(local_http); return NULL; }

    http_response_t *resp = http_post_json(h, url, json_body);
    free(json_body);
    if (!resp || !resp->body) {
        if (local_http) http_client_free(local_http);
        return NULL;
    }

    json_node_t *j = json_parse(resp->body, NULL);
    http_response_free(resp);

    int code = (int)json_get_num(j, "code", -1);
    if (code == 0) {
        const char *tok = json_get_str(j, "tenant_access_token", "");
        int expire = (int)json_get_num(j, "expire", 7200);
        if (*tok) {
            snprintf(g_tenant_token, sizeof(g_tenant_token), "%s", tok);
            g_token_expires = now + (double)expire;
        }
    }
    json_free(j);
    if (local_http) http_client_free(local_http);

    return g_tenant_token[0] ? g_tenant_token : NULL;
}

/* POST JSON to Feishu Open API with auth.
 * url: full endpoint path. body: json_node_t (will be freed).
 * Returns http_response_t (caller frees) or NULL on error.
 * If http is NULL, creates and frees a temporary client. */
static http_response_t *openapi_post(http_client_t *http, const char *url,
                                      json_node_t *body) {
    const char *tok = get_tenant_token(http);
    if (!tok) return NULL;

    http_client_t *h = http ? http : http_client_new(10);
    http_client_t *local_http = NULL;
    if (!h) { local_http = http_client_new(10); h = local_http; }
    if (!h) { json_free(body); return NULL; }

    char *json_body = json_serialize(body);
    json_free(body);
    if (!json_body) { if (local_http) http_client_free(local_http); return NULL; }

    char auth_header[1536];
    snprintf(auth_header, sizeof(auth_header),
             "Content-Type: application/json; charset=utf-8\r\n"
             "Authorization: Bearer %s", tok);

    http_response_t *resp = http_request(h, HTTP_POST, url,
                                          auth_header, json_body, strlen(json_body));
    free(json_body);
    if (local_http) http_client_free(local_http);
    return resp;
}

/* GET request to Feishu Open API with auth.
 * Returns http_response_t (caller frees) or NULL. */
static http_response_t *openapi_get(http_client_t *http, const char *url) {
    const char *tok = get_tenant_token(http);
    if (!tok) return NULL;

    http_client_t *h = http ? http : http_client_new(10);
    http_client_t *local_http = NULL;
    if (!h) { local_http = http_client_new(10); h = local_http; }
    if (!h) return NULL;

    char auth_header[1536];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s", tok);

    http_response_t *resp = http_get(h, url, auth_header);
    if (local_http) http_client_free(local_http);
    return resp;
}

/* Read a file into a malloc'd buffer. Returns NULL on error.
 * Caller must free(). Sets *out_len to file size. */
static char *read_file_buf(const char *path, size_t *out_len) {
    if (!path || !out_len) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    struct stat st;
    if (fstat(fileno(f), &st) != 0) { fclose(f); return NULL; }
    size_t len = (size_t)st.st_size;
    char *buf = (char *)malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, len, f);
    fclose(f);
    if (nread != len) { free(buf); return NULL; }
    buf[len] = '\0';
    *out_len = len;
    return buf;
}

/* ================================================================
 *  P114: Send plain text message (webhook, existing)
 * ================================================================ */

bool feishu_send_message(http_client_t *http, const char *text) {
    if (!g_feishu_webhook[0] || !text) return false;

    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    json_node_t *root = json_new_object();
    json_set(root, "msg_type", json_string("text"));
    json_node_t *content = json_new_object();
    json_set(content, "text", json_string(text));
    json_set(root, "content", content);

    char *body = json_serialize(root);
    json_free(root);

    char headers[256];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(http, HTTP_POST, g_feishu_webhook,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* ================================================================
 *  P114: Image messages
 * ================================================================ */

/* Send an image message to Feishu.
 * image_path: local file path to the image.
 * Uses feishu_upload_image to get the image_key, then sends
 * an image message via the webhook. */
bool feishu_send_image_webhook(http_client_t *http, const char *image_path) {
    if (!g_feishu_webhook[0] || !image_path) return false;

    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char *image_key = feishu_upload_image(http, image_path);
    if (!image_key) {
        if (local_http) http_client_free(local_http);
        return false;
    }

    json_node_t *root = json_new_object();
    json_set(root, "msg_type", json_string("image"));
    json_node_t *content = json_new_object();
    json_set(content, "image_key", json_string(image_key));
    json_set(root, "content", content);

    char *body = json_serialize(root);
    json_free(root);
    free(image_key);

    if (!body) { if (local_http) http_client_free(local_http); return false; }

    char headers[256];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(http, HTTP_POST, g_feishu_webhook,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* ================================================================
 *  P114: Card messages
 * ================================================================ */

/* Send interactive card via webhook (no app credentials needed).
 * The card JSON is placed directly in the "card" field with msg_type "interactive". */
bool feishu_send_interactive(http_client_t *http, json_node_t *card) {
    if (!g_feishu_webhook[0] || !card) return false;

    json_node_t *root = json_new_object();
    json_set(root, "msg_type", json_string("interactive"));
    /* For webhook, the card goes directly as the "card" field */
    json_set(root, "card", json_copy(card));

    return post_webhook(http, root);
}

/* Send interactive card to a specific receive_id via Open API.
 * The card JSON is stringified and placed in the "content" field. */
bool feishu_send_card(http_client_t *http, const char *receive_id,
                       json_node_t *card) {
    if (!card) return false;
    if (!receive_id || !*receive_id)
        receive_id = g_default_receive_id;
    if (!receive_id || !*receive_id) return false;

    /* Stringify the card JSON for the content field */
    char *card_str = json_serialize(card);
    if (!card_str) return false;

    json_node_t *body = json_new_object();
    json_set(body, "receive_id", json_string(receive_id));
    json_set(body, "msg_type", json_string("interactive"));
    json_set(body, "content", json_string(card_str));
    free(card_str);

    char url[512];
    snprintf(url, sizeof(url), "%s/im/v1/messages?receive_id_type=open_id",
             FEISHU_API_BASE);

    http_response_t *resp = openapi_post(http, url, body);
    if (!resp) return false;

    bool ok = resp->status >= 200 && resp->status < 300;
    http_response_free(resp);
    return ok;
}

/* Convenience: build and send a card with interactive buttons.
 *
 * buttons is a JSON array of button elements, each like:
 *   {"text":{"tag":"plain_text","content":"Button"},"type":"default","value":{"key":"val"}}
 *
 * If buttons is NULL, sends a card with just title + body (no buttons).
 * template: "blue", "green", "red", "purple", "yellow", "grey", "wathet" etc.
 */
bool feishu_send_card_with_buttons(http_client_t *http,
                                    const char *receive_id,
                                    const char *title,
                                    const char *body_text,
                                    json_node_t *buttons,
                                    const char *template) {
    if (!title && !body_text) return false;
    if (!receive_id || !*receive_id)
        receive_id = g_default_receive_id;
    if (!receive_id || !*receive_id) return false;

    /* Build card JSON */
    json_node_t *card = json_new_object();

    /* Config: wide screen mode */
    json_node_t *config = json_new_object();
    json_set(config, "wide_screen_mode", json_bool(true));
    json_set(card, "config", config);

    /* Header: title + template color */
    json_node_t *header = json_new_object();
    json_node_t *header_title = json_new_object();
    json_set(header_title, "tag", json_string("plain_text"));
    json_set(header_title, "content",
             json_string(title ? title : ""));
    json_set(header, "title", header_title);
    json_set(header, "template", json_string(template ? template : "blue"));
    json_set(card, "header", header);

    /* Elements */
    json_node_t *elements = json_new_array();

    /* Body text as markdown element */
    if (body_text && *body_text) {
        json_node_t *md_elem = json_new_object();
        json_set(md_elem, "tag", json_string("markdown"));
        json_set(md_elem, "content", json_string(body_text));
        json_append(elements, md_elem);
    }

    /* Add buttons as an action element (if provided) */
    if (buttons && json_len(buttons) > 0) {
        json_node_t *action_elem = json_new_object();
        json_set(action_elem, "tag", json_string("action"));
        json_set(action_elem, "actions", json_copy(buttons));
        json_append(elements, action_elem);
    }

    json_set(card, "elements", elements);

    return feishu_send_card(http, receive_id, card);
}

/* ================================================================
 *  P114: Doc integration (Open API)
 * ================================================================ */

/* Create a new Feishu doc.
 * folder_token: parent folder token, or NULL/"" for root.
 * title: document title.
 * Returns malloc'd JSON string with {document_id, title, url} or NULL. */
char *feishu_doc_create(http_client_t *http, const char *folder_token,
                         const char *title) {
    if (!title || !*title) return NULL;

    json_node_t *body = json_new_object();
    if (folder_token && *folder_token)
        json_set(body, "folder_token", json_string(folder_token));
    json_set(body, "title", json_string(title));

    char url[512];
    snprintf(url, sizeof(url), "%s/docx/v1/documents", FEISHU_API_BASE);

    http_response_t *resp = openapi_post(http, url, body);
    if (!resp) return NULL;

    char *result = NULL;
    if (resp->body) {
        json_node_t *j = json_parse(resp->body, NULL);
        if (j) {
            int code = (int)json_get_num(j, "code", -1);
            if (code == 0) {
                json_node_t *data = json_obj_get(j, "data");
                if (data) {
                    json_node_t *doc = json_obj_get(data, "document");
                    if (doc) {
                        /* Return the document info as JSON */
                        json_node_t *info = json_new_object();
                        json_node_t *did = json_obj_get(doc, "document_id");
                        json_node_t *dt = json_obj_get(doc, "title");
                        json_node_t *du = json_obj_get(doc, "url");
                        if (did) json_set(info, "document_id", json_copy(did));
                        if (dt)  json_set(info, "title", json_copy(dt));
                        if (du)  json_set(info, "url", json_copy(du));
                        result = json_serialize(info);
                        json_free(info);
                    }
                }
            }
            json_free(j);
        }
    }
    http_response_free(resp);
    return result;
}

/* Get doc raw content as plain text.
 * doc_id: the document_id from feishu_doc_create result.
 * Returns malloc'd content string or NULL. */
char *feishu_doc_get_raw_content(http_client_t *http, const char *doc_id) {
    if (!doc_id || !*doc_id) return NULL;

    char url[1024];
    snprintf(url, sizeof(url), "%s/docx/v1/documents/%s/raw_content",
             FEISHU_API_BASE, doc_id);

    http_response_t *resp = openapi_get(http, url);
    if (!resp) return NULL;

    char *content = NULL;
    if (resp->body) {
        json_node_t *j = json_parse(resp->body, NULL);
        if (j) {
            int code = (int)json_get_num(j, "code", -1);
            if (code == 0) {
                json_node_t *data = json_obj_get(j, "data");
                if (data) {
                    const char *raw = json_get_str(data, "content", NULL);
                    if (raw) content = strdup(raw);
                }
            }
            json_free(j);
        }
    }
    http_response_free(resp);
    return content;
}

/* ================================================================
 *  P114: Image upload and send (Open API)
 * ================================================================ */

/* Build a multipart/form-data body manually.
 * boundary: a unique boundary string.
 * parts: NULL-terminated array of {name, filename, data, data_len} structs.
 * Returns malloc'd body, sets *out_len. NULL on error. */
typedef struct {
    const char *name;
    const char *filename;   /* NULL for non-file fields */
    const char *data;
    size_t      data_len;
} multipart_part_t;

static char *build_multipart(const char *boundary,
                              const multipart_part_t *parts,
                              size_t *out_len) {
    if (!boundary || !parts || !out_len) return NULL;

    /* Calculate total length first */
    size_t total = 0;
    for (const multipart_part_t *p = parts; p->name; p++) {
        /* --boundary\r\n */
        total += 2 + strlen(boundary) + 2;
        /* Content-Disposition: form-data; name="..." */
        total += 44 + strlen(p->name);
        if (p->filename) {
            /* ; filename="..." */
            total += 12 + strlen(p->filename);
        }
        total += 2; /* \r\n */
        /* Content-Type for files */
        if (p->filename) {
            total += 38; /* Content-Type: application/octet-stream\r\n */
        }
        total += 2; /* \r\n (blank line before data) */
        total += p->data_len;
        total += 2; /* \r\n */
    }
    /* --boundary--\r\n */
    total += 2 + strlen(boundary) + 2 + 2;

    char *buf = (char *)malloc(total + 1);
    if (!buf) return NULL;

    size_t pos = 0;
    for (const multipart_part_t *p = parts; p->name; p++) {
        pos += snprintf(buf + pos, total - pos + 1, "--%s\r\n", boundary);
        if (p->filename) {
            pos += snprintf(buf + pos, total - pos + 1,
                            "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
                            "Content-Type: application/octet-stream\r\n\r\n",
                            p->name, p->filename);
        } else {
            pos += snprintf(buf + pos, total - pos + 1,
                            "Content-Disposition: form-data; name=\"%s\"\r\n\r\n",
                            p->name);
        }
        if (p->data_len > 0) {
            memcpy(buf + pos, p->data, p->data_len);
            pos += p->data_len;
        }
        pos += snprintf(buf + pos, total - pos + 1, "\r\n");
    }
    pos += snprintf(buf + pos, total - pos + 1, "--%s--\r\n", boundary);

    buf[pos] = '\0';
    *out_len = pos;
    return buf;
}

/* Upload an image to Feishu for later use in messages.
 * image_path: absolute path to the image file.
 * Returns malloc'd image_key string (caller free()s), or NULL. */
char *feishu_upload_image(http_client_t *http, const char *image_path) {
    if (!image_path || !*image_path) return NULL;

    /* Read the image file */
    size_t file_len = 0;
    char *file_data = read_file_buf(image_path, &file_len);
    if (!file_data) return NULL;

    /* Get access token */
    const char *tok = get_tenant_token(http);
    if (!tok) { free(file_data); return NULL; }

    http_client_t *h = http ? http : http_client_new(30);
    http_client_t *local_http = NULL;
    if (!h) { local_http = http_client_new(30); h = local_http; }
    if (!h) { free(file_data); return NULL; }

    /* Extract filename from path */
    const char *filename = strrchr(image_path, '/');
    filename = filename ? filename + 1 : image_path;

    /* Build multipart body */
    const char *boundary = "----HermesFeishuBoundary7MA4YWxkTrZu0gW";
    multipart_part_t parts[] = {
        {"image_type", NULL, "message", 7},
        {"image", filename, file_data, file_len},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len = 0;
    char *multipart_body = build_multipart(boundary, parts, &body_len);
    free(file_data);
    if (!multipart_body) { if (local_http) http_client_free(local_http); return NULL; }

    char headers[2048];
    snprintf(headers, sizeof(headers),
             "Content-Type: multipart/form-data; boundary=%s\r\n"
             "Authorization: Bearer %s", boundary, tok);

    char url[512];
    snprintf(url, sizeof(url), "%s/im/v1/images", FEISHU_API_BASE);

    http_response_t *resp = http_request(h, HTTP_POST, url,
                                          headers, multipart_body, body_len);
    free(multipart_body);
    if (local_http) http_client_free(local_http);

    if (!resp) return NULL;

    char *image_key = NULL;
    if (resp->body) {
        json_node_t *j = json_parse(resp->body, NULL);
        if (j) {
            int code = (int)json_get_num(j, "code", -1);
            if (code == 0) {
                json_node_t *data = json_obj_get(j, "data");
                if (data) {
                    const char *key = json_get_str(data, "image_key", NULL);
                    if (key) image_key = strdup(key);
                }
            }
            json_free(j);
        }
    }
    http_response_free(resp);
    return image_key;
}

/* Send an image by image_key to a specific receive_id via Open API. */
bool feishu_send_image(http_client_t *http, const char *receive_id,
                        const char *image_key) {
    if (!image_key || !*image_key) return false;
    if (!receive_id || !*receive_id)
        receive_id = g_default_receive_id;
    if (!receive_id || !*receive_id) return false;

    json_node_t *content = json_new_object();
    json_set(content, "image_key", json_string(image_key));
    char *content_str = json_serialize(content);
    json_free(content);
    if (!content_str) return false;

    json_node_t *body = json_new_object();
    json_set(body, "receive_id", json_string(receive_id));
    json_set(body, "msg_type", json_string("image"));
    json_set(body, "content", json_string(content_str));
    free(content_str);

    char url[512];
    snprintf(url, sizeof(url), "%s/im/v1/messages?receive_id_type=open_id",
             FEISHU_API_BASE);

    http_response_t *resp = openapi_post(http, url, body);
    if (!resp) return false;

    bool ok = resp->status >= 200 && resp->status < 300;
    http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P114: File upload and send (Open API)
 * ================================================================ */

/* Upload a file to Feishu.
 * file_path: absolute path to the file.
 * file_name: display name in chat (if NULL, uses basename of file_path).
 * Returns malloc'd file_key string (caller free()s), or NULL. */
char *feishu_upload_file(http_client_t *http, const char *file_path,
                          const char *file_name) {
    if (!file_path || !*file_path) return NULL;

    size_t file_len = 0;
    char *file_data = read_file_buf(file_path, &file_len);
    if (!file_data) return NULL;

    const char *tok = get_tenant_token(http);
    if (!tok) { free(file_data); return NULL; }

    http_client_t *h = http ? http : http_client_new(60);
    http_client_t *local_http = NULL;
    if (!h) { local_http = http_client_new(60); h = local_http; }
    if (!h) { free(file_data); return NULL; }

    /* Determine display name */
    const char *display_name = file_name;
    if (!display_name || !*display_name) {
        display_name = strrchr(file_path, '/');
        display_name = display_name ? display_name + 1 : file_path;
    }

    /* Build multipart body */
    const char *boundary = "----HermesFeishuFileBoundary7MA4YWxkTrZu0gW";
    multipart_part_t parts[] = {
        {"file_type", NULL, "stream", 6},
        {"file_name", NULL, display_name, strlen(display_name)},
        {"file", display_name, file_data, file_len},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len = 0;
    char *multipart_body = build_multipart(boundary, parts, &body_len);
    free(file_data);
    if (!multipart_body) { if (local_http) http_client_free(local_http); return NULL; }

    char headers[2048];
    snprintf(headers, sizeof(headers),
             "Content-Type: multipart/form-data; boundary=%s\r\n"
             "Authorization: Bearer %s", boundary, tok);

    char url[512];
    snprintf(url, sizeof(url), "%s/im/v1/files", FEISHU_API_BASE);

    http_response_t *resp = http_request(h, HTTP_POST, url,
                                          headers, multipart_body, body_len);
    free(multipart_body);
    if (local_http) http_client_free(local_http);

    if (!resp) return NULL;

    char *file_key = NULL;
    if (resp->body) {
        json_node_t *j = json_parse(resp->body, NULL);
        if (j) {
            int code = (int)json_get_num(j, "code", -1);
            if (code == 0) {
                json_node_t *data = json_obj_get(j, "data");
                if (data) {
                    const char *key = json_get_str(data, "file_key", NULL);
                    if (key) file_key = strdup(key);
                }
            }
            json_free(j);
        }
    }
    http_response_free(resp);
    return file_key;
}

/* Send a file by file_key to a specific receive_id via Open API. */
bool feishu_send_file(http_client_t *http, const char *receive_id,
                       const char *file_key) {
    if (!file_key || !*file_key) return false;
    if (!receive_id || !*receive_id)
        receive_id = g_default_receive_id;
    if (!receive_id || !*receive_id) return false;

    json_node_t *content = json_new_object();
    json_set(content, "file_key", json_string(file_key));
    char *content_str = json_serialize(content);
    json_free(content);
    if (!content_str) return false;

    json_node_t *body = json_new_object();
    json_set(body, "receive_id", json_string(receive_id));
    json_set(body, "msg_type", json_string("file"));
    json_set(body, "content", json_string(content_str));
    free(content_str);

    char url[512];
    snprintf(url, sizeof(url), "%s/im/v1/messages?receive_id_type=open_id",
             FEISHU_API_BASE);

    http_response_t *resp = openapi_post(http, url, body);
    if (!resp) return false;

    bool ok = resp->status >= 200 && resp->status < 300;
    http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Poll / update extractors (unchanged)
 * ================================================================ */

/* Feishu polls via webhook receiver (no long-polling API for bots) */
json_node_t *feishu_poll_messages(http_client_t *http) {
    (void)http;
    return NULL;
}

const char *feishu_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "feishu");
}

const char *feishu_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
