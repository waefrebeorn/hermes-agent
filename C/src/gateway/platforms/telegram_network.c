/*
 * telegram_network.c — Telegram network helpers for Hermes C.
 *
 * Port of Python gateway/platforms/telegram_network.py.
 * Provides: system DNS resolution, DoH queries, fallback IP discovery.
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes_telegram_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* POSIX networking */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

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
