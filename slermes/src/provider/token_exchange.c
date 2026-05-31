/*
 * token_exchange.c — OAuth PKCE token exchange for Hermes C.
 *
 * POSTs form-urlencoded to OAuth token endpoints, parses JSON response.
 * Supports: xAI, MiniMax, generic OAuth PKCE providers.
 *
 * Uses hermes_http for HTTPS, hermes_json for response parsing,
 * hermes_crypto for PKCE primitives. No libcurl dependency.
 */

#include "hermes.h"
#include "hermes_auth.h"
#include "hermes_crypto.h"
#include "hermes_http.h"
#include "hermes_json.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>

/* Thread-local last error buffer */
#define ERR_BUF_SZ 512
static __thread char g_last_error[ERR_BUF_SZ] = {0};

void _set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, ERR_BUF_SZ, fmt, args);
    va_end(args);
}

const char *oauth_last_error(void) {
    return g_last_error;
}

/* ================================================================
 *  URL-encode a simple key=value pair and append to buffer
 * ================================================================ */

static int _append_form_field(char *buf, size_t bufsz, size_t *pos,
                               const char *key, const char *value)
{
    if (!key || !value) return -1;
    char *ek = http_url_encode(key);
    char *ev = http_url_encode(value);
    if (!ek || !ev) {
        free(ek); free(ev);
        return -1;
    }
    int n = snprintf(buf + *pos, bufsz - *pos,
                     "%s%s=%s", *pos > 0 ? "&" : "", ek, ev);
    free(ek); free(ev);
    if (n < 0 || (size_t)n >= bufsz - *pos) return -1;
    *pos += (size_t)n;
    return 0;
}

/* ================================================================
 *  Build form-urlencoded body for PKCE token exchange
 * ================================================================ */

static char *_build_token_exchange_body(
    const char *code,
    const char *redirect_uri,
    const char *client_id,
    const char *code_verifier,
    const char *code_challenge)
{
    /* Max body size: ~512 bytes for typical PKCE fields */
    char *body = (char *)calloc(1, 1024);
    if (!body) return NULL;

    size_t pos = 0;

    if (_append_form_field(body, 1024, &pos, "grant_type", "authorization_code") < 0)
        goto fail;
    if (_append_form_field(body, 1024, &pos, "code", code) < 0)
        goto fail;
    if (_append_form_field(body, 1024, &pos, "redirect_uri", redirect_uri) < 0)
        goto fail;
    if (_append_form_field(body, 1024, &pos, "client_id", client_id) < 0)
        goto fail;
    if (_append_form_field(body, 1024, &pos, "code_verifier", code_verifier) < 0)
        goto fail;

    /* Defense-in-depth: echo code_challenge + method for providers
     * (like xAI, #26990) that re-validate at token endpoint */
    if (code_challenge && code_challenge[0]) {
        if (_append_form_field(body, 1024, &pos, "code_challenge", code_challenge) < 0)
            goto fail;
        if (_append_form_field(body, 1024, &pos, "code_challenge_method", "S256") < 0)
            goto fail;
    }

    return body;

fail:
    free(body);
    return NULL;
}

/* ================================================================
 *  Parse token response JSON into oauth_token_t
 * ================================================================ */

static oauth_token_t *_parse_token_response(const char *json_body) {
    char *err_msg = NULL;
    json_node_t *root = json_parse(json_body, &err_msg);
    if (!root) {
        _set_error("Token response parse failed: %s", err_msg ? err_msg : "unknown");
        free(err_msg);
        return NULL;
    }

    if (root->type != JSON_OBJECT) {
        _set_error("Token response is not a JSON object (got type %d)", root->type);
        json_free(root);
        return NULL;
    }

    /* Check for error response */
    json_node_t *err_node = json_object_get(root, "error");
    if (err_node && err_node->type == JSON_STRING) {
        const char *err_desc = json_object_get_string(root, "error_description", "");
        _set_error("OAuth error: %s — %s", err_node->str_val, err_desc);
        json_free(root);
        return NULL;
    }

    oauth_token_t *tok = (oauth_token_t *)calloc(1, sizeof(oauth_token_t));
    if (!tok) {
        json_free(root);
        _set_error("Out of memory");
        return NULL;
    }

    const char *at = json_object_get_string(root, "access_token", "");
    tok->access_token = at[0] ? strdup(at) : NULL;

    const char *rt = json_object_get_string(root, "refresh_token", "");
    tok->refresh_token = rt[0] ? strdup(rt) : NULL;

    const char *it = json_object_get_string(root, "id_token", "");
    tok->id_token = it[0] ? strdup(it) : NULL;

    const char *tt = json_object_get_string(root, "token_type", "Bearer");
    tok->token_type = strdup(tt);

    tok->expires_in = (int)json_object_get_number(root, "expires_in", 0);

    /* Compute expiry as unix timestamp */
    if (tok->expires_in > 0) {
        time_t now = time(NULL);
        tok->expires_at = (double)(now + tok->expires_in);
    } else {
        tok->expires_at = 0.0;
    }

    json_free(root);

    if (!tok->access_token) {
        oauth_token_free(tok);
        _set_error("Token response missing access_token");
        return NULL;
    }

    return tok;
}

/* ================================================================
 *  Core exchange: POST form-urlencoded, parse response
 * ================================================================ */

oauth_token_t *oauth_exchange_code(
    const char *token_endpoint,
    const char *code,
    const char *redirect_uri,
    const char *client_id,
    const char *code_verifier,
    const char *code_challenge,
    int timeout_sec)
{
    /* Validate required params */
    if (!token_endpoint || !token_endpoint[0]) {
        _set_error("token_endpoint is required");
        return NULL;
    }
    if (!code || !code[0]) {
        _set_error("authorization code is required");
        return NULL;
    }
    if (!redirect_uri || !redirect_uri[0]) {
        _set_error("redirect_uri is required");
        return NULL;
    }
    if (!client_id || !client_id[0]) {
        _set_error("client_id is required");
        return NULL;
    }
    if (!code_verifier || !code_verifier[0]) {
        _set_error("PKCE code_verifier is empty — refusing to send "
                   "auth code without PKCE (see RFC 7636 §4.5)");
        return NULL;
    }

    /* Build form-urlencoded body */
    char *body = _build_token_exchange_body(
        code, redirect_uri, client_id, code_verifier, code_challenge);
    if (!body) {
        _set_error("Failed to build token exchange request body");
        return NULL;
    }

    /* Build headers */
    char headers[512];
    snprintf(headers, sizeof(headers),
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Accept: application/json\r\n");

    /* Enforce minimum 20s timeout (floor for slow networks) */
    if (timeout_sec < 20) timeout_sec = 20;

    /* Create HTTP client and POST */
    http_client_t *client = http_client_new(timeout_sec);
    if (!client) {
        free(body);
        _set_error("Failed to create HTTP client");
        return NULL;
    }

    http_response_t *resp = http_request(
        client, HTTP_POST, token_endpoint, headers, body, strlen(body));
    free(body);

    if (!resp) {
        http_client_free(client);
        _set_error("Token exchange HTTP request failed (connection error)");
        return NULL;
    }

    /* Handle non-200 response */
    if (resp->status != 200) {
        char err_body[512] = {0};
        if (resp->body && resp->body_len > 0) {
            size_t copy = resp->body_len < 511 ? resp->body_len : 511;
            memcpy(err_body, resp->body, copy);
            err_body[copy] = '\0';
        }
        _set_error("Token exchange failed: HTTP %d — %s",
                   resp->status, err_body);
        http_response_free(resp);
        http_client_free(client);
        return NULL;
    }

    /* Parse JSON response */
    if (!resp->body || resp->body_len == 0) {
        _set_error("Token exchange returned empty response body");
        http_response_free(resp);
        http_client_free(client);
        return NULL;
    }

    oauth_token_t *tok = _parse_token_response(resp->body);
    http_response_free(resp);
    http_client_free(client);

    return tok;
}

/* ================================================================
 *  xAI-specific convenience wrapper
 * ================================================================ */

oauth_token_t *xai_oauth_exchange(
    const char *code,
    const char *redirect_uri,
    const char *code_verifier,
    const char *code_challenge,
    int timeout_sec)
{
    return oauth_exchange_code(
        "https://auth.x.ai/oauth2/token",
        code,
        redirect_uri,
        XAI_OAUTH_CLIENT_ID,
        code_verifier,
        code_challenge,
        timeout_sec
    );
}

/* ================================================================
 *  Free token
 * ================================================================ */

void oauth_token_free(oauth_token_t *tok) {
    if (!tok) return;
    free(tok->access_token);
    free(tok->refresh_token);
    free(tok->id_token);
    free(tok->token_type);
    free(tok);
}

/* ================================================================
 *  Auth store (auth.json)
 * ================================================================ */

static char *_auth_json_path(const char *hermes_home) {
    size_t len = strlen(hermes_home) + 16;
    char *path = (char *)malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s/auth.json", hermes_home);
    return path;
}

auth_entry_t *auth_store_load(const char *hermes_home, int *out_count) {
    if (out_count) *out_count = 0;

    char *path = _auth_json_path(hermes_home);
    if (!path) return NULL;

    char *err_msg = NULL;
    json_node_t *root = json_parse_file(path, &err_msg);
    free(path);

    if (!root) {
        free(err_msg);
        return NULL;  /* File doesn't exist or invalid JSON = empty store */
    }

    if (root->type != JSON_OBJECT) {
        json_free(root);
        return NULL;
    }

    /* Count entries */
    int count = (int)root->c.count;
    if (count == 0) {
        json_free(root);
        if (out_count) *out_count = 0;
        return NULL;
    }

    auth_entry_t *entries = (auth_entry_t *)calloc((size_t)(count + 1), sizeof(auth_entry_t));
    if (!entries) {
        json_free(root);
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        const char *provider = root->c.keys[i];
        json_node_t *node = root->c.items[i];
        if (!provider || !node || node->type != JSON_OBJECT) continue;

        strncpy(entries[i].provider, provider, 63);
        entries[i].provider[63] = '\0';

        const char *at = json_object_get_string(node, "access_token", "");
        entries[i].token.access_token = at[0] ? strdup(at) : NULL;
        entries[i].token.refresh_token = strdup(
            json_object_get_string(node, "refresh_token", ""));
        entries[i].token.id_token = strdup(
            json_object_get_string(node, "id_token", ""));
        entries[i].token.token_type = strdup(
            json_object_get_string(node, "token_type", "Bearer"));
        entries[i].token.expires_at =
            json_object_get_number(node, "expires_at", 0.0);
        entries[i].token.expires_in =
            (int)json_object_get_number(node, "expires_in", 0);
    }

    json_free(root);

    if (out_count) *out_count = count;
    return entries;
}

bool auth_store_save(const char *hermes_home, const auth_entry_t *entry) {
    if (!hermes_home || !entry || !entry->provider[0]) {
        _set_error("auth_store_save: invalid args");
        return false;
    }

    /* Load existing store, or create new */
    int existing_count = 0;
    auth_entry_t *existing = auth_store_load(hermes_home, &existing_count);

    char *path = _auth_json_path(hermes_home);
    if (!path) return false;

    /* Build JSON */
    json_node_t *root = json_new_object();
    if (!root) { free(path); return false; }

    /* Copy existing entries (skip the one we're replacing) */
    for (int i = 0; i < existing_count; i++) {
        if (strcmp(existing[i].provider, entry->provider) == 0) continue;
        json_node_t *node = json_new_object();
        if (existing[i].token.access_token)
            json_object_set(node, "access_token",
                json_new_string(existing[i].token.access_token));
        if (existing[i].token.refresh_token && existing[i].token.refresh_token[0])
            json_object_set(node, "refresh_token",
                json_new_string(existing[i].token.refresh_token));
        if (existing[i].token.id_token && existing[i].token.id_token[0])
            json_object_set(node, "id_token",
                json_new_string(existing[i].token.id_token));
        json_object_set(node, "token_type",
            json_new_string(existing[i].token.token_type ? existing[i].token.token_type : "Bearer"));
        json_object_set(node, "expires_at",
            json_new_number(existing[i].token.expires_at));
        json_object_set(node, "expires_in",
            json_new_number(existing[i].token.expires_in));
        json_object_set(root, existing[i].provider, node);
    }

    /* Add/replace the entry */
    {
        json_node_t *node = json_new_object();
        if (entry->token.access_token)
            json_object_set(node, "access_token",
                json_new_string(entry->token.access_token));
        if (entry->token.refresh_token && entry->token.refresh_token[0])
            json_object_set(node, "refresh_token",
                json_new_string(entry->token.refresh_token));
        if (entry->token.id_token && entry->token.id_token[0])
            json_object_set(node, "id_token",
                json_new_string(entry->token.id_token));
        json_object_set(node, "token_type",
            json_new_string(entry->token.token_type ? entry->token.token_type : "Bearer"));
        json_object_set(node, "expires_at",
            json_new_number(entry->token.expires_at));
        json_object_set(node, "expires_in",
            json_new_number(entry->token.expires_in));
        json_object_set(root, entry->provider, node);
    }

    /* Serialize */
    char *json_str = json_serialize(root);
    json_free(root);

    if (!json_str) {
        _set_error("Failed to serialize auth store");
        auth_store_free(existing, existing_count);
        free(path);
        return false;
    }

    /* Write to file */
    FILE *f = fopen(path, "w");
    if (!f) {
        _set_error("Failed to write %s: %s", path, strerror(errno));
        free(json_str);
        auth_store_free(existing, existing_count);
        free(path);
        return false;
    }
    fputs(json_str, f);
    fclose(f);

    /* Set restrictive permissions (owner only) */
    chmod(path, 0600);

    free(json_str);
    auth_store_free(existing, existing_count);
    free(path);
    return true;
}

bool auth_store_remove(const char *hermes_home, const char *provider) {
    if (!hermes_home || !provider) return false;

    int count = 0;
    auth_entry_t *entries = auth_store_load(hermes_home, &count);
    if (!entries || count == 0) {
        auth_store_free(entries, count);
        return true;  /* nothing to remove = success */
    }

    char *path = _auth_json_path(hermes_home);
    if (!path) { auth_store_free(entries, count); return false; }

    json_node_t *root = json_new_object();
    if (!root) { free(path); auth_store_free(entries, count); return false; }

    bool found = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].provider, provider) == 0) {
            found = true;
            continue;
        }
        json_node_t *node = json_new_object();
        if (entries[i].token.access_token)
            json_object_set(node, "access_token",
                json_new_string(entries[i].token.access_token));
        if (entries[i].token.refresh_token && entries[i].token.refresh_token[0])
            json_object_set(node, "refresh_token",
                json_new_string(entries[i].token.refresh_token));
        if (entries[i].token.id_token && entries[i].token.id_token[0])
            json_object_set(node, "id_token",
                json_new_string(entries[i].token.id_token));
        json_object_set(node, "token_type",
            json_new_string(entries[i].token.token_type ? entries[i].token.token_type : "Bearer"));
        json_object_set(node, "expires_at",
            json_new_number(entries[i].token.expires_at));
        json_object_set(node, "expires_in",
            json_new_number(entries[i].token.expires_in));
        json_object_set(root, entries[i].provider, node);
    }

    auth_store_free(entries, count);

    char *json_str = json_serialize(root);
    json_free(root);

    if (!json_str) {
        _set_error("Failed to serialize auth store after removal");
        free(path);
        return false;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        _set_error("Failed to write %s: %s", path, strerror(errno));
        free(json_str);
        free(path);
        return false;
    }
    fputs(json_str, f);
    fclose(f);
    chmod(path, 0600);

    free(json_str);
    free(path);
    return found;  /* true if we actually removed something */
}

void auth_store_free(auth_entry_t *entries, int count) {
    if (!entries) return;
    for (int i = 0; i < count; i++) {
        free(entries[i].token.access_token);
        free(entries[i].token.refresh_token);
        free(entries[i].token.id_token);
        free(entries[i].token.token_type);
    }
    free(entries);
}

/* ================================================================
 *  Device code helpers
 * ================================================================ */

oauth_device_code_t *oauth_device_code_new(void);
static oauth_token_t *_parse_token_response_helper(json_t *root);

oauth_device_code_t *oauth_device_code_new(void) {
    oauth_device_code_t *dc = (oauth_device_code_t *)calloc(1, sizeof(oauth_device_code_t));
    if (dc) {
        dc->interval = 5;   /* default RFC 8628 §3.2 */
    }
    return dc;
}

void oauth_device_code_free(oauth_device_code_t *dc) {
    if (!dc) return;
    free(dc->device_code);
    free(dc->user_code);
    free(dc->verification_uri);
    free(dc->verification_uri_complete);
    free(dc);
}

oauth_device_code_t *oauth_device_code_request(
    const char *device_endpoint,
    const char *client_id,
    const char *scope,
    int timeout_sec)
{
    if (!device_endpoint || !device_endpoint[0] || !client_id || !client_id[0]) {
        _set_error("device_endpoint and client_id are required");
        return NULL;
    }

    /* Build form-urlencoded body */
    char body[2048];
    size_t pos = 0;
    if (_append_form_field(body, sizeof(body), &pos, "client_id", client_id) < 0) {
        _set_error("Failed to build device code request body");
        return NULL;
    }
    if (scope && scope[0]) {
        if (_append_form_field(body, sizeof(body), &pos, "scope", scope) < 0) {
            _set_error("Failed to build device code request body (scope)");
            return NULL;
        }
    }

    /* Build headers */
    char headers[256];
    snprintf(headers, sizeof(headers),
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Accept: application/json\r\n");

    if (timeout_sec < 20) timeout_sec = 20;

    http_t *h = http_new(timeout_sec);
    if (!h) {
        _set_error("Failed to create HTTP client");
        return NULL;
    }

    http_resp_t *resp = http_request(h, HTTP_POST, device_endpoint,
                                     headers, body, strlen(body));
    if (!resp) {
        _set_error("Device code request failed (connection error)");
        http_free(h);
        return NULL;
    }

    if (resp->status != 200) {
        char eb[512] = {0};
        if (resp->body && resp->body_len > 0) {
            size_t cp = resp->body_len < 511 ? resp->body_len : 511;
            memcpy(eb, resp->body, cp);
            eb[cp] = '\0';
        }
        _set_error("Device code request failed: HTTP %d — %s", resp->status, eb);
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    if (!resp->body || resp->body_len == 0) {
        _set_error("Device code request returned empty body");
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    char *err = NULL;
    json_t *root = json_parse(resp->body, &err);
    http_resp_free(resp);
    http_free(h);

    if (!root) {
        _set_error("Device code response parse failed: %s", err ? err : "unknown");
        free(err);
        return NULL;
    }

    if (root->type != JSON_OBJECT) {
        _set_error("Device code response is not a JSON object");
        json_free(root);
        return NULL;
    }

    /* Check for error */
    const char *err_str = json_get_str(root, "error", NULL);
    if (err_str) {
        const char *ed = json_get_str(root, "error_description", "");
        _set_error("Device code error: %s — %s", err_str, ed);
        json_free(root);
        return NULL;
    }

    oauth_device_code_t *dc = oauth_device_code_new();
    if (!dc) {
        json_free(root);
        _set_error("Out of memory");
        return NULL;
    }

    const char *v;
    v = json_get_str(root, "device_code", NULL);
    dc->device_code = v ? strdup(v) : NULL;
    v = json_get_str(root, "user_code", NULL);
    dc->user_code = v ? strdup(v) : NULL;
    v = json_get_str(root, "verification_uri", NULL);
    dc->verification_uri = v ? strdup(v) : NULL;
    v = json_get_str(root, "verification_uri_complete", NULL);
    dc->verification_uri_complete = v ? strdup(v) : NULL;
    dc->interval = (int)json_get_num(root, "interval", 5);
    dc->expires_in = (int)json_get_num(root, "expires_in", 600);

    json_free(root);

    if (!dc->device_code || !dc->user_code) {
        _set_error("Device code response missing device_code or user_code");
        oauth_device_code_free(dc);
        return NULL;
    }

    return dc;
}

oauth_token_t *oauth_device_code_poll(
    const char *token_endpoint,
    const char *client_id,
    const char *device_code,
    int timeout_sec)
{
    if (!token_endpoint || !token_endpoint[0] ||
        !client_id || !client_id[0] ||
        !device_code || !device_code[0]) {
        _set_error("token_endpoint, client_id, and device_code are required");
        return NULL;
    }

    /* Build body: grant_type=urn:ietf:params:oauth:grant-type:device_code */
    char body[2048];
    size_t pos = 0;
    if (_append_form_field(body, sizeof(body), &pos,
            "grant_type", "urn:ietf:params:oauth:grant-type:device_code") < 0) {
        _set_error("Failed to build poll body");
        return NULL;
    }
    if (_append_form_field(body, sizeof(body), &pos, "device_code", device_code) < 0) {
        _set_error("Failed to build poll body (device_code)");
        return NULL;
    }
    if (_append_form_field(body, sizeof(body), &pos, "client_id", client_id) < 0) {
        _set_error("Failed to build poll body (client_id)");
        return NULL;
    }

    char headers[256];
    snprintf(headers, sizeof(headers),
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Accept: application/json\r\n");

    if (timeout_sec < 20) timeout_sec = 20;

    http_t *h = http_new(timeout_sec);
    if (!h) {
        _set_error("Failed to create HTTP client");
        return NULL;
    }

    http_resp_t *resp = http_request(h, HTTP_POST, token_endpoint,
                                     headers, body, strlen(body));
    if (!resp) {
        _set_error("Device code poll request failed (connection error)");
        http_free(h);
        return NULL;
    }

    if (!resp->body || resp->body_len == 0) {
        _set_error("Device code poll returned empty response");
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    char *err = NULL;
    json_t *root = json_parse(resp->body, &err);
    http_resp_free(resp);

    if (!root) {
        _set_error("Device code poll parse failed: %s", err ? err : "unknown");
        free(err);
        http_free(h);
        return NULL;
    }

    if (root->type != JSON_OBJECT) {
        _set_error("Device code poll response is not a JSON object");
        json_free(root);
        http_free(h);
        return NULL;
    }

    /* Check for authorization_pending (expected while user hasn't acted) */
    const char *err_field = json_get_str(root, "error", NULL);
    if (err_field) {
        if (strcmp(err_field, "authorization_pending") == 0 ||
            strcmp(err_field, "slow_down") == 0) {
            /* Not an error — user hasn't authorized yet */
            json_free(root);
            http_free(h);
            _set_error("pending");
            return NULL;
        }
        const char *ed = json_get_str(root, "error_description", "");
        _set_error("Device code poll error: %s — %s", err_field, ed);
        json_free(root);
        http_free(h);
        return NULL;
    }

    http_free(h);

    /* Parse token response */
    oauth_token_t *tok = _parse_token_response_helper(root);
    json_free(root);

    if (!tok) {
        /* _parse_token_response_helper sets last_error */
        return NULL;
    }

    return tok;
}

/* Internal helper: parse token from already-parsed JSON root.
 * Same as _parse_token_response but takes json_t* instead of string. */
static oauth_token_t *_parse_token_response_helper(json_t *root) {
    if (!root || root->type != JSON_OBJECT) {
        _set_error("Token response is not a JSON object");
        return NULL;
    }

    /* Check for error */
    const char *err_str = json_get_str(root, "error", NULL);
    if (err_str) {
        const char *ed = json_get_str(root, "error_description", "");
        _set_error("OAuth error: %s — %s", err_str, ed);
        return NULL;
    }

    oauth_token_t *tok = (oauth_token_t *)calloc(1, sizeof(oauth_token_t));
    if (!tok) {
        _set_error("Out of memory");
        return NULL;
    }

    const char *at = json_get_str(root, "access_token", "");
    tok->access_token = at[0] ? strdup(at) : NULL;

    const char *rt = json_get_str(root, "refresh_token", "");
    tok->refresh_token = rt[0] ? strdup(rt) : NULL;

    const char *it = json_get_str(root, "id_token", "");
    tok->id_token = it[0] ? strdup(it) : NULL;

    const char *tt = json_get_str(root, "token_type", "Bearer");
    tok->token_type = strdup(tt);

    tok->expires_in = (int)json_get_num(root, "expires_in", 0);

    if (tok->expires_in > 0) {
        time_t now = time(NULL);
        tok->expires_at = (double)(now + tok->expires_in);
    } else {
        tok->expires_at = 0.0;
    }

    if (!tok->access_token) {
        oauth_token_free(tok);
        _set_error("Token response missing access_token");
        return NULL;
    }

    return tok;
}

/* ================================================================
 *  Nous Portal device code login
 * ================================================================ */

oauth_token_t *nous_device_code_login(int timeout_sec) {
    /* Step 1: Request device code */
    printf("\n🔐 Nous Portal Device Code Login\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    oauth_device_code_t *dc = oauth_device_code_request(
        NOUS_OAUTH_DEVICE_ENDPOINT,
        DEFAULT_NOUS_CLIENT_ID,
        NOUS_OAUTH_SCOPE,
        timeout_sec);

    if (!dc) {
        printf("❌ Failed to start device code flow: %s\n", oauth_last_error());
        return NULL;
    }

    /* Step 2: Display instructions */
    printf("\n📋 Authorization Required\n\n");
    if (dc->verification_uri_complete && dc->verification_uri_complete[0]) {
        printf("   %s\n\n", dc->verification_uri_complete);
    } else {
        printf("   1. Open: %s\n", dc->verification_uri ? dc->verification_uri : "(unknown)");
        printf("   2. Enter code: %s\n", dc->user_code ? dc->user_code : "(unknown)");
    }
    printf("\n   Your code: %s\n", dc->user_code ? dc->user_code : "???");
    printf("\n⏳ Waiting for authorization ");
    fflush(stdout);

    /* Step 3: Poll for token */
    int max_attempts = dc->expires_in > 0 ?
        (dc->expires_in / (dc->interval > 0 ? dc->interval : 5)) : 120;
    int poll_interval = dc->interval > 0 ? dc->interval : 5;

    oauth_token_t *tok = NULL;
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        /* Wait polling interval */
        sleep(poll_interval);

        printf(". ");
        fflush(stdout);

        tok = oauth_device_code_poll(
            NOUS_OAUTH_TOKEN_ENDPOINT,
            DEFAULT_NOUS_CLIENT_ID,
            dc->device_code,
            timeout_sec);

        if (tok) {
            printf("✅\n");
            break;
        }

        const char *err = oauth_last_error();
        if (err && strcmp(err, "pending") != 0) {
            /* Real error, not just pending */
            printf("\n❌ Poll failed: %s\n", err);
            break;
        }
    }

    oauth_device_code_free(dc);

    if (!tok) {
        const char *err = oauth_last_error();
        if (err && strcmp(err, "pending") == 0) {
            printf("\n⏰ Timed out waiting for authorization.\n");
        } else if (err) {
            printf("\n❌ %s\n", err);
        } else {
            printf("\n❌ Authorization failed.\n");
        }
        return NULL;
    }

    return tok;
}

/* ================================================================
 *  Token refresh
 * ================================================================ */

oauth_token_t *oauth_refresh_token(
    const char *token_endpoint,
    const char *client_id,
    const char *refresh_token,
    int timeout_sec)
{
    if (!token_endpoint || !token_endpoint[0] ||
        !client_id || !client_id[0] ||
        !refresh_token || !refresh_token[0]) {
        _set_error("token_endpoint, client_id, and refresh_token are required");
        return NULL;
    }

    /* Build body: grant_type=refresh_token */
    char body[2048];
    size_t pos = 0;
    if (_append_form_field(body, sizeof(body), &pos, "grant_type", "refresh_token") < 0) {
        _set_error("Failed to build refresh body");
        return NULL;
    }
    if (_append_form_field(body, sizeof(body), &pos, "refresh_token", refresh_token) < 0) {
        _set_error("Failed to build refresh body (refresh_token)");
        return NULL;
    }
    if (_append_form_field(body, sizeof(body), &pos, "client_id", client_id) < 0) {
        _set_error("Failed to build refresh body (client_id)");
        return NULL;
    }

    char headers[256];
    snprintf(headers, sizeof(headers),
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Accept: application/json\r\n");

    if (timeout_sec < 20) timeout_sec = 20;

    http_t *h = http_new(timeout_sec);
    if (!h) {
        _set_error("Failed to create HTTP client");
        return NULL;
    }

    http_resp_t *resp = http_request(h, HTTP_POST, token_endpoint,
                                     headers, body, strlen(body));
    if (!resp) {
        _set_error("Token refresh request failed (connection error)");
        http_free(h);
        return NULL;
    }

    if (resp->status != 200) {
        char eb[512] = {0};
        if (resp->body && resp->body_len > 0) {
            size_t cp = resp->body_len < 511 ? resp->body_len : 511;
            memcpy(eb, resp->body, cp);
            eb[cp] = '\0';
        }
        _set_error("Token refresh failed: HTTP %d — %s", resp->status, eb);
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    if (!resp->body || resp->body_len == 0) {
        _set_error("Token refresh returned empty response body");
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    oauth_token_t *tok = _parse_token_response(resp->body);
    http_resp_free(resp);
    http_free(h);

    return tok;
}

/* External crypto functions used by PKCE flow */
extern char *crypto_pkce_verifier(void);
extern char *crypto_pkce_challenge(const char *code_verifier);
extern bool crypto_random_bytes(unsigned char *buf, size_t len);

/* ================================================================
 *  xAI OAuth callback server constants
 * ================================================================ */

#define XAI_OAUTH_AUTH_ENDPOINT     "https://auth.x.ai/oauth2/authorize"
#define XAI_OAUTH_SCOPE             "openid profile email offline_access grok-cli:access api:access"
#define XAI_OAUTH_REDIRECT_HOST     "127.0.0.1"
#define XAI_OAUTH_REDIRECT_PORT     56121
#define XAI_OAUTH_REDIRECT_PATH     "/callback"

/* ================================================================
 *  Build xAI OAuth authorize URL
 * ================================================================ */

static char *_xai_build_authorize_url(const char *redirect_uri,
                                       const char *code_challenge,
                                       const char *state,
                                       const char *nonce) {
    char *ruri = http_url_encode(redirect_uri);
    char *cch = http_url_encode(code_challenge);
    char *st  = http_url_encode(state);
    char *non = http_url_encode(nonce);
    if (!ruri || !cch || !st || !non) {
        free(ruri); free(cch); free(st); free(non);
        return NULL;
    }

    char url[4096];
    int n = snprintf(url, sizeof(url),
        "%s?response_type=code"
        "&client_id=%s"
        "&redirect_uri=%s"
        "&scope=%s"
        "&code_challenge=%s"
        "&code_challenge_method=S256"
        "&state=%s"
        "&nonce=%s"
        "&plan=generic"
        "&referrer=hermes-agent",
        XAI_OAUTH_AUTH_ENDPOINT,
        XAI_OAUTH_CLIENT_ID,
        ruri, XAI_OAUTH_SCOPE, cch, st, non);

    free(ruri); free(cch); free(st); free(non);

    if (n < 0 || (size_t)n >= sizeof(url)) return NULL;
    return strdup(url);
}

/* Simple URL percent-decode in-place (replaces %XX with actual byte) */
static char *_simple_url_decode(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '%' && i + 2 < len) {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            char *end = NULL;
            long val = strtol(hex, &end, 16);
            if (end && *end == '\0') {
                out[o++] = (char)val;
                i += 2;
                continue;
            }
        }
        out[o++] = src[i];
    }
    out[o] = '\0';
    return out;
}

/* ================================================================
 *  Parse xAI OAuth callback HTTP GET
 * ================================================================ */

static int _parse_xai_callback(int fd,
                                char **out_code,
                                char **out_state,
                                char **out_error) {
    char buf[8192];
    size_t pos = 0;
    struct timeval tv = { .tv_sec = 120, .tv_usec = 0 };

    while (pos < sizeof(buf) - 1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret <= 0) return -1;
        ssize_t n = read(fd, buf + pos, sizeof(buf) - 1 - pos);
        if (n <= 0) return -1;
        pos += (size_t)n;
        buf[pos] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }
    buf[pos] = '\0';

    char *method = buf;
    char *path_start = method;
    while (*path_start && *path_start != ' ') path_start++;
    if (!*path_start) return -1;
    *path_start = '\0';
    path_start++;
    if (strcmp(method, "GET") != 0) return -1;

    char *http_ver = path_start;
    while (*http_ver && *http_ver != ' ') http_ver++;
    if (*http_ver) *http_ver = '\0';

    char *qmark = strchr(path_start, '?');
    if (!qmark) return -1;
    *qmark = '\0';
    char *query = qmark + 1;

    if (strcmp(path_start, XAI_OAUTH_REDIRECT_PATH) != 0) return -1;

    char *code = NULL, *state = NULL, *error = NULL;
    char *tok = strtok(query, "&");
    while (tok) {
        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            char *val = eq + 1;
            char *decoded = _simple_url_decode(val);
            if (!decoded) { tok = strtok(NULL, "&"); continue; }
            if (strcmp(tok, "code") == 0) code = strdup(decoded);
            else if (strcmp(tok, "state") == 0) state = strdup(decoded);
            else if (strcmp(tok, "error") == 0) error = strdup(decoded);
            free(decoded);
        }
        tok = strtok(NULL, "&");
    }

    if (code || error) {
        *out_code = code;
        *out_state = state;
        *out_error = error;
        return 0;
    }
    free(code); free(state); free(error);
    return -1;
}

/* ================================================================
 *  Send success HTML response
 * ================================================================ */

static void _send_callback_response(int fd) {
    const char *body =
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<title>Hermes xAI OAuth</title>"
        "<style>body{font-family:sans-serif;display:flex;"
        "justify-content:center;align-items:center;height:100vh;"
        "margin:0;background:#1a1a2e;color:#e0e0e0}"
        ".card{text-align:center;padding:2em;border-radius:12px;"
        "background:#16213e;box-shadow:0 4px 24px rgba(0,0,0,0.3)}"
        ".check{font-size:3em;color:#8FBC8F}</style></head>"
        "<body><div class='card'><div class='check'>&#10003;</div>"
        "<h2>Authorization Received</h2>"
        "<p>You can close this window and return to Hermes.</p>"
        "</div></body></html>";

    char resp[4096];
    int n = snprintf(resp, sizeof(resp),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n%s",
        strlen(body), body);

    if (n > 0 && (size_t)n < sizeof(resp))
        write(fd, resp, (size_t)n);
}

/* ================================================================
 *  xAI OAuth loopback callback login
 * ================================================================ */

oauth_token_t *xai_oauth_callback_login(int timeout_sec) {
    if (timeout_sec < 30) timeout_sec = 30;

    printf("\n=== xAI Grok OAuth (SuperGrok / Premium+) ===\n");
    printf("========================================\n");
    printf("(Hermes creates its own local OAuth session)\n\n");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        _set_error("Failed to create callback server socket");
        return NULL;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(XAI_OAUTH_REDIRECT_HOST);

    int actual_port = 0;
    int ports[] = { XAI_OAUTH_REDIRECT_PORT, 0 };
    for (int i = 0; i < 2; i++) {
        addr.sin_port = htons((uint16_t)ports[i]);
        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            actual_port = ports[i];
            break;
        }
    }

    if (actual_port == 0) {
        _set_error("Could not bind callback server on 127.0.0.1");
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 1) < 0) {
        _set_error("listen failed on callback server");
        close(server_fd);
        return NULL;
    }

    /* Get actual port for ephemeral */
    if (actual_port == 0) {
        struct sockaddr_in bound;
        socklen_t bound_len = sizeof(bound);
        if (getsockname(server_fd, (struct sockaddr*)&bound, &bound_len) == 0)
            actual_port = ntohs(bound.sin_port);
        else
            actual_port = XAI_OAUTH_REDIRECT_PORT;
    }

    char redirect_uri[128];
    snprintf(redirect_uri, sizeof(redirect_uri),
             "http://%s:%d%s",
             XAI_OAUTH_REDIRECT_HOST, actual_port, XAI_OAUTH_REDIRECT_PATH);

    char *code_verifier = crypto_pkce_verifier();
    if (!code_verifier) {
        _set_error("Failed to generate PKCE verifier");
        close(server_fd);
        return NULL;
    }

    char *code_challenge = crypto_pkce_challenge(code_verifier);
    if (!code_challenge) {
        _set_error("Failed to generate PKCE challenge");
        free(code_verifier);
        close(server_fd);
        return NULL;
    }

    /* Generate random state and nonce */
    char state[65] = {0}, nonce[65] = {0};
    unsigned char rand_buf[32];
    if (!crypto_random_bytes(rand_buf, 32)) {
        _set_error("Failed to generate random bytes");
        free(code_verifier); free(code_challenge);
        close(server_fd);
        return NULL;
    }
    for (int i = 0; i < 32; i++)
        snprintf(state + i*2, 3, "%02x", rand_buf[i]);

    if (!crypto_random_bytes(rand_buf, 32)) {
        _set_error("Failed to generate random bytes");
        free(code_verifier); free(code_challenge);
        close(server_fd);
        return NULL;
    }
    for (int i = 0; i < 32; i++)
        snprintf(nonce + i*2, 3, "%02x", rand_buf[i]);

    char *auth_url = _xai_build_authorize_url(
        redirect_uri, code_challenge, state, nonce);
    if (!auth_url) {
        _set_error("Failed to build authorize URL");
        free(code_verifier); free(code_challenge);
        close(server_fd);
        return NULL;
    }

    printf("Open this URL to authorize Hermes with xAI:\n");
    printf("%s\n\n", auth_url);
    printf("Waiting for callback on %s\n", redirect_uri);
    printf("(timeout: %d seconds)\n\n", timeout_sec);
    fflush(stdout);

    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    oauth_token_t *tok = NULL;
    char *code = NULL, *cb_state = NULL, *cb_error = NULL;
    int client_fd = -1;

    time_t deadline = time(NULL) + timeout_sec;
    while (time(NULL) < deadline) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        client_fd = accept(server_fd,
                          (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd >= 0) break;
        if (errno != EAGAIN && errno != EWOULDBLOCK) break;
        usleep(100000);
    }

    if (client_fd < 0) {
        _set_error("xAI callback server timed out waiting for browser redirect");
        free(auth_url); free(code_verifier); free(code_challenge);
        close(server_fd);
        return NULL;
    }

    if (_parse_xai_callback(client_fd, &code, &cb_state, &cb_error) != 0) {
        _set_error("Failed to parse OAuth callback request");
        free(auth_url); free(code_verifier); free(code_challenge);
        close(client_fd); close(server_fd);
        return NULL;
    }

    _send_callback_response(client_fd);
    close(client_fd);
    close(server_fd);

    if (cb_error) {
        _set_error("xAI authorization failed: %s", cb_error);
        free(cb_error); free(code); free(cb_state);
        free(auth_url); free(code_verifier); free(code_challenge);
        return NULL;
    }

    if (cb_state && strcmp(cb_state, state) != 0) {
        _set_error("xAI authorization failed: state mismatch");
        free(cb_state); free(code); free(cb_error);
        free(auth_url); free(code_verifier); free(code_challenge);
        return NULL;
    }

    if (!code || !code[0]) {
        _set_error("xAI authorization failed: missing authorization code");
        free(cb_state); free(code); free(cb_error);
        free(auth_url); free(code_verifier); free(code_challenge);
        return NULL;
    }

    tok = xai_oauth_exchange(code, redirect_uri,
                              code_verifier, code_challenge,
                              timeout_sec);

    free(code); free(cb_state); free(cb_error);
    free(auth_url); free(code_verifier); free(code_challenge);

    if (!tok) return NULL;

    printf("\n✅ Authorization received and tokens exchanged.\n");
    return tok;
}
