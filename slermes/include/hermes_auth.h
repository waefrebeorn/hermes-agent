#ifndef HERMES_AUTH_H
#define HERMES_AUTH_H

/*
 * hermes_auth.h — OAuth token exchange for Hermes C.
 *
 * Supports PKCE (RFC 7636) authorization code exchange for OAuth 2.0
 * providers: xAI, MiniMax, Nous Portal, Anthropic, Spotify.
 * Uses OpenSSL + POSIX sockets for HTTPS. No libcurl dependency.
 *
 * Token state is stored in ~/.hermes/auth.json with file locking.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Constants
 * ================================================================ */

/* xAI OAuth */
#define XAI_OAUTH_CLIENT_ID     "hermes-cli"
#define XAI_OAUTH_BASE_URL      "https://api.x.ai/v1"

/* MiniMax OAuth */
#define MINIMAX_OAUTH_CLIENT_ID "78257093-7e40-4613-99e0-527b14b39113"
#define MINIMAX_OAUTH_SCOPE     "group_id profile model.completion"

/* Nous Portal defaults */
#define DEFAULT_NOUS_PORTAL_URL     "https://portal.nousresearch.com"
#define DEFAULT_NOUS_INFERENCE_URL  "https://inference-api.nousresearch.com/v1"
#define DEFAULT_NOUS_CLIENT_ID      "hermes-cli"

/* Nous OAuth device code endpoints (RFC 8628) */
#define NOUS_OAUTH_DEVICE_ENDPOINT  "https://portal.nousresearch.com/oauth/device/code"
#define NOUS_OAUTH_TOKEN_ENDPOINT   "https://portal.nousresearch.com/oauth/token"
#define NOUS_OAUTH_SCOPE            "openid profile email offline_access"

/* ================================================================
 *  Token data structures
 * ================================================================ */

/* OAuth token payload */
typedef struct {
    char *access_token;     /* Bearer token for API calls */
    char *refresh_token;    /* Long-lived refresh token (may be NULL) */
    char *id_token;         /* OpenID Connect ID token (may be NULL) */
    char *token_type;       /* "Bearer" typically */
    double expires_at;      /* Unix timestamp when access_token expires (0 = unknown) */
    int    expires_in;      /* Seconds until expiry (from server) */
} oauth_token_t;

/* Auth store entry for one provider */
typedef struct {
    char provider[64];      /* "xai-oauth", "minimax-oauth", "nous", etc. */
    oauth_token_t token;
} auth_entry_t;

/* ================================================================
 *  PKCE primitives (declared in hermes_crypto.h, summaries here)
 * ================================================================ */

/* crypto_pkce_verifier() — generate RFC 7636 code verifier */
/* crypto_pkce_challenge(v) — compute S256 challenge from verifier */

/* ================================================================
 *  Token Exchange API
 * ================================================================ */

/*
 * Exchange an authorization code for tokens via PKCE (RFC 7636).
 *
 * POSTs to token_endpoint with form-urlencoded body:
 *   grant_type=authorization_code
 *   code=<code>
 *   redirect_uri=<redirect_uri>
 *   client_id=<client_id>
 *   code_verifier=<code_verifier>
 *   [code_challenge=<code_challenge>]   (defense-in-depth for xAI #26990)
 *   [code_challenge_method=S256]
 *
 * Parameters:
 *   token_endpoint  — OAuth token URL (e.g. "https://auth.x.ai/oauth2/token")
 *   code            — authorization code from redirect
 *   redirect_uri    — redirect URI used in authorize step
 *   client_id       — OAuth client ID
 *   code_verifier   — PKCE verifier from authorize step (RFC 7636 §4.1)
 *   code_challenge  — PKCE challenge S256 (may be NULL/empty; some providers
 *                     re-validate at token endpoint — xAI #26990)
 *   timeout_sec     — HTTP timeout (minimum 20s enforced internally)
 *
 * Returns:
 *   malloc'd oauth_token_t on success (caller must free with oauth_token_free())
 *   NULL on failure (sets last_error with human-readable message)
 */
oauth_token_t *oauth_exchange_code(
    const char *token_endpoint,
    const char *code,
    const char *redirect_uri,
    const char *client_id,
    const char *code_verifier,
    const char *code_challenge,
    int timeout_sec
);

/* Free an oauth_token_t returned by oauth_exchange_code() */
void oauth_token_free(oauth_token_t *tok);

/* ================================================================
 *  xAI-specific convenience
 * ================================================================ */

/* Exchange code for tokens using xAI's OAuth endpoint. */
oauth_token_t *xai_oauth_exchange(
    const char *code,
    const char *redirect_uri,
    const char *code_verifier,
    const char *code_challenge,
    int timeout_sec
);

/* ================================================================
 *  Auth store (auth.json read/write)
 * ================================================================ */

/* Load auth entries from ~/.hermes/auth.json. Returns NULL-terminated array. */
auth_entry_t *auth_store_load(const char *hermes_home, int *count);

/* Save a single auth entry to auth.json (merges with existing). */
bool auth_store_save(const char *hermes_home, const auth_entry_t *entry);

/* Remove an auth entry by provider name. */
bool auth_store_remove(const char *hermes_home, const char *provider);

/* Free auth entry array */
void auth_store_free(auth_entry_t *entries, int count);

/* ================================================================
 *  Device code flow (RFC 8628)
 * ================================================================ */

/* Device code response from RFC 8628 device authorization endpoint */
typedef struct {
    char *device_code;          /* Device code for polling token endpoint */
    char *user_code;            /* User-facing code to display */
    char *verification_uri;     /* URL user visits in browser */
    char *verification_uri_complete; /* Full URL with user code embedded */
    int   interval;             /* Polling interval (seconds) */
    int   expires_in;           /* Seconds until device code expires */
} oauth_device_code_t;

/* Request a device code from OAuth provider (RFC 8628 §3.1).
 * Posts to device_endpoint with client_id + scope.
 * Returns malloc'd device_code_t (caller must free with oauth_device_code_free()).
 * Returns NULL on failure (check oauth_last_error()). */
oauth_device_code_t *oauth_device_code_request(
    const char *device_endpoint,
    const char *client_id,
    const char *scope,
    int timeout_sec);

/* Free a device code response */
void oauth_device_code_free(oauth_device_code_t *dc);

/* Poll token endpoint with device code (RFC 8628 §3.4).
 * Posts grant_type=urn:ietf:params:oauth:grant-type:device_code.
 * Handles authorization_pending (returns NULL, sets last_error).
 * Returns malloc'd oauth_token_t on success.
 * Returns NULL on failure (check oauth_last_error()). */
oauth_token_t *oauth_device_code_poll(
    const char *token_endpoint,
    const char *client_id,
    const char *device_code,
    int timeout_sec);

/* Full device code login for Nous Portal.
 * Orchestrates: request device code → display URI/code → poll → return token.
 * Displays instructions to stdout during flow.
 * Returns malloc'd oauth_token_t on success.
 * Returns NULL on failure (check oauth_last_error()). */
oauth_token_t *nous_device_code_login(int timeout_sec);

/* OAuth token refresh.
 * POSTs to token_endpoint with grant_type=refresh_token.
 * Returns malloc'd oauth_token_t on success.
 * Returns NULL on failure (check oauth_last_error()). */
oauth_token_t *oauth_refresh_token(
    const char *token_endpoint,
    const char *client_id,
    const char *refresh_token,
    int timeout_sec);

/* ================================================================
 *  Last error
 * ================================================================ */

/* Get last error message (thread-local, valid until next call) */
const char *oauth_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_AUTH_H */
