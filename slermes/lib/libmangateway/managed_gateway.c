/*
 * managed_gateway.c — Generic managed-tool gateway helpers for
 * Nous-hosted vendor passthroughs.
 * Port of Python tools/managed_tool_gateway.py.
 */

#include "managed_gateway.h"
#include "tool_backend.h"  /* managed_nous_tools_enabled() */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

/* ─── Constants ─────────────────────────────────────────── */

#define DEFAULT_GATEWAY_DOMAIN "nousresearch.com"
#define DEFAULT_GATEWAY_SCHEME "https"
#define ACCESS_TOKEN_REFRESH_SKEW 120  /* seconds */

/* ─── Internal helpers ──────────────────────────────────── */

/* Case-insensitive comparison */
static int strcasecmp_safe(const char *a, const char *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && *b) {
        int ca = toupper((unsigned char)*a);
        int cb = toupper((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Read file content into malloc'd buffer. Caller must free. */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Simple JSON field extractor — finds "key": "value" or "key": value */
static int json_extract_string(const char *json, const char *key,
                                char *buf, size_t sz)
{
    if (!json || !key || !buf || sz == 0) return 0;

    /* Find "key": */
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);

    /* Skip whitespace and colon */
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p != ':') return 0;
    p++;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    /* Handle string or non-string value */
    if (*p == '"') {
        p++; /* skip opening quote */
        size_t pos = 0;
        while (*p && *p != '"' && pos < sz - 1) {
            if (*p == '\\' && *(p+1)) {
                p++; /* skip escape */
                if (pos < sz - 1) buf[pos++] = *p;
            } else {
                buf[pos++] = *p;
            }
            p++;
        }
        buf[pos] = '\0';
        return pos > 0 ? 1 : 0;
    }

    /* Non-string value (number, bool) */
    size_t pos = 0;
    while (*p && *p != ',' && *p != '}' && *p != '\n' && pos < sz - 1) {
        buf[pos++] = *p++;
    }
    buf[pos] = '\0';
    return pos > 0 ? 1 : 0;
}

/* ─── Auth JSON path ─────────────────────────────────────── */

void managed_gw_auth_json_path(const char *hermes_home,
                                char *buf, size_t sz)
{
    if (!buf || sz == 0) return;
    if (hermes_home && hermes_home[0]) {
        snprintf(buf, sz, "%s/auth.json", hermes_home);
    } else {
        const char *home = getenv("HOME");
        if (home)
            snprintf(buf, sz, "%s/.slermes/auth.json", home);
        else
            snprintf(buf, sz, "auth.json");
    }
}

/* ─── Access token reader ───────────────────────────────── */

bool managed_gw_read_access_token(const char *hermes_home,
                                   char *buf, size_t sz)
{
    if (!buf || sz == 0) return false;
    buf[0] = '\0';

    /* Priority 1: TOOL_GATEWAY_USER_TOKEN env var */
    const char *explicit = getenv("TOOL_GATEWAY_USER_TOKEN");
    if (explicit && explicit[0]) {
        strncpy(buf, explicit, sz - 1);
        buf[sz - 1] = '\0';
        return true;
    }

    /* Read auth.json */
    char path[1024];
    managed_gw_auth_json_path(hermes_home, path, sizeof(path));

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        return false;

    char *json = read_file(path);
    if (!json) return false;

    /* Extract access_token */
    char token[GW_TOKEN_MAX];
    if (!json_extract_string(json, "access_token", token, sizeof(token))) {
        /* Try nested: providers -> nous -> access_token */
        const char *providers = strstr(json, "\"providers\"");
        if (providers) {
            const char *nous = strstr(providers, "\"nous\"");
            if (nous) {
                json_extract_string(nous, "access_token", token, sizeof(token));
            }
        }
    }

    /* Extract expires_at for skew check */
    char expires_str[64];
    bool has_expires = json_extract_string(json, "expires_at",
                                            expires_str, sizeof(expires_str));

    free(json);

    if (!token[0]) return false;

    /* Check if token is expiring */
    if (has_expires && expires_str[0]) {
        /* Simple timestamp parse (ISO-8601 or Unix timestamp) */
        time_t expires_at = 0;
        char *end = NULL;
        expires_at = (time_t)strtoll(expires_str, &end, 10);
        if (end == expires_str || expires_at <= 0) {
            /* Not a number — try parsing as ISO-8601 date */
            /* Simple heuristic: token is valid */
        } else {
            time_t now = time(NULL);
            if (now + ACCESS_TOKEN_REFRESH_SKEW >= expires_at) {
                /* Token is expiring — still return it as fallback */
                strncpy(buf, token, sz - 1);
                buf[sz - 1] = '\0';
                return true;
            }
        }
    }

    strncpy(buf, token, sz - 1);
    buf[sz - 1] = '\0';
    return true;
}

/* ─── Scheme ────────────────────────────────────────────── */

const char *managed_gw_get_scheme(void)
{
    const char *scheme = getenv("TOOL_GATEWAY_SCHEME");
    if (scheme) {
        if (strcasecmp_safe(scheme, "http") == 0 ||
            strcasecmp_safe(scheme, "https") == 0)
            return scheme;
    }
    return DEFAULT_GATEWAY_SCHEME;
}

/* ─── URL builder ───────────────────────────────────────── */

void managed_gw_build_url(const char *vendor, char *buf, size_t sz)
{
    if (!buf || sz == 0) return;
    buf[0] = '\0';
    if (!vendor || !vendor[0]) return;

    /* Priority 1: <VENDOR>_GATEWAY_URL env var */
    char env_name[128];
    size_t i;
    for (i = 0; vendor[i] && i < sizeof(env_name) - 20; i++)
        env_name[i] = (char)toupper((unsigned char)vendor[i]);
    env_name[i] = '\0';
    strcat(env_name, "_GATEWAY_URL");

    const char *explicit = getenv(env_name);
    if (explicit && explicit[0]) {
        strncpy(buf, explicit, sz - 1);
        buf[sz - 1] = '\0';
        return;
    }

    /* Priority 2: TOOL_GATEWAY_DOMAIN env var */
    const char *domain = getenv("TOOL_GATEWAY_DOMAIN");
    if (!domain || !domain[0])
        domain = DEFAULT_GATEWAY_DOMAIN;

    const char *scheme = managed_gw_get_scheme();
    snprintf(buf, sz, "%s://%s-gateway.%s", scheme, vendor, domain);
}

/* ─── Resolver ──────────────────────────────────────────── */

bool managed_gw_resolve(const char *hermes_home,
                         const char *vendor,
                         managed_gateway_config_t *config)
{
    if (!config) return false;

    memset(config, 0, sizeof(*config));

    /* Check managed tools are enabled */
    if (!managed_nous_tools_enabled())
        return false;

    if (!vendor || !vendor[0])
        return false;

    strncpy(config->vendor, vendor, sizeof(config->vendor) - 1);

    /* Build gateway URL */
    managed_gw_build_url(vendor, config->gateway_origin,
                         sizeof(config->gateway_origin));
    if (!config->gateway_origin[0])
        return false;

    /* Read Nous access token */
    if (!managed_gw_read_access_token(hermes_home,
                                       config->nous_user_token,
                                       sizeof(config->nous_user_token)))
        return false;

    config->managed_mode = true;
    return true;
}

/* ─── Readiness check ───────────────────────────────────── */

bool managed_gw_is_ready(const char *hermes_home, const char *vendor)
{
    managed_gateway_config_t cfg;
    return managed_gw_resolve(hermes_home, vendor, &cfg);
}
