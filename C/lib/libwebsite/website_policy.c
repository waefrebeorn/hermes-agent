/*
 * website_policy.c — Website access policy checking.
 * Port of Python tools/website_policy.py.
 *
 * Checks URLs against a user-managed website blocklist with
 * fnmatch glob pattern support. Used by web/browser tools to
 * enforce URL access policy.
 */

#define _GNU_SOURCE
#include "website_policy.h"

#include <ctype.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Helpers ────────────────────────────────────────────── */

static char *strdup_null(const char *s)
{
    if (!s) return NULL;
    return strdup(s);
}

static void trim_newline(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

static char *lowercase_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[len] = '\0';
    return out;
}

/* ─── Policy Init / Free ─────────────────────────────────── */

void website_policy_init(website_policy_t *policy)
{
    if (!policy) return;
    policy->enabled = true;
    policy->rule_count = 0;
    for (int i = 0; i < WEBSITE_MAX_RULES; i++) {
        policy->rules[i].pattern = NULL;
        policy->rules[i].source = NULL;
    }
}

void website_policy_free(website_policy_t *policy)
{
    if (!policy) return;
    for (int i = 0; i < policy->rule_count; i++) {
        free(policy->rules[i].pattern);
        free(policy->rules[i].source);
    }
    policy->rule_count = 0;
}

/* ─── Rule Management ────────────────────────────────────── */

int website_add_rule(website_policy_t *policy,
                     const char *pattern, const char *source)
{
    if (!policy || !pattern) return -1;
    if (policy->rule_count >= WEBSITE_MAX_RULES) return -1;

    policy->rules[policy->rule_count].pattern = lowercase_strdup(pattern);
    policy->rules[policy->rule_count].source = strdup_null(source);
    if (!policy->rules[policy->rule_count].pattern) return -1;

    policy->rule_count++;
    return 0;
}

int website_load_rules_from_file(website_policy_t *policy, const char *path)
{
    if (!policy || !path) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    int loaded = 0;
    char line[4096];

    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);

        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#')
            continue;

        if (website_add_rule(policy, line, path) == 0)
            loaded++;
    }

    fclose(f);
    return loaded;
}

/* ─── URL Host Extraction ────────────────────────────────── */

char *website_extract_host(const char *url)
{
    if (!url || !*url) return NULL;

    /* Skip scheme */
    const char *host_start = url;
    const char *scheme = strstr(url, "://");
    if (scheme) {
        host_start = scheme + 3;
    } else if (strncmp(url, "//", 2) == 0) {
        host_start = url + 2;
    }

    /* Skip to end of host (before path, query, fragment) */
    const char *end = host_start;
    while (*end && *end != '/' && *end != '?' && *end != '#' && *end != ':')
        end++;

    if (end == host_start) return NULL;

    size_t len = (size_t)(end - host_start);
    char *host = (char *)malloc(len + 1);
    if (!host) return NULL;

    for (size_t i = 0; i < len; i++)
        host[i] = (char)tolower((unsigned char)host_start[i]);
    host[len] = '\0';

    /* Strip trailing dot */
    if (len > 0 && host[len - 1] == '.')
        host[len - 1] = '\0';

    /* Strip leading www. */
    if (strncmp(host, "www.", 4) == 0) {
        size_t rest = len - 4;
        memmove(host, host + 4, rest + 1);
    }

    return host;
}

/* ─── Pattern Matching ───────────────────────────────────── */

bool website_match_host(const char *host, const char *pattern)
{
    if (!host || !pattern) return false;

    /* Try fnmatch first (supports wildcards like *.example.com) */
    if (fnmatch(pattern, host, FNM_CASEFOLD) == 0)
        return true;

    /* For patterns starting with *. (e.g. *.example.com),
     * also match the bare domain (example.com) */
    if (pattern[0] == '*' && pattern[1] == '.') {
        const char *bare = pattern + 2;
        if (strcasecmp(host, bare) == 0)
            return true;
        /* Also match sub.sub.example.com */
        size_t bare_len = strlen(bare);
        size_t host_len = strlen(host);
        if (host_len > bare_len + 1
            && strcasecmp(host + host_len - bare_len, bare) == 0
            && host[host_len - bare_len - 1] == '.')
            return true;
    }

    /* For plain patterns (e.g. badhost.org), also match sub.badhost.org */
    size_t plen = strlen(pattern);
    size_t hlen = strlen(host);
    if (hlen > plen
        && host[hlen - plen - 1] == '.'
        && strcasecmp(host + hlen - plen, pattern) == 0)
        return true;

    return false;
}

/* ─── Main Check API ─────────────────────────────────────── */

website_block_t *website_check_access(const char *url,
                                       const website_policy_t *policy)
{
    if (!url || !policy || !policy->enabled || policy->rule_count == 0)
        return NULL;

    char *host = website_extract_host(url);
    if (!host) return NULL;

    website_block_t *block = NULL;

    for (int i = 0; i < policy->rule_count; i++) {
        if (!policy->rules[i].pattern) continue;

        if (website_match_host(host, policy->rules[i].pattern)) {
            block = (website_block_t *)calloc(1, sizeof(website_block_t));
            if (!block) break;

            block->url = strdup(url);
            block->host = strdup(host);
            block->rule = strdup(policy->rules[i].pattern);
            block->source = strdup_null(policy->rules[i].source);

            /* Build message dynamically */
            size_t msg_len = strlen(host) + strlen(policy->rules[i].pattern) + 80;
            if (policy->rules[i].source)
                msg_len += strlen(policy->rules[i].source);

            block->message = (char *)calloc(1, msg_len);
            if (block->message) {
                snprintf(block->message, msg_len,
                    "Blocked by website policy: '%s' matched rule '%s'%s%s",
                    host, policy->rules[i].pattern,
                    policy->rules[i].source ? " from " : "",
                    policy->rules[i].source ? policy->rules[i].source : "");
            }
            break;
        }
    }

    free(host);
    return block;
}

void website_block_free(website_block_t *block)
{
    if (!block) return;
    free(block->url);
    free(block->host);
    free(block->rule);
    free(block->source);
    free(block->message);
    free(block);
}
