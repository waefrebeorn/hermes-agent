/*
 * sanitize.c — P166: Output sanitization for Hermes C.
 *
 * Strips sensitive data from tool results before LLM injection.
 * Works with the redact module to ensure outputs are clean.
 */

#include "hermes.h"
#include "hermes_json.h"
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
                                result = new_result;
                                memcpy(result, temp, tlen + 1);
                                p = result + vstart_off + rlen;
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
 *  B34: Surrogate Character Sanitization
 * ================================================================ */

/* Replace lone UTF-8 surrogate code points (U+D800-U+DFFF) with U+FFFD.
 * In UTF-8, surrogates are 3-byte sequences: 0xED [0xA0-0xBF] [0x80-0xBF].
 * U+FFFD encodes as EF BF BD.
 * Returns malloc'd string; caller must free. */
char *sanitize_surrogates(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    /* Worst case: all surrogates replaced with same-length replacement */
    char *result = strdup(text);
    if (!result) return NULL;

    char *write = result;
    const unsigned char *read = (const unsigned char *)text;

    while (*read) {
        if (read[0] == 0xED && read[1] >= 0xA0 && read[1] <= 0xBF
            && read[2] >= 0x80 && read[2] <= 0xBF) {
            /* Replace surrogate with U+FFFD (EF BF BD) */
            *write++ = (char)0xEF;
            *write++ = (char)0xBF;
            *write++ = (char)0xBD;
            read += 3;
        } else {
            *write++ = (char)*read++;
        }
    }
    *write = '\0';

    /* Only realloc if we actually changed something */
    size_t new_len = (size_t)(write - result);
    if (new_len < len) {
        char *shrunk = (char *)realloc(result, new_len + 1);
        if (shrunk) result = shrunk;
    }

    return result;
}

/* Recursively walk a json_t object and replace surrogate characters
 * in all string values. Returns true if any surrogates were replaced.
 * Mutates the json_t tree in-place. */
static bool walk_json(json_t *node) {
    if (!node) return false;
    bool found = false;

    if (node->type == JSON_STRING) {
        char *fixed = sanitize_surrogates(node->str_val);
        if (fixed && strcmp(fixed, node->str_val) != 0) {
            free(node->str_val);
            node->str_val = fixed;
            found = true;
        } else if (fixed) {
            free(fixed);
        }
        return found;
    }

    if (node->type == JSON_OBJECT) {
        for (size_t i = 0; i < node->c.count; i++) {
            /* Fix keys too (unlikely but defensive) */
            char *kfix = sanitize_surrogates(node->c.keys[i]);
            if (kfix && strcmp(kfix, node->c.keys[i]) != 0) {
                free(node->c.keys[i]);
                node->c.keys[i] = kfix;
                found = true;
            } else if (kfix) {
                free(kfix);
            }
            if (walk_json(node->c.items[i]))
                found = true;
        }
        return found;
    }

    if (node->type == JSON_ARRAY) {
        for (size_t i = 0; i < node->c.count; i++) {
            if (walk_json(node->c.items[i]))
                found = true;
        }
        return found;
    }

    return false;
}

bool sanitize_json_surrogates(void *json_obj) {
    return walk_json((json_t *)json_obj);
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

/* ================================================================
 *  P180: JSON Repair (port of Python message_sanitization.py)
 * ================================================================ */

/* Check if character at position i in str is escaped (odd number of preceding backslashes) */
static bool repair_is_escaped(const char *str, int i) {
    int bs = 0;
    for (int j = i - 1; j >= 0 && str[j] == '\\'; j--) bs++;
    return (bs % 2) == 1;
}

/* Escape unescaped control chars (0x00-0x1F) inside JSON string values.
 * Returns malloc'd string, or NULL on error. */
static char *escape_control_chars(const char *raw) {
    if (!raw) return NULL;
    size_t n = strlen(raw);
    size_t cap = n * 6 + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t pos = 0;
    bool in_str = false;

    for (size_t i = 0; i < n && pos < cap - 8; i++) {
        char ch = raw[i];
        if (in_str) {
            if (ch == '\\' && i + 1 < n) {
                out[pos++] = ch;
                out[pos++] = raw[++i];
                continue;
            }
            if (ch == '"' && !repair_is_escaped(raw, (int)i)) {
                in_str = false;
                out[pos++] = ch;
            } else if ((unsigned char)ch < 0x20) {
                pos += snprintf(out + pos, cap - pos, "\\u%04x", (unsigned char)ch);
            } else {
                out[pos++] = ch;
            }
        } else {
            if (ch == '"') in_str = true;
            out[pos++] = ch;
        }
    }
    out[pos] = '\0';
    return out;
}

/* Strips trailing commas before } or ]. Returns malloc'd copy. */
static char *strip_trailing_commas(const char *s, size_t len) {
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t w = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == ',') {
            size_t j = i + 1;
            while (j < len && (s[j] == ' ' || s[j] == '\t' || s[j] == '\n' || s[j] == '\r'))
                j++;
            if (j < len && (s[j] == '}' || s[j] == ']'))
                continue; /* skip trailing comma */
        }
        out[w++] = s[i];
    }
    out[w] = '\0';
    return out;
}

/* Attempt to repair malformed tool_call argument JSON.
 * Port of Python agent/message_sanitization.py:_repair_tool_call_arguments().
 * Returns malloc'd string — caller must free. Never returns NULL. */
char *repair_tool_call_arguments(const char *raw_args) {
    if (!raw_args) return strdup("{}");

    /* Strip leading/trailing whitespace */
    while (*raw_args == ' ' || *raw_args == '\t' || *raw_args == '\n' || *raw_args == '\r')
        raw_args++;
    const char *rend = raw_args + strlen(raw_args);
    while (rend > raw_args && (rend[-1] == ' ' || rend[-1] == '\t' || rend[-1] == '\n' || rend[-1] == '\r'))
        rend--;

    size_t len = (size_t)(rend - raw_args);
    if (len == 0) return strdup("{}");

    /* Handle "None" literal */
    if (len == 4 && memcmp(raw_args, "None", 4) == 0)
        return strdup("{}");

    /* Copy to mutable buffer + room for expansion */
    size_t cap = len * 2 + 32;
    char *buf = malloc(cap);
    if (!buf) return strdup("{}");
    memcpy(buf, raw_args, len);
    buf[len] = '\0';

    /* 1. Escape control chars inside JSON strings */
    char *escaped = escape_control_chars(buf);
    if (escaped) {
        free(buf);
        buf = escaped;
        len = strlen(buf);
        cap = len * 2 + 32;
    }

    /* 2. Strip trailing commas */
    char *no_trailing = strip_trailing_commas(buf, len);
    if (no_trailing) {
        free(buf);
        buf = no_trailing;
        len = strlen(buf);
    }

    /* 3. Close unclosed braces/brackets */
    bool in_str = false;
    int open_curly = 0, open_bracket = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '"' && !repair_is_escaped(buf, (int)i)) in_str = !in_str;
        if (in_str) continue;
        if (buf[i] == '{') open_curly++;
        else if (buf[i] == '}') open_curly--;
        else if (buf[i] == '[') open_bracket++;
        else if (buf[i] == ']') open_bracket--;
    }
    if (open_curly > 0 || open_bracket > 0) {
        size_t extra = (size_t)(open_curly + open_bracket);
        char *exp = realloc(buf, len + extra + 1);
        if (!exp) { free(buf); return strdup("{}"); }
        buf = exp;
        for (int i = 0; i < open_curly; i++) buf[len++] = '}';
        for (int i = 0; i < open_bracket; i++) buf[len++] = ']';
        buf[len] = '\0';
    }

    /* 4. Remove excess closing braces/brackets (bounded 50 iterations) */
    for (int iter = 0; iter < 50; iter++) {
        in_str = false;
        int cur = 0, brk = 0;
        for (size_t i = 0; i < len; i++) {
            if (buf[i] == '"' && !repair_is_escaped(buf, (int)i)) in_str = !in_str;
            if (in_str) continue;
            if (buf[i] == '{') cur++;
            else if (buf[i] == '}') cur--;
            else if (buf[i] == '[') brk++;
            else if (buf[i] == ']') brk--;
        }
        if (cur >= 0 && brk >= 0) break;
        /* Remove last excess closer */
        for (int i = (int)len - 1; i >= 0; i--) {
            if (buf[i] == '}' && cur < 0) {
                memmove(buf + i, buf + i + 1, len - (size_t)i);
                len--; cur++;
                break;
            }
            if (buf[i] == ']' && brk < 0) {
                memmove(buf + i, buf + i + 1, len - (size_t)i);
                len--; brk++;
                break;
            }
        }
    }
    buf[len] = '\0';
    return buf;
}
