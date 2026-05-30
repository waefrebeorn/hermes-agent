/*
 * test_wecom_callback.c — Tests for WeCom callback utilities.
 *
 * Tests wecom_xml_extract_tag() and wecom_callback_user_app_key().
 */

#include "hermes_wecom_callback.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_tests = 0;
static int g_pass = 0;

#define TEST(name, expr) do { \
    g_tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        g_pass++; \
    } \
} while(0)

/* ─── Sample WeCom callback XML ────────────────────────── */

static const char *SAMPLE_XML =
    "<xml>\n"
    "<ToUserName><![CDATA[wxCorpId]]></ToUserName>\n"
    "<FromUserName><![CDATA[FromUser]]></FromUserName>\n"
    "<CreateTime>123456789</CreateTime>\n"
    "<MsgType><![CDATA[text]]></MsgType>\n"
    "<Content><![CDATA[Hello, world!]]></Content>\n"
    "<MsgId>987654321</MsgId>\n"
    "<AgentID>1</AgentID>\n"
    "</xml>";

static const char *SAMPLE_EVENT_XML =
    "<xml>\n"
    "<ToUserName><![CDATA[wxCorpId]]></ToUserName>\n"
    "<FromUserName><![CDATA[FromUser]]></FromUserName>\n"
    "<CreateTime>123456789</CreateTime>\n"
    "<MsgType><![CDATA[event]]></MsgType>\n"
    "<Event><![CDATA[subscribe]]></Event>\n"
    "</xml>";

/* ─── Test wecom_xml_extract_tag ───────────────────────── */

static void test_xml_extract_tag(void)
{
    char buf[256];

    /* 1: Extract CDATA text */
    memset(buf, 0, sizeof(buf));
    int ret = wecom_xml_extract_tag(SAMPLE_XML, "MsgType", buf, sizeof(buf));
    TEST("MsgType found", ret == 0);
    TEST("MsgType = text", strcmp(buf, "text") == 0);

    /* 2: Extract plain text (no CDATA) */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_XML, "CreateTime", buf, sizeof(buf));
    TEST("CreateTime found", ret == 0);
    TEST("CreateTime = 123456789", strcmp(buf, "123456789") == 0);

    /* 3: Extract Content with CDATA */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_XML, "Content", buf, sizeof(buf));
    TEST("Content found", ret == 0);
    TEST("Content = Hello, world!", strcmp(buf, "Hello, world!") == 0);

    /* 4: Extract ToUserName */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_XML, "ToUserName", buf, sizeof(buf));
    TEST("ToUserName found", ret == 0);
    TEST("ToUserName = wxCorpId", strcmp(buf, "wxCorpId") == 0);

    /* 5: Extract FromUserName */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_XML, "FromUserName", buf, sizeof(buf));
    TEST("FromUserName found", ret == 0);
    TEST("FromUserName = FromUser", strcmp(buf, "FromUser") == 0);

    /* 6: Extract MsgId */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_XML, "MsgId", buf, sizeof(buf));
    TEST("MsgId found", ret == 0);
    TEST("MsgId = 987654321", strcmp(buf, "987654321") == 0);

    /* 7: Extract AgentID */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_XML, "AgentID", buf, sizeof(buf));
    TEST("AgentID found", ret == 0);
    TEST("AgentID = 1", strcmp(buf, "1") == 0);

    /* 8: Nonexistent tag */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_XML, "NonExistent", buf, sizeof(buf));
    TEST("NonExistent tag not found", ret == -1);

    /* 9: NULL xml */
    ret = wecom_xml_extract_tag(NULL, "MsgType", buf, sizeof(buf));
    TEST("NULL xml returns -1", ret == -1);

    /* 10: NULL tag */
    ret = wecom_xml_extract_tag(SAMPLE_XML, NULL, buf, sizeof(buf));
    TEST("NULL tag returns -1", ret == -1);

    /* 11: NULL out */
    ret = wecom_xml_extract_tag(SAMPLE_XML, "MsgType", NULL, 0);
    TEST("NULL out returns -1", ret == -1);

    /* 12: Buffer too small */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_XML, "Content", buf, 1);
    TEST("Small buffer returns -1", ret == -1);

    /* 13: Event XML - MsgType */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_EVENT_XML, "MsgType", buf, sizeof(buf));
    TEST("Event MsgType found", ret == 0);
    TEST("Event MsgType = event", strcmp(buf, "event") == 0);

    /* 14: Event XML - Event tag */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(SAMPLE_EVENT_XML, "Event", buf, sizeof(buf));
    TEST("Event tag found", ret == 0);
    TEST("Event = subscribe", strcmp(buf, "subscribe") == 0);

    /* 15: Empty XML */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag("", "MsgType", buf, sizeof(buf));
    TEST("Empty xml returns -1", ret == -1);

    /* 16: Plain text tag (no CDATA) */
    ret = wecom_xml_extract_tag("<xml><Item>plain</Item></xml>", "Item", buf, sizeof(buf));
    TEST("Plain text tag found", ret == 0);
    TEST("Plain text = plain", strcmp(buf, "plain") == 0);

    /* 17: Empty tag value */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag("<xml><Empty></Empty></xml>", "Empty", buf, sizeof(buf));
    TEST("Empty tag found", ret == 0);
    TEST("Empty tag value", strcmp(buf, "") == 0);

    /* 18: CDATA with newlines */
    memset(buf, 0, sizeof(buf));
    ret = wecom_xml_extract_tag(
        "<xml><Text><![CDATA[line1\nline2]]></Text></xml>",
        "Text", buf, sizeof(buf));
    TEST("CDATA with newline found", ret == 0);
    TEST("CDATA newline content", strcmp(buf, "line1\nline2") == 0);
}

/* ─── Test wecom_callback_user_app_key ─────────────────── */

static void test_user_app_key(void)
{
    char buf[256];

    /* 1: Both corp_id and user_id */
    memset(buf, 0, sizeof(buf));
    int ret = wecom_callback_user_app_key("wxCorpId", "FromUser", buf, sizeof(buf));
    TEST("user_app_key both", ret == 0);
    TEST("key = wxCorpId:FromUser", strcmp(buf, "wxCorpId:FromUser") == 0);

    /* 2: Empty corp_id */
    memset(buf, 0, sizeof(buf));
    ret = wecom_callback_user_app_key("", "FromUser", buf, sizeof(buf));
    TEST("user_app_key empty corp", ret == 0);
    TEST("key = FromUser", strcmp(buf, "FromUser") == 0);

    /* 3: NULL corp_id (should handle as empty) */
    memset(buf, 0, sizeof(buf));
    ret = wecom_callback_user_app_key(NULL, "FromUser", buf, sizeof(buf));
    TEST("user_app_key NULL corp", ret == 0);
    TEST("key = FromUser (NULL corp)", strcmp(buf, "FromUser") == 0);

    /* 4: NULL user_id */
    memset(buf, 0, sizeof(buf));
    ret = wecom_callback_user_app_key("wxCorpId", NULL, buf, sizeof(buf));
    TEST("user_app_key NULL user", ret == -1);

    /* 5: NULL out */
    ret = wecom_callback_user_app_key("wxCorpId", "FromUser", NULL, 0);
    TEST("user_app_key NULL out", ret == -1);

    /* 6: Buffer too small */
    memset(buf, 0, sizeof(buf));
    ret = wecom_callback_user_app_key("wxCorpId", "FromUser", buf, 1);
    TEST("user_app_key small buf", ret == -1);

    /* 7: Long values */
    memset(buf, 0, sizeof(buf));
    char long_corp[128];
    char long_user[128];
    memset(long_corp, 'A', 120);
    long_corp[120] = '\0';
    memset(long_user, 'B', 120);
    long_user[120] = '\0';
    ret = wecom_callback_user_app_key(long_corp, long_user, buf, sizeof(buf));
    TEST("user_app_key long values", ret == 0);
    TEST("key starts with AAAA", strncmp(buf, long_corp, 10) == 0);
    TEST("key contains colon", strstr(buf, ":") != NULL);
}

/* ─── Test wecom_callback_build_event ────────────────── */

static void test_build_event(void)
{
    wecom_callback_event_t ev;

    /* 1: Text message parsing */
    memset(&ev, 0, sizeof(ev));
    int ret = wecom_callback_build_event(SAMPLE_XML, "wxCorpId", &ev);
    TEST("build_event text success", ret == 0);
    TEST("build_event msg_type", strcmp(ev.msg_type, "text") == 0);
    TEST("build_event from_user", strcmp(ev.from_user_name, "FromUser") == 0);
    TEST("build_event content", strcmp(ev.content, "Hello, world!") == 0);
    TEST("build_event msg_id", strcmp(ev.msg_id, "987654321") == 0);
    TEST("build_event create_time", strcmp(ev.create_time, "123456789") == 0);
    TEST("build_event not lifecycle", ev.is_lifecycle == false);
    TEST("build_event scoped_chat_id", strcmp(ev.scoped_chat_id, "wxCorpId:FromUser") == 0);

    /* 2: Event message (subscribe) */
    memset(&ev, 0, sizeof(ev));
    ret = wecom_callback_build_event(SAMPLE_EVENT_XML, "wxCorpId", &ev);
    TEST("build_event event success", ret == 0);
    TEST("build_event event msg_type", strcmp(ev.msg_type, "event") == 0);
    TEST("build_event event.name", strcmp(ev.event, "subscribe") == 0);
    TEST("build_event event is_lifecycle", ev.is_lifecycle == true);
    TEST("build_event event content /start", strcmp(ev.content, "/start") == 0);

    /* 3: Null/empty inputs */
    memset(&ev, 0, sizeof(ev));
    ret = wecom_callback_build_event(NULL, "wxCorpId", &ev);
    TEST("build_event NULL xml", ret == -1);
    memset(&ev, 0, sizeof(ev));
    ret = wecom_callback_build_event(SAMPLE_XML, NULL, &ev);
    TEST("build_event NULL corp_id", ret == 0);
    TEST("build_event NULL corp_id chat_id", strcmp(ev.scoped_chat_id, "FromUser") == 0);

    /* 4: Missing MsgType (invalid XML) */
    memset(&ev, 0, sizeof(ev));
    ret = wecom_callback_build_event("<xml><NoMsgType/></xml>", "corp", &ev);
    TEST("build_event invalid xml", ret == -1);

    /* 5: Lifecycle event - enter_agent */
    const char *enter_agent_xml =
        "<xml>\n"
        "<ToUserName>corp</ToUserName>\n"
        "<FromUserName>User</FromUserName>\n"
        "<CreateTime>111</CreateTime>\n"
        "<MsgType><![CDATA[event]]></MsgType>\n"
        "<Event><![CDATA[enter_agent]]></Event>\n"
        "</xml>";
    memset(&ev, 0, sizeof(ev));
    ret = wecom_callback_build_event(enter_agent_xml, "corp", &ev);
    TEST("build_event enter_agent success", ret == 0);
    TEST("build_event enter_agent is_lifecycle", ev.is_lifecycle == true);

    /* 6: Unknown event type (not text or event) */
    const char *unknown_xml =
        "<xml>\n"
        "<MsgType><![CDATA[image]]></MsgType>\n"
        "</xml>";
    memset(&ev, 0, sizeof(ev));
    ret = wecom_callback_build_event(unknown_xml, "corp", &ev);
    TEST("build_event unknown type", ret == -1);
}

int main(void)
{
    printf("=== WeCom Callback Tests ===\n");

    test_xml_extract_tag();
    test_user_app_key();
    test_build_event();

    printf("=== Results: %d/%d passed ===\n", g_pass, g_tests);
    return (g_pass == g_tests) ? 0 : 1;
}
