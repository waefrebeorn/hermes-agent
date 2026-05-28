/*
 * test_discord_interactions.c — M08: Discord interaction parsing tests.
 * Tests: slash commands, modals, components, interaction metadata,
 * message parsing, edge cases.
 */
#include "hermes.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

int main(void) {
    printf("=== Discord Interaction Tests (M08) ===\n\n");

    /* ──────────── Interaction type detection ──────────── */
    printf("--- Interaction Type Detection ---\n");

    {
        /* Interaction type 2 = slash command */
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":\"123456789\",\"type\":2,\"name\":\"hello\","
            "\"token\":\"TOKEN123\"},"
            "\"channel_id\":\"456789\",\"guild_id\":\"789123\"}", NULL);
        TEST("interaction type 2 found",
             update && discord_get_interaction_type(update) == 2);
        json_free(update);
    }

    {
        /* Interaction type 3 = component (button/select) */
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":\"111\",\"type\":3,\"token\":\"T\"}}", NULL);
        TEST("interaction type 3 found",
             update && discord_get_interaction_type(update) == 3);
        json_free(update);
    }

    {
        /* Interaction type 4 = modal submit */
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":\"222\",\"type\":5,\"token\":\"T\"}}", NULL);
        TEST("interaction type 5 found",
             update && discord_get_interaction_type(update) == 5);
        json_free(update);
    }

    {
        /* No interaction (regular message) */
        json_node_t *update = json_parse(
            "{\"channel_id\":\"123\",\"content\":\"hello\"}", NULL);
        TEST("no interaction returns 0",
             update && discord_get_interaction_type(update) == 0);
        json_free(update);
    }

    {
        /* NULL safety */
        TEST("NULL interaction type returns 0",
             discord_get_interaction_type(NULL) == 0);
    }

    /* ──────────── Interaction ID extraction ──────────── */
    printf("\n--- Interaction ID Extraction ---\n");

    {
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":999888777,\"type\":2,\"token\":\"tok\"}}", NULL);
        const char *id = discord_get_interaction_id(update);
        TEST("interaction ID extracted as string",
             id && strlen(id) > 0);
        json_free(update);
    }

    {
        /* No interaction */
        json_node_t *update = json_parse("{\"content\":\"hi\"}", NULL);
        TEST("no interaction returns NULL for ID",
             discord_get_interaction_id(update) == NULL);
        json_free(update);
    }

    {
        TEST("NULL returns NULL for ID",
             discord_get_interaction_id(NULL) == NULL);
    }

    /* ──────────── Interaction token extraction ──────────── */
    printf("\n--- Interaction Token Extraction ---\n");

    {
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":1,\"type\":2,\"token\":\"aW52YWxpZDEyMzQ\"}}", NULL);
        const char *tok = discord_get_interaction_token(update);
        TEST("interaction token extracted",
             tok && strcmp(tok, "aW52YWxpZDEyMzQ") == 0);
        json_free(update);
    }

    {
        json_node_t *update = json_parse("{\"content\":\"hi\"}", NULL);
        TEST("no interaction returns NULL for token",
             discord_get_interaction_token(update) == NULL);
        json_free(update);
    }

    {
        TEST("NULL returns NULL for token",
             discord_get_interaction_token(NULL) == NULL);
    }

    /* ──────────── Channel ID extraction ──────────── */
    printf("\n--- Channel ID Extraction ---\n");

    /* discord_get_chat_id looks for update["message"]["chat"]["channel_id"] */
    {
        json_node_t *update = json_parse(
            "{\"message\":{\"chat\":{\"channel_id\":\"123456789\"}}}", NULL);
        const char *cid = discord_get_chat_id(update);
        TEST("channel_id extracted from message.chat",
             cid && strcmp(cid, "123456789") == 0);
        json_free(update);
    }

    {
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":1},\"message\":{\"chat\":{\"channel_id\":\"987\"}}}", NULL);
        const char *cid = discord_get_chat_id(update);
        TEST("channel_id with interaction and message.chat",
             cid && strcmp(cid, "987") == 0);
        json_free(update);
    }

    {
        json_node_t *update = json_parse("{}", NULL);
        /* Falls back to empty global channel_id when no message.chat */
        const char *cid = discord_get_chat_id(update);
        TEST("empty update returns global channel_id (empty string)",
             cid && cid[0] == '\0');
        json_free(update);
    }

    {
        TEST("NULL returns NULL for chat_id",
             discord_get_chat_id(NULL) == NULL);
    }

    /* ──────────── Text/message content extraction ──────────── */
    printf("\n--- Message Content Extraction ---\n");

    /* discord_get_text looks for update["message"]["text"] */
    {
        json_node_t *update = json_parse(
            "{\"message\":{\"text\":\"Hello world!\",\"author\":{\"bot\":false}}}", NULL);
        const char *text = discord_get_text(update);
        TEST("regular message text extracted from message.text",
             text && strcmp(text, "Hello world!") == 0);
        json_free(update);
    }

    {
        /* Slash command interaction data */
        json_node_t *update = json_parse(
            "{\"message\":{\"text\":\"/deploy env=prod\",\"interaction\":{\"type\":2,\"name\":\"deploy\"},"
            "\"data\":{\"options\":[{\"name\":\"env\",\"value\":\"prod\"}]}}}", NULL);
        const char *text = discord_get_text(update);
        TEST("slash command text includes /deploy",
             text && strstr(text, "/deploy") != NULL);
        json_free(update);
    }

    {
        /* Empty text */
        json_node_t *update = json_parse("{\"message\":{\"text\":\"\"}}", NULL);
        const char *text = discord_get_text(update);
        TEST("empty text returns empty string",
             text && strcmp(text, "") == 0);
        json_free(update);
    }

    {
        /* No message object */
        json_node_t *update = json_parse("{}", NULL);
        TEST("no message object returns NULL",
             discord_get_text(update) == NULL);
        json_free(update);
    }

    {
        /* NULL safety */
        TEST("NULL returns NULL for text",
             discord_get_text(NULL) == NULL);
    }

    /* ──────────── Modal/Component data ──────────── */
    printf("\n--- Modal/Component Interaction Type ---\n");

    {
        /* Modal submit interaction (type 5) */
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":1,\"type\":5,\"token\":\"t\"},"
            "\"message\":{\"text\":\"modal data: custom_id=ticket_form, value=user input text\"}}", NULL);
        const char *text = discord_get_text(update);
        TEST("modal text includes custom_id",
             text && strstr(text, "ticket_form") != NULL);
        TEST("modal text includes value",
             text && strstr(text, "user input text") != NULL);
        json_free(update);
    }

    {
        /* Button component interaction (type 3) */
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":1,\"type\":3,\"token\":\"t\"},"
            "\"message\":{\"text\":\"button: approve_btn\"}}", NULL);
        const char *text = discord_get_text(update);
        TEST("button interaction text included",
             text && strstr(text, "approve_btn") != NULL);
        json_free(update);
    }

    {
        /* Select menu interaction */
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":1,\"type\":3,\"token\":\"t\"},"
            "\"message\":{\"text\":\"select: role_select, values=admin,mod\"}}", NULL);
        const char *text = discord_get_text(update);
        TEST("select menu text includes role_select",
             text && strstr(text, "role_select") != NULL);
        TEST("select menu text includes values",
             text && strstr(text, "admin") != NULL);
        json_free(update);
    }

    /* ──────────── Edge cases ──────────── */
    printf("\n--- Edge Cases ---\n");

    {
        /* Interaction with missing fields */
        json_node_t *update = json_parse(
            "{\"interaction\":{\"id\":1}}", NULL);
        TEST("missing token in interaction",
             discord_get_interaction_token(update) == NULL);
        TEST("missing type in interaction returns 0",
             discord_get_interaction_type(update) == 0);
        json_free(update);
    }

    {
        /* Very long text */
        char long_text[4096];
        memset(long_text, 'A', sizeof(long_text) - 1);
        long_text[sizeof(long_text) - 1] = '\0';
        char json_str[8192];
        snprintf(json_str, sizeof(json_str),
                 "{\"message\":{\"text\":\"%s\"}}", long_text);
        json_node_t *update = json_parse(json_str, NULL);
        const char *text = discord_get_text(update);
        TEST("long text handled",
             text && strlen(text) > 1000);
        json_free(update);
    }

    /* ──────────── Summary ──────────── */
    printf("\n========================================\n");
    printf("  Results: %s\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    printf("  Failures: %d\n", failures);
    printf("========================================\n");

    return failures > 0 ? 1 : 0;
}
