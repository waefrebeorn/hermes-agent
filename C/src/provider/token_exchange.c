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
    if (resp->status_code != 200) {
        char err_body[512] = {0};
        if (resp->body && resp->body_len > 0) {
            size_t copy = resp->body_len < 511 ? resp->body_len : 511;
            memcpy(err_body, resp->body, copy);
            err_body[copy] = '\0';
        }
        _set_error("Token exchange failed: HTTP %d — %s",
                   resp->status_code, err_body);
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
    int count = (int)root->collection.count;
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
        const char *provider = root->collection.keys[i];
        json_node_t *node = root->collection.items[i];
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
