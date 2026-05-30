#ifndef HERMES_URL_SAFETY_H
#define HERMES_URL_SAFETY_H

/*
 * url_safety.h — SSRF protection and URL safety checks for Hermes C.
 * Mirrors Python tools/url_safety.py: DNS resolution + IP range checks.
 * Phase 160: Website blocklist — domain deny list + content category blocking.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Core safety checks */

/* Check if a URL is safe to fetch (not internal/SSRF).
 * Resolves hostname via DNS, checks IP against private ranges.
 * Returns: true if safe, false if blocked. */
bool url_is_safe(const char *url);

/* Quick check for always-blocked URLs (localhost, private IPs, etc.)
 * Does NOT do DNS resolution — checks the URL string directly. */
bool url_is_always_blocked(const char *url);

/* DNS-based IP check: resolve hostname, check if all IPs are public.
 * Returns: true if safe (public), false if blocked (private/loopback). */
bool url_check_ip(const char *hostname);

/* Check URL scheme — only http/https allowed for fetch tools. */
bool url_has_valid_scheme(const char *url);

/* Enable/disable private URL access (for testing/dev).
 * Default: disabled (private URLs blocked). */
void url_set_allow_private(bool allow);

/* Reset allow_private cache (for testing). */
void url_reset_allow_private(void);

/* ================================================================
 *  P160: Website Blocklist API
 * ================================================================ */

/* Enable/disable the website blocklist */
void url_blocklist_enable(bool enabled);

/* Add a domain to the blocklist (e.g., "facebook.com", "*.malware.site") */
bool url_blocklist_add_domain(const char *domain);

/* Remove a domain from the blocklist */
bool url_blocklist_remove_domain(const char *domain);

/* Add a content category to block (e.g., "social_media", "streaming",
 * "gaming", "adult", "downloads", "news") */
bool url_blocklist_add_category(const char *category);

/* Remove a content category from the blocklist */
bool url_blocklist_remove_category(const char *category);

/* Clear all blocklist entries */
void url_blocklist_clear(void);

/* ================================================================
 *  URL Parsing Utilities (P158)
 * ================================================================ */

/* Extract hostname from URL. Returns malloc'd string (caller must free). */
char *url_extract_hostname(const char *url);

/* Check if a URL's hostname matches an expected host (or subdomain).
 * e.g. url_host_matches("https://api.openai.com/v1/chat", "api.openai.com") == true
 *      url_host_matches("https://evil.com/", "api.openai.com") == false
 * Returns true if host matches or is subdomain of expected. */
bool url_host_matches(const char *url, const char *expected_host);

/* Check if URL contains embedded API keys/secrets (exfiltration prevention).
 * Checks raw URL and URL-decoded version for common API key prefix patterns.
 * Returns the matched prefix string or NULL if clean. */
const char *url_has_secret(const char *url);

/* ================================================================
 *  URL Basename Extraction
 * ================================================================ */

/* Extract filename from URL path. Strips query/fragment, finds last path
 * component after '/'. Returns malloc'd string (caller must free) or
 * empty string on error. Mirrors Python yuanbao_media._basename_from_url(). */
char *url_extract_basename(const char *url);

/* ================================================================
 *  URL Logging Safety
 * ================================================================ */

/* Return a URL string safe for logs (no userinfo, query, fragment).
 * Strips credentials (user:pass@), query parameters, and fragment from the
 * URL, then truncates to max_len characters. The returned string preserves
 * the scheme, host, and a condensed path (basename only if possible).
 * Mirrors Python gateway/platforms/base.py safe_url_for_log().
 * Returns malloc'd string (caller must free) or NULL on error. */
char *url_safe_for_log(const char *url, int max_len);

/* ================================================================
 *  MIME Type Detection
 * ================================================================ */

/* Guess MIME type from filename extension.
 * Returns pointer to static string (never NULL).
 * Mirrors Python yuanbao_media.guess_mime_type(). */
const char *url_guess_mime_type(const char *filename);

/* Check if filename extension is a known image type (.jpg, .png, .gif, etc.).
 * Mirrors Python yuanbao_media.is_image(). */
bool url_is_image_extension(const char *filename);

/* ================================================================
 *  Network Accessibility
 * ================================================================ */

/* Check if a hostname/IP would expose the server beyond loopback.
 * Loopback addresses (127.0.0.1, ::1) are local-only.
 * Hostnames are resolved via getaddrinfo; DNS failure fails closed (true).
 * Mirrors Python gateway/platforms/base.py is_network_accessible(). */
bool url_is_network_accessible(const char *host);

/* ================================================================
 *  Image Format & Dimension Parsing
 * ================================================================ */

/* Map MIME type to TIM image format number.
 * Mirrors Python yuanbao_media.get_image_format(). */
int url_get_image_format(const char *mime_type);

/* Parse image dimensions from raw bytes. Supports PNG, JPEG, GIF, WebP.
 * Returns true on success and sets *width and *height.
 * Mirrors Python yuanbao_media.parse_image_size(). */
bool url_parse_image_size(const unsigned char *data, size_t len,
                          int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_URL_SAFETY_H */
