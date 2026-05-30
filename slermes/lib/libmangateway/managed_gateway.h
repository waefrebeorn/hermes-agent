#ifndef HERMES_MANAGED_GATEWAY_H
#define HERMES_MANAGED_GATEWAY_H

/*
 * managed_gateway.h — Generic managed-tool gateway helpers for
 * Nous-hosted vendor passthroughs.
 * Port of Python tools/managed_tool_gateway.py.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/** Max gateway URL length */
#define GW_URL_MAX 512

/** Max vendor name length */
#define GW_VENDOR_MAX 64

/** Max auth token length */
#define GW_TOKEN_MAX 4096

/**
 * Resolved managed-tool gateway configuration.
 */
typedef struct {
    char vendor[GW_VENDOR_MAX];    /**< Vendor name (e.g. "fal") */
    char gateway_origin[GW_URL_MAX]; /**< Gateway URL */
    char nous_user_token[GW_TOKEN_MAX]; /**< Nous OAuth access token */
    bool managed_mode;              /**< True when gateway mode is active */
} managed_gateway_config_t;

/**
 * Build the path to the Hermes auth store (auth.json within hermes_home).
 * Returns the full path in buf.
 */
void managed_gw_auth_json_path(const char *hermes_home, char *buf, size_t sz);

/**
 * Read the Nous Subscriber OAuth access token.
 *
 * Priority:
 * 1. TOOL_GATEWAY_USER_TOKEN env var
 * 2. auth.json -> providers.nous.access_token (if not expiring)
 * 3. auth.json cached token (even if expiring, as fallback)
 *
 * @param hermes_home  Path to SLERMES_HOME for auth.json lookup.
 * @param buf          Output buffer for the token.
 * @param sz           Buffer size.
 * @return             true if a token was resolved (buf is set).
 */
bool managed_gw_read_access_token(const char *hermes_home,
                                   char *buf, size_t sz);

/**
 * Get the configured gateway URL scheme (http or https).
 * Reads TOOL_GATEWAY_SCHEME env var; defaults to "https".
 */
const char *managed_gw_get_scheme(void);

/**
 * Build the gateway URL for a specific vendor.
 *
 * Resolution order:
 * 1. <VENDOR>_GATEWAY_URL env var (e.g. FAL_GATEWAY_URL)
 * 2. TOOL_GATEWAY_DOMAIN env var -> scheme://vendor-gateway.<domain>
 * 3. Default domain (nousresearch.com)
 *
 * @param vendor  Vendor name (e.g. "fal", "browser").
 * @param buf     Output buffer.
 * @param sz      Buffer size.
 */
void managed_gw_build_url(const char *vendor, char *buf, size_t sz);

/**
 * Resolve managed-tool gateway config for a vendor.
 *
 * Returns true when gateway is available, populates config.
 * Returns false when managed tools are disabled or gateway
 * cannot be configured (no token, no URL).
 *
 * @param hermes_home  Path to SLERMES_HOME.
 * @param vendor       Vendor name.
 * @param config       Output config (only set when return is true).
 * @return             true if gateway is ready.
 */
bool managed_gw_resolve(const char *hermes_home,
                         const char *vendor,
                         managed_gateway_config_t *config);

/**
 * Quick check: is the managed tool gateway ready for a vendor?
 * Returns true when gateway URL and Nous access token are available.
 */
bool managed_gw_is_ready(const char *hermes_home, const char *vendor);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MANAGED_GATEWAY_H */
