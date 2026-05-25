/*
 * skill_preprocessing.c — SKILL.md preprocessing for Hermes C.
 * Port of Python agent/skill_preprocessing.py (139 lines).
 *
 * Provides template variable substitution and inline shell execution
 * for SKILL.md content loaded by the skills system.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/wait.h>

#define INLINE_SHELL_MAX_OUTPUT 4000

/* ================================================================
 *  Template variable substitution
 * ================================================================ */

/* Replace ${HERMES_SKILL_DIR} and ${HERMES_SESSION_ID} tokens.
 * Returns malloc'd string (caller must free) or NULL on error.
 * Unresolved tokens are left in place. */
char *skill_substitute_template_vars(const char *content,
                                      const char *skill_dir,
                                      const char *session_id) {
    if (!content) return NULL;

    /* Count tokens to estimate output size */
    size_t out_cap = strlen(content) + 1024;
    char *out = malloc(out_cap);
    if (!out) return NULL;
    size_t pos = 0;

    const char *p = content;
    while (*p) {
        const char *dollar = strstr(p, "${");
        if (!dollar) {
            /* No more tokens: copy rest */
            size_t remaining = strlen(p);
            if (pos + remaining + 1 > out_cap) {
                out_cap = pos + remaining + 1024;
                char *nb = realloc(out, out_cap);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            memcpy(out + pos, p, remaining);
            pos += remaining;
            break;
        }

        /* Copy text before ${ */
        size_t before = (size_t)(dollar - p);
        if (pos + before + 1 > out_cap) {
            out_cap = pos + before + 1024;
            char *nb = realloc(out, out_cap);
            if (!nb) { free(out); return NULL; }
            out = nb;
        }
        memcpy(out + pos, p, before);
        pos += before;
        p = dollar;

        /* Find closing } */
        const char *close = strchr(p + 2, '}');
        if (!close) {
            /* No closing brace: copy rest verbatim */
            size_t remaining = strlen(p);
            if (pos + remaining + 1 > out_cap) {
                out_cap = pos + remaining + 1024;
                char *nb = realloc(out, out_cap);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            memcpy(out + pos, p, remaining);
            pos += remaining;
            break;
        }

        /* Extract token name */
        size_t token_len = (size_t)(close - p - 2);
        char token[256];
        if (token_len >= sizeof(token)) token_len = sizeof(token) - 1;
        memcpy(token, p + 2, token_len);
        token[token_len] = '\0';

        /* Substitute if we have a value */
        const char *replacement = NULL;
        if (strcmp(token, "HERMES_SKILL_DIR") == 0)
            replacement = skill_dir;
        else if (strcmp(token, "HERMES_SESSION_ID") == 0)
            replacement = session_id;

        if (replacement) {
            size_t rlen = strlen(replacement);
            if (pos + rlen + 1 > out_cap) {
                out_cap = pos + rlen + 1024;
                char *nb = realloc(out, out_cap);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            memcpy(out + pos, replacement, rlen);
            pos += rlen;
        } else {
            /* Keep original token */
            size_t tlen = (size_t)(close - p + 1);
            if (pos + tlen + 1 > out_cap) {
                out_cap = pos + tlen + 1024;
                char *nb = realloc(out, out_cap);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            memcpy(out + pos, p, tlen);
            pos += tlen;
        }

        p = close + 1;
    }

    out[pos] = '\0';
    return out;
}

/* ================================================================
 *  Inline shell execution
 * ================================================================ */

/* Execute a command via popen and return its stdout.
 * Returns malloc'd string (caller must free). On error, returns
 * an error marker string (also malloc'd). */
static char *run_inline_shell(const char *command, int timeout_sec) {
    if (!command || !command[0])
        return strdup("");

    /* Build command with timeout wrapper */
    char cmd_buf[16384];
    snprintf(cmd_buf, sizeof(cmd_buf),
             "timeout %d bash -c '%s' 2>&1",
             timeout_sec > 0 ? timeout_sec : 10,
             command);

    FILE *fp = popen(cmd_buf, "r");
    if (!fp) {
        char err[128];
        snprintf(err, sizeof(err),
                 "[inline-shell error: popen failed]");
        return strdup(err);
    }

    char *output = malloc(INLINE_SHELL_MAX_OUTPUT + 128);
    if (!output) { pclose(fp); return NULL; }

    size_t pos = 0;
    int ch;
    while ((ch = fgetc(fp)) != EOF && pos < INLINE_SHELL_MAX_OUTPUT) {
        output[pos++] = (char)ch;
    }
    output[pos] = '\0';

    int status = pclose(fp);

    /* Trim trailing newlines */
    while (pos > 0 && output[pos - 1] == '\n')
        output[--pos] = '\0';

    /* On timeout, return error marker */
    if (status != 0 && pos == 0) {
        char err[256];
        snprintf(err, sizeof(err),
                 "[inline-shell error: exit code %d]",
                 WEXITSTATUS(status));
        free(output);
        return strdup(err);
    }

    return output;
}

/* ================================================================
 *  Expand inline shell snippets
 * ================================================================ */

/* Find and expand `!`cmd`` snippets in content.
 * Returns malloc'd string (caller must free). */
char *skill_expand_inline_shell(const char *content, int timeout_sec) {
    if (!content || !strstr(content, "!`"))
        return content ? strdup(content) : NULL;

    /* Estimate output capacity */
    size_t out_cap = strlen(content) + 4096;
    char *out = malloc(out_cap);
    if (!out) return NULL;
    size_t pos = 0;

    const char *p = content;
    while (*p) {
        /* Find !` */
        const char *marker = strstr(p, "!`");
        if (!marker) {
            size_t remaining = strlen(p);
            if (pos + remaining + 1 > out_cap) {
                out_cap = pos + remaining + 1024;
                char *nb = realloc(out, out_cap);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            memcpy(out + pos, p, remaining);
            pos += remaining;
            break;
        }

        /* Copy text before !` */
        size_t before = (size_t)(marker - p);
        if (pos + before + 1 > out_cap) {
            out_cap = pos + before + 1024;
            char *nb = realloc(out, out_cap);
            if (!nb) { free(out); return NULL; }
            out = nb;
        }
        memcpy(out + pos, p, before);
        pos += before;
        p = marker + 2;

        /* Find closing ` */
        const char *close = strchr(p, '`');
        if (!close) {
            /* No closing backtick: copy rest verbatim */
            size_t remaining = strlen(p);
            if (pos + remaining + 1 > out_cap) {
                out_cap = pos + remaining + 1024;
                char *nb = realloc(out, out_cap);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            memcpy(out + pos, p, remaining);
            pos += remaining;
            break;
        }

        /* Extract command */
        size_t cmd_len = (size_t)(close - p);
        char cmd[4096];
        if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
        memcpy(cmd, p, cmd_len);
        cmd[cmd_len] = '\0';

        /* Execute and append output */
        char *result = run_inline_shell(cmd, timeout_sec);
        if (result) {
            size_t rlen = strlen(result);
            if (pos + rlen + 1 > out_cap) {
                out_cap = pos + rlen + 1024;
                char *nb = realloc(out, out_cap);
                if (!nb) { free(out); free(result); return NULL; }
                out = nb;
            }
            memcpy(out + pos, result, rlen);
            pos += rlen;
            free(result);
        }

        p = close + 1;
    }

    out[pos] = '\0';
    return out;
}

/* ================================================================
 *  Main preprocessing entry point
 * ================================================================ */

/* Apply configured SKILL.md preprocessing.
 * Returns malloc'd string (caller must free) or NULL on error.
 * If template_vars is true, substitutes ${HERMES_SKILL_DIR/SESSION_ID}.
 * If inline_shell is true, expands !`cmd` snippets. */
char *skill_preprocess_content(const char *content,
                                const char *skill_dir,
                                const char *session_id,
                                bool template_vars,
                                bool inline_shell,
                                int inline_shell_timeout) {
    if (!content) return NULL;

    const char *current = content;

    if (template_vars) {
        char *subbed = skill_substitute_template_vars(current, skill_dir, session_id);
        if (!subbed) return NULL;
        if (inline_shell) {
            char *expanded = skill_expand_inline_shell(subbed, inline_shell_timeout);
            free(subbed);
            return expanded ? expanded : strdup(content);
        }
        return subbed;
    }

    if (inline_shell) {
        return skill_expand_inline_shell(current, inline_shell_timeout);
    }

    return strdup(content);
}
