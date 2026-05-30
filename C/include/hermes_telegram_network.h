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
 * Free an IP list returned by telegram_resolve_system_dns().
 */
void telegram_free_ip_list(char **ips, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TELEGRAM_NETWORK_H */
