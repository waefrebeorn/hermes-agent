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

int main(void)
{
    printf("=== WeCom Callback Tests ===\n");

    test_xml_extract_tag();
    test_user_app_key();

    printf("=== Results: %d/%d passed ===\n", g_pass, g_tests);
    return (g_pass == g_tests) ? 0 : 1;
}
