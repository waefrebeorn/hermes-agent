#ifndef LIBMCP_OAUTH_H
#define LIBMCP_OAUTH_H

/*
 * libmcp_oauth.h — MCP OAuth 2.1 Client Support.
 * Port of Python tools/mcp_oauth.py.
 *
 * Implements the glue layer for OAuth 2.1 authorization code flow with PKCE
 * for MCP servers: token storage, callback server, metadata discovery,
 * token exchange/refresh, PKCE challenge generation.
 *
 * The caller (mcp_tool.c / mcp_server) wires the resulting access token
 * into mcp_server_set_headers() as "Authorization: Bearer <token>".
 *
 * File layout under HERMES_HOME:
 *   mcp-tokens/<server_name>.json         -- tokens
 *   mcp-tokens/<server_name>.client.json   -- client info
 *   mcp-tokens/<server_name>.meta.json     -- OAuth server metadata
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Token storage
 * ================================================================ */

typedef struct mcp_oauth_storage mcp_oauth_storage_t;

/* Create storage for a server. server_name is sanitized for filename use. */
mcp_oauth_storage_t *mcp_oauth_storage_new(const char *server_name);

/* Free storage handle */
void mcp_oauth_storage_free(mcp_oauth_storage_t *s);

/* Check if tokens exist on disk (may be expired) */
bool mcp_oauth_storage_has_tokens(mcp_oauth_storage_t *s);

/* Get tokens JSON. Returns malloc'd string, caller free()s. NULL on absence/error. */
char *mcp_oauth_storage_get_tokens(mcp_oauth_storage_t *s);

/* Set tokens from JSON string. Overwrites existing. */
bool mcp_oauth_storage_set_tokens(mcp_oauth_storage_t *s, const char *json);

/* Get client info JSON. Returns malloc'd string, caller free()s. */
char *mcp_oauth_storage_get_client_info(mcp_oauth_storage_t *s);

/* Set client info from JSON string. */
bool mcp_oauth_storage_set_client_info(mcp_oauth_storage_t *s, const char *json);

/* Get OAuth metadata JSON. Returns malloc'd string, caller free()s. */
char *mcp_oauth_storage_get_metadata(mcp_oauth_storage_t *s);

/* Set OAuth metadata from JSON string. */
bool mcp_oauth_storage_set_metadata(mcp_oauth_storage_t *s, const char *json);

/* Delete all stored OAuth state for this server */
void mcp_oauth_storage_remove(mcp_oauth_storage_t *s);

/* ================================================================
 *  PKCE helpers
 * ================================================================ */

/* Generate a cryptographically random code_verifier (43-128 chars).
 * Returns malloc'd string, caller free()s. */
char *mcp_oauth_generate_code_verifier(void);

/* Compute PKCE code_challenge = base64url(SHA256(code_verifier)).
 * Returns malloc'd string, caller free()s. */
char *mcp_oauth_code_challenge(const char *code_verifier);

/* ================================================================
 *  Callback server (ephemeral localhost HTTP)
 * ================================================================ */

/* Find an available TCP port on localhost. Returns 0 on error. */
int mcp_oauth_find_free_port(void);

/*
 * Start a callback HTTP server on localhost:port, wait for the OAuth
 * redirect, capture the authorization code.
 *
 * Port: the localhost port to listen on.
 * Timeout_sec: max seconds to wait (default 300).
 * Out_code: set to malloc'd authorization code string on success, or NULL.
 * Out_state: set to malloc'd state string on success, or NULL.
 * Out_error: set to malloc'd error string on auth failure, or NULL.
 *
 * Returns true if code was received. All out params are malloc'd.
 */
bool mcp_oauth_wait_for_callback(int port, int timeout_sec,
                                  char **out_code, char **out_state,
                                  char **out_error);

/* ================================================================
 *  Metadata discovery
 * ================================================================ */

/*
 * Discover OAuth metadata from an MCP server's well-known endpoint.
 * server_url: The MCP server base URL (e.g. "https://mcp.example.com/mcp").
 * Returns malloc'd JSON string with metadata, or NULL on failure.
 * Caller free()s.
 */
char *mcp_oauth_discover_metadata(const char *server_url);

/* ================================================================
 *  Token exchange / refresh
 * ================================================================ */

/*
 * Exchange an authorization code for tokens.
 * Returns malloc'd JSON string with {access_token, token_type, expires_in,
 * refresh_token?, scope?}, or NULL on failure. Caller free()s.
 */
char *mcp_oauth_exchange_code(const char *token_url,
                               const char *client_id,
                               const char *code,
                               const char *code_verifier,
                               const char *redirect_uri);

/*
 * Refresh an access token.
 * Returns malloc'd JSON with new tokens, or NULL on failure. Caller free()s.
 */
char *mcp_oauth_refresh_token(const char *token_url,
                               const char *client_id,
                               const char *refresh_token);

/* ================================================================
 *  Browser helpers
 * ================================================================ */

/* Check if we can open a browser (has DISPLAY/WAYLAND_DISPLAY, not SSH) */
bool mcp_oauth_can_open_browser(void);

/* Open a URL in the system browser. Returns true if successful. */
bool mcp_oauth_open_browser(const char *url);

/* ================================================================
 *  High-level API
 * ================================================================ */

/*
 * Build OAuth authorization for an MCP server.
 * oauth_config_json: JSON string with OAuth config (client_id, client_name,
 *                    scope, redirect_port, etc.). May be empty.
 * Returns malloc'd JSON with result. Caller free()s.
 * On success: {"success":true, "access_token":"...", "token_type":"...",
 *              "expires_in":3600}
 * On failure: {"success":false, "error":"..."}
 *
 * NOTE: This is a simplified C replacement for Python's build_oauth_auth().
 * It does ONE complete OAuth flow (discovery → callback → exchange) and
 * returns the resulting access token. The caller (MCP transport) should
 * cache and refresh the token.
 */
char *mcp_oauth_build_auth(const char *server_name,
                            const char *server_url,
                            const char *oauth_config_json);

/* Remove stored OAuth tokens for a server */
bool mcp_oauth_remove_tokens(const char *server_name);

#ifdef __cplusplus
}
#endif

#endif /* LIBMCP_OAUTH_H */
