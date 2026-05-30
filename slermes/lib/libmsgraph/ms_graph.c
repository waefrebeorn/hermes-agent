/*
 * ms_graph.c — Microsoft Graph REST client for Hermes C.
 * Port of Python tools/microsoft_graph_auth.py + microsoft_graph_client.py.
 *
 * OAuth2 app-only auth + REST client for Microsoft Graph API.
 * Depends on libhttp and libjson.
 */

#include "ms_graph.h"
#include "http.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ── Credentials ── */

bool msgraph_credentials_from_env(msgraph_credentials_t *creds, bool required)
{
    if (!creds) return false;

    const char *tenant_id = getenv("MSGRAPH_TENANT_ID");
    const char *client_id = getenv("MSGRAPH_CLIENT_ID");
    const char *client_secret = getenv("MSGRAPH_CLIENT_SECRET");
    const char *scope = getenv("MSGRAPH_SCOPE");
    const char *auth_url = getenv("MSGRAPH_AUTHORITY_URL");

    if (!tenant_id || !*tenant_id || !client_id || !*client_id ||
        !client_secret || !*client_secret) {
        (void)required;
        return false;
    }

    strncpy(creds->tenant_id, tenant_id, sizeof(creds->tenant_id) - 1);
    creds->tenant_id[sizeof(creds->tenant_id) - 1] = '\0';

    strncpy(creds->client_id, client_id, sizeof(creds->client_id) - 1);
    creds->client_id[sizeof(creds->client_id) - 1] = '\0';

    strncpy(creds->client_secret, client_secret, sizeof(creds->client_secret) - 1);
    creds->client_secret[sizeof(creds->client_secret) - 1] = '\0';

    strncpy(creds->scope,
            (scope && *scope) ? scope : MSGRAPH_DEFAULT_SCOPE,
            sizeof(creds->scope) - 1);
    creds->scope[sizeof(creds->scope) - 1] = '\0';

    strncpy(creds->authority_url,
            (auth_url && *auth_url) ? auth_url : MSGRAPH_DEFAULT_AUTH,
            sizeof(creds->authority_url) - 1);
    creds->authority_url[sizeof(creds->authority_url) - 1] = '\0';

    msgraph_credentials_build_token_url(creds);
    return true;
}

void msgraph_credentials_build_token_url(msgraph_credentials_t *creds)
{
    if (!creds) return;

    size_t alen = strlen(creds->authority_url);
    while (alen > 0 && creds->authority_url[alen - 1] == '/')
        creds->authority_url[--alen] = '\0';

    size_t tlen = strlen(creds->tenant_id);
    while (tlen > 0 && creds->tenant_id[tlen - 1] == '/')
        creds->tenant_id[--tlen] = '\0';

    snprintf(creds->token_url, sizeof(creds->token_url),
             "%s/%s/oauth2/v2.0/token",
             creds->authority_url, creds->tenant_id);
}

/* ── Token Provider ── */

void msgraph_token_provider_init(msgraph_token_provider_t *tp,
                                  const msgraph_credentials_t *creds)
{
    if (!tp || !creds) return;
    tp->credentials = *creds;
    tp->has_cached = false;
    tp->timeout_seconds = MSGRAPH_DEFAULT_TIMEOUT;
    tp->skew_seconds = MSGRAPH_TOKEN_SKEW;
    memset(&tp->cached, 0, sizeof(tp->cached));
}

static const __attribute__((unused)) char *msgraph_extract_error(const char *body)
{
    static char buf[MSGRAPH_MAX_ERROR];
    if (!body || !*body) return "unknown error";

    json_t *root = json_parse(body, NULL);
    if (!root) return body;

    const char *desc = json_get_str(root, "error_description", NULL);
    if (desc) {
        strncpy(buf, desc, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        json_free(root);
        return buf;
    }

    const char *err = json_get_str(root, "error", NULL);
    if (err) {
        strncpy(buf, err, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        json_free(root);
        return buf;
    }

    json_free(root);
    return body;
}

const char *msgraph_get_access_token(msgraph_token_provider_t *tp,
                                      char *out, size_t out_size)
{
    if (!tp || !out || out_size == 0) return NULL;

    if (tp->has_cached && !msgraph_token_is_expired(&tp->cached, tp->skew_seconds)) {
        strncpy(out, tp->cached.access_token, out_size - 1);
        out[out_size - 1] = '\0';
        return out;
    }

    char body[4096];
    snprintf(body, sizeof(body),
             "grant_type=client_credentials&client_id=%s&client_secret=%s&scope=%s",
             tp->credentials.client_id,
             tp->credentials.client_secret,
             tp->credentials.scope);

    http_t *h = http_new((int)tp->timeout_seconds);
    if (!h) return NULL;

    http_resp_t *resp = http_request(h, HTTP_POST,
                                      tp->credentials.token_url,
                                      "Content-Type: application/x-www-form-urlencoded",
                                      body, strlen(body));

    if (!resp || resp->status < 200 || resp->status >= 300) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    json_t *json = json_parse(resp->body, NULL);
    if (!json) {
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    const char *access_token = json_get_str(json, "access_token", NULL);
    const char *token_type = json_get_str(json, "token_type", NULL);
    double expires_in = json_get_num(json, "expires_in", 0);

    if (!access_token || expires_in <= 0) {
        json_free(json);
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    strncpy(tp->cached.access_token, access_token,
            sizeof(tp->cached.access_token) - 1);
    tp->cached.access_token[sizeof(tp->cached.access_token) - 1] = '\0';

    if (token_type && *token_type) {
        strncpy(tp->cached.token_type, token_type,
                sizeof(tp->cached.token_type) - 1);
        tp->cached.token_type[sizeof(tp->cached.token_type) - 1] = '\0';
    } else {
        strncpy(tp->cached.token_type, "Bearer",
                sizeof(tp->cached.token_type) - 1);
    }

    tp->cached.expires_at = (double)time(NULL) + expires_in;
    tp->has_cached = true;

    strncpy(out, access_token, out_size - 1);
    out[out_size - 1] = '\0';

    json_free(json);
    http_resp_free(resp);
    http_free(h);

    return out;
}

bool msgraph_token_is_expired(const msgraph_cached_token_t *token,
                               int skew_seconds)
{
    if (!token) return true;
    if (!token->access_token[0]) return true;
    double now = (double)time(NULL);
    return token->expires_at <= (now + (double)(skew_seconds > 0 ? skew_seconds : 0));
}

void msgraph_clear_cache(msgraph_token_provider_t *tp)
{
    if (tp) tp->has_cached = false;
}

/* ── Graph Client ── */

void msgraph_client_init(msgraph_client_t *client,
                          msgraph_token_provider_t *provider)
{
    if (!client) return;
    client->provider = provider;
    strncpy(client->base_url, MSGRAPH_DEFAULT_BASE, sizeof(client->base_url) - 1);
    client->base_url[sizeof(client->base_url) - 1] = '\0';
    client->timeout_seconds = MSGRAPH_DEFAULT_TIMEOUT;
    client->max_retries = MSGRAPH_MAX_RETRIES;
    strncpy(client->user_agent, MSGRAPH_USER_AGENT,
            sizeof(client->user_agent) - 1);
    client->user_agent[sizeof(client->user_agent) - 1] = '\0';
}

static void _resolve_url(const msgraph_client_t *client,
                          const char *path,
                          char *out, size_t out_size)
{
    if (!path || !out || out_size == 0) return;

    if (strncmp(path, "http://", 7) == 0 ||
        strncmp(path, "https://", 8) == 0) {
        strncpy(out, path, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    const char *p = path;
    while (*p == '/') p++;

    snprintf(out, out_size, "%s/%s", client->base_url, p);
}

static void _build_auth_header(msgraph_token_provider_t *tp,
                                char *auth, size_t auth_size,
                                bool force_refresh)
{
    if (force_refresh) {
        msgraph_clear_cache(tp);
    }

    char token[MSGRAPH_MAX_TOKEN];
    const char *t = msgraph_get_access_token(tp, token, sizeof(token));
    if (t) {
        snprintf(auth, auth_size, "Authorization: Bearer %s", token);
    } else {
        auth[0] = '\0';
    }
}

static void _build_error(msgraph_error_t *err, int status,
                          const char *method, const char *url,
                          const char *body)
{
    if (!err) return;
    memset(err, 0, sizeof(*err));
    err->status_code = status;
    if (method) {
        strncpy(err->method, method, sizeof(err->method) - 1);
        err->method[sizeof(err->method) - 1] = '\0';
    }
    if (url) {
        strncpy(err->url, url, sizeof(err->url) - 1);
        err->url[sizeof(err->url) - 1] = '\0';
    }
    err->retry_after_seconds = -1;

    if (!body || !*body) return;

    json_t *json = json_parse(body, NULL);
    if (json) {
        const char *msg = NULL;
        json_t *error_node = json_obj_get(json, "error");
        if (error_node) {
            msg = json_get_str(error_node, "message", NULL);
            if (!msg) msg = json_get_str(error_node, "code", NULL);
        }
        if (!msg) msg = json_get_str(json, "error_description", NULL);
        if (msg) {
            strncpy(err->message, msg, sizeof(err->message) - 1);
            err->message[sizeof(err->message) - 1] = '\0';
        } else {
            strncpy(err->message, body, sizeof(err->message) - 1);
        }
        json_free(json);
    } else {
        strncpy(err->message, body, sizeof(err->message) - 1);
        err->message[sizeof(err->message) - 1] = '\0';
    }
}

static char *_request_internal(msgraph_client_t *client,
                                const char *method,
                                const char *path,
                                const char *json_body,
                                msgraph_error_t *err)
{
    char url[MSGRAPH_MAX_URL];
    _resolve_url(client, path, url, sizeof(url));

    char auth[4096];
    int attempt = 0;
    bool last_was_401 = false;

    while (attempt <= client->max_retries) {
        _build_auth_header(client->provider, auth, sizeof(auth),
                           attempt > 0 && last_was_401);

        if (!auth[0]) {
            if (err) {
                err->status_code = -1;
                snprintf(err->message, sizeof(err->message),
                         "Failed to acquire access token");
            }
            return NULL;
        }

        char headers[8192];
        int n = snprintf(headers, sizeof(headers),
                         "%s\r\nAccept: application/json\r\nUser-Agent: %s",
                         auth, client->user_agent);
        if (json_body && *json_body) {
            snprintf(headers + (size_t)n,
                     sizeof(headers) - (size_t)n,
                     "\r\nContent-Type: application/json");
        }

        http_method_t meth = HTTP_GET;
        if (strcmp(method, "POST") == 0) meth = HTTP_POST;
        else if (strcmp(method, "PATCH") == 0) meth = HTTP_POST;
        else if (strcmp(method, "DELETE") == 0) meth = HTTP_DELETE;

        const char *body_data = (json_body && *json_body) ? json_body : NULL;
        size_t body_len = body_data ? strlen(body_data) : 0;

        http_t *h = http_new((int)client->timeout_seconds);
        if (!h) {
            if (err) {
                err->status_code = -1;
                snprintf(err->message, sizeof(err->message),
                         "Failed to create HTTP client");
            }
            return NULL;
        }

        http_resp_t *resp = http_request(h, meth, url, headers,
                                          body_data, body_len);

        if (!resp) {
            http_free(h);
            attempt++;
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 500000000 };
            nanosleep(&ts, NULL);
            continue;
        }

        if (resp->status >= 200 && resp->status < 300) {
            char *result = resp->body ? strdup(resp->body) : strdup("{}");
            http_resp_free(resp);
            http_free(h);
            return result;
        }

        if (resp->status == 401 && attempt < client->max_retries) {
            msgraph_clear_cache(client->provider);
            _build_error(err, resp->status, method, url, resp->body);
            last_was_401 = true;
            http_resp_free(resp);
            http_free(h);
            attempt++;
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 500000000 };
            nanosleep(&ts, NULL);
            continue;
        }

        bool retryable = (resp->status == 429 || resp->status >= 500);
        if (retryable && attempt < client->max_retries) {
            _build_error(err, resp->status, method, url, resp->body);
            last_was_401 = false;
            http_resp_free(resp);
            http_free(h);
            attempt++;
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000000 };
            nanosleep(&ts, NULL);
            continue;
        }

        _build_error(err, resp->status, method, url, resp->body);
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    if (err && !err->message[0]) {
        snprintf(err->message, sizeof(err->message),
                 "Request exhausted retries for %s %s", method, url);
    }
    return NULL;
}

char *msgraph_get(msgraph_client_t *client, const char *path,
                   msgraph_error_t *err)
{
    return _request_internal(client, "GET", path, NULL, err);
}

char *msgraph_post(msgraph_client_t *client, const char *path,
                    const char *body_json, msgraph_error_t *err)
{
    return _request_internal(client, "POST", path, body_json, err);
}

char *msgraph_patch(msgraph_client_t *client, const char *path,
                     const char *body_json, msgraph_error_t *err)
{
    return _request_internal(client, "PATCH", path, body_json, err);
}

char *msgraph_delete(msgraph_client_t *client, const char *path,
                      msgraph_error_t *err)
{
    return _request_internal(client, "DELETE", path, NULL, err);
}

bool msgraph_download(msgraph_client_t *client, const char *path,
                       const char *destination,
                       msgraph_download_result_t *result,
                       msgraph_error_t *err)
{
    char url[MSGRAPH_MAX_URL];
    _resolve_url(client, path, url, sizeof(url));

    char auth[4096];
    _build_auth_header(client->provider, auth, sizeof(auth), false);

    if (!auth[0]) {
        if (err) {
            err->status_code = -1;
            snprintf(err->message, sizeof(err->message),
                     "Failed to acquire access token for download");
        }
        return false;
    }

    char headers[8192];
    snprintf(headers, sizeof(headers),
             "%s\r\nAccept: application/json\r\nUser-Agent: %s",
             auth, client->user_agent);

    http_t *h = http_new((int)client->timeout_seconds);
    if (!h) return false;

    http_resp_t *resp = http_get(h, url, headers);

    if (!resp || resp->status < 200 || resp->status >= 300) {
        if (resp) {
            _build_error(err, resp->status, "GET", url, resp->body);
            http_resp_free(resp);
        }
        http_free(h);
        return false;
    }

    FILE *f = fopen(destination, "wb");
    if (!f) {
        if (err) {
            err->status_code = -1;
            snprintf(err->message, sizeof(err->message),
                     "Cannot open destination file: %s", destination);
        }
        http_resp_free(resp);
        http_free(h);
        return false;
    }

    if (resp->body && resp->body_len > 0) {
        fwrite(resp->body, 1, resp->body_len, f);
    }
    fclose(f);

    if (result) {
        strncpy(result->path, destination, sizeof(result->path) - 1);
        result->path[sizeof(result->path) - 1] = '\0';
        result->size_bytes = (long)(resp ? resp->body_len : 0);

        const char *ct = resp->headers ? strstr(resp->headers, "Content-Type:") : NULL;
        if (ct) {
            ct += 13;
            while (*ct == ' ') ct++;
            const char *ct_end = ct;
            while (*ct_end && *ct_end != '\r' && *ct_end != '\n') ct_end++;
            size_t ct_len = (size_t)(ct_end - ct);
            if (ct_len > sizeof(result->content_type) - 1)
                ct_len = sizeof(result->content_type) - 1;
            memcpy(result->content_type, ct, ct_len);
            result->content_type[ct_len] = '\0';
        }
    }

    http_resp_free(resp);
    http_free(h);
    return true;
}

void msgraph_error_cleanup(msgraph_error_t *err)
{
    (void)err;
}
