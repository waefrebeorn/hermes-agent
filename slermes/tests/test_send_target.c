/* test_send_target.c — Tests for parse_send_target().
 * Compile: gcc -O2 -Wall -Wextra -I../include test_send_target.c \
 *          ../src/tools/send_message.c ../lib/libjson/json.c \
 *          -o /tmp/t_send_target -lm -Wl,--unresolved-symbols=ignore-all
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Forward declaration from send_message.c */
typedef struct {
    char platform[64];
    char chat_id[256];
    char thread_id[64];
} send_target_t;
bool parse_send_target(const char *target, const char *platform_override, send_target_t *out);

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); failures++; } \
    else printf("  PASS: %s\n", name); \
} while(0)

int main(void) {
    printf("=== Parse Send Target Tests ===\n\n");

    /* Test 1: platform:chat_id */
    {
        send_target_t t;
        bool ok = parse_send_target("telegram:-100123456", NULL, &t);
        TEST("platform:chat_id returns true", ok);
        TEST("platform extracted", strcmp(t.platform, "telegram") == 0);
        TEST("chat_id extracted", strcmp(t.chat_id, "-100123456") == 0);
        TEST("thread_id empty", t.thread_id[0] == '\0');
    }

    /* Test 2: platform:chat_id:thread_id */
    {
        send_target_t t;
        bool ok = parse_send_target("discord:999888777:555444333", NULL, &t);
        TEST("platform:chat_id:thread_id returns true", ok);
        TEST("platform extracted", strcmp(t.platform, "discord") == 0);
        TEST("chat_id extracted", strcmp(t.chat_id, "999888777") == 0);
        TEST("thread_id extracted", strcmp(t.thread_id, "555444333") == 0);
    }

    /* Test 3: bare stdout */
    {
        send_target_t t;
        bool ok = parse_send_target("stdout", NULL, &t);
        TEST("bare stdout returns true", ok);
        TEST("bare stdout platform empty", t.platform[0] == '\0');
        TEST("bare stdout chat_id empty", t.chat_id[0] == '\0');
    }

    /* Test 4: bare local */
    {
        send_target_t t;
        bool ok = parse_send_target("local", NULL, &t);
        TEST("bare local returns true", ok);
        TEST("bare local platform empty", t.platform[0] == '\0');
    }

    /* Test 5: platform_override with bare target */
    {
        send_target_t t;
        bool ok = parse_send_target("local", "stdout", &t);
        TEST("override + bare target returns true", ok);
        TEST("override sets platform", strcmp(t.platform, "stdout") == 0);
        TEST("override + no colon = no chat_id", t.chat_id[0] == '\0');
    }

    /* Test 6: platform_override with colon target */
    {
        send_target_t t;
        bool ok = parse_send_target("telegram:-100123456", "discord", &t);
        TEST("override + colon target returns true", ok);
        TEST("override sets platform", strcmp(t.platform, "discord") == 0);
        TEST("chat_id still parsed from target", strcmp(t.chat_id, "-100123456") == 0);
    }

    /* Test 7: override with thread_id in target */
    {
        send_target_t t;
        bool ok = parse_send_target("telegram:-100123456:42", "signal", &t);
        TEST("override + thread_id returns true", ok);
        TEST("override platform", strcmp(t.platform, "signal") == 0);
        TEST("chat_id from target", strcmp(t.chat_id, "-100123456") == 0);
        TEST("thread_id from target", strcmp(t.thread_id, "42") == 0);
    }

    /* Test 8: NULL target */
    {
        send_target_t t;
        bool ok = parse_send_target(NULL, NULL, &t);
        TEST("NULL target returns false", !ok);
    }

    /* Test 9: Empty target */
    {
        send_target_t t;
        bool ok = parse_send_target("", NULL, &t);
        TEST("empty target returns false", !ok);
    }

    /* Test 10: Target with only colon at start (bare target) */
    {
        send_target_t t;
        bool ok = parse_send_target(":chatid", NULL, &t);
        TEST("colon at start = bare target (no error)", ok);
        TEST("bare target platform empty", t.platform[0] == '\0');
    }

    /* Test 11: Long platform name within limits */
    {
        send_target_t t;
        char long_target[256];
        snprintf(long_target, sizeof(long_target), "%.50s:chat", "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqr");
        bool ok = parse_send_target(long_target, NULL, &t);
        TEST("long platform fits", ok);
        TEST("long platform extracted", t.platform[0] != '\0');
    }

    /* Test 12: platform: prefix without chat_id (edge case) */
    {
        send_target_t t;
        bool ok = parse_send_target("telegram:", NULL, &t);
        TEST("platform: with empty chat_id", ok);
        TEST("platform extracted", strcmp(t.platform, "telegram") == 0);
        TEST("chat_id empty for 'telegram:'", t.chat_id[0] == '\0');
    }

    /* Test 13: override + no colon + bare stdout */
    {
        send_target_t t;
        bool ok = parse_send_target("stdout", "telegram", &t);
        TEST("override + stdout target", ok);
        TEST("platform is override", strcmp(t.platform, "telegram") == 0);
        TEST("chat_id empty", t.chat_id[0] == '\0');
    }

    /* Test 14: Very long chat_id truncated */
    {
        send_target_t t;
        char huge[512];
        snprintf(huge, sizeof(huge), "telegram:%.300s", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
        bool ok = parse_send_target(huge, NULL, &t);
        TEST("long chat_id handled without crash", ok);
        TEST("platform correct", strcmp(t.platform, "telegram") == 0);
    }

    /* Test 15: Real telegram chat ID with thread */
    {
        send_target_t t;
        bool ok = parse_send_target("telegram:-1001234567890:17585", NULL, &t);
        TEST("real format returns true", ok);
        TEST("real chat_id", strcmp(t.chat_id, "-1001234567890") == 0);
        TEST("real thread_id", strcmp(t.thread_id, "17585") == 0);
    }

    /* Test 16: Bare Telegram topic format without platform prefix */
    {
        send_target_t t;
        bool ok = parse_send_target("-1001234567890:42", NULL, &t);
        TEST("bare telegram topic returns true", ok);
        TEST("bare platform is telegram", strcmp(t.platform, "telegram") == 0);
        TEST("bare chat_id", strcmp(t.chat_id, "-1001234567890") == 0);
        TEST("bare thread_id", strcmp(t.thread_id, "42") == 0);
    }

    /* Test 17: Bare short negative number without colon — not a telegram topic */
    {
        send_target_t t;
        bool ok = parse_send_target("-42", NULL, &t);
        TEST("bare negative number returns true", ok);
        TEST("bare negative has no platform (use target directly)", t.platform[0] == '\0');
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All parse_send_target tests PASSED");
    return failures ? 1 : 0;
}
