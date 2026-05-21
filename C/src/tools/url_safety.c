/*
 * url_safety.c — SSRF protection and URL safety checks.
 * DNS-based: resolves hostname via getaddrinfo, checks IP against private ranges.
 * Phase 111-115: Security — URL safety, path traversal, Tirith.
 */

#include "hermes.h"
#include "hermes_url_safety.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

/* ================================================================
 *  Internal State
 * ================================================================ */

static bool g_allow_private = false;

void url_set_allow_private(bool allow) {
    g_allow_private = allow;
}

void url_reset_allow_private(void) {
    g_allow_private = false;
}

/* ================================================================
 *  IP Range Checks
 * ================================================================ */

/* Check if an IPv4 address is private/loopback/link-local */
static bool is_private_ipv4(uint32_t addr) {
    /* Convert from network byte order to host */
    uint32_t h = ntohl(addr);

    /* 127.0.0.0/8 — loopback */
    if ((h & 0xFF000000) == 0x7F000000) return true;
    /* 10.0.0.0/8 — private */
    if ((h & 0xFF000000) == 0x0A000000) return true;
    /* 172.16.0.0/12 — private */
    if ((h & 0xFFF00000) == 0xAC100000) return true;
    /* 192.168.0.0/16 — private */
    if ((h & 0xFFFF0000) == 0xC0A80000) return true;
    /* 169.254.0.0/16 — link-local */
    if ((h & 0xFFFF0000) == 0xA9FE0000) return true;
    /* 0.0.0.0/8 — current network */
    if ((h & 0xFF000000) == 0x00000000) return true;
    /* 100.64.0.0/10 — Carrier-grade NAT */
    if ((h & 0xFFC00000) == 0x64400000) return true;
    /* 198.18.0.0/15 — benchmark testing */
    if ((h & 0xFFFE0000) == 0xC6120000) return true;

    return false;
}

/* Check if an IPv6 address is private/unique-local/loopback */
static bool is_private_ipv6(const struct in6_addr *addr) {
    /* ::1 — loopback */
    if (memcmp(addr, &in6addr_loopback, sizeof(*addr)) == 0)
        return true;

    /* fe80::/10 — link-local */
    if (addr->s6_addr[0] == 0xFE && (addr->s6_addr[1] & 0xC0) == 0x80)
        return true;

    /* fc00::/7 — unique local address (ULA) */
    if ((addr->s6_addr[0] & 0xFE) == 0xFC)
        return true;

    return false;
}

/* ================================================================
 *  URL Parsing
 * ================================================================ */

/* Extract hostname from URL. Returns malloc'd string or NULL. */
static char *extract_hostname(const char *url) {
    if (!url) return NULL;

    /* Find :// */
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) return NULL;

    const char *host_start = scheme_end + 3;

    /* Find end of hostname (next /, :, ?, or end) */
    const char *host_end = host_start;
    while (*host_end && *host_end != '/' && *host_end != ':' &&
           *host_end != '?' && *host_end != '#')
        host_end++;

    if (host_start == host_end) return NULL;

    size_t len = (size_t)(host_end - host_start);
    char *host = (char *)malloc(len + 1);
    if (!host) return NULL;
    memcpy(host, host_start, len);
    host[len] = '\0';

    return host;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool url_has_valid_scheme(const char *url) {
    if (!url) return false;
    return (strncmp(url, "http://", 7) == 0) ||
           (strncmp(url, "https://", 8) == 0);
}

bool url_is_always_blocked(const char *url) {
    if (!url) return true;

    /* Check scheme */
    if (!url_has_valid_scheme(url)) return true;

    /* Check raw strings in URL for common internal patterns */
    const char *lower = url;

    /* Localhost patterns */
    if (strstr(lower, "localhost")) return true;
    if (strstr(lower, "127.0.0.1")) return true;
    if (strstr(lower, "169.254.")) return true;

    /* Private IPs (quick string check, no DNS) */
    if (strstr(lower, "://10.")) return true;
    if (strstr(lower, "://172.16.")) return true;
    if (strstr(lower, "://172.17.")) return true;
    if (strstr(lower, "://172.18.")) return true;
    if (strstr(lower, "://172.19.")) return true;
    if (strstr(lower, "://172.20.")) return true;
    if (strstr(lower, "://172.21.")) return true;
    if (strstr(lower, "://172.22.")) return true;
    if (strstr(lower, "://172.23.")) return true;
    if (strstr(lower, "://172.24.")) return true;
    if (strstr(lower, "://172.25.")) return true;
    if (strstr(lower, "://172.26.")) return true;
    if (strstr(lower, "://172.27.")) return true;
    if (strstr(lower, "://172.28.")) return true;
    if (strstr(lower, "://172.29.")) return true;
    if (strstr(lower, "://172.30.")) return true;
    if (strstr(lower, "://172.31.")) return true;
    if (strstr(lower, "://192.168.")) return true;

    /* Internal hostname patterns */
    if (strstr(lower, ".internal")) return true;
    if (strstr(lower, ".corp")) return true;
    if (strstr(lower, ".private")) return true;
    if (strstr(lower, ".local")) return true;

    return false;
}

bool url_check_ip(const char *hostname) {
    if (!hostname || !*hostname) return false;

    struct addrinfo hints;
    struct addrinfo *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(hostname, NULL, &hints, &result);
    if (rc != 0 || !result) {
        /* DNS resolution failed — fail closed (block) */
        if (result) freeaddrinfo(result);
        return false;
    }

    bool all_public = true;
    for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)rp->ai_addr;
            if (is_private_ipv4(sin->sin_addr.s_addr)) {
                all_public = false;
                break;
            }
        } else if (rp->ai_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)rp->ai_addr;
            if (is_private_ipv6(&sin6->sin6_addr)) {
                all_public = false;
                break;
            }
        }
    }

    freeaddrinfo(result);

    if (g_allow_private) return true; /* Allow all if configured */
    return all_public;
}

bool url_is_safe(const char *url) {
    if (!url) return false;

    /* Check scheme first */
    if (!url_has_valid_scheme(url)) return false;

    /* Quick string check for blocked patterns */
    if (url_is_always_blocked(url)) return false;

    /* DNS resolution + IP check */
    char *hostname = extract_hostname(url);
    if (!hostname) return false;

    bool safe = url_check_ip(hostname);
    free(hostname);
    return safe;
}
