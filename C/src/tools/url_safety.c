/*
 * url_safety.c — SSRF protection and URL safety checks.
 * DNS-based: resolves hostname via getaddrinfo, checks IP against private ranges.
 * Phase 111-115: Security — URL safety, path traversal, Tirith.
 * Phase 160: Website blocklist — domain deny list + content category blocking.
 */

#include "hermes.h"
#include "hermes_url_safety.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>

/* Forward declaration */
static char *extract_hostname(const char *url);
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
 *  P160: Website Blocklist
 * ================================================================ */

#define MAX_BLOCKED_DOMAINS 256
#define MAX_BLOCKED_CATEGORIES 32

/* Blocked domain list */
static char *g_blocked_domains[MAX_BLOCKED_DOMAINS];
static int g_blocked_domain_count = 0;

/* Blocked content categories */
static char *g_blocked_categories[MAX_BLOCKED_CATEGORIES];
static int g_blocked_category_count = 0;

/* Whether blocklist is enabled */
static bool g_blocklist_enabled = true;

/* Extract registered domain from hostname (e.g., "www.google.com" -> "google.com") */
__attribute__((unused)) static char *extract_registered_domain(const char *hostname) {
    if (!hostname) return NULL;
    /* Find the last two domain components */
    const char *last_dot = strrchr(hostname, '.');
    if (!last_dot || last_dot == hostname) return hostname;

    /* Find the second-to-last dot */
    const char *prev_dot = hostname;
    const char *scan = hostname;
    while ((scan = strchr(scan + 1, '.')) != NULL) {
        if (scan == last_dot) break;
        prev_dot = scan;
    }
    if (prev_dot != hostname)
        return prev_dot + 1;
    return hostname;
}

/* Check if hostname matches a blocked domain */
static bool is_domain_blocked(const char *hostname) {
    if (!hostname || !g_blocklist_enabled || g_blocked_domain_count == 0)
        return false;

    /* Check full hostname and each parent domain */
    const char *p = hostname;
    while (p && *p) {
        for (int i = 0; i < g_blocked_domain_count; i++) {
            if (g_blocked_domains[i] && strcasecmp(p, g_blocked_domains[i]) == 0)
                return true;
        }
        /* Move to parent domain */
        p = strchr(p, '.');
        if (p) p++;
    }
    return false;
}

/* Check if URL matches blocked content category */
static bool is_content_category_blocked(const char *url) {
    if (!url || !g_blocklist_enabled || g_blocked_category_count == 0)
        return false;

    /* Convert URL to lowercase for matching */
    char url_lower[2048];
    size_t ulen = strlen(url);
    if (ulen >= sizeof(url_lower)) ulen = sizeof(url_lower) - 1;
    for (size_t i = 0; i < ulen; i++)
        url_lower[i] = (char)tolower((unsigned char)url[i]);
    url_lower[ulen] = '\0';

    /* Category keywords to match in URL/path */
    for (int i = 0; i < g_blocked_category_count; i++) {
        if (!g_blocked_categories[i]) continue;

        const char *cat = g_blocked_categories[i];

        /* Use a simple keyword-matching approach */
        if (strcmp(cat, "social_media") == 0) {
            static const char *social_domains[] = {
                "facebook.com", "twitter.com", "x.com", "instagram.com",
                "linkedin.com", "tiktok.com", "reddit.com", "snapchat.com",
                "pinterest.com", "tumblr.com", "threads.net", NULL
            };
            const char *host = extract_hostname(url);
            if (host) {
                for (int d = 0; social_domains[d]; d++) {
                    if (strcasestr(host, social_domains[d])) {
                        free((void*)host);
                        return true;
                    }
                }
                free((void*)host);
            }
        } else if (strcmp(cat, "streaming") == 0) {
            static const char *streaming_domains[] = {
                "youtube.com", "netflix.com", "hulu.com", "twitch.tv",
                "vimeo.com", "spotify.com", "disneyplus.com", "hbomax.com",
                "peacocktv.com", "paramountplus.com", NULL
            };
            const char *host = extract_hostname(url);
            if (host) {
                for (int d = 0; streaming_domains[d]; d++) {
                    if (strcasestr(host, streaming_domains[d])) {
                        free((void*)host);
                        return true;
                    }
                }
                free((void*)host);
            }
        } else if (strcmp(cat, "gaming") == 0) {
            static const char *gaming_domains[] = {
                "steam", "epicgames", "xbox.com", "playstation.com",
                "nintendo.com", "roblox.com", "minecraft.net", NULL
            };
            if (strstr(url_lower, "game") || strstr(url_lower, "gaming")) {
                const char *host = extract_hostname(url);
                if (host) {
                    for (int d = 0; gaming_domains[d]; d++) {
                        if (strcasestr(host, gaming_domains[d])) {
                            free((void*)host);
                            return true;
                        }
                    }
                    free((void*)host);
                }
            }
        } else if (strcmp(cat, "adult") == 0) {
            /* Simple keyword check for adult content */
            static const char *adult_keywords[] = {
                "porn", "xxx", "adult", "nsfw", "onlyfans", NULL
            };
            for (int k = 0; adult_keywords[k]; k++) {
                if (strstr(url_lower, adult_keywords[k]))
                    return true;
            }
        } else if (strcmp(cat, "downloads") == 0) {
            static const char *download_ext[] = {
                ".torrent", "magnet:", ".exe", ".msi", ".dmg", ".iso", NULL
            };
            for (int k = 0; download_ext[k]; k++) {
                if (strstr(url_lower, download_ext[k]))
                    return true;
            }
        } else if (strcmp(cat, "news") == 0) {
            static const char *news_domains[] = {
                "cnn.com", "foxnews.com", "bbc.com", "bbc.co.uk",
                "nytimes.com", "wsj.com", "reuters.com", NULL
            };
            const char *host = extract_hostname(url);
            if (host) {
                for (int d = 0; news_domains[d]; d++) {
                    if (strcasestr(host, news_domains[d])) {
                        free((void*)host);
                        return true;
                    }
                }
                free((void*)host);
            }
        }
    }
    return false;
}

/* Public API for blocklist management */
void url_blocklist_enable(bool enabled) {
    g_blocklist_enabled = enabled;
}

bool url_blocklist_add_domain(const char *domain) {
    if (!domain || g_blocked_domain_count >= MAX_BLOCKED_DOMAINS) return false;

    /* Lowercase the domain */
    char lower[256];
    size_t dlen = strlen(domain);
    if (dlen >= sizeof(lower)) dlen = sizeof(lower) - 1;
    for (size_t i = 0; i < dlen; i++)
        lower[i] = (char)tolower((unsigned char)domain[i]);
    lower[dlen] = '\0';

    /* Check for duplicates */
    for (int i = 0; i < g_blocked_domain_count; i++) {
        if (g_blocked_domains[i] && strcmp(g_blocked_domains[i], lower) == 0)
            return true; /* Already exists */
    }

    g_blocked_domains[g_blocked_domain_count] = strdup(lower);
    if (g_blocked_domains[g_blocked_domain_count]) {
        g_blocked_domain_count++;
        return true;
    }
    return false;
}

bool url_blocklist_remove_domain(const char *domain) {
    if (!domain) return false;
    for (int i = 0; i < g_blocked_domain_count; i++) {
        if (g_blocked_domains[i] && strcasecmp(g_blocked_domains[i], domain) == 0) {
            free(g_blocked_domains[i]);
            g_blocked_domains[i] = g_blocked_domains[--g_blocked_domain_count];
            g_blocked_domains[g_blocked_domain_count] = NULL;
            return true;
        }
    }
    return false;
}

bool url_blocklist_add_category(const char *category) {
    if (!category || g_blocked_category_count >= MAX_BLOCKED_CATEGORIES) return false;

    /* Check for duplicates */
    for (int i = 0; i < g_blocked_category_count; i++) {
        if (g_blocked_categories[i] && strcmp(g_blocked_categories[i], category) == 0)
            return true;
    }

    g_blocked_categories[g_blocked_category_count] = strdup(category);
    if (g_blocked_categories[g_blocked_category_count]) {
        g_blocked_category_count++;
        return true;
    }
    return false;
}

bool url_blocklist_remove_category(const char *category) {
    if (!category) return false;
    for (int i = 0; i < g_blocked_category_count; i++) {
        if (g_blocked_categories[i] && strcmp(g_blocked_categories[i], category) == 0) {
            free(g_blocked_categories[i]);
            g_blocked_categories[i] = g_blocked_categories[--g_blocked_category_count];
            g_blocked_categories[g_blocked_category_count] = NULL;
            return true;
        }
    }
    return false;
}

void url_blocklist_clear(void) {
    for (int i = 0; i < g_blocked_domain_count; i++) {
        free(g_blocked_domains[i]);
        g_blocked_domains[i] = NULL;
    }
    g_blocked_domain_count = 0;
    for (int i = 0; i < g_blocked_category_count; i++) {
        free(g_blocked_categories[i]);
        g_blocked_categories[i] = NULL;
    }
    g_blocked_category_count = 0;
}

/* Load blocklist config from security_config_t */
void url_blocklist_load_config(const security_config_t *cfg) {
    if (!cfg) return;
    g_blocklist_enabled = cfg->website_blocklist_enabled;

    /* Note: domains and categories would be loaded from config YAML.
     * For now, they're managed via runtime API calls. */
}

/*
 * URL Parsing
 *
 * extract_hostname() defined below in the URL Parsing section.
 * The blocklist code uses it via forward declaration.
 */

/* strcasestr-like — use custom name to avoid conflict with glibc's strcasestr */
__attribute__((unused)) static char *strcasestr_safe(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    size_t nlen = strlen(needle);
    if (nlen == 0) return haystack;
    while (*haystack) {
        if (strncasecmp(haystack, needle, nlen) == 0)
            return haystack;
        haystack++;
    }
    return NULL;
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
    if (strstr(lower, "metadata.goog")) return true;
    if (strstr(lower, "100.100.100.200")) return true;
    if (strstr(lower, "fd00:ec2")) return true;
    if (strstr(lower, "::ffff:")) return true;
    if (strstr(lower, "100.64.")) return true;

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

    /* Check blocklist domains */
    if (g_blocklist_enabled) {
        for (int i = 0; i < g_blocked_domain_count; i++) {
            if (g_blocked_domains[i] && strstr(lower, g_blocked_domains[i]))
                return true;
        }
    }

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

    /* P160: Check website blocklist */
    if (is_content_category_blocked(url)) return false;

    /* Extract hostname for domain check */
    char *hostname = extract_hostname(url);
    if (!hostname) return false;

    /* P160: Check domain blocklist */
    if (is_domain_blocked(hostname)) {
        free(hostname);
        return false;
    }

    /* DNS resolution + IP check */
    bool safe = url_check_ip(hostname);
    free(hostname);
    return safe;
}

/* ================================================================
 *  Public URL Parsing API
 * ================================================================ */

/* Public wrapper: extract hostname from URL */
char *url_extract_hostname(const char *url) {
    return extract_hostname(url);
}

/* Check if URL host matches expected host (exact or subdomain match) */
bool url_host_matches(const char *url, const char *expected_host) {
    if (!url || !expected_host) return false;

    char *host = extract_hostname(url);
    if (!host) return false;

    /* Exact match */
    bool match = (strcasecmp(host, expected_host) == 0);

    /* Subdomain match: host ends with ".expected_host" */
    if (!match) {
        size_t host_len = strlen(host);
        size_t expected_len = strlen(expected_host);
        if (host_len > expected_len + 1) {
            if (host[host_len - expected_len - 1] == '.' &&
                strcasecmp(host + host_len - expected_len, expected_host) == 0) {
                match = true;
            }
        }
    }

    free(host);
    return match;
}

/* Check if a URL contains embedded API keys/secrets (exfiltration prevention).
 * Checks raw URL and URL-decoded version for common secret prefix patterns.
 * Returns the matched prefix string or NULL if clean. */
const char *url_has_secret(const char *url) {
    if (!url || !*url) return NULL;

    /* Common API key prefixes to check (subset of Python agent/redact.py _PREFIX_PATTERNS) */
    static const char *prefixes[] = {
        "sk-", "sk-ant-",
        "ghp_", "github_pat_", "gho_", "ghu_", "ghs_", "ghr_",
        "xoxb-", "xoxp-", "xoxa-", "xoxr-",
        "AIza", "pplx-", "fal_", "fc-", "bb_live_",
        "AKIA",
        "sk_live_", "sk_test_", "rk_live_",
        "SG.", "hf_", "r8_", "npm_", "pypi-",
        "dop_v1_", "doo_v1_",
        "am_", "tvly-", "exa_", "gsk_",
        "syt_", "retaindb_", "hsk-",
        "mem0_", "brv_", "xai-",
        NULL
    };

    /* Check raw URL */
    for (int i = 0; prefixes[i]; i++) {
        if (strstr(url, prefixes[i]))
            return prefixes[i];
    }

    /* URL-decode a copy for percent-encoded variants (%73k- etc.) */
    char decoded[4096];
    int di = 0;
    for (int si = 0; url[si] && di < (int)sizeof(decoded) - 1; si++) {
        if (url[si] == '%' && url[si+1] && url[si+2]) {
            char hex[3] = {url[si+1], url[si+2], 0};
            char *end = NULL;
            long val = strtol(hex, &end, 16);
            if (end && *end == 0 && val >= 0 && val <= 255) {
                decoded[di++] = (char)val;
                si += 2;
                continue;
            }
        }
        decoded[di++] = url[si];
    }
    decoded[di] = 0;

    if (strcmp(decoded, url) != 0) {
        for (int i = 0; prefixes[i]; i++) {
            if (strstr(decoded, prefixes[i]))
                return prefixes[i];
        }
    }

    return NULL;
}

/* ================================================================
 *  URL Basename Extraction
 * ================================================================ */

/* Extract filename from URL path. Strips query/fragment, returns last
 * path component after '/'. Mirrors Python _basename_from_url(). */
char *url_extract_basename(const char *url) {
    if (!url || !*url) return strdup("");

    /* Skip scheme:// */
    const char *path = strstr(url, "://");
    if (path) {
        path += 3; /* skip "://" */
        /* Move past host:port to path */
        path = strchr(path, '/');
    } else {
        path = url;
    }
    if (!path) return strdup("");

    /* Skip query/fragment after the path */
    const char *query = strchr(path, '?');
    const char *fragment = strchr(path, '#');
    const char *end = NULL;
    if (query && (!fragment || query < fragment)) {
        end = query;
    } else if (fragment) {
        end = fragment;
    }

    /* Find last '/' to get basename */
    const char *last_slash = NULL;
    const char *scan = path;
    while (scan < (end ? end : scan + strlen(scan))) {
        if (*scan == '/') last_slash = scan;
        scan++;
    }

    const char *basename_start = last_slash ? last_slash + 1 : path;
    size_t basename_len = end ? (size_t)(end - basename_start) : strlen(basename_start);

    if (basename_len == 0) return strdup("");

    char *out = (char *)malloc(basename_len + 1);
    if (!out) return NULL;
    memcpy(out, basename_start, basename_len);
    out[basename_len] = '\0';
    return out;
}

/* ================================================================
 *  MIME Type Utilities
 * ================================================================ */

/* Extension-to-MIME lookup table. */
static const char *_mime_for_ext(const char *ext) {
    if (!ext || !*ext) return "application/octet-stream";
    struct { const char *ext; const char *mime; } map[] = {
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".png", "image/png"}, {".gif", "image/gif"},
        {".webp", "image/webp"}, {".bmp", "image/bmp"},
        {".heic", "image/heic"}, {".tiff", "image/tiff"},
        {".ico", "image/x-icon"},
        {".pdf", "application/pdf"},
        {".doc", "application/msword"},
        {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {".xls", "application/vnd.ms-excel"},
        {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {".ppt", "application/vnd.ms-powerpoint"},
        {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {".txt", "text/plain"},
        {".zip", "application/zip"}, {".tar", "application/x-tar"},
        {".gz", "application/gzip"},
        {".mp3", "audio/mpeg"}, {".mp4", "video/mp4"},
        {".wav", "audio/wav"}, {".ogg", "audio/ogg"},
        {".webm", "video/webm"},
        {NULL, NULL}
    };
    for (int i = 0; map[i].ext; i++) {
        if (strcasecmp(ext, map[i].ext) == 0)
            return map[i].mime;
    }
    return "application/octet-stream";
}

/* Guess MIME type from filename extension.
 * Returns pointer to static string (never NULL, always valid).
 * Mirrors Python yuanbao_media.guess_mime_type(). */
const char *url_guess_mime_type(const char *filename) {
    if (!filename || !*filename) return "application/octet-stream";
    /* Find last dot */
    const char *dot = strrchr(filename, '.');
    if (!dot) return "application/octet-stream";
    return _mime_for_ext(dot);
}

/* Check if filename extension is a known image type.
 * Mirrors Python yuanbao_media.is_image(). */
bool url_is_image_extension(const char *filename) {
    if (!filename || !*filename) return false;
    const char *dot = strrchr(filename, '.');
    if (!dot) return false;
    const char *ext = dot;
    static const char *image_exts[] = {
        ".jpg", ".jpeg", ".png", ".gif", ".webp",
        ".bmp", ".heic", ".tiff", ".ico", NULL
    };
    for (int i = 0; image_exts[i]; i++) {
        if (strcasecmp(ext, image_exts[i]) == 0) return true;
    }
    return false;
}

/* ================================================================
 *  Network Accessibility Check
 * ================================================================ */

/* Check if a hostname/IP would expose the server beyond loopback.
 * Loopback addresses (127.0.0.1, ::1) are local-only.
 * Hostnames are resolved via getaddrinfo; DNS failure fails closed (true).
 * Mirrors Python gateway/platforms/base.py is_network_accessible(). */
bool url_is_network_accessible(const char *host) {
    if (!host || !*host) return true; /* fail closed on invalid */

    /* Try IPv4 first */
    struct in_addr addr4;
    if (inet_pton(AF_INET, host, &addr4) == 1) {
        /* Check for 127.0.0.0/8 (all loopback) */
        return (addr4.s_addr & htonl(0xFF000000)) != htonl(0x7F000000);
    }

    /* Try IPv6 */
    struct in6_addr addr6;
    if (inet_pton(AF_INET6, host, &addr6) == 1) {
        /* Check for ::1 (loopback) */
        static const unsigned char loopback[16] =
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
        if (memcmp(&addr6, loopback, 16) == 0) return false;

        /* Check for IPv4-mapped IPv6 loopback (::ffff:127.x.x.x) */
        static const unsigned char ipv4_mapped_prefix[12] =
            {0,0,0,0,0,0,0,0,0,0,0xFF,0xFF};
        if (memcmp(&addr6, ipv4_mapped_prefix, 12) == 0) {
            /* Extract the embedded IPv4 and check loopback */
            struct in_addr embedded;
            memcpy(&embedded, ((const unsigned char *)&addr6) + 12, 4);
            if ((embedded.s_addr & htonl(0xFF000000)) == htonl(0x7F000000))
                return false;
        }
        return true; /* non-loopback IPv6 is network-accessible */
    }

    /* Resolve hostname via getaddrinfo */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = NULL;
    int rc = getaddrinfo(host, NULL, &hints, &result);
    if (rc != 0 || !result) {
        return true; /* DNS failure — fail closed (assume accessible) */
    }

    bool accessible = false;
    for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)rp->ai_addr;
            if ((sin->sin_addr.s_addr & htonl(0xFF000000)) != htonl(0x7F000000)) {
                accessible = true;
                break;
            }
        } else if (rp->ai_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)rp->ai_addr;
            /* Check for ::1 (loopback) */
            static const unsigned char loopback6[16] =
                {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
            if (memcmp(&sin6->sin6_addr, loopback6, 16) != 0) {
                accessible = true;
                break;
            }
        }
    }

    freeaddrinfo(result);
    return accessible;
}

/* ================================================================
 *  URL Logging Safety
 * ================================================================ */

/* Return a URL string safe for logs (no userinfo, query, fragment).
 * Mirrors Python gateway/platforms/base.py safe_url_for_log().
 * Returns malloc'd string (caller must free) or NULL on empty/error. */
char *url_safe_for_log(const char *url, int max_len) {
    if (!url || !*url || max_len <= 0) return NULL;

    /* Find scheme separator "://" */
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end || scheme_end == url) {
        /* Not a proper URL — just truncate raw string */
        size_t raw_len = strlen(url);
        size_t out_len = (raw_len < (size_t)max_len) ? raw_len : (size_t)max_len;
        char *out = (char *)malloc(out_len + 1);
        if (!out) return NULL;
        memcpy(out, url, out_len);
        out[out_len] = '\0';
        return out;
    }

    /* Split into scheme and rest */
    size_t scheme_len = (size_t)(scheme_end - url);
    const char *rest = scheme_end + 3; /* skip "://" */

    /* Find where the path starts (first '/' after host[:port]) */
    const char *path_start = strchr(rest, '/');
    const char *netloc_end = path_start ? path_start : rest + strlen(rest);

    /* Extract netloc (host[:port] potentially with userinfo) */
    size_t netloc_len = (size_t)(netloc_end - rest);
    char netloc[512];
    if (netloc_len >= sizeof(netloc)) netloc_len = sizeof(netloc) - 1;
    memcpy(netloc, rest, netloc_len);
    netloc[netloc_len] = '\0';

    /* Strip userinfo: find last '@' in netloc, use whatever's after it */
    const char *at = strrchr(netloc, '@');
    const char *clean_netloc = at ? (at + 1) : netloc;

    /* Build safe base: scheme://clean_netloc */
    char base[1024];
    int base_len = snprintf(base, sizeof(base), "%.*s://%s",
                            (int)scheme_len, url, clean_netloc);
    if (base_len < 0 || (size_t)base_len >= sizeof(base)) {
        base_len = (int)sizeof(base) - 1;
        base[base_len] = '\0';
    }

    /* Build the safe URL with condensed path */
    char safe[2048];
    int safe_len;

    if (path_start && *path_start) {
        /* Get the basename (last path component after '/') */
        const char *last_slash = strrchr(path_start, '/');
        const char *basename = last_slash ? (last_slash + 1) : path_start;

        /* Only use /.../basename if basename exists and is not empty */
        if (basename && *basename && basename != path_start) {
            safe_len = snprintf(safe, sizeof(safe), "%s/.../%s", base, basename);
        } else {
            safe_len = snprintf(safe, sizeof(safe), "%s/...", base);
        }
    } else {
        safe_len = snprintf(safe, sizeof(safe), "%s", base);
    }

    if (safe_len < 0) return NULL;
    if ((size_t)safe_len >= sizeof(safe)) {
        safe_len = (int)sizeof(safe) - 1;
        safe[safe_len] = '\0';
    }

    /* Truncate to max_len */
    size_t out_len = ((size_t)safe_len < (size_t)max_len) ? (size_t)safe_len : (size_t)max_len;
    if (out_len == 0) return NULL;
    if (out_len > 3 && out_len < (size_t)safe_len) {
        /* Add ellipsis when truncated */
        out_len -= 3;
    }

    char *out = (char *)malloc(out_len + 1);
    if (!out) return NULL;
    memcpy(out, safe, out_len);
    out[out_len] = '\0';

    return out;
}

/* ================================================================
 *  Image Format & Dimension Parsing
 * ================================================================ */

/* MIME type → TIM image format number.
 * Mirrors Python yuanbao_media.get_image_format(). */
int url_get_image_format(const char *mime_type) {
    if (!mime_type || !*mime_type) return 255;
    static const struct {
        const char *mime;
        int fmt;
    } map[] = {
        {"image/jpeg", 1},
        {"image/jpg",  1},
        {"image/gif",  2},
        {"image/png",  3},
        {"image/bmp",  4},
        {"image/webp", 255},
        {"image/heic", 255},
        {"image/tiff", 255},
        {NULL, 0}
    };
    for (int i = 0; map[i].mime; i++) {
        if (strcasecmp(mime_type, map[i].mime) == 0)
            return map[i].fmt;
    }
    return 255;
}

/* Parse PNG dimensions from raw bytes.
 * Mirrors Python yuanbao_media._parse_png_size(). */
static bool _parse_png_size(const unsigned char *data, size_t len,
                            int *width, int *height) {
    if (len < 24) return false;
    if (data[0] != 0x89 || data[1] != 'P' || data[2] != 'N' || data[3] != 'G')
        return false;
    *width  = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
    *height = (data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23];
    return true;
}

/* Parse JPEG dimensions from raw bytes.
 * Mirrors Python yuanbao_media._parse_jpeg_size(). */
static bool _parse_jpeg_size(const unsigned char *data, size_t len,
                             int *width, int *height) {
    if (len < 4) return false;
    if (data[0] != 0xFF || data[1] != 0xD8) return false;
    size_t i = 2;
    while (i < len - 1) {
        if (data[i] != 0xFF) { i++; continue; }
        unsigned char marker = data[i + 1];
        if (marker == 0xC0 || marker == 0xC2) {
            if (i + 9 > len) return false;
            *height = (data[i + 5] << 8) | data[i + 6];
            *width  = (data[i + 7] << 8) | data[i + 8];
            return true;
        }
        if (i + 3 <= len) {
            unsigned seg_len = (data[i + 2] << 8) | data[i + 3];
            i += 2 + seg_len;
        } else break;
    }
    return false;
}

/* Parse GIF dimensions from raw bytes.
 * Mirrors Python yuanbao_media._parse_gif_size(). */
static bool _parse_gif_size(const unsigned char *data, size_t len,
                            int *width, int *height) {
    if (len < 10) return false;
    /* Check GIF signature */
    if (memcmp(data, "GIF87a", 6) != 0 && memcmp(data, "GIF89a", 6) != 0)
        return false;
    *width  = data[6] | (data[7] << 8);
    *height = data[8] | (data[9] << 8);
    return true;
}

/* Parse WebP dimensions from raw bytes.
 * Supports VP8 (lossy), VP8L (lossless), VP8X (extended).
 * Mirrors Python yuanbao_media._parse_webp_size(). */
static bool _parse_webp_size(const unsigned char *data, size_t len,
                             int *width, int *height) {
    if (len < 16) return false;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WEBP", 4) != 0)
        return false;
    /* Determine sub-format from bytes 12-15 */
    if (len >= 30 && memcmp(data + 12, "VP8 ", 4) == 0) {
        if (data[23] == 0x9D && data[24] == 0x01 && data[25] == 0x2A) {
            *width  = (data[26] | (data[27] << 8)) & 0x3FFF;
            *height = (data[28] | (data[29] << 8)) & 0x3FFF;
            return true;
        }
    } else if (len >= 25 && memcmp(data + 12, "VP8L", 4) == 0) {
        if (data[20] == 0x2F) {
            unsigned bits = (unsigned)data[21] | ((unsigned)data[22] << 8)
                          | ((unsigned)data[23] << 16) | ((unsigned)data[24] << 24);
            *width  = (bits & 0x3FFF) + 1;
            *height = ((bits >> 14) & 0x3FFF) + 1;
            return true;
        }
    } else if (len >= 30 && memcmp(data + 12, "VP8X", 4) == 0) {
        *width  = (data[24] | (data[25] << 8) | (data[26] << 16)) + 1;
        *height = (data[27] | (data[28] << 8) | (data[29] << 16)) + 1;
        return true;
    }
    return false;
}

/* Parse image dimensions from raw bytes. Supports PNG, JPEG, GIF, WebP.
 * Returns true on success and sets *width and *height.
 * Mirrors Python yuanbao_media.parse_image_size(). */
bool url_parse_image_size(const unsigned char *data, size_t len,
                          int *width, int *height) {
    if (!data || !len || !width || !height) return false;
    return _parse_png_size(data, len, width, height)
        || _parse_jpeg_size(data, len, width, height)
        || _parse_gif_size(data, len, width, height)
        || _parse_webp_size(data, len, width, height);
}
