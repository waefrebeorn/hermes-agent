#ifndef HERMES_MS_GRAPH_H
#define HERMES_MS_GRAPH_H

/*
 * ms_graph.h — Microsoft Graph REST client for Hermes C.
 * Port of Python tools/microsoft_graph_auth.py + microsoft_graph_client.py.
 *
 * OAuth2 app-only auth + REST client for Microsoft Graph API.
 * Config via env vars: MSGRAPH_TENANT_ID, MSGRAPH_CLIENT_ID, MSGRAPH_CLIENT_SECRET.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ── */

#define MSGRAPH_MAX_URL      4096
#define MSGRAPH_MAX_TOKEN    8192
#define MSGRAPH_MAX_ERROR    1024
#define MSGRAPH_DEFAULT_SCOPE  "https://graph.microsoft.com/.default"
#define MSGRAPH_DEFAULT_AUTH  "https://login.microsoftonline.com"
#define MSGRAPH_DEFAULT_BASE  "https://graph.microsoft.com/v1.0"
#define MSGRAPH_TOKEN_SKEW    120        /* refresh token 120s early */
#define MSGRAPH_DEFAULT_TIMEOUT 60
#define MSGRAPH_MAX_RETRIES  3
#define MSGRAPH_USER_AGENT   "Hermes-Agent/msgraph-c"

/* ── Types ── */

/** Microsoft Graph app-only credentials (from env vars) */
typedef struct {
    char tenant_id[256];
    char client_id[256];
    char client_secret[512];
    char scope[512];
    char authority_url[512];
    char token_url[1024];
} msgraph_credentials_t;

/** Cached access token */
typedef struct {
    char access_token[MSGRAPH_MAX_TOKEN];
    double expires_at;        /* time_t seconds */
    char token_type[32];
} msgraph_cached_token_t;

/** Microsoft Graph token provider */
typedef struct {
    msgraph_credentials_t credentials;
    msgraph_cached_token_t cached;
    bool has_cached;
    double timeout_seconds;
    int skew_seconds;
} msgraph_token_provider_t;

/** Microsoft Graph REST client */
typedef struct {
    msgraph_token_provider_t *provider;
    char base_url[MSGRAPH_MAX_URL];
    double timeout_seconds;
    int max_retries;
    char user_agent[128];
} msgraph_client_t;

/** Graph API error */
typedef struct {
    int status_code;
    char method[8];
    char url[MSGRAPH_MAX_URL];
    char message[MSGRAPH_MAX_ERROR];
    double retry_after_seconds;
} msgraph_error_t;

/** Download result */
typedef struct {
    char path[MSGRAPH_MAX_URL];
    long size_bytes;
    char content_type[128];
} msgraph_download_result_t;

/* ── Credentials API ── */

/** Load Graph credentials from environment variables. Returns false if missing. */
bool msgraph_credentials_from_env(msgraph_credentials_t *creds, bool required);

/** Build token URL from credentials */
void msgraph_credentials_build_token_url(msgraph_credentials_t *creds);

/* ── Token Provider API ── */

/** Initialize a token provider with credentials */
void msgraph_token_provider_init(msgraph_token_provider_t *tp,
                                  const msgraph_credentials_t *creds);

/** Get an access token (cached or fresh). Returns NULL on error. */
const char *msgraph_get_access_token(msgraph_token_provider_t *tp,
                                      char *out, size_t out_size);

/** Force refresh on next call */
void msgraph_clear_cache(msgraph_token_provider_t *tp);

/** Check if cached token is expired */
bool msgraph_token_is_expired(const msgraph_cached_token_t *token,
                               int skew_seconds);

/* ── Graph Client API ── */

/** Initialize a Graph REST client */
void msgraph_client_init(msgraph_client_t *client,
                          msgraph_token_provider_t *provider);

/**
 * Perform a GET request to the Graph API.
 * Returns JSON string (caller must free), or NULL on error.
 * On error, populates err if non-NULL.
 */
char *msgraph_get(msgraph_client_t *client, const char *path,
                   msgraph_error_t *err);

/**
 * Perform a POST request to the Graph API.
 * body_json is the JSON body string, or NULL for no body.
 * Returns JSON string (caller must free), or NULL on error.
 */
char *msgraph_post(msgraph_client_t *client, const char *path,
                    const char *body_json, msgraph_error_t *err);

/**
 * Perform a PATCH request.
 * Returns JSON string or NULL.
 */
char *msgraph_patch(msgraph_client_t *client, const char *path,
                     const char *body_json, msgraph_error_t *err);

/**
 * Perform a DELETE request.
 * Returns JSON string or NULL.
 */
char *msgraph_delete(msgraph_client_t *client, const char *path,
                      msgraph_error_t *err);

/**
 * Download a Graph resource to a local file.
 * Downloads by streaming the response to disk.
 */
bool msgraph_download(msgraph_client_t *client, const char *path,
                       const char *destination,
                       msgraph_download_result_t *result,
                       msgraph_error_t *err);

/* ── Utility ── */

/** Free an error's allocated resources (no-op for struct-based impl) */
void msgraph_error_cleanup(msgraph_error_t *err);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MS_GRAPH_H */
