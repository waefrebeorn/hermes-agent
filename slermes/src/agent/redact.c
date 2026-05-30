/*
 * redact.c — P159: Secrets redaction for Hermes C.
 *
 * Pattern-based redaction (API keys, tokens, passwords, JWTs)
 * from tool output and logs. Uses pattern matching with wildcards.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Redaction Patterns
 * ================================================================ */

/* Max patterns and redaction entries */
#define MAX_REDACT_PATTERNS 64
#define REDACT_REPLACEMENT  "********"

/* Pattern entry */
typedef struct {
    char prefix[64];    /* Key-like prefix to match */
    char suffix[64];    /* Optional suffix for context */
    size_t min_len;     /* Minimum value length to redact */
    size_t max_show;    /* Characters from start to show before *** */
} redact_pattern_t;

/* Built-in patterns */
static const redact_pattern_t BUILTIN_PATTERNS[] = {
    {"api_key",          "",              6,  4},
    {"api-key",          "",              6,  4},
    {"apikey",           "",              6,  4},
    {"token",            "",              6,  4},
    {"secret",           "",              6,  4},
    {"password",         "",              4,  2},
    {"passwd",           "",              4,  2},
    {"authorization",    "",              6,  4},
    {"bearer ",          "",              8,  4},
    {"x-api-key",        "",              6,  4},
    {"x-auth-token",     "",              6,  4},
    {"ssh-private-key",  "",              8,  4},
    {"-----begin",       "-----",         20, 10},
    {"sk-",              "",              20, 4},  /* OpenAI keys */
    {"pk-",              "",              20, 4},  /* Project keys */
    {"ghp_",             "",              20, 4},  /* GitHub PAT */
    {"gho_",             "",              20, 4},  /* GitHub OAuth */
    {"ghu_",             "",              20, 4},  /* GitHub user token */
    {"ghs_",             "",              20, 4},  /* GitHub server-to-server */
    {"ghr_",             "",              20, 4},  /* GitHub refresh */
    {"xoxb-",            "",              20, 4},  /* Slack bot */
    {"xoxp-",            "",              20, 4},  /* Slack user */
    {"xapp-",            "",              20, 4},  /* Slack app */
    {"" /* JWT wildcard */,  "",          30, 6},  /* JWT-like patterns */
};

static int g_num_builtin = sizeof(BUILTIN_PATTERNS) / sizeof(BUILTIN_PATTERNS[0]);

/* Custom user patterns (loaded from config) */
static redact_pattern_t g_custom_patterns[MAX_REDACT_PATTERNS];
static int g_custom_count = 0;

/* ================================================================
 *  JWT Detection
 * ================================================================ */

/* Check if a token looks like a JWT (three base64 segments separated by dots) */
static bool looks_like_jwt(const char *start, size_t len) {
    if (len < 30) return false;

    int dots = 0;
    for (size_t i = 0; i < len && i < 200; i++) {
        if (start[i] == '.') dots++;
        if (dots > 2) break;
        /* Only base64url characters expected */
        if (!isalnum((unsigned char)start[i]) && start[i] != '-' &&
            start[i] != '_' && start[i] != '.')
            return false;
    }
    return dots >= 2;
}

/* ================================================================
 *  Pattern matching helpers
 * ================================================================ */

/* Check if prefix appears as a key:value pattern in text */
static const char *find_key_value(const char *text, const char *key,
                                   size_t key_len, size_t *val_len) {
    if (!text || !key || key_len == 0) return NULL;

    const char *p = text;
    while (*p) {
        /* Try to find key (case-insensitive) */
        const char *found = strstr(p, key);
        if (!found) return NULL;

        /* Check it's a key context (followed by =, :, ", or whitespace) */
        const char *after = found + key_len;
        if (*after == '=' || *after == ':' || *after == '"' || *after == '\'') {
            /* Skip separator */
            if (*after == '=' || *after == ':') after++;
            while (*after == ' ' || *after == '\t' || *after == '"' || *after == '\'')
                after++;

            /* Find end of value */
            const char *val_start = after;
            const char *val_end = after;
            while (*val_end && *val_end != ',' && *val_end != '\n' &&
                   *val_end != '\r' && *val_end != '}' && *val_end != ']' &&
                   *val_end != ' ' && *val_end != '\t')
                val_end++;

            *val_len = (size_t)(val_end - val_start);
            if (*val_len > 0) return val_start;
        }
        p = found + 1;
    }
    return NULL;
}

/* ================================================================
 *  Redaction engine
 * ================================================================ */

/* Redact a single value in-place within the string.
 * Returns the number of characters replaced. */
static size_t redact_value(char *text, const char *val_start, size_t val_len,
                           size_t max_show) {
    if (!text || !val_start || val_len == 0) return 0;

    size_t offset = (size_t)(val_start - text);
    size_t show = max_show < val_len ? max_show : val_len / 4;
    if (show > 32) show = 32;

    char redacted[512];
    int n;
    if (show > 0) {
        n = snprintf(redacted, sizeof(redacted), "%.*s***REDACTED***",
                     (int)show, val_start);
    } else {
        n = snprintf(redacted, sizeof(redacted), "***REDACTED***");
    }

    size_t redacted_len = (size_t)n;
    size_t remaining = strlen(text + offset + val_len) + 1;

    /* Check buffer bounds */
    if (offset + redacted_len + remaining > 65536)
        return 0;

    /* Shift and replace */
    memmove(text + offset + redacted_len, text + offset + val_len, remaining);
    memcpy(text + offset, redacted, redacted_len);

    return redacted_len > val_len ? redacted_len - val_len : 0;
}

/* Scan text for JWT-like tokens and redact them */
static size_t redact_jwts(char *text) {
    size_t total = 0;
    char *p = text;

    while (*p) {
        /* Look for potential JWT start (base64url char) */
        if (isalnum((unsigned char)*p)) {
            /* Check if this could be a JWT start */
            size_t remaining = strlen(p);
            size_t max_check = remaining < 300 ? remaining : 300;

            /* JWT starts with base64url, ends with base64url or = */
            size_t seg_len = 0;
            int dots = 0;
            for (size_t i = 0; i < max_check; i++) {
                if (p[i] == '.') {
                    dots++;
                    seg_len = 0;
                } else if (isalnum((unsigned char)p[i]) || p[i] == '-' || p[i] == '_' || p[i] == '=') {
                    seg_len++;
                } else {
                    break;
                }
            }

            if (dots >= 2 && looks_like_jwt(p, max_check)) {
                /* Find end of JWT */
                const char *end = p;
                while (*end && (isalnum((unsigned char)*end) || *end == '.' ||
                       *end == '-' || *end == '_' || *end == '='))
                    end++;

                size_t jwt_len = (size_t)(end - p);
                if (jwt_len >= 30) {
                    /* Redact: show first 10 + last 6 chars */
                    size_t show_front = 10;
                    size_t show_back = 6;
                    if (jwt_len > show_front + show_back) {
                        char redacted[512];
                        int n = snprintf(redacted, sizeof(redacted),
                            "%.*s...REDACTED_JWT...%.*s",
                            (int)show_front, p,
                            (int)show_back, end - show_back);

                        size_t rl = (size_t)n;
                        if (rl < jwt_len) {
                            size_t remaining_b = strlen(end) + 1;
                            memmove(p + rl, end, remaining_b);
                            memcpy(p, redacted, rl);
                            size_t diff = jwt_len - rl;
                            total += diff;
                            p = p + rl;
                            continue;
                        }
                    }
                }
            }
        }
        p++;
    }
    return total;
}

/* ================================================================
 *  Main redaction function
 * ================================================================ */

char *hermes_redact(const char *input) {
    if (!input) return NULL;

    /* Make a writable copy with extra space for redaction expansion */
    size_t len = strlen(input);
    if (len > 65536) len = 65536;
    size_t alloc_len = len + 512; /* Extra space for ***REDACTED*** expansion */
    char *result = (char *)malloc(alloc_len);
    if (!result) return NULL;
    memcpy(result, input, len);
    result[len] = '\0';

    /* Redact JWT tokens */
    redact_jwts(result);

    /* Redact built-in patterns */
    for (int pi = 0; pi < g_num_builtin; pi++) {
        const redact_pattern_t *pat = &BUILTIN_PATTERNS[pi];
        size_t key_len = strlen(pat->prefix);

        if (key_len == 0) continue; /* Skip empty (JWT handled above) */

        const char *p = result;
        while (*p) {
            size_t val_len = 0;
            const char *val_start = find_key_value(p, pat->prefix, key_len, &val_len);

            if (val_start && val_len >= pat->min_len) {
                size_t offset = (size_t)(val_start - result);
                redact_value(result, val_start, val_len, pat->max_show);
                p = result + offset + sizeof("***REDACTED***") - 1;
            } else {
                break;
            }
        }
    }

    /* Redact custom patterns */
    for (int pi = 0; pi < g_custom_count; pi++) {
        const redact_pattern_t *pat = &g_custom_patterns[pi];
        size_t key_len = strlen(pat->prefix);

        if (key_len == 0) continue;

        const char *p = result;
        while (*p) {
            size_t val_len = 0;
            const char *val_start = find_key_value(p, pat->prefix, key_len, &val_len);

            if (val_start && val_len >= pat->min_len) {
                size_t offset = (size_t)(val_start - result);
                redact_value(result, val_start, val_len, pat->max_show);
                p = result + offset + sizeof("***REDACTED***") - 1;
            } else {
                break;
            }
        }
    }

    return result;
}

/* ================================================================
 *  Custom pattern management
 * ================================================================ */

bool hermes_redact_add_pattern(const char *pattern) {
    if (!pattern || g_custom_count >= MAX_REDACT_PATTERNS) return false;

    redact_pattern_t *pat = &g_custom_patterns[g_custom_count];
    memset(pat, 0, sizeof(*pat));

    /* Parse pattern format: "prefix:min_len:max_show" */
    const char *colon = strchr(pattern, ':');
    if (colon) {
        size_t prefix_len = (size_t)(colon - pattern);
        if (prefix_len > 0 && prefix_len < sizeof(pat->prefix)) {
            memcpy(pat->prefix, pattern, prefix_len);
            pat->prefix[prefix_len] = '\0';

            const char *min_str = colon + 1;
            colon = strchr(min_str, ':');
            if (colon) {
                pat->min_len = (size_t)atol(min_str);
                pat->max_show = (size_t)atol(colon + 1);
            } else {
                pat->min_len = (size_t)atol(min_str);
                pat->max_show = 4;
            }
            g_custom_count++;
            return true;
        }
    } else {
        /* Just a prefix */
        snprintf(pat->prefix, sizeof(pat->prefix), "%s", pattern);
        pat->min_len = 6;
        pat->max_show = 4;
        g_custom_count++;
        return true;
    }

    return false;
}

void hermes_redact_clear_patterns(void) {
    g_custom_count = 0;
}

/* Load patterns from config string (comma-separated) */
void hermes_redact_load_config(const char *patterns_str) {
    if (!patterns_str) return;

    char copy[1024];
    snprintf(copy, sizeof(copy), "%s", patterns_str);

    char *tok = strtok(copy, ",");
    while (tok && g_custom_count < MAX_REDACT_PATTERNS) {
        /* Trim whitespace */
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && (*end == ' ' || *end == '\t')) *end-- = '\0';

        if (*tok)
            hermes_redact_add_pattern(tok);
        tok = strtok(NULL, ",");
    }
}
