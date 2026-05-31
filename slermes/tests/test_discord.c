/*
 * test_discord.c — T06: Discord tool unit tests.
 *
 * Tests argument validation, action dispatch, and error handling.
 * For action dispatch tests, sets DISCORD_BOT_TOKEN to bypass
 * the "not set" gate (unsetenv after to avoid env pollution).
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       -I lib/libhttp \
 *       tests/test_discord.c src/tools/discord.c src/tools/registry.c \
 *       src/tools/api_helpers.c src/tools/tool_config.c src/agent/vault.c \
 *       lib/libjson/json.c lib/libhttp/http.c lib/libcrypto/crypto.c \
 *       lib/libtoolbackend/tool_backend.c \
 *       -o /tmp/hermes_test_discord -lm -lssl -lcrypto -lz
 *
 * Run:
 *   /tmp/hermes_test_discord
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

/* External handler from discord.c */
extern char *discord_handler(const char *args_json, const char *task_id);

/* Test helpers */
static int g_passed = 0, g_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS do { printf("PASS\n"); g_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_failed++; } while(0)

/* Check if result JSON contains a substring */
static bool json_contains(const char *json_str, const char *sub) {
    return json_str && strstr(json_str, sub) != NULL;
}

/* Set a fake DISCORD_BOT_TOKEN so action dispatch paths work */
static void set_token(void) {
    setenv("DISCORD_BOT_TOKEN", "test_token_12345", 1);
}

static void clear_token(void) {
    unsetenv("DISCORD_BOT_TOKEN");
}

/* === Tests === */

static bool test_empty_args(void) {
    clear_token();
    char *result = discord_handler("", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "error")) { FAIL("expected error"); free(result); return false; }
    free(result);
    PASS; return true;
}

static bool test_null_args(void) {
    clear_token();
    char *result = discord_handler(NULL, "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "error")) { FAIL("expected error"); free(result); return false; }
    free(result);
    PASS; return true;
}

static bool test_invalid_json(void) {
    clear_token();
    char *result = discord_handler("{bad json}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "error")) { FAIL("expected error"); free(result); return false; }
    free(result);
    PASS; return true;
}

static bool test_missing_action(void) {
    set_token();
    char *result = discord_handler("{\"guild_id\":\"123\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "Missing 'action'")) {
        FAIL("expected 'Missing action' error"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_unknown_action(void) {
    set_token();
    char *result = discord_handler("{\"action\":\"nonexistent\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "Unknown")) {
        FAIL("expected 'Unknown action' error"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_no_token(void) {
    clear_token();
    char *result = discord_handler("{\"action\":\"list_guilds\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "not set")) {
        FAIL("expected token not set error"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_missing_guild_id(void) {
    set_token();
    /* Actions requiring guild_id should error when missing */
    char *result = discord_handler("{\"action\":\"guild_info\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "guild_id required")) {
        FAIL("expected 'guild_id required'"); free(result); return false;
    }
    free(result);

    result = discord_handler("{\"action\":\"list_channels\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "guild_id required")) {
        FAIL("expected 'guild_id required' for list_channels"); free(result); return false;
    }
    free(result);

    result = discord_handler("{\"action\":\"list_roles\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "guild_id required")) {
        FAIL("expected 'guild_id required' for list_roles"); free(result); return false;
    }
    free(result);

    PASS; return true;
}

static bool test_missing_channel_id(void) {
    set_token();
    char *result = discord_handler("{\"action\":\"channel_info\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "channel_id required")) {
        FAIL("expected 'channel_id required'"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_missing_user_id(void) {
    set_token();
    char *result = discord_handler("{\"action\":\"member_info\",\"guild_id\":\"123\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "user_id required")) {
        FAIL("expected 'user_id required'"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_send_message_missing_params(void) {
    set_token();
    /* Missing channel_id */
    char *result = discord_handler("{\"action\":\"send_message\",\"content\":\"hello\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "channel_id required")) {
        FAIL("expected 'channel_id required'"); free(result); return false;
    }
    free(result);

    /* Missing content */
    result = discord_handler("{\"action\":\"send_message\",\"channel_id\":\"456\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "content required")) {
        FAIL("expected 'content required'"); free(result); return false;
    }
    free(result);

    PASS; return true;
}

static bool test_delete_message_missing_params(void) {
    set_token();
    char *result = discord_handler("{\"action\":\"delete_message\",\"channel_id\":\"456\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "message_id required")) {
        FAIL("expected 'message_id required'"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_admin_action_missing_params(void) {
    set_token();
    /* create_channel: missing guild_id */
    char *result = discord_handler("{\"action\":\"create_channel\",\"name\":\"test\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "guild_id and name required")) {
        FAIL("expected guild_id/name error for create_channel"); free(result); return false;
    }
    free(result);

    /* kick_member: missing user_id */
    result = discord_handler("{\"action\":\"kick_member\",\"guild_id\":\"123\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "required")) {
        FAIL("expected required params error for kick_member"); free(result); return false;
    }
    free(result);

    /* ban_member: missing user_id */
    result = discord_handler("{\"action\":\"ban_member\",\"guild_id\":\"123\"}", "test");
    if (!result) { FAIL("null result"); return false; }
    if (!json_contains(result, "required")) {
        FAIL("expected required params error for ban_member"); free(result); return false;
    }
    free(result);

    PASS; return true;
}

static bool test_admin_action_string_matching(void) {
    set_token();
    /* Verify that admin actions can be dispatched via the main handler */
    /* These will actually call the action handler (no token needed for string match test) */
    char *result = discord_handler("{\"action\":\"create_channel\",\"guild_id\":\"123\",\"name\":\"test\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (json_contains(result, "Unknown")) {
        FAIL("create_channel should be recognized, not unknown"); free(result); return false;
    }
    free(result);

    result = discord_handler("{\"action\":\"kick_member\",\"guild_id\":\"123\",\"user_id\":\"456\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (json_contains(result, "Unknown")) {
        FAIL("kick_member should be recognized, not unknown"); free(result); return false;
    }
    free(result);

    result = discord_handler("{\"action\":\"ban_member\",\"guild_id\":\"123\",\"user_id\":\"456\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (json_contains(result, "Unknown")) {
        FAIL("ban_member should be recognized, not unknown"); free(result); return false;
    }
    free(result);

    PASS; return true;
}

/* ─── Phase 405: More action edge cases ─── */

static bool test_empty_action(void) {
    set_token();
    char *result = discord_handler("{\"action\":\"\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (!json_contains(result, "Missing 'action'")) {
        FAIL("expected 'Missing action' for empty action"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_fetch_messages_missing_channel(void) {
    set_token();
    char *result = discord_handler("{\"action\":\"fetch_messages\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (!json_contains(result, "channel_id required")) {
        FAIL("expected 'channel_id required' for fetch_messages"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_search_members_missing_params(void) {
    set_token();
    /* Missing guild_id */
    char *result = discord_handler("{\"action\":\"search_members\",\"query\":\"test\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (!json_contains(result, "required")) {
        FAIL("expected required params error for search_members"); free(result); return false;
    }
    free(result);
    /* Missing query */
    result = discord_handler("{\"action\":\"search_members\",\"guild_id\":\"123\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (!json_contains(result, "required")) {
        FAIL("expected required params error for search_members (no query)"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_pin_unpin_missing_params(void) {
    set_token();
    /* pin_message: missing message_id */
    char *result = discord_handler("{\"action\":\"pin_message\",\"channel_id\":\"123\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (!json_contains(result, "required")) {
        FAIL("expected required error for pin_message (no message_id)"); free(result); return false;
    }
    free(result);
    /* unpin_message: missing channel_id */
    result = discord_handler("{\"action\":\"unpin_message\",\"message_id\":\"456\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (!json_contains(result, "channel_id")) {
        FAIL("expected 'channel_id required' for unpin_message"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_create_thread_missing_params(void) {
    set_token();
    /* Missing name */
    char *result = discord_handler("{\"action\":\"create_thread\",\"channel_id\":\"123\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (!json_contains(result, "required")) {
        FAIL("expected required error for create_thread (no name)"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

static bool test_role_mgmt_missing_params(void) {
    set_token();
    /* add_role: missing role_id */
    char *result = discord_handler("{\"action\":\"add_role\",\"guild_id\":\"1\",\"user_id\":\"2\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (!json_contains(result, "required")) {
        FAIL("expected required error for add_role (no role_id)"); free(result); return false;
    }
    free(result);
    /* remove_role: missing user_id */
    result = discord_handler("{\"action\":\"remove_role\",\"guild_id\":\"1\",\"role_id\":\"3\"}", "test");
    if (!result) { FAIL("null result"); free(result); return false; }
    if (!json_contains(result, "required")) {
        FAIL("expected required error for remove_role (no user_id)"); free(result); return false;
    }
    free(result);
    PASS; return true;
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== Discord Tool Tests (T06) ===\n");

    TEST("empty args");           test_empty_args();
    TEST("null args");            test_null_args();
    TEST("invalid JSON");         test_invalid_json();
    TEST("missing action");       test_missing_action();
    TEST("unknown action");       test_unknown_action();
    TEST("no token");             test_no_token();
    TEST("missing guild_id");     test_missing_guild_id();
    TEST("missing channel_id");   test_missing_channel_id();
    TEST("missing user_id");      test_missing_user_id();
    TEST("send_message params");  test_send_message_missing_params();
    TEST("delete_message params"); test_delete_message_missing_params();
    TEST("admin action params");  test_admin_action_missing_params();
    TEST("admin action dispatch"); test_admin_action_string_matching();
    /* Phase 405 */
    TEST("empty action");         test_empty_action();
    TEST("fetch_messages channel"); test_fetch_messages_missing_channel();
    TEST("search_members params"); test_search_members_missing_params();
    TEST("pin/unpin params");     test_pin_unpin_missing_params();
    TEST("create_thread params"); test_create_thread_missing_params();
    TEST("role mgmt params");     test_role_mgmt_missing_params();

    printf("\nResults: %d/%d passed, %d failed\n", g_passed, g_passed + g_failed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
