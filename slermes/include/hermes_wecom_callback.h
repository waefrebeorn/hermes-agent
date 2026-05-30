/**
 * @file hermes_wecom_callback.h
 * @brief WeCom callback mode utilities — XML parsing for callback messages.
 *
 * Provides XML tag extraction and helper functions for processing
 * WeCom callback payloads (decrypted XML messages).
 *
 * Port of Python gateway/platforms/wecom_callback.py (425 LOC, 20 functions).
 * Phase 181: XML tag extraction + user_app_key.
 */
#ifndef HERMES_WECOM_CALLBACK_H
#define HERMES_WECOM_CALLBACK_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extract the content of an XML tag from simple XML.
 *
 * Handles both plain text and CDATA sections:
 *   <Tag>value</Tag>          -> "value"
 *   <Tag><![CDATA[value]]></Tag> -> "value"
 *
 * Uses string search (no XML parser dependency).
 *
 * @param xml       NUL-terminated XML string
 * @param tag       Tag name (e.g. "MsgType", "Content")
 * @param out       Buffer for extracted content
 * @param out_size  Size of output buffer
 * @return 0 on success, -1 if tag not found or buffer too small
 */
int wecom_xml_extract_tag(const char *xml, const char *tag,
                          char *out, size_t out_size);

/**
 * Build a WeCom callback user_app_key from corp_id and user_id.
 *
 * Port of Python _user_app_key() — returns "%s:%s" format.
 *
 * @param corp_id   WeCom corp ID (may be empty)
 * @param user_id   WeCom user ID
 * @param out       Buffer for result (at least out_size bytes)
 * @param out_size  Size of output buffer
 * @return 0 on success, -1 if buffer too small
 */
int wecom_callback_user_app_key(const char *corp_id, const char *user_id,
                                char *out, size_t out_size);

/**
 * Event fields extracted from decrypted WeCom callback XML.
 * Port of Python _build_event() return values.
 */
typedef struct {
    char  msg_type[32];            /* "text", "event", etc. */
    char  event[64];               /* "enter_agent", "subscribe", etc. */
    char  from_user_name[128];     /* WeCom user ID */
    char  to_user_name[128];       /* Corp ID */
    char  content[4096];           /* Message text (stripped) */
    char  msg_id[256];             /* Unique message ID */
    char  create_time[32];         /* Raw create time string */
    char  scoped_chat_id[512];     /* corp_id:user_id scoped key */
    bool  is_lifecycle;            /* True for enter_agent/subscribe */
} wecom_callback_event_t;

/**
 * Parse a decrypted WeCom callback XML message into event fields.
 *
 * Extracts MsgType, Event, FromUserName, ToUserName, Content, MsgId,
 * CreateTime. Filters lifecycle events (enter_agent, subscribe).
 * Builds scoped_chat_id from corp_id and from_user_name.
 *
 * @param xml_text    Decrypted WeCom callback XML (NUL-terminated)
 * @param corp_id     WeCom corp ID for scoping
 * @param event       Output event structure (caller-allocated)
 * @return 0 on success, -1 if XML could not be parsed
 */
int wecom_callback_build_event(const char *xml_text, const char *corp_id,
                                wecom_callback_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_WECOM_CALLBACK_H */
