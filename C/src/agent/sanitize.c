/*
 * sanitize.c — P166: Output sanitization for Hermes C.
 *
 * Strips sensitive data from tool results before LLM injection.
 * Works with the redact module to ensure outputs are clean.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Sanitization Rules
 * ================================================================ */

/* Maximum result size before truncation */
#define MAX_RESULT_SIZE (256 * 1024) /* 256 KB */

/* Environment variable names to redact from output */
static const char *ENV_VARS_TO_REDACT[] = {
    "API_KEY", "API_SECRET", "ACCESS_KEY", "SECRET_KEY",
    "PASSWORD", "PASSWD", "TOKEN", "AUTH_TOKEN",
    "BEARER", "AUTHORIZATION", "PRIVATE_KEY",
    "CLIENT_SECRET", "DB_PASSWORD", "REDIS_PASSWORD",
    "SSH_KEY", "SLACK_TOKEN", "DISCORD_TOKEN",
    "TELEGRAM_TOKEN", "OPENAI_API_KEY", "ANTHROPIC_API_KEY",
    NULL
};

/* File paths to redact from results (show path, hide contents) */
static const char *SENSITIVE_FILE_PATTERNS[] = {
    "/.ssh/", "/.gnupg/", "/.aws/", "/.azure/",
    "/.config/", "/.slermes/", "/.env",
    "id_rsa", "id_ed25519", "known_hosts",
    NULL
};

/* ================================================================
 *  Sanitization Functions
 * ================================================================ */

/* Truncate result to maximum size */
static char *truncate_result(const char *input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    if (len <= MAX_RESULT_SIZE) {
        return strdup(input);
    }

    char *result = (char *)malloc(MAX_RESULT_SIZE + 128);
    if (!result) return NULL;

    memcpy(result, input, MAX_RESULT_SIZE / 2);
    snprintf(result + MAX_RESULT_SIZE / 2, 128,
             "\n\n[... TRUNCATED: %zu bytes total, showing first %u ...]\n",
             len, (unsigned int)(MAX_RESULT_SIZE / 2));

    return result;
}

/* Redact environment variable assignments from text */
static char *redact_env_vars(const char *input) {
    if (!input) return NULL;

    /* Count lines first for allocation */
    size_t len = strlen(input);
    char *result = strdup(input);
    if (!result) return NULL;

    /* Look for pattern: VAR_NAME=value or export VAR_NAME=value */
    char *p = result;
    while (*p) {
        /* Look for potential env var */
        /* Pattern: starts with uppercase or underscore, has = */
        if (isupper((unsigned char)*p) || *p == '_') {
            /* Find end of variable name */
            const char *var_start = p;
            const char *var_end = p;
            while (isalnum((unsigned char)*var_end) || *var_end == '_')
                var_end++;

            /* Check if followed by = */
            if (*var_end == '=') {
                /* Check if this var should be redacted */
                size_t vlen = (size_t)(var_end - var_start);
                for (int i = 0; ENV_VARS_TO_REDACT[i]; i++) {
                    if (strncasecmp(var_start, ENV_VARS_TO_REDACT[i], vlen) == 0) {
                        /* Redact the value */
                        const char *val_start = var_end + 1;
                        const char *val_end = val_start;
                        while (*val_end && *val_end != '\n' && *val_end != '\r')
                            val_end++;

                        size_t vstart_off = (size_t)(val_start - result);
                        size_t vlen_orig = (size_t)(val_end - val_start);
                        const char *replacement = "***REDACTED***";
                        size_t rlen = strlen(replacement);

                        if (rlen < vlen_orig) {
                            memcpy(result + vstart_off, replacement, rlen);
                            memmove(result + vstart_off + rlen, val_end,
                                    strlen(val_end) + 1);
                            p = result + vstart_off + rlen;
                        } else {
                            /* Replacement is longer — realloc to fit */
                            char temp[4096];
                            snprintf(temp, sizeof(temp), "%.*s%s%s",
                                     (int)(val_start - result), result,
                                     replacement, val_end);
                            size_t tlen = strlen(temp);
                            if (tlen > len) {
                                char *new_result = (char *)realloc(result, tlen + 64);
                                if (!new_result) return result;
                                /* Recalculate offset in new buffer */
                                size_t old_start_off = (size_t)(val_start - result);
                                result = new_result;
                                memcpy(result, temp, tlen + 1);
                                p = result + old_start_off + rlen;
                            } else {
                                memcpy(result, temp, tlen + 1);
                                p = result + vstart_off + rlen;
                            }
                        }
                        break;
                    }
                }
            }
        }
        p++;
    }

    return result;
}

/* Remove sensitive file contents from results */
static char *sanitize_file_paths(const char *input) {
    if (!input) return NULL;

    char *result = strdup(input);
    if (!result) return NULL;

    /* Replace sensitive file paths with redacted markers */
    for (int i = 0; SENSITIVE_FILE_PATTERNS[i]; i++) {
        const char *pattern = SENSITIVE_FILE_PATTERNS[i];
        char *p = result;
        while ((p = strstr(p, pattern)) != NULL) {
            /* Create redacted marker */
            char before[4096];
            snprintf(before, sizeof(before), "%.*s[REDACTED SENSITIVE FILE]",
                     (int)(p - result), result);
            /* Find end of line */
            char *eol = p;
            while (*eol && *eol != '\n') eol++;
            char *rest = eol;
            size_t new_len = strlen(before) + strlen(rest) + 1;
            if (new_len < 8192) {
                char *new_result = (char *)malloc(new_len);
                if (new_result) {
                    snprintf(new_result, new_len, "%s%s", before, rest);
                    free(result);
                    result = new_result;
                }
            }
            break; /* Only handle first match per pattern */
        }
    }

    return result;
}

/* ================================================================
 *  Main Sanitization Entry Point
 * ================================================================ */

char *hermes_sanitize_output(const char *tool_name, const char *raw_output) {
    if (!raw_output) return NULL;
    if (!tool_name) return strdup(raw_output);

    char *result = NULL;

    /* Step 1: Truncate */
    result = truncate_result(raw_output);
    if (!result) return strdup("{\"error\": \"OOM during sanitization\"}");

    /* Step 2: Redact env vars from results */
    char *env_cleaned = redact_env_vars(result);
    if (env_cleaned) {
        free(result);
        result = env_cleaned;
    }

    /* Step 3: Sanitize file paths */
    char *file_cleaned = sanitize_file_paths(result);
    if (file_cleaned) {
        free(result);
        result = file_cleaned;
    }

    /* Step 4: Redact secrets using hermes_redact */
    char *redacted = hermes_redact(result);
    if (redacted) {
        free(result);
        result = redacted;
    }

    return result;
}
