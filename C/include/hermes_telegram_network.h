/**
 * @file hermes_telegram_network.h
 * @brief Telegram network helper functions.
 *
 * Port of Python gateway/platforms/telegram_network.py.
 * Provides: system DNS resolution, DoH queries, fallback IP discovery.
 *
 * MIT License — WuBu Slermes Project
 */
#ifndef HERMES_TELEGRAM_NETWORK_H
#define HERMES_TELEGRAM_NETWORK_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolve hostname to IPv4 addresses via system resolver (getaddrinfo).
 * Port of Python _resolve_system_dns().
 *
 * @param hostname  Hostname to resolve (e.g. "api.telegram.org")
 * @param out_ips   Output: malloc'd array of malloc'd IP strings.
 *                  Caller must free with telegram_free_ip_list().
 * @param out_count Output: number of IPs found (0 on error/empty).
 * @return true on success (even if empty), false on allocation failure.
 */
bool telegram_resolve_system_dns(const char *hostname,
                                 char ***out_ips,
                                 size_t *out_count);

/**
 * Free an IP list returned by telegram_resolve_system_dns() or telegram_query_doh().
 */
void telegram_free_ip_list(char **ips, size_t count);

/**
 * Query a DNS-over-HTTPS provider for A records of a hostname.
 * Port of Python _query_doh_provider().
 *
 * @param doh_url   DoH provider URL (e.g. "https://dns.google/resolve")
 * @param hostname  Hostname to resolve
 * @param extra_headers  Extra HTTP headers (e.g. "Accept: application/dns-json"), may be NULL
 * @param timeout_sec    HTTP timeout in seconds
 * @param out_ips   Output: malloc'd array of malloc'd IP strings.
 * @param out_count Output: number of IPs found.
 * @return true on success (even if empty), false on error.
 */
bool telegram_query_doh(const char *doh_url,
                        const char *hostname,
                        const char *extra_headers,
                        int timeout_sec,
                        char ***out_ips,
                        size_t *out_count);

/**
 * Parse a DoH JSON response string into a list of A-record IPs.
 * Port of Python _query_doh_provider() JSON parsing logic.
 * Useful for testing without network access.
 *
 * @param doh_response  JSON response body from DoH provider
 * @param out_ips       Output: malloc'd array of malloc'd IP strings.
 * @param out_count     Output: number of IPs found.
 * @return true on success (even if no A records), false on parse error.
 */
bool telegram_parse_doh_response(const char *doh_response,
                                  char ***out_ips,
                                  size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TELEGRAM_NETWORK_H */
