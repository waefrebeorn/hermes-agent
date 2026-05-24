/*
 * account_usage.c — Provider account usage tracking.
 * Port of Python agent/account_usage.py (326 lines).
 *
 * Fetches usage data from provider APIs and renders for display.
 * Supports: openrouter, openai-codex, anthropic.
 */
#define _GNU_SOURCE
#include "hermes_account_usage.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

/*
 *  Helpers (port of Python helper functions)
 */

/* Parse a datetime value to unix timestamp. Returns 0 on failure. */
static int64_t parse_dt(const char *value) {
    if (!value || !value[0]) return 0;

    struct tm tm = {0};
    const char *p = strptime(value, "%Y-%m-%dT%H:%M:%S", &tm);
    if (p) {
        tm.tm_isdst = -1;
        time_t t = timegm(&tm);
        return (int64_t)t;
    }
    return 0;
}

/* Format a reset timestamp for display. Returns a static buffer. */
static const char *format_reset(int64_t reset_at) {
    static char buf[256];
    if (reset_at <= 0) return "unknown";

    time_t now = time(NULL);
    time_t reset = (time_t)reset_at;
    double diff_sec = difftime(reset, now);

    struct tm *lt = localtime(&reset);
    char time_str[64] = "";
    if (lt) strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M %Z", lt);

    if (diff_sec <= 0) {
        snprintf(buf, sizeof(buf), "now (%s)", time_str);
        return buf;
    }

    int total_sec = (int)diff_sec;
    int hours = total_sec / 3600;
    int minutes = (total_sec % 3600) / 60;

    char rel[64];
    if (hours >= 24) {
        int days = hours / 24;
        hours = hours % 24;
        snprintf(rel, sizeof(rel), "in %dd %dh", days, hours);
    } else if (hours > 0) {
        snprintf(rel, sizeof(rel), "in %dh %dm", hours, minutes);
    } else {
        snprintf(rel, sizeof(rel), "in %dm", minutes);
    }

    snprintf(buf, sizeof(buf), "%s (%s)", rel, time_str);
    return buf;
}

/*
 *  Provider-specific fetchers
 */

static account_usage_snapshot_t *fetch_openrouter(const char *base_url,
    const char *api_key)
{
    if (!api_key || !api_key[0]) return NULL;

    char credits_url[512], key_url[512];
    const char *root = (base_url && base_url[0])
        ? base_url : "https://openrouter.ai/api/v1";
    (void)snprintf(credits_url, sizeof(credits_url), "%s/credits", root);
    (void)snprintf(key_url, sizeof(key_url), "%s/key", root);

    http_t *h = http_new(10);
    if (!h) return NULL;

    char auth_header[1024];
    (void)snprintf(auth_header, sizeof(auth_header),
        "Authorization: Bearer %s\r\n"
        "Accept: application/json\r\n",
        api_key);

    http_resp_t *credits_resp = http_get(h, credits_url, auth_header);
    if (!credits_resp || credits_resp->status != 200) {
        if (credits_resp) http_resp_free(credits_resp);
        http_free(h);
        return NULL;
    }

    json_t *credits_data = json_parse(credits_resp->body, NULL);
    http_resp_free(credits_resp);
    if (!credits_data) { http_free(h); return NULL; }

    json_t *credits = json_obj_get(credits_data, "data");
    if (!credits) { json_free(credits_data); http_free(h); return NULL; }

    double total_credits = json_get_num(credits, "total_credits", 0);
    double total_usage = json_get_num(credits, "total_usage", 0);

    json_t *key_root = NULL;
    http_resp_t *key_resp = http_get(h, key_url, auth_header);
    if (key_resp && key_resp->status == 200) {
        key_root = json_parse(key_resp->body, NULL);
        http_resp_free(key_resp);
    } else if (key_resp) {
        http_resp_free(key_resp);
    }

    http_free(h);

    json_t *key_data = NULL;
    if (key_root) {
        key_data = json_obj_get(key_root, "data");
        if (!key_data) key_data = key_root;
    }

    account_usage_snapshot_t *snap = calloc(1, sizeof(*snap));
    if (!snap) { json_free(credits_data); if (key_root) json_free(key_root); return NULL; }

    (void)snprintf(snap->provider, sizeof(snap->provider), "openrouter");
    (void)snprintf(snap->source, sizeof(snap->source), "credits_api");
    snap->fetched_at = (int64_t)time(NULL);
    (void)snprintf(snap->title, sizeof(snap->title), "Account limits");

    double balance = total_credits - total_usage;
    if (balance < 0) balance = 0;
    (void)snprintf(snap->details[0], sizeof(snap->details[0]),
        "Credits balance: $%.2f", balance);
    snap->detail_count = 1;

    if (key_data) {
        double limit = json_get_num(key_data, "limit", 0);
        double limit_remaining = json_get_num(key_data, "limit_remaining", 0);
        if (limit > 0 && limit_remaining >= 0 && limit_remaining <= limit) {
            double used_pct = ((limit - limit_remaining) / limit) * 100.0;
            (void)snprintf(snap->windows[0].label, sizeof(snap->windows[0].label),
                "API key quota");
            snap->windows[0].used_percent = used_pct;
            const char *limit_reset = json_get_str(key_data, "limit_reset", NULL);
            if (limit_reset) snap->windows[0].reset_at = parse_dt(limit_reset);
            snap->window_count = 1;
        }

        double usage = json_get_num(key_data, "usage", -1);
        if (usage >= 0) {
            (void)snprintf(snap->details[snap->detail_count],
                sizeof(snap->details[snap->detail_count]),
                "API key usage: $%.2f total", usage);
            snap->detail_count++;
        }
    }

    snap->available = (snap->window_count > 0 || snap->detail_count > 0);

    json_free(credits_data);
    if (key_root) json_free(key_root);
    return snap;
}

/*
 *  Public API
 */

account_usage_snapshot_t *account_usage_fetch(const char *provider,
    const char *base_url, const char *api_key)
{
    if (!provider || !provider[0]) return NULL;

    if (strcmp(provider, "openrouter") == 0)
        return fetch_openrouter(base_url, api_key);

    /* TODO: port _fetch_codex_account_usage, _fetch_anthropic_account_usage */

    return NULL;
}

void account_usage_free(account_usage_snapshot_t *snap) {
    free(snap);
}

/*
 *  Render
 */

static char *str_alloc(const char *s) {
    if (!s) return NULL;
    return strdup(s);
}

char **account_usage_render(const account_usage_snapshot_t *snap,
    bool markdown)
{
    if (!snap) {
        char **empty = calloc(1, sizeof(char*));
        return empty;
    }

    int max_lines = 2 + snap->window_count + snap->detail_count + 2;
    char **lines = calloc(max_lines + 1, sizeof(char*));
    if (!lines) return NULL;

    int idx = 0;

    /* Header */
    int unused = 0;
    if (markdown)
        unused = asprintf(&lines[idx], "\360\237\223\210 **%s**", snap->title);
    else
        unused = asprintf(&lines[idx], "\360\237\223\210 %s", snap->title);
    (void)unused;
    idx++;

    /* Provider line */
    unused = 0;
    if (snap->plan[0])
        unused = asprintf(&lines[idx], "Provider: %s (%s)", snap->provider, snap->plan);
    else
        unused = asprintf(&lines[idx], "Provider: %s", snap->provider);
    (void)unused;
    idx++;

    /* Windows */
    for (int i = 0; i < snap->window_count && i < ACCOUNT_USAGE_WINDOWS_MAX; i++) {
        const account_usage_window_t *w = &snap->windows[i];
        char base[512];

        if (w->used_percent < 0) {
            (void)snprintf(base, sizeof(base), "%s: unavailable", w->label);
        } else {
            int remaining = (int)fmax(0, round(100.0 - w->used_percent));
            int used2 = (int)fmax(0, round(w->used_percent));
            (void)snprintf(base, sizeof(base),
                "%s: %d%% remaining (%d%% used)", w->label, remaining, used2);
        }

        unused = 0;
        if (w->reset_at > 0)
            unused = asprintf(&lines[idx], "%s \342\200\242 resets %s", base, format_reset(w->reset_at));
        else if (w->detail[0])
            unused = asprintf(&lines[idx], "%s \342\200\242 %s", base, w->detail);
        else
            lines[idx] = str_alloc(base);
        (void)unused;
        idx++;
    }

    /* Details */
    for (int i = 0; i < snap->detail_count && i < ACCOUNT_USAGE_DETAILS_MAX; i++) {
        lines[idx] = str_alloc(snap->details[i]);
        idx++;
    }

    /* Unavailable reason */
    if (snap->unavailable_reason[0]) {
        unused = 0;
        unused = asprintf(&lines[idx], "Unavailable: %s", snap->unavailable_reason);
        (void)unused;
        idx++;
    }

    return lines;
}

void account_usage_free_lines(char **lines) {
    if (!lines) return;
    for (int i = 0; lines[i]; i++) free(lines[i]);
    free(lines);
}
