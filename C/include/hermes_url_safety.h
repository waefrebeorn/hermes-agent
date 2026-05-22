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

#ifdef __cplusplus
}
#endif

#endif /* HERMES_URL_SAFETY_H */
