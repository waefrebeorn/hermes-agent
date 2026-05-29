/*
 * feishu_tools.c — D22: Feishu doc/drive tool support for Hermes C.
 *
 * Registers two tools:
 *   feishu_doc_read   — read Feishu/Lark document content as plain text
 *   feishu_drive_list — list files in a Feishu Drive folder
 *
 * Uses FEISHU_APP_ID + FEISHU_APP_SECRET env vars (same as gateway feishu.c).
 */
#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define FEISHU_API_BASE "https://open.feishu.cn/open-apis"

/* ── Auth ───────────────────────────────────────────────────────── */

static char g_tool_app_id[256] = "";
static char g_tool_app_secret[512] = "";
static char g_tool_token[1024] = "";
static double g_tool_token_expires = 0.0;

static const char *feishu_get_token(http_client_t *http) {
    if (!g_tool_app_id[0] || !g_tool_app_secret[0]) {
        /* Initialize from env on first call */
        const char *id = getenv("FEISHU_APP_ID");
        const char *sec = getenv("FEISHU_APP_SECRET");
        if (!id || !sec) return NULL;
        snprintf(g_tool_app_id, sizeof(g_tool_app_id), "%s", id);
        snprintf(g_tool_app_secret, sizeof(g_tool_app_secret), "%s", sec);
    }
    double now = (double)time(NULL);
    if (g_tool_token_expires > now + 60 && g_tool_token[0])
        return g_tool_token;

    http_client_t *h = http ? http : http_client_new(10);
    http_client_t *local = NULL;
    if (!h) { local = http_client_new(10); h = local; }
    if (!h) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s/auth/v3/tenant_access_token/internal", FEISHU_API_BASE);

    json_t *body = json_object();
    json_set(body, "app_id", json_string(g_tool_app_id));
    json_set(body, "app_secret", json_string(g_tool_app_secret));
    char *json_body = json_serialize(body);
    json_free(body);
    if (!json_body) { if (local) http_client_free(local); return NULL; }

    http_response_t *resp = http_post_json(h, url, json_body);
    free(json_body);
    if (!resp || !resp->body) { if (local) http_client_free(local); return NULL; }

    json_t *j = json_parse(resp->body, NULL);
    http_response_free(resp);
    if (!j) { if (local) http_client_free(local); return NULL; }

    int code = (int)json_get_num(j, "code", -1);
    const char *tok = NULL;
    if (code == 0) {
        tok = json_get_str(j, "tenant_access_token", "");
        int expire = (int)json_get_num(j, "expire", 7200);
        if (tok && *tok) {
            snprintf(g_tool_token, sizeof(g_tool_token), "%s", tok);
            g_tool_token_expires = now + (double)expire;
        }
    }
    json_free(j);
    if (local) http_client_free(local);
    return g_tool_token[0] ? g_tool_token : NULL;
}

/* ── Tool handlers ──────────────────────────────────────────────── */

/* feishu_doc_read(doc_id) — read Feishu document as plain text */
char *handle_feishu_doc_read(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *req = json_parse(args_json, NULL);
    if (!req) return strdup("{\"error\":\"invalid JSON\"}");

    const char *doc_id = json_get_str(req, "doc_id", "");
    if (!doc_id[0]) { json_free(req); return strdup("{\"error\":\"missing doc_id\"}"); }
    json_free(req);

    http_client_t *http = http_client_new(30);
    if (!http) return strdup("{\"error\":\"failed to create HTTP client\"}");

    const char *token = feishu_get_token(http);
    if (!token) { http_client_free(http); return strdup("{\"error\":\"auth failed — set FEISHU_APP_ID + FEISHU_APP_SECRET\"}"); }

    char url[512];
    snprintf(url, sizeof(url), "%s/docx/v1/documents/%s/raw_content", FEISHU_API_BASE, doc_id);

    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr),
             "Authorization: Bearer %s\r\n", token);

    http_response_t *resp = http_request(http, HTTP_GET, url, auth_hdr, NULL, 0);
    if (!resp) { http_client_free(http); return strdup("{\"error\":\"HTTP request failed\"}"); }

    json_t *j = json_parse(resp->body, NULL);
    int status = resp->status;
    http_response_free(resp);
    http_client_free(http);

    if (!j) {
        char err[128]; snprintf(err, sizeof(err), "{\"error\":\"parse failed\",\"status\":%d}", status);
        return strdup(err);
    }

    int code = (int)json_get_num(j, "code", -1);
    if (code != 0) {
        const char *msg = json_get_str(j, "msg", "unknown error");
        char err[512]; snprintf(err, sizeof(err), "{\"error\":\"%s\",\"code\":%d}", msg ? msg : "unknown", code);
        json_free(j);
        return strdup(err);
    }

    /* Extract content from data.content */
    const json_t *data = json_obj_get(j, "data");
    const char *content = data ? json_get_str(data, "content", "") : "";
    if (!content) content = "";

    /* Escape for JSON response */
    char resp_buf[32768];
    /* Basic JSON escaping */
    size_t pos = 0;
    pos += snprintf(resp_buf + pos, sizeof(resp_buf) - pos, "{\"content\":\"");
    for (const char *p = content; *p && pos < sizeof(resp_buf) - 8; p++) {
        if (*p == '\"') { resp_buf[pos++] = '\\'; resp_buf[pos++] = '\"'; }
        else if (*p == '\\') { resp_buf[pos++] = '\\'; resp_buf[pos++] = '\\'; }
        else if (*p == '\n') { resp_buf[pos++] = '\\'; resp_buf[pos++] = 'n'; }
        else if (*p == '\r') { resp_buf[pos++] = '\\'; resp_buf[pos++] = 'r'; }
        else if (*p == '\t') { resp_buf[pos++] = '\\'; resp_buf[pos++] = 't'; }
        else if ((unsigned char)*p < 0x20) continue;
        else resp_buf[pos++] = *p;
    }
    snprintf(resp_buf + pos, sizeof(resp_buf) - pos, "\",\"doc_id\":\"%s\"}", doc_id);
    json_free(j);
    return strdup(resp_buf);
}

/* feishu_drive_list(folder_token) — list files in a Feishu Drive folder */
char *handle_feishu_drive_list(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *req = json_parse(args_json, NULL);
    if (!req) return strdup("{\"error\":\"invalid JSON\"}");

    const char *folder_token = json_get_str(req, "folder_token", "");
    const char *page_size_raw = json_get_str(req, "page_size", "20");
    json_free(req);

    http_client_t *http = http_client_new(30);
    if (!http) return strdup("{\"error\":\"failed to create HTTP client\"}");

    const char *token = feishu_get_token(http);
    if (!token) { http_client_free(http); return strdup("{\"error\":\"auth failed — set FEISHU_APP_ID + FEISHU_APP_SECRET\"}"); }

    char url[1024];
    if (folder_token[0]) {
        snprintf(url, sizeof(url), "%s/drive/v1/files?page_size=%s&folder_token=%s",
                 FEISHU_API_BASE, page_size_raw, folder_token);
    } else {
        snprintf(url, sizeof(url), "%s/drive/v1/files?page_size=%s",
                 FEISHU_API_BASE, page_size_raw);
    }

    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s\r\n", token);

    http_response_t *resp = http_request(http, HTTP_GET, url, auth_hdr, NULL, 0);
    if (!resp) { http_client_free(http); return strdup("{\"error\":\"HTTP request failed\"}"); }

    int status = resp->status;
    const char *body = resp->body ? resp->body : "";
    char *out = malloc(strlen(body) + 128);
    if (!out) { http_response_free(resp); http_client_free(http); return strdup("{\"error\":\"OOM\"}"); }
    sprintf(out, "{\"status\":%d,\"data\":%s}", status, body);
    http_response_free(resp);
    http_client_free(http);
    return out;
}

/* feishu_drive_list_comments(file_token, file_type, is_whole, page_size)
 * List comments on a Feishu document. Ported from Python feishu_drive_tool.py. */
char *handle_feishu_drive_list_comments(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *req = json_parse(args_json, NULL);
    if (!req) return strdup("{\"error\":\"invalid JSON\"}");

    const char *file_token = json_get_str(req, "file_token", "");
    if (!file_token[0]) { json_free(req); return strdup("{\"error\":\"missing file_token\"}"); }
    const char *file_type = json_get_str(req, "file_type", "docx");
    if (!file_type[0]) file_type = "docx";
    int page_size = (int)json_get_num(req, "page_size", 100);
    json_free(req);

    http_client_t *http = http_client_new(30);
    if (!http) return strdup("{\"error\":\"failed to create HTTP client\"}");

    const char *token = feishu_get_token(http);
    if (!token) { http_client_free(http); return strdup("{\"error\":\"auth failed\"}"); }

    char url[1024];
    snprintf(url, sizeof(url),
             "%s/drive/v1/files/%s/comments?file_type=%s&user_id_type=open_id&page_size=%d",
             FEISHU_API_BASE, file_token, file_type, page_size > 0 && page_size <= 100 ? page_size : 100);

    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr),
             "Authorization: Bearer %s", token);

    http_response_t *resp = http_request(http, HTTP_GET, url, auth_hdr, NULL, 0);
    if (!resp) { http_client_free(http); return strdup("{\"error\":\"HTTP request failed\"}"); }

    int status = resp->status;
    const char *body = resp->body ? resp->body : "";
    char *out = malloc(strlen(body) + 128);
    if (!out) { http_response_free(resp); http_client_free(http); return strdup("{\"error\":\"OOM\"}"); }
    sprintf(out, "{\"status\":%d,\"data\":%s}", status, body);
    http_response_free(resp);
    http_client_free(http);
    return out;
}

/* feishu_drive_list_comment_replies(file_token, comment_id, file_type, page_size)
 * List replies in a comment thread. Ported from Python feishu_drive_tool.py. */
char *handle_feishu_drive_list_comment_replies(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *req = json_parse(args_json, NULL);
    if (!req) return strdup("{\"error\":\"invalid JSON\"}");

    const char *file_token = json_get_str(req, "file_token", "");
    const char *comment_id = json_get_str(req, "comment_id", "");
    if (!file_token[0] || !comment_id[0]) {
        json_free(req);
        return strdup("{\"error\":\"missing file_token or comment_id\"}");
    }
    const char *file_type = json_get_str(req, "file_type", "docx");
    if (!file_type[0]) file_type = "docx";
    int page_size = (int)json_get_num(req, "page_size", 100);
    json_free(req);

    http_client_t *http = http_client_new(30);
    if (!http) return strdup("{\"error\":\"failed to create HTTP client\"}");

    const char *token = feishu_get_token(http);
    if (!token) { http_client_free(http); return strdup("{\"error\":\"auth failed\"}"); }

    char url[1024];
    snprintf(url, sizeof(url),
             "%s/drive/v1/files/%s/comments/%s/replies?file_type=%s&user_id_type=open_id&page_size=%d",
             FEISHU_API_BASE, file_token, comment_id, file_type,
             page_size > 0 && page_size <= 100 ? page_size : 100);

    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);

    http_response_t *resp = http_request(http, HTTP_GET, url, auth_hdr, NULL, 0);
    if (!resp) { http_client_free(http); return strdup("{\"error\":\"HTTP request failed\"}"); }

    int status = resp->status;
    const char *body = resp->body ? resp->body : "";
    char *out = malloc(strlen(body) + 128);
    if (!out) { http_response_free(resp); http_client_free(http); return strdup("{\"error\":\"OOM\"}"); }
    sprintf(out, "{\"status\":%d,\"data\":%s}", status, body);
    http_response_free(resp);
    http_client_free(http);
    return out;
}

/* feishu_drive_reply_comment(file_token, comment_id, content, file_type)
 * Reply to a comment thread on a Feishu document. POST-based. */
char *handle_feishu_drive_reply_comment(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *req = json_parse(args_json, NULL);
    if (!req) return strdup("{\"error\":\"invalid JSON\"}");

    const char *file_token = json_get_str(req, "file_token", "");
    const char *comment_id = json_get_str(req, "comment_id", "");
    const char *content = json_get_str(req, "content", "");
    if (!file_token[0] || !comment_id[0] || !content[0]) {
        json_free(req);
        return strdup("{\"error\":\"missing file_token, comment_id, or content\"}");
    }
    const char *file_type = json_get_str(req, "file_type", "docx");
    if (!file_type[0]) file_type = "docx";
    json_free(req);

    http_client_t *http = http_client_new(30);
    if (!http) return strdup("{\"error\":\"failed to create HTTP client\"}");

    const char *token = feishu_get_token(http);
    if (!token) { http_client_free(http); return strdup("{\"error\":\"auth failed\"}"); }

    char url[1024];
    snprintf(url, sizeof(url),
             "%s/drive/v1/files/%s/comments/%s/replies?file_type=%s",
             FEISHU_API_BASE, file_token, comment_id, file_type);

    /* Build JSON body: {"content":{"elements":[{"type":"text_run","text_run":{"text":"..."}}]}} */
    json_t *body_j = json_object();
    json_t *content_j = json_object();
    json_t *elements = json_array();
    json_t *text_run = json_object();
    json_t *text_run_inner = json_object();
    json_set(text_run_inner, "text", json_string(content));
    json_set(text_run, "text_run", text_run_inner);
    json_set(text_run, "type", json_string("text_run"));
    json_array_append(elements, text_run);
    json_set(content_j, "elements", elements);
    json_set(body_j, "content", content_j);
    char *json_body = json_serialize(body_j);
    json_free(body_j);
    if (!json_body) { http_client_free(http); return strdup("{\"error\":\"OOM\"}"); }

    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);

    http_response_t *resp = http_post_json_auth(http, url, json_body, auth_hdr);
    free(json_body);
    if (!resp) { http_client_free(http); return strdup("{\"error\":\"HTTP request failed\"}"); }

    int status = resp->status;
    const char *body = resp->body ? resp->body : "";
    char *out = malloc(strlen(body) + 128);
    if (!out) { http_response_free(resp); http_client_free(http); return strdup("{\"error\":\"OOM\"}"); }
    sprintf(out, "{\"status\":%d,\"success\":true,\"data\":%s}", status, body);
    http_response_free(resp);
    http_client_free(http);
    return out;
}

/* feishu_drive_add_comment(file_token, content, file_type)
 * Add a whole-document comment. POST-based. */
char *handle_feishu_drive_add_comment(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *req = json_parse(args_json, NULL);
    if (!req) return strdup("{\"error\":\"invalid JSON\"}");

    const char *file_token = json_get_str(req, "file_token", "");
    const char *content = json_get_str(req, "content", "");
    if (!file_token[0] || !content[0]) {
        json_free(req);
        return strdup("{\"error\":\"missing file_token or content\"}");
    }
    const char *file_type = json_get_str(req, "file_type", "docx");
    if (!file_type[0]) file_type = "docx";
    json_free(req);

    http_client_t *http = http_client_new(30);
    if (!http) return strdup("{\"error\":\"failed to create HTTP client\"}");

    const char *token = feishu_get_token(http);
    if (!token) { http_client_free(http); return strdup("{\"error\":\"auth failed\"}"); }

    char url[1024];
    snprintf(url, sizeof(url),
             "%s/drive/v1/files/%s/new_comments",
             FEISHU_API_BASE, file_token);

    /* Build JSON body: {"file_type":"docx","reply_elements":[{"type":"text","text":"..."}]} */
    json_t *body_j = json_object();
    json_set(body_j, "file_type", json_string(file_type));
    json_t *reply_elems = json_array();
    json_t *elem = json_object();
    json_set(elem, "type", json_string("text"));
    json_set(elem, "text", json_string(content));
    json_array_append(reply_elems, elem);
    json_set(body_j, "reply_elements", reply_elems);
    char *json_body = json_serialize(body_j);
    json_free(body_j);
    if (!json_body) { http_client_free(http); return strdup("{\"error\":\"OOM\"}"); }

    char auth_hdr[1024];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);

    http_response_t *resp = http_post_json_auth(http, url, json_body, auth_hdr);
    free(json_body);
    if (!resp) { http_client_free(http); return strdup("{\"error\":\"HTTP request failed\"}"); }

    int status = resp->status;
    const char *body = resp->body ? resp->body : "";
    char *out = malloc(strlen(body) + 128);
    if (!out) { http_response_free(resp); http_client_free(http); return strdup("{\"error\":\"OOM\"}"); }
    sprintf(out, "{\"status\":%d,\"success\":true,\"data\":%s}", status, body);
    http_response_free(resp);
    http_client_free(http);
    return out;
}

static const char *FEISHU_DOC_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"doc_id\":{\"type\":\"string\",\"description\":\"Feishu document ID to read\"}"
    "},\"required\":[\"doc_id\"]}";

static const char *FEISHU_DRIVE_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"folder_token\":{\"type\":\"string\",\"description\":\"Drive folder token (optional, root if empty)\"},"
    "\"page_size\":{\"type\":\"string\",\"description\":\"Results per page (default 20)\"}"
    "}}";

/* ── Ports from Python feishu_comment.py ──────────────────────────── */

/* Port of _sanitize_comment_text: escape XML special chars in Feishu text_run content. */
char *feishu_sanitize_comment_text(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    /* Worst case: all chars become &amp; (5x). Oversized=ok. */
    size_t cap = len * 5 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t pos = 0;
    for (size_t i = 0; i < len && pos < cap - 6; i++) {
        char c = text[i];
        switch (c) {
            case '&':  memcpy(out + pos, "&amp;", 5);  pos += 5; break;
            case '<':  memcpy(out + pos, "&lt;", 4);   pos += 4; break;
            case '>':  memcpy(out + pos, "&gt;", 4);   pos += 4; break;
            default:   out[pos++] = c; break;
        }
    }
    out[pos] = '\0';
    return out;
}

/* Port of _get_reply_user_id: extract user_id from a reply JSON node.
 * Returns static string pointer into the JSON tree — do NOT free. */
const char *feishu_get_reply_user_id(json_node_t *reply) {
    if (!reply || reply->type != JSON_OBJECT) return "";
    json_node_t *user_id = json_object_get(reply, "user_id");
    if (!user_id) return "";
    if (user_id->type == JSON_STRING) return user_id->str_val;
    if (user_id->type == JSON_OBJECT) {
        json_node_t *open_id = json_object_get(user_id, "open_id");
        if (open_id && open_id->type == JSON_STRING && open_id->str_val[0])
            return open_id->str_val;
        json_node_t *uid = json_object_get(user_id, "user_id");
        if (uid && uid->type == JSON_STRING) return uid->str_val;
    }
    return "";
}

/* Port of _extract_reply_text: extract plain text from a reply JSON node.
 * Walks content.elements[] for text_run/documents_link/person types.
 * Returns malloc'd string — caller must free(). */
char *feishu_extract_reply_text(json_node_t *reply) {
    if (!reply || reply->type != JSON_OBJECT) return strdup("");

    json_node_t *content = json_object_get(reply, "content");
    if (!content) return strdup("");

    /* If content is a JSON string, parse it */
    if (content->type == JSON_STRING) {
        char *err = NULL;
        json_node_t *parsed = json_parse(content->str_val, &err);
        free(err);
        if (parsed) {
            char *result = feishu_extract_reply_text(parsed);
            json_free(parsed);
            return result;
        }
        return strdup(content->str_val);
    }

    if (content->type != JSON_OBJECT) return strdup("");

    json_node_t *elements = json_object_get(content, "elements");
    if (!elements || elements->type != JSON_ARRAY) return strdup("");

    size_t count = json_len(elements);
    if (count == 0) return strdup("");

    /* Estimate max output size: worst case 256 chars per element */
    size_t cap = count * 256 + 1;
    char *result = (char *)malloc(cap);
    if (!result) return NULL;
    result[0] = '\0';
    size_t pos = 0;

    for (size_t i = 0; i < count; i++) {
        json_node_t *elem = json_array_get(elements, i);
        if (!elem || elem->type != JSON_OBJECT) continue;

        const char *type = json_object_get_string(elem, "type", "");
        if (strcmp(type, "text_run") == 0) {
            json_node_t *text_run = json_object_get(elem, "text_run");
            if (text_run && text_run->type == JSON_OBJECT) {
                const char *text = json_object_get_string(text_run, "text", "");
                size_t tlen = strlen(text);
                if (pos + tlen < cap - 1) {
                    memcpy(result + pos, text, tlen);
                    pos += tlen;
                }
            }
        } else if (strcmp(type, "docs_link") == 0) {
            json_node_t *docs_link = json_object_get(elem, "docs_link");
            if (docs_link && docs_link->type == JSON_OBJECT) {
                const char *url = json_object_get_string(docs_link, "url", "");
                size_t ulen = strlen(url);
                if (pos + ulen < cap - 1) {
                    memcpy(result + pos, url, ulen);
                    pos += ulen;
                }
            }
        } else if (strcmp(type, "person") == 0) {
            json_node_t *person = json_object_get(elem, "person");
            if (person && person->type == JSON_OBJECT) {
                const char *uid = json_object_get_string(person, "user_id", "unknown");
                /* Format: @user_id */
                result[pos++] = '@';
                size_t ulen = strlen(uid);
                if (pos + ulen < cap - 1) {
                    memcpy(result + pos, uid, ulen);
                    pos += ulen;
                }
            }
        }
    }

    result[pos] = '\0';
    /* Realloc to exact size */
    char *shrunk = (char *)realloc(result, pos + 1);
    return shrunk ? shrunk : result;
}

/* Port of _truncate: truncate text for prompt embedding, appending "..." when over limit. */
char *feishu_truncate_text(const char *text, int limit) {
    if (!text) return NULL;
    size_t len = strlen(text);
    if (limit > 0 && (int)len <= limit) return strdup(text);
    if (limit <= 0) limit = 0; /* truncate to "..." */
    char *out = (char *)malloc((size_t)limit + 4);
    if (!out) return NULL;
    if (limit > 0) memcpy(out, text, (size_t)limit);
    out[limit] = '.';
    out[limit + 1] = '.';
    out[limit + 2] = '.';
    out[limit + 3] = '\0';
    return out;
}

/* Port of _extract_semantic_text: extract semantic text, stripping self @mentions.
 * Works on a reply JSON node (same structure as feishu_extract_reply_text).
 * Returns malloc'd string — caller must free(). When self_open_id is non-empty,
 * person elements matching that user_id are skipped. */
char *feishu_extract_semantic_text(json_node_t *reply, const char *self_open_id) {
    if (!reply || reply->type != JSON_OBJECT) return strdup("");

    json_node_t *content = json_object_get(reply, "content");
    if (!content) return strdup("");

    /* If content is a JSON string, parse it */
    if (content->type == JSON_STRING) {
        char *err = NULL;
        json_node_t *parsed = json_parse(content->str_val, &err);
        free(err);
        if (parsed) {
            char *result = feishu_extract_semantic_text(parsed, self_open_id);
            json_free(parsed);
            return result;
        }
        return strdup(content->str_val);
    }

    if (content->type != JSON_OBJECT) return strdup("");

    json_node_t *elements = json_object_get(content, "elements");
    if (!elements || elements->type != JSON_ARRAY) return strdup("");

    size_t count = json_len(elements);
    if (count == 0) return strdup("");

    /* Estimate max output size: worst case 128 chars per element */
    size_t cap = count * 128 + 1;
    char *result = (char *)malloc(cap);
    if (!result) return NULL;
    result[0] = '\0';
    size_t pos = 0;

    for (size_t i = 0; i < count; i++) {
        json_node_t *elem = json_array_get(elements, i);
        if (!elem || elem->type != JSON_OBJECT) continue;

        const char *type = json_object_get_string(elem, "type", "");
        if (strcmp(type, "person") == 0) {
            json_node_t *person = json_object_get(elem, "person");
            if (person && person->type == JSON_OBJECT) {
                const char *uid = json_object_get_string(person, "user_id", "");
                /* Skip self @mention */
                if (self_open_id && self_open_id[0] && strcmp(uid, self_open_id) == 0)
                    continue;
                /* Add @user_id */
                if (pos + 2 + strlen(uid) < cap - 1) {
                    result[pos++] = ' ';
                    result[pos++] = '@';
                    size_t ulen = strlen(uid);
                    memcpy(result + pos, uid, ulen);
                    pos += ulen;
                }
            }
        } else if (strcmp(type, "text_run") == 0) {
            json_node_t *text_run = json_object_get(elem, "text_run");
            if (text_run && text_run->type == JSON_OBJECT) {
                const char *text = json_object_get_string(text_run, "text", "");
                size_t tlen = strlen(text);
                if (pos + 1 + tlen < cap - 1) {
                    result[pos++] = ' ';
                    memcpy(result + pos, text, tlen);
                    pos += tlen;
                }
            }
        } else if (strcmp(type, "docs_link") == 0) {
            json_node_t *docs_link = json_object_get(elem, "docs_link");
            if (docs_link && docs_link->type == JSON_OBJECT) {
                const char *url = json_object_get_string(docs_link, "url", "");
                size_t ulen = strlen(url);
                if (pos + 1 + ulen < cap - 1) {
                    result[pos++] = ' ';
                    memcpy(result + pos, url, ulen);
                    pos += ulen;
                }
            }
        }
    }

    /* Trim leading space */
    if (pos > 0 && result[0] == ' ') {
        memmove(result, result + 1, pos - 1);
        pos--;
    }

    result[pos] = '\0';
    /* Remove extra whitespace: " ".join("".join(parts).split()) */
    if (pos > 0) {
        /* Compact multiple spaces into one */
        size_t wp = 0;
        bool in_space = false;
        for (size_t i = 0; i < pos; i++) {
            if (result[i] == ' ' || result[i] == '\t' || result[i] == '\n' || result[i] == '\r') {
                if (!in_space) {
                    result[wp++] = ' ';
                    in_space = true;
                }
            } else {
                result[wp++] = result[i];
                in_space = false;
            }
        }
        /* Trim trailing space */
        while (wp > 0 && result[wp - 1] == ' ') wp--;
        result[wp] = '\0';
        /* Realloc to exact size */
        char *shrunk = (char *)realloc(result, wp + 1);
        return shrunk ? shrunk : result;
    }

    /* Realloc to exact size */
    char *shrunk = (char *)realloc(result, pos + 1);
    return shrunk ? shrunk : result;
}

/* ── Registry init ──────────────────────────────────────────────── */

void registry_init_feishu_tools(void) {
    registry_register("feishu_doc_read",
        "Read a Feishu/Lark document as plain text. Usage: {\"doc_id\":\"...\"}",
        FEISHU_DOC_SCHEMA, handle_feishu_doc_read);

    registry_register_ex("feishu_drive_list",
        "List files in a Feishu Drive folder. Usage: {\"folder_token\":\"...\"}",
        FEISHU_DRIVE_SCHEMA, "file", handle_feishu_drive_list);

    /* ── Feishu Drive comment tools (ports from Python feishu_drive_tool.py) ── */

    static const char *FEISHU_COMMENTS_SCHEMA =
        "{\"type\":\"object\",\"properties\":{"
        "\"file_token\":{\"type\":\"string\",\"description\":\"Document file token (required)\"},"
        "\"file_type\":{\"type\":\"string\",\"description\":\"File type (default docx)\"},"
        "\"is_whole\":{\"type\":\"boolean\",\"description\":\"Only return whole-document comments\"},"
        "\"page_size\":{\"type\":\"integer\",\"description\":\"Results per page (max 100)\"}"
        "},\"required\":[\"file_token\"]}";

    registry_register_ex("feishu_drive_list_comments",
        "List comments on a Feishu document.",
        FEISHU_COMMENTS_SCHEMA, "file", handle_feishu_drive_list_comments);

    /* feishu_drive_list_comment_replies — list replies in a comment thread */
    static const char *FEISHU_REPLIES_SCHEMA =
        "{\"type\":\"object\",\"properties\":{"
        "\"file_token\":{\"type\":\"string\",\"description\":\"Document file token (required)\"},"
        "\"comment_id\":{\"type\":\"string\",\"description\":\"Comment ID (required)\"},"
        "\"file_type\":{\"type\":\"string\",\"description\":\"File type (default docx)\"},"
        "\"page_size\":{\"type\":\"integer\",\"description\":\"Results per page (max 100)\"}"
        "},\"required\":[\"file_token\",\"comment_id\"]}";

    registry_register_ex("feishu_drive_list_comment_replies",
        "List replies in a comment thread on a Feishu document.",
        FEISHU_REPLIES_SCHEMA, "file", handle_feishu_drive_list_comment_replies);

    /* ── feishu_drive_reply_comment — reply to a comment thread ── */
    static const char *FEISHU_REPLY_SCHEMA =
        "{\"type\":\"object\",\"properties\":{"
        "\"file_token\":{\"type\":\"string\",\"description\":\"Document file token (required)\"},"
        "\"comment_id\":{\"type\":\"string\",\"description\":\"Comment ID to reply to (required)\"},"
        "\"content\":{\"type\":\"string\",\"description\":\"Reply text content (plain text, required)\"},"
        "\"file_type\":{\"type\":\"string\",\"description\":\"File type (default docx)\"}"
        "},\"required\":[\"file_token\",\"comment_id\",\"content\"]}";

    registry_register_ex("feishu_drive_reply_comment",
        "Reply to a comment thread on a Feishu document.",
        FEISHU_REPLY_SCHEMA, "file", handle_feishu_drive_reply_comment);

    /* ── feishu_drive_add_comment — add a whole-document comment ── */
    static const char *FEISHU_ADD_SCHEMA =
        "{\"type\":\"object\",\"properties\":{"
        "\"file_token\":{\"type\":\"string\",\"description\":\"Document file token (required)\"},"
        "\"content\":{\"type\":\"string\",\"description\":\"Comment text content (required)\"},"
        "\"file_type\":{\"type\":\"string\",\"description\":\"File type (default docx)\"}"
        "},\"required\":[\"file_token\",\"content\"]}";

    registry_register_ex("feishu_drive_add_comment",
        "Add a whole-document comment to a Feishu document.",
        FEISHU_ADD_SCHEMA, "file", handle_feishu_drive_add_comment);
}
