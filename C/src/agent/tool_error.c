/*
 * tool_error.c — Tool error sanitization implementation.
 * Port of Python model_tools._sanitize_tool_error().
 *
 * Strips structural framing tokens from tool error messages:
 *   - XML role tags (<tool_call>, </function_call>, <result>, etc.)
 *   - Markdown code fence opens (```, ```json, etc.)
 *   - Markdown code fence closes (trailing ```)
 *   - CDATA sections (<![CDATA[...]]>)
 *
 * All operations are O(n) single-pass string processing.
 */

#include "hermes_tool_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  XML role tag stripping
 * ================================================================
 *
 * Strips tags matching: </?(tool_call|function_call|result|response|
 *                            output|input|system|assistant|user)>
 *
 * Case-insensitive matching on tag name, tags must be well-formed
 * (angle-bracket delimited).
 */

static const char *ROLE_TAG_NAMES[] = {
    "tool_call", "function_call", "result", "response",
    "output", "input", "system", "assistant", "user",
    NULL /* sentinel */
};

/* Check if 'p' points to a role tag opener (<tag>, </tag>).
 * Returns length of entire tag (including < and >), or 0 if no match. */
static int match_role_tag(const char *p) {
    if (!p || *p != '<') return 0;

    const char *start = p;
    p++; /* skip '<' */

    bool is_close = (*p == '/');
    if (is_close) p++;

    /* Try each role tag name */
    for (int i = 0; ROLE_TAG_NAMES[i]; i++) {
        const char *name = ROLE_TAG_NAMES[i];
        const char *q = p;
        const char *n = name;

        /* Case-insensitive name match */
        while (*n && *q && (tolower((unsigned char)*q) == tolower((unsigned char)*n))) {
            q++; n++;
        }
        if (*n) continue; /* didn't match full name */

        /* Must be followed by '>' */
        if (*q == '>') {
            return (int)(q - start + 1); /* include '>' */
        }
    }

    return 0;
}

/* ================================================================
 *  Code fence stripping
 * ================================================================
 *
 * Fence open: lines starting with ``` possibly followed by json/xml/html/markdown
 * Fence close: lines ending with ```
 */

/* Check if 'p' starts a code fence open (``` at line start with optional lang).
 * Returns length of fence + lang marker, or 0. */
static int match_fence_open(const char *p) {
    if (!p) return 0;
    /* Must be at line start or string start */
    if (p > (const char *)p) {
        /* check if previous char is newline or we're at start */
        const char *prev = p - 1;
        if (*prev != '\n' && prev > (const char *)p - 100) {
            /* Not at line start */
            return 0;
        }
    }

    if (p[0] != '`' || p[1] != '`' || p[2] != '`') return 0;

    const char *q = p + 3; /* past ``` */

    /* Optional language tag: json, xml, html, markdown */
    const char *langs[] = {"json", "xml", "html", "markdown", NULL};
    if (*q) {
        /* Skip whitespace after ``` before lang */
        while (*q && isspace((unsigned char)*q)) q++;

        for (int i = 0; langs[i]; i++) {
            const char *lang = langs[i];
            const char *r = q;
            const char *l = lang;
            while (*l && *r && tolower((unsigned char)*r) == (unsigned char)*l) {
                r++; l++;
            }
            if (*l == '\0') {
                /* Matched language — check it's end of word */
                if (!*r || isspace((unsigned char)*r) || *r == '\n') {
                    return (int)(r - p);
                }
            }
        }
        /* No language match — the ``` line is just ``` */
    }

    /* Just the ``` itself — match up to newline */
    while (*q && *q != '\n') q++;
    return (int)(q - p);
}

/* Check if *p is at a code fence close (trailing ```).
 * The Python pattern is: \s*```\s*$
 * We match: optional whitespace, ```, optional whitespace, end of line */
static int match_fence_close(const char *p) {
    if (!p) return 0;

    const char *q = p;

    /* Skip leading whitespace */
    while (*q && isspace((unsigned char)*q) && *q != '\n') q++;

    /* Check for ``` */
    if (q[0] != '`' || q[1] != '`' || q[2] != '`') return 0;

    const char *r = q + 3;

    /* Skip trailing whitespace */
    while (*r && isspace((unsigned char)*r) && *r != '\n') r++;

    /* Must be at end of line */
    if (*r == '\n' || *r == '\0') {
        return (int)(r - p);
    }

    return 0;
}

/* ================================================================
 *  CDATA stripping
 * ================================================================
 *
 * Match: <![CDATA[...]]>
 * We use a simple substring search since the pattern is flat.
 */

/* Find and strip one CDATA section from buf, writing result to out.
 * Returns bytes written to out, or 0 if no CDATA found. */
static int strip_cdata_block(const char *buf, char *out, size_t out_sz) {
    if (!buf || !out || out_sz == 0) return 0;

    const char *cdata_start = strstr(buf, "<![CDATA[");
    if (!cdata_start) return 0;

    const char *cdata_end = strstr(cdata_start, "]]>");
    if (!cdata_end) return 0;

    size_t prefix_len = (size_t)(cdata_start - buf);
    size_t suffix_len = strlen(cdata_end + 3);

    if (prefix_len + suffix_len >= out_sz)
        return 0;

    memcpy(out, buf, prefix_len);
    memcpy(out + prefix_len, cdata_end + 3, suffix_len);
    out[prefix_len + suffix_len] = '\0';

    return (int)(prefix_len + suffix_len);
}

/* ================================================================
 *  Public API
 * ================================================================ */

char *tool_error_sanitize(const char *error_msg) {
    if (!error_msg || !*error_msg) {
        return strdup("[TOOL_ERROR] ");
    }

    /* First, strip CDATA sections (they can contain nested content) */
    char *cdata_scratch = NULL;
    const char *input = error_msg;

    if (strstr(error_msg, "<![CDATA[")) {
        /* CDATA found — allocate scratch buffer and strip ALL CDATA blocks */
        size_t in_len = strlen(error_msg);
        cdata_scratch = (char *)malloc(in_len + 1);
        if (!cdata_scratch) goto done;

        /* Loop: strip one CDATA block at a time until none left */
        char *current = cdata_scratch;
        const char *src = error_msg;
        size_t remaining = in_len + 1;
        int stripped = 0;

        while (1) {
            int result_len = strip_cdata_block(src, current, remaining);
            if (result_len <= 0) {
                /* No more CDATA — copy remaining and break */
                memcpy(current, src, strlen(src) + 1);
                break;
            }
            stripped = 1;
            /* Continue iterating on the newly stripped buffer for nested CDATA */
            src = current;
            size_t cur_len = strlen(current);
            /* If no progress, break to avoid infinite loop */
            if (cur_len == 0 || cur_len >= in_len) break;
        }

        if (stripped) {
            input = cdata_scratch;
        }
    }

    /* Pass 1: strip role tags, fence opens, fence closes */
    size_t in_len = strlen(input);
    /* Allocate worst case: same size as input (stripping only removes) */
    char *result = (char *)malloc(in_len * 2 + 64);
    if (!result) {
        free(cdata_scratch);
        return strdup("[TOOL_ERROR] ");
    }

    size_t pos = 0;
    const char *p = input;

    while (*p) {
        int tag_len;

        /* Try role tag */
        tag_len = match_role_tag(p);
        if (tag_len > 0) {
            p += tag_len;
            continue;
        }

        /* Try fence close (check first to distinguish from fence open) */
        tag_len = match_fence_close(p);
        if (tag_len > 0) {
            p += tag_len;
            continue;
        }

        /* Try fence open (only at line start) */
        tag_len = match_fence_open(p);
        if (tag_len > 0) {
            p += tag_len;
            continue;
        }

        /* Copy character */
        result[pos++] = *p;
        if (pos >= in_len * 2 + 60) break; /* safety */
        p++;
    }
    result[pos] = '\0';

    free(cdata_scratch);

    /* Truncate if too long */
    if (pos > TOOL_ERROR_MAX_LEN) {
        result[TOOL_ERROR_MAX_LEN - 3] = '\0';
        size_t trunc_len = strlen(result);
        /* Append "..." */
        size_t dots_needed = 3;
        if (trunc_len + dots_needed < TOOL_ERROR_MAX_LEN + 4) {
            memcpy(result + trunc_len, "...", 3);
            result[trunc_len + 3] = '\0';
        }
    }

    /* Build final output with prefix */
    char final[TOOL_ERROR_MAX_LEN + 32];
    snprintf(final, sizeof(final), "[TOOL_ERROR] %s", result);
    free(result);

    return strdup(final);

done:
    /* Fallback: just add prefix */
    char fallback[TOOL_ERROR_MAX_LEN + 32];
    int fn = snprintf(fallback, sizeof(fallback), "[TOOL_ERROR] %s", error_msg);
    if (fn > (int)(TOOL_ERROR_MAX_LEN + 31))
        fn = TOOL_ERROR_MAX_LEN + 31;
    if ((size_t)fn > TOOL_ERROR_MAX_LEN + 31) {
        /* Truncate */
        fallback[TOOL_ERROR_MAX_LEN + 31] = '\0';
        size_t flen = strlen(fallback);
        if (flen > 3) {
            memcpy(fallback + flen - 3, "...", 3);
        }
    }
    return strdup(fallback);
}
