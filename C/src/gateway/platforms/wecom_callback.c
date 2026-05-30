/*
 * wecom_callback.c — WeCom callback mode utilities.
 *
 * Port of Python gateway/platforms/wecom_callback.py (425 LOC, 20 functions).
 * Phase 181: XML tag extraction + user_app_key.
 *
 * These utility functions handle parsing WeCom callback XML messages
 * (decrypted by wecom_crypto.c) into their constituent fields.
 *
 * Python functions ported in this phase:
 *   - _user_app_key()  → wecom_callback_user_app_key()
 *   - _build_event()   → wecom_xml_extract_tag() (dependency)
 *                        + callback field extraction
 */

#include "hermes_wecom_callback.h"
#include <stdio.h>
#include <string.h>

/* ─── XML tag extraction ────────────────────────────────── */

int wecom_xml_extract_tag(const char *xml, const char *tag,
                          char *out, size_t out_size)
{
    if (!xml || !tag || !out || out_size == 0)
        return -1;

    out[0] = '\0';

    /* Build open/close tag patterns: <tag> and </tag> */
    size_t tag_len = strlen(tag);
    if (tag_len == 0) return -1;

    /* Maximum tag size: < + tag + > = tag_len + 2, but +1 for null is fine */
    char open_tag[256];
    char close_tag[256];
    if (tag_len + 3 > sizeof(open_tag) || tag_len + 4 > sizeof(close_tag))
        return -1;

    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    /* Find opening tag */
    const char *open = strstr(xml, open_tag);
    if (!open)
        return -1;

    /* Content starts after opening tag */
    const char *content_start = open + strlen(open_tag);

    /* Find closing tag */
    const char *close = strstr(content_start, close_tag);
    if (!close)
        return -1;

    size_t content_len = (size_t)(close - content_start);

    /* Handle CDATA section */
    const char *cdata_start = strstr(content_start, "<![CDATA[");
    if (cdata_start && cdata_start < close) {
        const char *cdata_content = cdata_start + 9; /* skip "<![CDATA[" */
        const char *cdata_end = strstr(cdata_content, "]]>");
        if (cdata_end && cdata_end < close) {
            content_start = cdata_content;
            content_len = (size_t)(cdata_end - cdata_content);
        }
    }

    /* Trim leading/trailing whitespace */
    while (content_len > 0 && (content_start[0] == ' ' ||
           content_start[0] == '\n' || content_start[0] == '\r' ||
           content_start[0] == '\t')) {
        content_start++;
        content_len--;
    }
    while (content_len > 0 && (content_start[content_len - 1] == ' ' ||
           content_start[content_len - 1] == '\n' ||
           content_start[content_len - 1] == '\r' ||
           content_start[content_len - 1] == '\t')) {
        content_len--;
    }

    if (content_len >= out_size)
        return -1;

    memcpy(out, content_start, content_len);
    out[content_len] = '\0';
    return 0;
}

/* ─── User app key (corp_id:user_id) ───────────────────── */

int wecom_callback_user_app_key(const char *corp_id, const char *user_id,
                                char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return -1;

    out[0] = '\0';

    if (!user_id)
        return -1;

    if (corp_id && corp_id[0]) {
        int n = snprintf(out, out_size, "%s:%s", corp_id, user_id);
        if (n < 0 || (size_t)n >= out_size)
            return -1;
    } else {
        size_t len = strlen(user_id);
        if (len >= out_size)
            return -1;
        memcpy(out, user_id, len);
        out[len] = '\0';
    }
    return 0;
}
