/*
 * agent_message_sanitize.c — Post-response message sanitization pipeline.
 *
 * Port of the sanitization steps inside Python's build_assistant_message().
 * Applied to each assistant message after creation to ensure content
 * is clean before entering conversation history:
 *
 *   1. Surrogate character sanitization  (sanitize_surrogates)
 *   2. Think block stripping              (inline tag stripper)
 *   3. Secret redaction                   (hermes_redact)
 */

#include "hermes.h"
#include "hermes_think_scrubber.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* ================================================================
 *  Think Block Stripping (non-streaming)
 *
 *  Strips <think>, <thinking>, <reasoning>, <thought>,
 *  <REASONING_SCRATCHPAD> blocks from complete text.
 *  Matches Python agent_runtime_helpers.strip_think_blocks().
 * ================================================================ */

/* Tag definitions — must match think_scrubber.c */
#define STRIP_TAG_COUNT 5
static const char *STRIP_OPEN[] = {
    "<think>", "<thinking>", "<reasoning>", "<thought>", "<REASONING_SCRATCHPAD>"
};
static const char *STRIP_CLOSE[] = {
    "</think>", "</thinking>", "</reasoning>", "</thought>", "</REASONING_SCRATCHPAD>"
};

/* Additional tool-call XML blocks to strip (Gemma-style inline tool calls) */
#define TOOLCALL_TAG_COUNT 6
static const char *TOOLCALL_TAGS[] = {
    "tool_call", "tool_calls", "tool_result",
    "function_call", "function_calls", "function"
};

/* Case-insensitive match at position pos */
static int tag_match_at(const char *text, int text_len, int pos,
                        const char *tag, int tag_len) {
    if (pos + tag_len > text_len) return 0;
    for (int j = 0; j < tag_len; j++) {
        if (tolower((unsigned char)text[pos + j]) != tolower((unsigned char)tag[j]))
            return 0;
    }
    return 1;
}

/* Find next open tag (any variant) starting from pos. Returns -1 if none. */
static int find_next_open(const char *text, int text_len, int pos,
                          int *tag_idx, int *tag_pos) {
    for (int i = pos; i < text_len; i++) {
        if (text[i] != '<') continue;
        for (int t = 0; t < STRIP_TAG_COUNT; t++) {
            int tlen = (int)strlen(STRIP_OPEN[t]);
            if (tag_match_at(text, text_len, i, STRIP_OPEN[t], tlen)) {
                *tag_idx = t;
                *tag_pos = i;
                return 1;
            }
        }
    }
    return 0;
}

/* Find close tag for given tag index starting from pos. Returns -1 if none. */
static int find_close_for(const char *text, int text_len, int pos, int tag_idx) {
    int tlen = (int)strlen(STRIP_CLOSE[tag_idx]);
    for (int i = pos; i <= text_len - tlen; i++) {
        if (tag_match_at(text, text_len, i, STRIP_CLOSE[tag_idx], tlen))
            return i + tlen;
    }
    return -1;
}

/* Strip all think/reasoning blocks and tool-call XML from text.
 * Returns malloc'd string; caller must free. Returns NULL on NULL input. */
static char *strip_think_blocks(const char *text) {
    if (!text) return NULL;

    int text_len = (int)strlen(text);
    if (text_len == 0) return strdup("");

    /* Allocate worst-case output buffer (same size) */
    char *out = (char *)malloc((size_t)text_len + 1);
    if (!out) return NULL;
    int out_pos = 0;
    int src_pos = 0;

    /* Pass 1: Strip closed <tag>...</tag> pairs for reasoning tags */
    while (src_pos < text_len) {
        int tag_idx, open_pos;
        if (find_next_open(text, text_len, src_pos, &tag_idx, &open_pos)) {
            int close_pos = find_close_for(text, text_len,
                                          open_pos + (int)strlen(STRIP_OPEN[tag_idx]),
                                          tag_idx);
            if (close_pos >= 0) {
                /* Copy everything before the open tag */
                while (src_pos < open_pos && out_pos < text_len)
                    out[out_pos++] = text[src_pos++];
                /* Skip the pair */
                src_pos = close_pos;
                continue;
            }
        }
        if (out_pos < text_len)
            out[out_pos++] = text[src_pos++];
    }
    out[out_pos] = '\0';

    /* Re-alloc after pass 1 */
    int cur_len = out_pos;
    char *cur = (char *)realloc(out, (size_t)cur_len + 1);
    if (!cur) { free(out); return NULL; }
    out = cur;

    /* Pass 2: Strip tool-call XML blocks (<tool_call>...</tool_call>, etc.) */
    {
        int out2_pos = 0;
        int src2_pos = 0;
        int cur2_len = cur_len;
        char *out2 = (char *)malloc((size_t)cur2_len + 1);
        if (!out2) { free(out); return NULL; }

        while (src2_pos < cur2_len) {
            int found = 0;
            for (int t = 0; t < TOOLCALL_TAG_COUNT && !found; t++) {
                char open_buf[64];
                int olen = snprintf(open_buf, sizeof(open_buf), "<%s", TOOLCALL_TAGS[t]);
                char close_buf[64];
                int clen = snprintf(close_buf, sizeof(close_buf), "</%s>", TOOLCALL_TAGS[t]);

                if (tag_match_at(out, cur2_len, src2_pos, open_buf, olen)) {
                    /* Find matching close tag */
                    for (int i = src2_pos + olen; i <= cur2_len - clen; i++) {
                        if (tag_match_at(out, cur2_len, i, close_buf, clen)) {
                            /* Skip entire block: open + content + close */
                            src2_pos = i + clen;
                            found = 1;
                            break;
                        }
                    }
                }
            }
            if (!found) {
                if (out2_pos < cur2_len)
                    out2[out2_pos++] = out[src2_pos++];
            }
        }
        out2[out2_pos] = '\0';
        free(out);

        cur = (char *)realloc(out2, (size_t)out2_pos + 1);
        if (!cur) { free(out2); return NULL; }
        out = cur;
        cur_len = out2_pos;
    }

    /* Pass 3: Strip unterminated open tags at block boundaries */
    {
        int out3_pos = 0;
        int src3_pos = 0;
        int boundary_start = 1;
        char *out3 = (char *)malloc((size_t)cur_len + 1);
        if (!out3) { free(out); return NULL; }

        while (src3_pos < cur_len) {
            /* Check for open tag at boundary */
            int tag_idx, open_pos = -1;
            int found = 0;
            if (boundary_start || (src3_pos > 0 && out[src3_pos - 1] == '\n')) {
                /* Check for open tag starting here or after optional whitespace */
                int check = src3_pos;
                while (check < cur_len && (out[check] == ' ' || out[check] == '\t'))
                    check++;
                if (find_next_open(out, cur_len, check, &tag_idx, &open_pos) &&
                    open_pos == check) {
                    /* Unterminated open tag — strip to end */
                    found = 1;
                }
            }
            if (found) {
                break;  /* strip remaining */
            }
            out3[out3_pos++] = out[src3_pos++];
            if (out[src3_pos - 1] == '\n')
                boundary_start = 1;
        }
        out3[out3_pos] = '\0';
        free(out);

        char *shrunk = (char *)realloc(out3, (size_t)out3_pos + 1);
        if (!shrunk) { free(out3); return NULL; }
        out = shrunk;
        cur_len = out3_pos;
    }

    /* Pass 4: Strip orphan tags (any remaining open/close reasoning tags) */
    {
        int out4_pos = 0;
        int src4_pos = 0;
        char *out4 = (char *)malloc((size_t)cur_len + 1);
        if (!out4) { free(out); return NULL; }

        while (src4_pos < cur_len) {
            int stripped = 0;
            /* Orphan open tag */
            int tag_idx;
            int open_pos;
            if (find_next_open(out, cur_len, src4_pos, &tag_idx, &open_pos) &&
                open_pos == src4_pos) {
                int tlen = (int)strlen(STRIP_OPEN[tag_idx]);
                src4_pos += tlen;
                stripped = 1;
            }
            /* Orphan close tag */
            if (!stripped) {
                for (int t = 0; t < STRIP_TAG_COUNT && !stripped; t++) {
                    int clen = (int)strlen(STRIP_CLOSE[t]);
                    if (tag_match_at(out, cur_len, src4_pos, STRIP_CLOSE[t], clen)) {
                        src4_pos += clen;
                        /* skip trailing whitespace */
                        while (src4_pos < cur_len &&
                               (out[src4_pos] == ' ' || out[src4_pos] == '\t' ||
                                out[src4_pos] == '\n' || out[src4_pos] == '\r'))
                            src4_pos++;
                        stripped = 1;
                    }
                }
            }
            if (!stripped) {
                out4[out4_pos++] = out[src4_pos++];
            }
        }
        out4[out4_pos] = '\0';
        free(out);

        char *shrunk = (char *)realloc(out4, (size_t)out4_pos + 1);
        if (!shrunk) { free(out4); return NULL; }
        out = shrunk;
    }

    return out;
}

/* ================================================================
 *  Public API: hermes_message_sanitize
 * ================================================================ */

/* Sanitize a single assistant message:
 *   1. Sanitize surrogates in content and reasoning
 *   2. Strip think/reasoning blocks from content
 *   3. Redact sensitive text from content and tool call arguments
 *
 * Returns true on success (always succeeds, even if no changes needed).
 * Returns false only if allocation fails for ALL fields.
 */
bool hermes_message_sanitize(message_t *msg) {
    if (!msg) return false;
    bool any_ok = false;

    /* Step 1: Surrogate sanitization on content */
    if (msg->content) {
        char *sanitized = sanitize_surrogates(msg->content);
        if (sanitized) {
            if (strcmp(sanitized, msg->content) != 0) {
                free(msg->content);
                msg->content = sanitized;
            } else {
                free(sanitized);
            }
            any_ok = true;
        }
    }

    /* Step 2: Surrogate sanitization on reasoning */
    if (msg->reasoning) {
        char *sanitized = sanitize_surrogates(msg->reasoning);
        if (sanitized) {
            if (strcmp(sanitized, msg->reasoning) != 0) {
                free(msg->reasoning);
                msg->reasoning = sanitized;
            } else {
                free(sanitized);
            }
            any_ok = true;
        }
    }

    /* Step 3: Strip think blocks from content */
    if (msg->content) {
        char *stripped = strip_think_blocks(msg->content);
        if (stripped) {
            if (strcmp(stripped, msg->content) != 0) {
                free(msg->content);
                msg->content = stripped;
            } else {
                free(stripped);
            }
            any_ok = true;
        }
    }

    /* Step 4: Redact sensitive text from content */
    if (msg->content) {
        char *redacted = hermes_redact(msg->content);
        if (redacted) {
            free(msg->content);
            msg->content = redacted;
            any_ok = true;
        }
    }

    /* Step 5: Redact sensitive text from tool call arguments */
    for (int i = 0; i < msg->tool_calls_count && i < 64; i++) {
        if (msg->tool_calls[i].arguments[0]) {
            char *redacted = hermes_redact(msg->tool_calls[i].arguments);
            if (redacted) {
                strncpy(msg->tool_calls[i].arguments, redacted,
                        sizeof(msg->tool_calls[i].arguments) - 1);
                msg->tool_calls[i].arguments[sizeof(msg->tool_calls[i].arguments) - 1] = '\0';
                free(redacted);
                any_ok = true;
            }
        }
    }

    return any_ok;
}
