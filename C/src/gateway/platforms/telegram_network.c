/*
 * telegram_network.c — Telegram network helpers for Hermes C.
 *
 * Port of Python gateway/platforms/telegram_network.py.
 * Provides: system DNS resolution, DoH queries, fallback IP discovery.
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes_telegram_network.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* POSIX networking */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

/* libhttp for DoH queries */
#include "http.h"

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static void *xmalloc(size_t sz) {
    void *p = malloc(sz ? sz : 1);
    if (!p) { fprintf(stderr, "telegram_network: OOM\n"); exit(1); }
    return p;
}

/* ================================================================
 *  telegram_resolve_system_dns
 *  Port of Python _resolve_system_dns().
 *  Returns IPv4 addresses for hostname via getaddrinfo().
 * ================================================================ */

bool telegram_resolve_system_dns(const char *hostname,
                                 char ***out_ips,
                                 size_t *out_count)
{
    if (!out_ips || !out_count) return false;
    *out_ips = NULL;
    *out_count = 0;

    if (!hostname || !hostname[0]) return false;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       /* IPv4 only */
    hints.ai_socktype = SOCK_STREAM; /* TCP */

    struct addrinfo *result = NULL;
    int rc = getaddrinfo(hostname, NULL, &hints, &result);
    if (rc != 0 || !result)
        return true; /* empty result, not an error */

    /* First pass: count IPv4 results */
    size_t count = 0;
    for (struct addrinfo *rp = result; rp; rp = rp->ai_next) {
        if (rp->ai_addr && rp->ai_addr->sa_family == AF_INET)
            count++;
    }

    if (count == 0) {
        freeaddrinfo(result);
        return true;
    }

    /* Allocate IP array */
    char **ips = (char **)xmalloc(count * sizeof(char *));
    size_t idx = 0;

    for (struct addrinfo *rp = result; rp; rp = rp->ai_next) {
        if (rp->ai_addr && rp->ai_addr->sa_family == AF_INET) {
            char buf[INET_ADDRSTRLEN];
            struct sockaddr_in *sin = (struct sockaddr_in *)rp->ai_addr;
            if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
                ips[idx] = strdup(buf);
                if (!ips[idx]) {
                    /* OOM — clean up partial */
                    for (size_t j = 0; j < idx; j++) free(ips[j]);
                    free(ips);
                    freeaddrinfo(result);
                    return false;
                }
                idx++;
            }
        }
    }

    freeaddrinfo(result);
    *out_ips = ips;
    *out_count = count;
    return true;
}

/* ================================================================
 *  telegram_free_ip_list
 * ================================================================ */

void telegram_free_ip_list(char **ips, size_t count)
{
    if (!ips) return;
    for (size_t i = 0; i < count; i++) free(ips[i]);
    free(ips);
}

/* ================================================================
 *  telegram_parse_doh_response
 *  Port of Python _query_doh_provider() JSON parsing.
 *  Parses a DNS-over-HTTPS JSON response and extracts A-record IPs.
 *
 *  Expected JSON format:
 *    { "Answer": [ {"name": "...", "type": 1, "data": "x.x.x.x"}, ... ] }
 * ================================================================ */

bool telegram_parse_doh_response(const char *doh_response,
                                  char ***out_ips,
                                  size_t *out_count)
{
    if (!out_ips || !out_count) return false;
    *out_ips = NULL;
    *out_count = 0;

    if (!doh_response || !doh_response[0]) return false;

    json_t *doc = json_parse(doh_response, NULL);
    if (!doc) return false;

    json_t *answer = json_obj_get(doc, "Answer");
    if (!answer || json_len(answer) == 0) {
        json_free(doc);
        return true;
    }

    /* First pass: count A records (type == 1) */
    size_t count = 0;
    for (size_t i = 0; i < json_len(answer); i++) {
        json_t *item = json_get(answer, i);
        if (!item) continue;
        double type = json_get_num(item, "type", -1);
        if (type == 1) count++;
    }

    if (count == 0) {
        json_free(doc);
        return true;
    }

    /* Allocate and fill */
    char **ips = (char **)xmalloc(count * sizeof(char *));
    size_t idx = 0;

    for (size_t i = 0; i < json_len(answer); i++) {
        json_t *item = json_get(answer, i);
        if (!item) continue;
        double type = json_get_num(item, "type", -1);
        if (type == 1) {
            const char *data = json_get_str(item, "data", "");
            if (data[0]) {
                ips[idx] = strdup(data);
                if (!ips[idx]) {
                    /* OOM */
                    for (size_t j = 0; j < idx; j++) free(ips[j]);
                    free(ips);
                    json_free(doc);
                    return false;
                }
                idx++;
            }
        }
    }

    json_free(doc);
    *out_ips = ips;
    *out_count = count;
    return true;
}

/* ================================================================
 *  telegram_query_doh
 *  Port of Python _query_doh_provider().
 *  Sends HTTP GET to a DNS-over-HTTPS provider, parses response.
 *  Returns A-record IPv4 addresses.
 * ================================================================ */

bool telegram_query_doh(const char *doh_url,
                        const char *hostname,
                        const char *extra_headers,
                        int timeout_sec,
                        char ***out_ips,
                        size_t *out_count)
{
    if (!out_ips || !out_count) return false;
    *out_ips = NULL;
    *out_count = 0;

    if (!doh_url || !hostname || !hostname[0]) return false;

    /* Build the URL with query params */
    size_t url_len = strlen(doh_url) + 64;
    char *url = (char *)xmalloc(url_len);
    snprintf(url, url_len, "%s?name=%s&type=A", doh_url, hostname);
    /* URL-encode: replace spaces with %20 (simplistic) */
    for (char *p = url; *p; p++) {
        if (*p == ' ') *p = '+';
    }

    /* Create HTTP client and send request */
    http_t *h = http_new(timeout_sec > 0 ? timeout_sec : 10);
    if (!h) { free(url); return false; }

    http_resp_t *resp = http_get(h, url, extra_headers);
    free(url);

    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return false;
    }

    bool ok = telegram_parse_doh_response(resp->body, out_ips, out_count);
    http_resp_free(resp);
    http_free(h);
    return ok;
}
